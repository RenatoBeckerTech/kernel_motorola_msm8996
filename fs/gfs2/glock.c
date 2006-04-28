/*
 * Copyright (C) Sistina Software, Inc.  1997-2003 All rights reserved.
 * Copyright (C) 2004-2005 Red Hat, Inc.  All rights reserved.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <linux/delay.h>
#include <linux/sort.h>
#include <linux/jhash.h>
#include <linux/kref.h>
#include <linux/kallsyms.h>
#include <linux/gfs2_ondisk.h>
#include <asm/semaphore.h>
#include <asm/uaccess.h>

#include "gfs2.h"
#include "lm_interface.h"
#include "incore.h"
#include "glock.h"
#include "glops.h"
#include "inode.h"
#include "lm.h"
#include "lops.h"
#include "meta_io.h"
#include "quota.h"
#include "super.h"
#include "util.h"

/*  Must be kept in sync with the beginning of struct gfs2_glock  */
struct glock_plug {
	struct list_head gl_list;
	unsigned long gl_flags;
};

struct greedy {
	struct gfs2_holder gr_gh;
	struct work_struct gr_work;
};

typedef void (*glock_examiner) (struct gfs2_glock * gl);

static int gfs2_dump_lockstate(struct gfs2_sbd *sdp);

/**
 * relaxed_state_ok - is a requested lock compatible with the current lock mode?
 * @actual: the current state of the lock
 * @requested: the lock state that was requested by the caller
 * @flags: the modifier flags passed in by the caller
 *
 * Returns: 1 if the locks are compatible, 0 otherwise
 */

static inline int relaxed_state_ok(unsigned int actual, unsigned requested,
				   int flags)
{
	if (actual == requested)
		return 1;

	if (flags & GL_EXACT)
		return 0;

	if (actual == LM_ST_EXCLUSIVE && requested == LM_ST_SHARED)
		return 1;

	if (actual != LM_ST_UNLOCKED && (flags & LM_FLAG_ANY))
		return 1;

	return 0;
}

/**
 * gl_hash() - Turn glock number into hash bucket number
 * @lock: The glock number
 *
 * Returns: The number of the corresponding hash bucket
 */

static unsigned int gl_hash(struct lm_lockname *name)
{
	unsigned int h;

	h = jhash(&name->ln_number, sizeof(uint64_t), 0);
	h = jhash(&name->ln_type, sizeof(unsigned int), h);
	h &= GFS2_GL_HASH_MASK;

	return h;
}

/**
 * glock_free() - Perform a few checks and then release struct gfs2_glock
 * @gl: The glock to release
 *
 * Also calls lock module to release its internal structure for this glock.
 *
 */

static void glock_free(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct inode *aspace = gl->gl_aspace;

	gfs2_lm_put_lock(sdp, gl->gl_lock);

	if (aspace)
		gfs2_aspace_put(aspace);

	kmem_cache_free(gfs2_glock_cachep, gl);
}

/**
 * gfs2_glock_hold() - increment reference count on glock
 * @gl: The glock to hold
 *
 */

void gfs2_glock_hold(struct gfs2_glock *gl)
{
	kref_get(&gl->gl_ref);
}

/* All work is done after the return from kref_put() so we
   can release the write_lock before the free. */

static void kill_glock(struct kref *kref)
{
	struct gfs2_glock *gl = container_of(kref, struct gfs2_glock, gl_ref);
	struct gfs2_sbd *sdp = gl->gl_sbd;

	gfs2_assert(sdp, gl->gl_state == LM_ST_UNLOCKED);
	gfs2_assert(sdp, list_empty(&gl->gl_reclaim));
	gfs2_assert(sdp, list_empty(&gl->gl_holders));
	gfs2_assert(sdp, list_empty(&gl->gl_waiters1));
	gfs2_assert(sdp, list_empty(&gl->gl_waiters2));
	gfs2_assert(sdp, list_empty(&gl->gl_waiters3));
}

/**
 * gfs2_glock_put() - Decrement reference count on glock
 * @gl: The glock to put
 *
 */

int gfs2_glock_put(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_gl_hash_bucket *bucket = gl->gl_bucket;
	int rv = 0;

	mutex_lock(&sdp->sd_invalidate_inodes_mutex);

	write_lock(&bucket->hb_lock);
	if (kref_put(&gl->gl_ref, kill_glock)) {
		list_del_init(&gl->gl_list);
		write_unlock(&bucket->hb_lock);
		BUG_ON(spin_is_locked(&gl->gl_spin));
		glock_free(gl);
		rv = 1;
		goto out;
	}
	write_unlock(&bucket->hb_lock);
 out:
	mutex_unlock(&sdp->sd_invalidate_inodes_mutex);
	return rv;
}

/**
 * queue_empty - check to see if a glock's queue is empty
 * @gl: the glock
 * @head: the head of the queue to check
 *
 * This function protects the list in the event that a process already
 * has a holder on the list and is adding a second holder for itself.
 * The glmutex lock is what generally prevents processes from working
 * on the same glock at once, but the special case of adding a second
 * holder for yourself ("recursive" locking) doesn't involve locking
 * glmutex, making the spin lock necessary.
 *
 * Returns: 1 if the queue is empty
 */

static inline int queue_empty(struct gfs2_glock *gl, struct list_head *head)
{
	int empty;
	spin_lock(&gl->gl_spin);
	empty = list_empty(head);
	spin_unlock(&gl->gl_spin);
	return empty;
}

/**
 * search_bucket() - Find struct gfs2_glock by lock number
 * @bucket: the bucket to search
 * @name: The lock name
 *
 * Returns: NULL, or the struct gfs2_glock with the requested number
 */

static struct gfs2_glock *search_bucket(struct gfs2_gl_hash_bucket *bucket,
					struct lm_lockname *name)
{
	struct gfs2_glock *gl;

	list_for_each_entry(gl, &bucket->hb_list, gl_list) {
		if (test_bit(GLF_PLUG, &gl->gl_flags))
			continue;
		if (!lm_name_equal(&gl->gl_name, name))
			continue;

		kref_get(&gl->gl_ref);

		return gl;
	}

	return NULL;
}

/**
 * gfs2_glock_find() - Find glock by lock number
 * @sdp: The GFS2 superblock
 * @name: The lock name
 *
 * Returns: NULL, or the struct gfs2_glock with the requested number
 */

static struct gfs2_glock *gfs2_glock_find(struct gfs2_sbd *sdp,
					  struct lm_lockname *name)
{
	struct gfs2_gl_hash_bucket *bucket = &sdp->sd_gl_hash[gl_hash(name)];
	struct gfs2_glock *gl;

	read_lock(&bucket->hb_lock);
	gl = search_bucket(bucket, name);
	read_unlock(&bucket->hb_lock);

	return gl;
}

/**
 * gfs2_glock_get() - Get a glock, or create one if one doesn't exist
 * @sdp: The GFS2 superblock
 * @number: the lock number
 * @glops: The glock_operations to use
 * @create: If 0, don't create the glock if it doesn't exist
 * @glp: the glock is returned here
 *
 * This does not lock a glock, just finds/creates structures for one.
 *
 * Returns: errno
 */

int gfs2_glock_get(struct gfs2_sbd *sdp, uint64_t number,
		   struct gfs2_glock_operations *glops, int create,
		   struct gfs2_glock **glp)
{
	struct lm_lockname name;
	struct gfs2_glock *gl, *tmp;
	struct gfs2_gl_hash_bucket *bucket;
	int error;

	name.ln_number = number;
	name.ln_type = glops->go_type;
	bucket = &sdp->sd_gl_hash[gl_hash(&name)];

	read_lock(&bucket->hb_lock);
	gl = search_bucket(bucket, &name);
	read_unlock(&bucket->hb_lock);

	if (gl || !create) {
		*glp = gl;
		return 0;
	}

	gl = kmem_cache_alloc(gfs2_glock_cachep, GFP_KERNEL);
	if (!gl)
		return -ENOMEM;

	memset(gl, 0, sizeof(struct gfs2_glock));

	INIT_LIST_HEAD(&gl->gl_list);
	gl->gl_name = name;
	kref_init(&gl->gl_ref);

	spin_lock_init(&gl->gl_spin);

	gl->gl_state = LM_ST_UNLOCKED;
	INIT_LIST_HEAD(&gl->gl_holders);
	INIT_LIST_HEAD(&gl->gl_waiters1);
	INIT_LIST_HEAD(&gl->gl_waiters2);
	INIT_LIST_HEAD(&gl->gl_waiters3);

	gl->gl_ops = glops;

	gl->gl_bucket = bucket;
	INIT_LIST_HEAD(&gl->gl_reclaim);

	gl->gl_sbd = sdp;

	lops_init_le(&gl->gl_le, &gfs2_glock_lops);
	INIT_LIST_HEAD(&gl->gl_ail_list);

	/* If this glock protects actual on-disk data or metadata blocks,
	   create a VFS inode to manage the pages/buffers holding them. */
	if (glops == &gfs2_inode_glops ||
	    glops == &gfs2_rgrp_glops ||
	    glops == &gfs2_meta_glops) {
		gl->gl_aspace = gfs2_aspace_get(sdp);
		if (!gl->gl_aspace) {
			error = -ENOMEM;
			goto fail;
		}
	}

	error = gfs2_lm_get_lock(sdp, &name, &gl->gl_lock);
	if (error)
		goto fail_aspace;

	write_lock(&bucket->hb_lock);
	tmp = search_bucket(bucket, &name);
	if (tmp) {
		write_unlock(&bucket->hb_lock);
		glock_free(gl);
		gl = tmp;
	} else {
		list_add_tail(&gl->gl_list, &bucket->hb_list);
		write_unlock(&bucket->hb_lock);
	}

	*glp = gl;

	return 0;

 fail_aspace:
	if (gl->gl_aspace)
		gfs2_aspace_put(gl->gl_aspace);

 fail:
	kmem_cache_free(gfs2_glock_cachep, gl);	

	return error;
}

/**
 * gfs2_holder_init - initialize a struct gfs2_holder in the default way
 * @gl: the glock
 * @state: the state we're requesting
 * @flags: the modifier flags
 * @gh: the holder structure
 *
 */

void gfs2_holder_init(struct gfs2_glock *gl, unsigned int state, unsigned flags,
		      struct gfs2_holder *gh)
{
	INIT_LIST_HEAD(&gh->gh_list);
	gh->gh_gl = gl;
	gh->gh_ip = (unsigned long)__builtin_return_address(0);
	gh->gh_owner = current;
	gh->gh_state = state;
	gh->gh_flags = flags;
	gh->gh_error = 0;
	gh->gh_iflags = 0;
	init_completion(&gh->gh_wait);

	if (gh->gh_state == LM_ST_EXCLUSIVE)
		gh->gh_flags |= GL_LOCAL_EXCL;

	gfs2_glock_hold(gl);
}

/**
 * gfs2_holder_reinit - reinitialize a struct gfs2_holder so we can requeue it
 * @state: the state we're requesting
 * @flags: the modifier flags
 * @gh: the holder structure
 *
 * Don't mess with the glock.
 *
 */

void gfs2_holder_reinit(unsigned int state, unsigned flags, struct gfs2_holder *gh)
{
	gh->gh_state = state;
	gh->gh_flags = flags;
	if (gh->gh_state == LM_ST_EXCLUSIVE)
		gh->gh_flags |= GL_LOCAL_EXCL;

	gh->gh_iflags &= 1 << HIF_ALLOCED;
	gh->gh_ip = (unsigned long)__builtin_return_address(0);
}

/**
 * gfs2_holder_uninit - uninitialize a holder structure (drop glock reference)
 * @gh: the holder structure
 *
 */

void gfs2_holder_uninit(struct gfs2_holder *gh)
{
	gfs2_glock_put(gh->gh_gl);
	gh->gh_gl = NULL;
	gh->gh_ip = 0;
}

/**
 * gfs2_holder_get - get a struct gfs2_holder structure
 * @gl: the glock
 * @state: the state we're requesting
 * @flags: the modifier flags
 * @gfp_flags: __GFP_NOFAIL
 *
 * Figure out how big an impact this function has.  Either:
 * 1) Replace it with a cache of structures hanging off the struct gfs2_sbd
 * 2) Leave it like it is
 *
 * Returns: the holder structure, NULL on ENOMEM
 */

static struct gfs2_holder *gfs2_holder_get(struct gfs2_glock *gl,
					   unsigned int state,
					   int flags, gfp_t gfp_flags)
{
	struct gfs2_holder *gh;

	gh = kmalloc(sizeof(struct gfs2_holder), gfp_flags);
	if (!gh)
		return NULL;

	gfs2_holder_init(gl, state, flags, gh);
	set_bit(HIF_ALLOCED, &gh->gh_iflags);
	gh->gh_ip = (unsigned long)__builtin_return_address(0);
	return gh;
}

/**
 * gfs2_holder_put - get rid of a struct gfs2_holder structure
 * @gh: the holder structure
 *
 */

static void gfs2_holder_put(struct gfs2_holder *gh)
{
	gfs2_holder_uninit(gh);
	kfree(gh);
}

/**
 * rq_mutex - process a mutex request in the queue
 * @gh: the glock holder
 *
 * Returns: 1 if the queue is blocked
 */

static int rq_mutex(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;

	list_del_init(&gh->gh_list);
	/*  gh->gh_error never examined.  */
	set_bit(GLF_LOCK, &gl->gl_flags);
	complete(&gh->gh_wait);

	return 1;
}

/**
 * rq_promote - process a promote request in the queue
 * @gh: the glock holder
 *
 * Acquire a new inter-node lock, or change a lock state to more restrictive.
 *
 * Returns: 1 if the queue is blocked
 */

static int rq_promote(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;

	if (!relaxed_state_ok(gl->gl_state, gh->gh_state, gh->gh_flags)) {
		if (list_empty(&gl->gl_holders)) {
			gl->gl_req_gh = gh;
			set_bit(GLF_LOCK, &gl->gl_flags);
			spin_unlock(&gl->gl_spin);

			if (atomic_read(&sdp->sd_reclaim_count) >
			    gfs2_tune_get(sdp, gt_reclaim_limit) &&
			    !(gh->gh_flags & LM_FLAG_PRIORITY)) {
				gfs2_reclaim_glock(sdp);
				gfs2_reclaim_glock(sdp);
			}

			glops->go_xmote_th(gl, gh->gh_state,
					   gh->gh_flags);

			spin_lock(&gl->gl_spin);
		}
		return 1;
	}

	if (list_empty(&gl->gl_holders)) {
		set_bit(HIF_FIRST, &gh->gh_iflags);
		set_bit(GLF_LOCK, &gl->gl_flags);
	} else {
		struct gfs2_holder *next_gh;
		if (gh->gh_flags & GL_LOCAL_EXCL)
			return 1;
		next_gh = list_entry(gl->gl_holders.next, struct gfs2_holder,
				     gh_list);
		if (next_gh->gh_flags & GL_LOCAL_EXCL)
			 return 1;
	}

	list_move_tail(&gh->gh_list, &gl->gl_holders);
	gh->gh_error = 0;
	set_bit(HIF_HOLDER, &gh->gh_iflags);

	complete(&gh->gh_wait);

	return 0;
}

/**
 * rq_demote - process a demote request in the queue
 * @gh: the glock holder
 *
 * Returns: 1 if the queue is blocked
 */

static int rq_demote(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_glock_operations *glops = gl->gl_ops;

	if (!list_empty(&gl->gl_holders))
		return 1;

	if (gl->gl_state == gh->gh_state || gl->gl_state == LM_ST_UNLOCKED) {
		list_del_init(&gh->gh_list);
		gh->gh_error = 0;
		spin_unlock(&gl->gl_spin);
		if (test_bit(HIF_DEALLOC, &gh->gh_iflags))
			gfs2_holder_put(gh);
		else
			complete(&gh->gh_wait);
		spin_lock(&gl->gl_spin);
	} else {
		gl->gl_req_gh = gh;
		set_bit(GLF_LOCK, &gl->gl_flags);
		spin_unlock(&gl->gl_spin);

		if (gh->gh_state == LM_ST_UNLOCKED ||
		    gl->gl_state != LM_ST_EXCLUSIVE)
			glops->go_drop_th(gl);
		else
			glops->go_xmote_th(gl, gh->gh_state, gh->gh_flags);

		spin_lock(&gl->gl_spin);
	}

	return 0;
}

/**
 * rq_greedy - process a queued request to drop greedy status
 * @gh: the glock holder
 *
 * Returns: 1 if the queue is blocked
 */

static int rq_greedy(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;

	list_del_init(&gh->gh_list);
	/*  gh->gh_error never examined.  */
	clear_bit(GLF_GREEDY, &gl->gl_flags);
	spin_unlock(&gl->gl_spin);

	gfs2_holder_uninit(gh);
	kfree(container_of(gh, struct greedy, gr_gh));

	spin_lock(&gl->gl_spin);		

	return 0;
}

/**
 * run_queue - process holder structures on a glock
 * @gl: the glock
 *
 */
static void run_queue(struct gfs2_glock *gl)
{
	struct gfs2_holder *gh;
	int blocked = 1;

	for (;;) {
		if (test_bit(GLF_LOCK, &gl->gl_flags))
			break;

		if (!list_empty(&gl->gl_waiters1)) {
			gh = list_entry(gl->gl_waiters1.next,
					struct gfs2_holder, gh_list);

			if (test_bit(HIF_MUTEX, &gh->gh_iflags))
				blocked = rq_mutex(gh);
			else
				gfs2_assert_warn(gl->gl_sbd, 0);

		} else if (!list_empty(&gl->gl_waiters2) &&
			   !test_bit(GLF_SKIP_WAITERS2, &gl->gl_flags)) {
			gh = list_entry(gl->gl_waiters2.next,
					struct gfs2_holder, gh_list);

			if (test_bit(HIF_DEMOTE, &gh->gh_iflags))
				blocked = rq_demote(gh);
			else if (test_bit(HIF_GREEDY, &gh->gh_iflags))
				blocked = rq_greedy(gh);
			else
				gfs2_assert_warn(gl->gl_sbd, 0);

		} else if (!list_empty(&gl->gl_waiters3)) {
			gh = list_entry(gl->gl_waiters3.next,
					struct gfs2_holder, gh_list);

			if (test_bit(HIF_PROMOTE, &gh->gh_iflags))
				blocked = rq_promote(gh);
			else
				gfs2_assert_warn(gl->gl_sbd, 0);

		} else
			break;

		if (blocked)
			break;
	}
}

/**
 * gfs2_glmutex_lock - acquire a local lock on a glock
 * @gl: the glock
 *
 * Gives caller exclusive access to manipulate a glock structure.
 */

void gfs2_glmutex_lock(struct gfs2_glock *gl)
{
	struct gfs2_holder gh;

	gfs2_holder_init(gl, 0, 0, &gh);
	set_bit(HIF_MUTEX, &gh.gh_iflags);

	spin_lock(&gl->gl_spin);
	if (test_and_set_bit(GLF_LOCK, &gl->gl_flags))
		list_add_tail(&gh.gh_list, &gl->gl_waiters1);
	else
		complete(&gh.gh_wait);
	spin_unlock(&gl->gl_spin);

	wait_for_completion(&gh.gh_wait);
	gfs2_holder_uninit(&gh);
}

/**
 * gfs2_glmutex_trylock - try to acquire a local lock on a glock
 * @gl: the glock
 *
 * Returns: 1 if the glock is acquired
 */

static int gfs2_glmutex_trylock(struct gfs2_glock *gl)
{
	int acquired = 1;

	spin_lock(&gl->gl_spin);
	if (test_and_set_bit(GLF_LOCK, &gl->gl_flags))
		acquired = 0;
	spin_unlock(&gl->gl_spin);

	return acquired;
}

/**
 * gfs2_glmutex_unlock - release a local lock on a glock
 * @gl: the glock
 *
 */

void gfs2_glmutex_unlock(struct gfs2_glock *gl)
{
	spin_lock(&gl->gl_spin);
	clear_bit(GLF_LOCK, &gl->gl_flags);
	run_queue(gl);
	BUG_ON(!spin_is_locked(&gl->gl_spin));
	spin_unlock(&gl->gl_spin);
}

/**
 * handle_callback - add a demote request to a lock's queue
 * @gl: the glock
 * @state: the state the caller wants us to change to
 *
 */

static void handle_callback(struct gfs2_glock *gl, unsigned int state)
{
	struct gfs2_holder *gh, *new_gh = NULL;

 restart:
	spin_lock(&gl->gl_spin);

	list_for_each_entry(gh, &gl->gl_waiters2, gh_list) {
		if (test_bit(HIF_DEMOTE, &gh->gh_iflags) &&
		    gl->gl_req_gh != gh) {
			if (gh->gh_state != state)
				gh->gh_state = LM_ST_UNLOCKED;
			goto out;
		}
	}

	if (new_gh) {
		list_add_tail(&new_gh->gh_list, &gl->gl_waiters2);
		new_gh = NULL;
	} else {
		spin_unlock(&gl->gl_spin);

		new_gh = gfs2_holder_get(gl, state, LM_FLAG_TRY,
					 GFP_KERNEL | __GFP_NOFAIL),
		set_bit(HIF_DEMOTE, &new_gh->gh_iflags);
		set_bit(HIF_DEALLOC, &new_gh->gh_iflags);

		goto restart;
	}

 out:
	spin_unlock(&gl->gl_spin);

	if (new_gh)
		gfs2_holder_put(new_gh);
}

/**
 * state_change - record that the glock is now in a different state
 * @gl: the glock
 * @new_state the new state
 *
 */

static void state_change(struct gfs2_glock *gl, unsigned int new_state)
{
	int held1, held2;

	held1 = (gl->gl_state != LM_ST_UNLOCKED);
	held2 = (new_state != LM_ST_UNLOCKED);

	if (held1 != held2) {
		if (held2)
			gfs2_glock_hold(gl);
		else
			gfs2_glock_put(gl);
	}

	gl->gl_state = new_state;
}

/**
 * xmote_bh - Called after the lock module is done acquiring a lock
 * @gl: The glock in question
 * @ret: the int returned from the lock module
 *
 */

static void xmote_bh(struct gfs2_glock *gl, unsigned int ret)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;
	struct gfs2_holder *gh = gl->gl_req_gh;
	int prev_state = gl->gl_state;
	int op_done = 1;

	gfs2_assert_warn(sdp, test_bit(GLF_LOCK, &gl->gl_flags));
	gfs2_assert_warn(sdp, queue_empty(gl, &gl->gl_holders));
	gfs2_assert_warn(sdp, !(ret & LM_OUT_ASYNC));

	state_change(gl, ret & LM_OUT_ST_MASK);

	if (prev_state != LM_ST_UNLOCKED && !(ret & LM_OUT_CACHEABLE)) {
		if (glops->go_inval)
			glops->go_inval(gl, DIO_METADATA | DIO_DATA);
	} else if (gl->gl_state == LM_ST_DEFERRED) {
		/* We might not want to do this here.
		   Look at moving to the inode glops. */
		if (glops->go_inval)
			glops->go_inval(gl, DIO_DATA);
	}

	/*  Deal with each possible exit condition  */

	if (!gh)
		gl->gl_stamp = jiffies;

	else if (unlikely(test_bit(SDF_SHUTDOWN, &sdp->sd_flags))) {
		spin_lock(&gl->gl_spin);
		list_del_init(&gh->gh_list);
		gh->gh_error = -EIO;
		spin_unlock(&gl->gl_spin);

	} else if (test_bit(HIF_DEMOTE, &gh->gh_iflags)) {
		spin_lock(&gl->gl_spin);
		list_del_init(&gh->gh_list);
		if (gl->gl_state == gh->gh_state ||
		    gl->gl_state == LM_ST_UNLOCKED)
			gh->gh_error = 0;
		else {
			if (gfs2_assert_warn(sdp, gh->gh_flags &
					(LM_FLAG_TRY | LM_FLAG_TRY_1CB)) == -1)
				fs_warn(sdp, "ret = 0x%.8X\n", ret);
			gh->gh_error = GLR_TRYFAILED;
		}
		spin_unlock(&gl->gl_spin);

		if (ret & LM_OUT_CANCELED)
			handle_callback(gl, LM_ST_UNLOCKED); /* Lame */

	} else if (ret & LM_OUT_CANCELED) {
		spin_lock(&gl->gl_spin);
		list_del_init(&gh->gh_list);
		gh->gh_error = GLR_CANCELED;
		spin_unlock(&gl->gl_spin);

	} else if (relaxed_state_ok(gl->gl_state, gh->gh_state, gh->gh_flags)) {
		spin_lock(&gl->gl_spin);
		list_move_tail(&gh->gh_list, &gl->gl_holders);
		gh->gh_error = 0;
		set_bit(HIF_HOLDER, &gh->gh_iflags);
		spin_unlock(&gl->gl_spin);

		set_bit(HIF_FIRST, &gh->gh_iflags);

		op_done = 0;

	} else if (gh->gh_flags & (LM_FLAG_TRY | LM_FLAG_TRY_1CB)) {
		spin_lock(&gl->gl_spin);
		list_del_init(&gh->gh_list);
		gh->gh_error = GLR_TRYFAILED;
		spin_unlock(&gl->gl_spin);

	} else {
		if (gfs2_assert_withdraw(sdp, 0) == -1)
			fs_err(sdp, "ret = 0x%.8X\n", ret);
	}

	if (glops->go_xmote_bh)
		glops->go_xmote_bh(gl);

	if (op_done) {
		spin_lock(&gl->gl_spin);
		gl->gl_req_gh = NULL;
		gl->gl_req_bh = NULL;
		clear_bit(GLF_LOCK, &gl->gl_flags);
		run_queue(gl);
		spin_unlock(&gl->gl_spin);
	}

	gfs2_glock_put(gl);

	if (gh) {
		if (test_bit(HIF_DEALLOC, &gh->gh_iflags))
			gfs2_holder_put(gh);
		else
			complete(&gh->gh_wait);
	}
}

/**
 * gfs2_glock_xmote_th - Call into the lock module to acquire or change a glock
 * @gl: The glock in question
 * @state: the requested state
 * @flags: modifier flags to the lock call
 *
 */

void gfs2_glock_xmote_th(struct gfs2_glock *gl, unsigned int state, int flags)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;
	int lck_flags = flags & (LM_FLAG_TRY | LM_FLAG_TRY_1CB |
				 LM_FLAG_NOEXP | LM_FLAG_ANY |
				 LM_FLAG_PRIORITY);
	unsigned int lck_ret;

	gfs2_assert_warn(sdp, test_bit(GLF_LOCK, &gl->gl_flags));
	gfs2_assert_warn(sdp, queue_empty(gl, &gl->gl_holders));
	gfs2_assert_warn(sdp, state != LM_ST_UNLOCKED);
	gfs2_assert_warn(sdp, state != gl->gl_state);

	if (gl->gl_state == LM_ST_EXCLUSIVE) {
		if (glops->go_sync)
			glops->go_sync(gl,
				       DIO_METADATA | DIO_DATA | DIO_RELEASE);
	}

	gfs2_glock_hold(gl);
	gl->gl_req_bh = xmote_bh;

	lck_ret = gfs2_lm_lock(sdp, gl->gl_lock, gl->gl_state, state,
			       lck_flags);

	if (gfs2_assert_withdraw(sdp, !(lck_ret & LM_OUT_ERROR)))
		return;

	if (lck_ret & LM_OUT_ASYNC)
		gfs2_assert_warn(sdp, lck_ret == LM_OUT_ASYNC);
	else
		xmote_bh(gl, lck_ret);
}

/**
 * drop_bh - Called after a lock module unlock completes
 * @gl: the glock
 * @ret: the return status
 *
 * Doesn't wake up the process waiting on the struct gfs2_holder (if any)
 * Doesn't drop the reference on the glock the top half took out
 *
 */

static void drop_bh(struct gfs2_glock *gl, unsigned int ret)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;
	struct gfs2_holder *gh = gl->gl_req_gh;

	clear_bit(GLF_PREFETCH, &gl->gl_flags);

	gfs2_assert_warn(sdp, test_bit(GLF_LOCK, &gl->gl_flags));
	gfs2_assert_warn(sdp, queue_empty(gl, &gl->gl_holders));
	gfs2_assert_warn(sdp, !ret);

	state_change(gl, LM_ST_UNLOCKED);

	if (glops->go_inval)
		glops->go_inval(gl, DIO_METADATA | DIO_DATA);

	if (gh) {
		spin_lock(&gl->gl_spin);
		list_del_init(&gh->gh_list);
		gh->gh_error = 0;
		spin_unlock(&gl->gl_spin);
	}

	if (glops->go_drop_bh)
		glops->go_drop_bh(gl);

	spin_lock(&gl->gl_spin);
	gl->gl_req_gh = NULL;
	gl->gl_req_bh = NULL;
	clear_bit(GLF_LOCK, &gl->gl_flags);
	run_queue(gl);
	spin_unlock(&gl->gl_spin);

	gfs2_glock_put(gl);

	if (gh) {
		if (test_bit(HIF_DEALLOC, &gh->gh_iflags))
			gfs2_holder_put(gh);
		else
			complete(&gh->gh_wait);
	}
}

/**
 * gfs2_glock_drop_th - call into the lock module to unlock a lock
 * @gl: the glock
 *
 */

void gfs2_glock_drop_th(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;
	unsigned int ret;

	gfs2_assert_warn(sdp, test_bit(GLF_LOCK, &gl->gl_flags));
	gfs2_assert_warn(sdp, queue_empty(gl, &gl->gl_holders));
	gfs2_assert_warn(sdp, gl->gl_state != LM_ST_UNLOCKED);

	if (gl->gl_state == LM_ST_EXCLUSIVE) {
		if (glops->go_sync)
			glops->go_sync(gl,
				       DIO_METADATA | DIO_DATA | DIO_RELEASE);
	}

	gfs2_glock_hold(gl);
	gl->gl_req_bh = drop_bh;

	ret = gfs2_lm_unlock(sdp, gl->gl_lock, gl->gl_state);

	if (gfs2_assert_withdraw(sdp, !(ret & LM_OUT_ERROR)))
		return;

	if (!ret)
		drop_bh(gl, ret);
	else
		gfs2_assert_warn(sdp, ret == LM_OUT_ASYNC);
}

/**
 * do_cancels - cancel requests for locks stuck waiting on an expire flag
 * @gh: the LM_FLAG_PRIORITY holder waiting to acquire the lock
 *
 * Don't cancel GL_NOCANCEL requests.
 */

static void do_cancels(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;

	spin_lock(&gl->gl_spin);

	while (gl->gl_req_gh != gh &&
	       !test_bit(HIF_HOLDER, &gh->gh_iflags) &&
	       !list_empty(&gh->gh_list)) {
		if (gl->gl_req_bh &&
		    !(gl->gl_req_gh &&
		      (gl->gl_req_gh->gh_flags & GL_NOCANCEL))) {
			spin_unlock(&gl->gl_spin);
			gfs2_lm_cancel(gl->gl_sbd, gl->gl_lock);
			msleep(100);
			spin_lock(&gl->gl_spin);
		} else {
			spin_unlock(&gl->gl_spin);
			msleep(100);
			spin_lock(&gl->gl_spin);
		}
	}

	spin_unlock(&gl->gl_spin);
}

/**
 * glock_wait_internal - wait on a glock acquisition
 * @gh: the glock holder
 *
 * Returns: 0 on success
 */

static int glock_wait_internal(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;

	if (test_bit(HIF_ABORTED, &gh->gh_iflags))
		return -EIO;

	if (gh->gh_flags & (LM_FLAG_TRY | LM_FLAG_TRY_1CB)) {
		spin_lock(&gl->gl_spin);
		if (gl->gl_req_gh != gh &&
		    !test_bit(HIF_HOLDER, &gh->gh_iflags) &&
		    !list_empty(&gh->gh_list)) {
			list_del_init(&gh->gh_list);
			gh->gh_error = GLR_TRYFAILED;
			run_queue(gl);
			spin_unlock(&gl->gl_spin);
			return gh->gh_error;
		}
		spin_unlock(&gl->gl_spin);
	}

	if (gh->gh_flags & LM_FLAG_PRIORITY)
		do_cancels(gh);

	wait_for_completion(&gh->gh_wait);

	if (gh->gh_error)
		return gh->gh_error;

	gfs2_assert_withdraw(sdp, test_bit(HIF_HOLDER, &gh->gh_iflags));
	gfs2_assert_withdraw(sdp, relaxed_state_ok(gl->gl_state,
						   gh->gh_state,
						   gh->gh_flags));

	if (test_bit(HIF_FIRST, &gh->gh_iflags)) {
		gfs2_assert_warn(sdp, test_bit(GLF_LOCK, &gl->gl_flags));

		if (glops->go_lock) {
			gh->gh_error = glops->go_lock(gh);
			if (gh->gh_error) {
				spin_lock(&gl->gl_spin);
				list_del_init(&gh->gh_list);
				spin_unlock(&gl->gl_spin);
			}
		}

		spin_lock(&gl->gl_spin);
		gl->gl_req_gh = NULL;
		gl->gl_req_bh = NULL;
		clear_bit(GLF_LOCK, &gl->gl_flags);
		run_queue(gl);
		spin_unlock(&gl->gl_spin);
	}

	return gh->gh_error;
}

static inline struct gfs2_holder *
find_holder_by_owner(struct list_head *head, struct task_struct *owner)
{
	struct gfs2_holder *gh;

	list_for_each_entry(gh, head, gh_list) {
		if (gh->gh_owner == owner)
			return gh;
	}

	return NULL;
}

/**
 * add_to_queue - Add a holder to the wait queue (but look for recursion)
 * @gh: the holder structure to add
 *
 */

static void add_to_queue(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_holder *existing;

	BUG_ON(!gh->gh_owner);

	existing = find_holder_by_owner(&gl->gl_holders, gh->gh_owner);
	if (existing) {
		print_symbol(KERN_WARNING "original: %s\n", existing->gh_ip);
		print_symbol(KERN_WARNING "new: %s\n", gh->gh_ip);
		BUG();
	}

	existing = find_holder_by_owner(&gl->gl_waiters3, gh->gh_owner);
	if (existing) {
		print_symbol(KERN_WARNING "original: %s\n", existing->gh_ip);
		print_symbol(KERN_WARNING "new: %s\n", gh->gh_ip);
		BUG();
	}

	if (gh->gh_flags & LM_FLAG_PRIORITY)
		list_add(&gh->gh_list, &gl->gl_waiters3);
	else
		list_add_tail(&gh->gh_list, &gl->gl_waiters3);	
}

/**
 * gfs2_glock_nq - enqueue a struct gfs2_holder onto a glock (acquire a glock)
 * @gh: the holder structure
 *
 * if (gh->gh_flags & GL_ASYNC), this never returns an error
 *
 * Returns: 0, GLR_TRYFAILED, or errno on failure
 */

int gfs2_glock_nq(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_sbd *sdp = gl->gl_sbd;
	int error = 0;

 restart:
	if (unlikely(test_bit(SDF_SHUTDOWN, &sdp->sd_flags))) {
		set_bit(HIF_ABORTED, &gh->gh_iflags);
		return -EIO;
	}

	set_bit(HIF_PROMOTE, &gh->gh_iflags);

	spin_lock(&gl->gl_spin);
	add_to_queue(gh);
	run_queue(gl);
	spin_unlock(&gl->gl_spin);

	if (!(gh->gh_flags & GL_ASYNC)) {
		error = glock_wait_internal(gh);
		if (error == GLR_CANCELED) {
			msleep(100);
			goto restart;
		}
	}

	clear_bit(GLF_PREFETCH, &gl->gl_flags);

	return error;
}

/**
 * gfs2_glock_poll - poll to see if an async request has been completed
 * @gh: the holder
 *
 * Returns: 1 if the request is ready to be gfs2_glock_wait()ed on
 */

int gfs2_glock_poll(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	int ready = 0;

	spin_lock(&gl->gl_spin);

	if (test_bit(HIF_HOLDER, &gh->gh_iflags))
		ready = 1;
	else if (list_empty(&gh->gh_list)) {
		if (gh->gh_error == GLR_CANCELED) {
			spin_unlock(&gl->gl_spin);
			msleep(100);
			if (gfs2_glock_nq(gh))
				return 1;
			return 0;
		} else
			ready = 1;
	}

	spin_unlock(&gl->gl_spin);

	return ready;
}

/**
 * gfs2_glock_wait - wait for a lock acquisition that ended in a GLR_ASYNC
 * @gh: the holder structure
 *
 * Returns: 0, GLR_TRYFAILED, or errno on failure
 */

int gfs2_glock_wait(struct gfs2_holder *gh)
{
	int error;

	error = glock_wait_internal(gh);
	if (error == GLR_CANCELED) {
		msleep(100);
		gh->gh_flags &= ~GL_ASYNC;
		error = gfs2_glock_nq(gh);
	}

	return error;
}

/**
 * gfs2_glock_dq - dequeue a struct gfs2_holder from a glock (release a glock)
 * @gh: the glock holder
 *
 */

void gfs2_glock_dq(struct gfs2_holder *gh)
{
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_glock_operations *glops = gl->gl_ops;

	if (gh->gh_flags & GL_SYNC)
		set_bit(GLF_SYNC, &gl->gl_flags);

	if (gh->gh_flags & GL_NOCACHE)
		handle_callback(gl, LM_ST_UNLOCKED);

	gfs2_glmutex_lock(gl);

	spin_lock(&gl->gl_spin);
	list_del_init(&gh->gh_list);

	if (list_empty(&gl->gl_holders)) {
		spin_unlock(&gl->gl_spin);

		if (glops->go_unlock)
			glops->go_unlock(gh);

		if (test_bit(GLF_SYNC, &gl->gl_flags)) {
			if (glops->go_sync)
				glops->go_sync(gl, DIO_METADATA | DIO_DATA);
		}

		gl->gl_stamp = jiffies;

		spin_lock(&gl->gl_spin);
	}

	clear_bit(GLF_LOCK, &gl->gl_flags);
	run_queue(gl);
	spin_unlock(&gl->gl_spin);
}

/**
 * gfs2_glock_prefetch - Try to prefetch a glock
 * @gl: the glock
 * @state: the state to prefetch in
 * @flags: flags passed to go_xmote_th()
 *
 */

static void gfs2_glock_prefetch(struct gfs2_glock *gl, unsigned int state,
				int flags)
{
	struct gfs2_glock_operations *glops = gl->gl_ops;

	spin_lock(&gl->gl_spin);

	if (test_bit(GLF_LOCK, &gl->gl_flags) ||
	    !list_empty(&gl->gl_holders) ||
	    !list_empty(&gl->gl_waiters1) ||
	    !list_empty(&gl->gl_waiters2) ||
	    !list_empty(&gl->gl_waiters3) ||
	    relaxed_state_ok(gl->gl_state, state, flags)) {
		spin_unlock(&gl->gl_spin);
		return;
	}

	set_bit(GLF_PREFETCH, &gl->gl_flags);
	set_bit(GLF_LOCK, &gl->gl_flags);
	spin_unlock(&gl->gl_spin);

	glops->go_xmote_th(gl, state, flags);
}

static void greedy_work(void *data)
{
	struct greedy *gr = data;
	struct gfs2_holder *gh = &gr->gr_gh;
	struct gfs2_glock *gl = gh->gh_gl;
	struct gfs2_glock_operations *glops = gl->gl_ops;

	clear_bit(GLF_SKIP_WAITERS2, &gl->gl_flags);

	if (glops->go_greedy)
		glops->go_greedy(gl);

	spin_lock(&gl->gl_spin);

	if (list_empty(&gl->gl_waiters2)) {
		clear_bit(GLF_GREEDY, &gl->gl_flags);
		spin_unlock(&gl->gl_spin);
		gfs2_holder_uninit(gh);
		kfree(gr);
	} else {
		gfs2_glock_hold(gl);
		list_add_tail(&gh->gh_list, &gl->gl_waiters2);
		run_queue(gl);
		spin_unlock(&gl->gl_spin);
		gfs2_glock_put(gl);
	}
}

/**
 * gfs2_glock_be_greedy -
 * @gl:
 * @time:
 *
 * Returns: 0 if go_greedy will be called, 1 otherwise
 */

int gfs2_glock_be_greedy(struct gfs2_glock *gl, unsigned int time)
{
	struct greedy *gr;
	struct gfs2_holder *gh;

	if (!time ||
	    gl->gl_sbd->sd_args.ar_localcaching ||
	    test_and_set_bit(GLF_GREEDY, &gl->gl_flags))
		return 1;

	gr = kmalloc(sizeof(struct greedy), GFP_KERNEL);
	if (!gr) {
		clear_bit(GLF_GREEDY, &gl->gl_flags);
		return 1;
	}
	gh = &gr->gr_gh;

	gfs2_holder_init(gl, 0, 0, gh);
	set_bit(HIF_GREEDY, &gh->gh_iflags);
	INIT_WORK(&gr->gr_work, greedy_work, gr);

	set_bit(GLF_SKIP_WAITERS2, &gl->gl_flags);
	schedule_delayed_work(&gr->gr_work, time);

	return 0;
}

/**
 * gfs2_glock_dq_uninit - dequeue a holder from a glock and initialize it
 * @gh: the holder structure
 *
 */

void gfs2_glock_dq_uninit(struct gfs2_holder *gh)
{
	gfs2_glock_dq(gh);
	gfs2_holder_uninit(gh);
}

/**
 * gfs2_glock_nq_num - acquire a glock based on lock number
 * @sdp: the filesystem
 * @number: the lock number
 * @glops: the glock operations for the type of glock
 * @state: the state to acquire the glock in
 * @flags: modifier flags for the aquisition
 * @gh: the struct gfs2_holder
 *
 * Returns: errno
 */

int gfs2_glock_nq_num(struct gfs2_sbd *sdp, uint64_t number,
		      struct gfs2_glock_operations *glops, unsigned int state,
		      int flags, struct gfs2_holder *gh)
{
	struct gfs2_glock *gl;
	int error;

	error = gfs2_glock_get(sdp, number, glops, CREATE, &gl);
	if (!error) {
		error = gfs2_glock_nq_init(gl, state, flags, gh);
		gfs2_glock_put(gl);
	}

	return error;
}

/**
 * glock_compare - Compare two struct gfs2_glock structures for sorting
 * @arg_a: the first structure
 * @arg_b: the second structure
 *
 */

static int glock_compare(const void *arg_a, const void *arg_b)
{
	struct gfs2_holder *gh_a = *(struct gfs2_holder **)arg_a;
	struct gfs2_holder *gh_b = *(struct gfs2_holder **)arg_b;
	struct lm_lockname *a = &gh_a->gh_gl->gl_name;
	struct lm_lockname *b = &gh_b->gh_gl->gl_name;
	int ret = 0;

	if (a->ln_number > b->ln_number)
		ret = 1;
	else if (a->ln_number < b->ln_number)
		ret = -1;
	else {
		if (gh_a->gh_state == LM_ST_SHARED &&
		    gh_b->gh_state == LM_ST_EXCLUSIVE)
			ret = 1;
		else if (!(gh_a->gh_flags & GL_LOCAL_EXCL) &&
			 (gh_b->gh_flags & GL_LOCAL_EXCL))
			ret = 1;
	}

	return ret;
}

/**
 * nq_m_sync - synchonously acquire more than one glock in deadlock free order
 * @num_gh: the number of structures
 * @ghs: an array of struct gfs2_holder structures
 *
 * Returns: 0 on success (all glocks acquired),
 *          errno on failure (no glocks acquired)
 */

static int nq_m_sync(unsigned int num_gh, struct gfs2_holder *ghs,
		     struct gfs2_holder **p)
{
	unsigned int x;
	int error = 0;

	for (x = 0; x < num_gh; x++)
		p[x] = &ghs[x];

	sort(p, num_gh, sizeof(struct gfs2_holder *), glock_compare, NULL);

	for (x = 0; x < num_gh; x++) {
		p[x]->gh_flags &= ~(LM_FLAG_TRY | GL_ASYNC);

		error = gfs2_glock_nq(p[x]);
		if (error) {
			while (x--)
				gfs2_glock_dq(p[x]);
			break;
		}
	}

	return error;
}

/**
 * gfs2_glock_nq_m - acquire multiple glocks
 * @num_gh: the number of structures
 * @ghs: an array of struct gfs2_holder structures
 *
 * Figure out how big an impact this function has.  Either:
 * 1) Replace this code with code that calls gfs2_glock_prefetch()
 * 2) Forget async stuff and just call nq_m_sync()
 * 3) Leave it like it is
 *
 * Returns: 0 on success (all glocks acquired),
 *          errno on failure (no glocks acquired)
 */

int gfs2_glock_nq_m(unsigned int num_gh, struct gfs2_holder *ghs)
{
	int *e;
	unsigned int x;
	int borked = 0, serious = 0;
	int error = 0;

	if (!num_gh)
		return 0;

	if (num_gh == 1) {
		ghs->gh_flags &= ~(LM_FLAG_TRY | GL_ASYNC);
		return gfs2_glock_nq(ghs);
	}

	e = kcalloc(num_gh, sizeof(struct gfs2_holder *), GFP_KERNEL);
	if (!e)
		return -ENOMEM;

	for (x = 0; x < num_gh; x++) {
		ghs[x].gh_flags |= LM_FLAG_TRY | GL_ASYNC;
		error = gfs2_glock_nq(&ghs[x]);
		if (error) {
			borked = 1;
			serious = error;
			num_gh = x;
			break;
		}
	}

	for (x = 0; x < num_gh; x++) {
		error = e[x] = glock_wait_internal(&ghs[x]);
		if (error) {
			borked = 1;
			if (error != GLR_TRYFAILED && error != GLR_CANCELED)
				serious = error;
		}
	}

	if (!borked) {
		kfree(e);
		return 0;
	}

	for (x = 0; x < num_gh; x++)
		if (!e[x])
			gfs2_glock_dq(&ghs[x]);

	if (serious)
		error = serious;
	else {
		for (x = 0; x < num_gh; x++)
			gfs2_holder_reinit(ghs[x].gh_state, ghs[x].gh_flags,
					  &ghs[x]);
		error = nq_m_sync(num_gh, ghs, (struct gfs2_holder **)e);
	}

	kfree(e);

	return error;
}

/**
 * gfs2_glock_dq_m - release multiple glocks
 * @num_gh: the number of structures
 * @ghs: an array of struct gfs2_holder structures
 *
 */

void gfs2_glock_dq_m(unsigned int num_gh, struct gfs2_holder *ghs)
{
	unsigned int x;

	for (x = 0; x < num_gh; x++)
		gfs2_glock_dq(&ghs[x]);
}

/**
 * gfs2_glock_dq_uninit_m - release multiple glocks
 * @num_gh: the number of structures
 * @ghs: an array of struct gfs2_holder structures
 *
 */

void gfs2_glock_dq_uninit_m(unsigned int num_gh, struct gfs2_holder *ghs)
{
	unsigned int x;

	for (x = 0; x < num_gh; x++)
		gfs2_glock_dq_uninit(&ghs[x]);
}

/**
 * gfs2_glock_prefetch_num - prefetch a glock based on lock number
 * @sdp: the filesystem
 * @number: the lock number
 * @glops: the glock operations for the type of glock
 * @state: the state to acquire the glock in
 * @flags: modifier flags for the aquisition
 *
 * Returns: errno
 */

void gfs2_glock_prefetch_num(struct gfs2_sbd *sdp, uint64_t number,
			     struct gfs2_glock_operations *glops,
			     unsigned int state, int flags)
{
	struct gfs2_glock *gl;
	int error;

	if (atomic_read(&sdp->sd_reclaim_count) <
	    gfs2_tune_get(sdp, gt_reclaim_limit)) {
		error = gfs2_glock_get(sdp, number, glops, CREATE, &gl);
		if (!error) {
			gfs2_glock_prefetch(gl, state, flags);
			gfs2_glock_put(gl);
		}
	}
}

/**
 * gfs2_lvb_hold - attach a LVB from a glock
 * @gl: The glock in question
 *
 */

int gfs2_lvb_hold(struct gfs2_glock *gl)
{
	int error;

	gfs2_glmutex_lock(gl);

	if (!atomic_read(&gl->gl_lvb_count)) {
		error = gfs2_lm_hold_lvb(gl->gl_sbd, gl->gl_lock, &gl->gl_lvb);
		if (error) {
			gfs2_glmutex_unlock(gl);
			return error;
		}
		gfs2_glock_hold(gl);
	}
	atomic_inc(&gl->gl_lvb_count);

	gfs2_glmutex_unlock(gl);

	return 0;
}

/**
 * gfs2_lvb_unhold - detach a LVB from a glock
 * @gl: The glock in question
 *
 */

void gfs2_lvb_unhold(struct gfs2_glock *gl)
{
	gfs2_glock_hold(gl);
	gfs2_glmutex_lock(gl);

	gfs2_assert(gl->gl_sbd, atomic_read(&gl->gl_lvb_count) > 0);
	if (atomic_dec_and_test(&gl->gl_lvb_count)) {
		gfs2_lm_unhold_lvb(gl->gl_sbd, gl->gl_lock, gl->gl_lvb);
		gl->gl_lvb = NULL;
		gfs2_glock_put(gl);
	}

	gfs2_glmutex_unlock(gl);
	gfs2_glock_put(gl);
}

#if 0
void gfs2_lvb_sync(struct gfs2_glock *gl)
{
	gfs2_glmutex_lock(gl);

	gfs2_assert(gl->gl_sbd, atomic_read(&gl->gl_lvb_count));
	if (!gfs2_assert_warn(gl->gl_sbd, gfs2_glock_is_held_excl(gl)))
		gfs2_lm_sync_lvb(gl->gl_sbd, gl->gl_lock, gl->gl_lvb);

	gfs2_glmutex_unlock(gl);
}
#endif  /*  0  */

static void blocking_cb(struct gfs2_sbd *sdp, struct lm_lockname *name,
			unsigned int state)
{
	struct gfs2_glock *gl;

	gl = gfs2_glock_find(sdp, name);
	if (!gl)
		return;

	if (gl->gl_ops->go_callback)
		gl->gl_ops->go_callback(gl, state);
	handle_callback(gl, state);

	spin_lock(&gl->gl_spin);
	run_queue(gl);
	spin_unlock(&gl->gl_spin);

	gfs2_glock_put(gl);
}

/**
 * gfs2_glock_cb - Callback used by locking module
 * @fsdata: Pointer to the superblock
 * @type: Type of callback
 * @data: Type dependent data pointer
 *
 * Called by the locking module when it wants to tell us something.
 * Either we need to drop a lock, one of our ASYNC requests completed, or
 * a journal from another client needs to be recovered.
 */

void gfs2_glock_cb(lm_fsdata_t *fsdata, unsigned int type, void *data)
{
	struct gfs2_sbd *sdp = (struct gfs2_sbd *)fsdata;

	switch (type) {
	case LM_CB_NEED_E:
		blocking_cb(sdp, data, LM_ST_UNLOCKED);
		return;

	case LM_CB_NEED_D:
		blocking_cb(sdp, data, LM_ST_DEFERRED);
		return;

	case LM_CB_NEED_S:
		blocking_cb(sdp, data, LM_ST_SHARED);
		return;

	case LM_CB_ASYNC: {
		struct lm_async_cb *async = data;
		struct gfs2_glock *gl;

		gl = gfs2_glock_find(sdp, &async->lc_name);
		if (gfs2_assert_warn(sdp, gl))
			return;
		if (!gfs2_assert_warn(sdp, gl->gl_req_bh))
			gl->gl_req_bh(gl, async->lc_ret);
		gfs2_glock_put(gl);
		return;
	}

	case LM_CB_NEED_RECOVERY:
		gfs2_jdesc_make_dirty(sdp, *(unsigned int *)data);
		if (sdp->sd_recoverd_process)
			wake_up_process(sdp->sd_recoverd_process);
		return;

	case LM_CB_DROPLOCKS:
		gfs2_gl_hash_clear(sdp, NO_WAIT);
		gfs2_quota_scan(sdp);
		return;

	default:
		gfs2_assert_warn(sdp, 0);
		return;
	}
}

/**
 * gfs2_try_toss_inode - try to remove a particular inode struct from cache
 * sdp: the filesystem
 * inum: the inode number
 *
 */

void gfs2_try_toss_inode(struct gfs2_sbd *sdp, struct gfs2_inum *inum)
{
	struct gfs2_glock *gl;
	struct gfs2_inode *ip;
	int error;

	error = gfs2_glock_get(sdp, inum->no_addr, &gfs2_inode_glops,
			       NO_CREATE, &gl);
	if (error || !gl)
		return;

	if (!gfs2_glmutex_trylock(gl))
		goto out;

	ip = gl->gl_object;
	if (!ip)
		goto out_unlock;

	if (atomic_read(&ip->i_count))
		goto out_unlock;

	gfs2_inode_destroy(ip, 1);

 out_unlock:
	gfs2_glmutex_unlock(gl);

 out:
	gfs2_glock_put(gl);
}

/**
 * gfs2_iopen_go_callback - Try to kick the inode/vnode associated with an
 *                          iopen glock from memory
 * @io_gl: the iopen glock
 * @state: the state into which the glock should be put
 *
 */

void gfs2_iopen_go_callback(struct gfs2_glock *io_gl, unsigned int state)
{
	struct gfs2_glock *i_gl;

	if (state != LM_ST_UNLOCKED)
		return;

	spin_lock(&io_gl->gl_spin);
	i_gl = io_gl->gl_object;
	if (i_gl) {
		gfs2_glock_hold(i_gl);
		spin_unlock(&io_gl->gl_spin);
	} else {
		spin_unlock(&io_gl->gl_spin);
		return;
	}

	if (gfs2_glmutex_trylock(i_gl)) {
		struct gfs2_inode *ip = i_gl->gl_object;
		if (ip) {
			gfs2_try_toss_vnode(ip);
			gfs2_glmutex_unlock(i_gl);
			gfs2_glock_schedule_for_reclaim(i_gl);
			goto out;
		}
		gfs2_glmutex_unlock(i_gl);
	}

 out:
	gfs2_glock_put(i_gl);
}

/**
 * demote_ok - Check to see if it's ok to unlock a glock
 * @gl: the glock
 *
 * Returns: 1 if it's ok
 */

static int demote_ok(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	struct gfs2_glock_operations *glops = gl->gl_ops;
	int demote = 1;

	if (test_bit(GLF_STICKY, &gl->gl_flags))
		demote = 0;
	else if (test_bit(GLF_PREFETCH, &gl->gl_flags))
		demote = time_after_eq(jiffies,
				    gl->gl_stamp +
				    gfs2_tune_get(sdp, gt_prefetch_secs) * HZ);
	else if (glops->go_demote_ok)
		demote = glops->go_demote_ok(gl);

	return demote;
}

/**
 * gfs2_glock_schedule_for_reclaim - Add a glock to the reclaim list
 * @gl: the glock
 *
 */

void gfs2_glock_schedule_for_reclaim(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;

	spin_lock(&sdp->sd_reclaim_lock);
	if (list_empty(&gl->gl_reclaim)) {
		gfs2_glock_hold(gl);
		list_add(&gl->gl_reclaim, &sdp->sd_reclaim_list);
		atomic_inc(&sdp->sd_reclaim_count);
	}
	spin_unlock(&sdp->sd_reclaim_lock);

	wake_up(&sdp->sd_reclaim_wq);
}

/**
 * gfs2_reclaim_glock - process the next glock on the filesystem's reclaim list
 * @sdp: the filesystem
 *
 * Called from gfs2_glockd() glock reclaim daemon, or when promoting a
 * different glock and we notice that there are a lot of glocks in the
 * reclaim list.
 *
 */

void gfs2_reclaim_glock(struct gfs2_sbd *sdp)
{
	struct gfs2_glock *gl;

	spin_lock(&sdp->sd_reclaim_lock);
	if (list_empty(&sdp->sd_reclaim_list)) {
		spin_unlock(&sdp->sd_reclaim_lock);
		return;
	}
	gl = list_entry(sdp->sd_reclaim_list.next,
			struct gfs2_glock, gl_reclaim);
	list_del_init(&gl->gl_reclaim);
	spin_unlock(&sdp->sd_reclaim_lock);

	atomic_dec(&sdp->sd_reclaim_count);
	atomic_inc(&sdp->sd_reclaimed);

	if (gfs2_glmutex_trylock(gl)) {
		if (gl->gl_ops == &gfs2_inode_glops) {
			struct gfs2_inode *ip = gl->gl_object;
			if (ip && !atomic_read(&ip->i_count))
				gfs2_inode_destroy(ip, 1);
		}
		if (queue_empty(gl, &gl->gl_holders) &&
		    gl->gl_state != LM_ST_UNLOCKED &&
		    demote_ok(gl))
			handle_callback(gl, LM_ST_UNLOCKED);
		gfs2_glmutex_unlock(gl);
	}

	gfs2_glock_put(gl);
}

/**
 * examine_bucket - Call a function for glock in a hash bucket
 * @examiner: the function
 * @sdp: the filesystem
 * @bucket: the bucket
 *
 * Returns: 1 if the bucket has entries
 */

static int examine_bucket(glock_examiner examiner, struct gfs2_sbd *sdp,
			  struct gfs2_gl_hash_bucket *bucket)
{
	struct glock_plug plug;
	struct list_head *tmp;
	struct gfs2_glock *gl;
	int entries;

	/* Add "plug" to end of bucket list, work back up list from there */
	memset(&plug.gl_flags, 0, sizeof(unsigned long));
	set_bit(GLF_PLUG, &plug.gl_flags);

	write_lock(&bucket->hb_lock);
	list_add(&plug.gl_list, &bucket->hb_list);
	write_unlock(&bucket->hb_lock);

	for (;;) {
		write_lock(&bucket->hb_lock);

		for (;;) {
			tmp = plug.gl_list.next;

			if (tmp == &bucket->hb_list) {
				list_del(&plug.gl_list);
				entries = !list_empty(&bucket->hb_list);
				write_unlock(&bucket->hb_lock);
				return entries;
			}
			gl = list_entry(tmp, struct gfs2_glock, gl_list);

			/* Move plug up list */
			list_move(&plug.gl_list, &gl->gl_list);

			if (test_bit(GLF_PLUG, &gl->gl_flags))
				continue;

			/* examiner() must glock_put() */
			gfs2_glock_hold(gl);

			break;
		}

		write_unlock(&bucket->hb_lock);

		examiner(gl);
	}
}

/**
 * scan_glock - look at a glock and see if we can reclaim it
 * @gl: the glock to look at
 *
 */

static void scan_glock(struct gfs2_glock *gl)
{
	if (gfs2_glmutex_trylock(gl)) {
		if (gl->gl_ops == &gfs2_inode_glops) {
			struct gfs2_inode *ip = gl->gl_object;
			if (ip && !atomic_read(&ip->i_count))
				goto out_schedule;
		}
		if (queue_empty(gl, &gl->gl_holders) &&
		    gl->gl_state != LM_ST_UNLOCKED &&
		    demote_ok(gl))
			goto out_schedule;

		gfs2_glmutex_unlock(gl);
	}

	gfs2_glock_put(gl);

	return;

 out_schedule:
	gfs2_glmutex_unlock(gl);
	gfs2_glock_schedule_for_reclaim(gl);
	gfs2_glock_put(gl);
}

/**
 * gfs2_scand_internal - Look for glocks and inodes to toss from memory
 * @sdp: the filesystem
 *
 */

void gfs2_scand_internal(struct gfs2_sbd *sdp)
{
	unsigned int x;

	for (x = 0; x < GFS2_GL_HASH_SIZE; x++) {
		examine_bucket(scan_glock, sdp, &sdp->sd_gl_hash[x]);
		cond_resched();
	}
}

/**
 * clear_glock - look at a glock and see if we can free it from glock cache
 * @gl: the glock to look at
 *
 */

static void clear_glock(struct gfs2_glock *gl)
{
	struct gfs2_sbd *sdp = gl->gl_sbd;
	int released;

	spin_lock(&sdp->sd_reclaim_lock);
	if (!list_empty(&gl->gl_reclaim)) {
		list_del_init(&gl->gl_reclaim);
		atomic_dec(&sdp->sd_reclaim_count);
		spin_unlock(&sdp->sd_reclaim_lock);
		released = gfs2_glock_put(gl);
		gfs2_assert(sdp, !released);
	} else {
		spin_unlock(&sdp->sd_reclaim_lock);
	}

	if (gfs2_glmutex_trylock(gl)) {
		if (gl->gl_ops == &gfs2_inode_glops) {
			struct gfs2_inode *ip = gl->gl_object;
			if (ip && !atomic_read(&ip->i_count))
				gfs2_inode_destroy(ip, 1);
		}
		if (queue_empty(gl, &gl->gl_holders) &&
		    gl->gl_state != LM_ST_UNLOCKED)
			handle_callback(gl, LM_ST_UNLOCKED);

		gfs2_glmutex_unlock(gl);
	}

	gfs2_glock_put(gl);
}

/**
 * gfs2_gl_hash_clear - Empty out the glock hash table
 * @sdp: the filesystem
 * @wait: wait until it's all gone
 *
 * Called when unmounting the filesystem, or when inter-node lock manager
 * requests DROPLOCKS because it is running out of capacity.
 */

void gfs2_gl_hash_clear(struct gfs2_sbd *sdp, int wait)
{
	unsigned long t;
	unsigned int x;
	int cont;

	t = jiffies;

	for (;;) {
		cont = 0;

		for (x = 0; x < GFS2_GL_HASH_SIZE; x++)
			if (examine_bucket(clear_glock, sdp,
					   &sdp->sd_gl_hash[x]))
				cont = 1;

		if (!wait || !cont)
			break;

		if (time_after_eq(jiffies,
				  t + gfs2_tune_get(sdp, gt_stall_secs) * HZ)) {
			fs_warn(sdp, "Unmount seems to be stalled. "
				     "Dumping lock state...\n");
			gfs2_dump_lockstate(sdp);
			t = jiffies;
		}

		/* invalidate_inodes() requires that the sb inodes list
		   not change, but an async completion callback for an
		   unlock can occur which does glock_put() which
		   can call iput() which will change the sb inodes list.
		   invalidate_inodes_mutex prevents glock_put()'s during
		   an invalidate_inodes() */

		mutex_lock(&sdp->sd_invalidate_inodes_mutex);
		invalidate_inodes(sdp->sd_vfs);
		mutex_unlock(&sdp->sd_invalidate_inodes_mutex);
		yield();
	}
}

/*
 *  Diagnostic routines to help debug distributed deadlock
 */

/**
 * dump_holder - print information about a glock holder
 * @str: a string naming the type of holder
 * @gh: the glock holder
 *
 * Returns: 0 on success, -ENOBUFS when we run out of space
 */

static int dump_holder(char *str, struct gfs2_holder *gh)
{
	unsigned int x;
	int error = -ENOBUFS;

	printk(KERN_INFO "  %s\n", str);
	printk(KERN_INFO "    owner = %ld\n",
		   (gh->gh_owner) ? (long)gh->gh_owner->pid : -1);
	printk(KERN_INFO "    gh_state = %u\n", gh->gh_state);
	printk(KERN_INFO "    gh_flags =");
	for (x = 0; x < 32; x++)
		if (gh->gh_flags & (1 << x))
			printk(" %u", x);
	printk(" \n");
	printk(KERN_INFO "    error = %d\n", gh->gh_error);
	printk(KERN_INFO "    gh_iflags =");
	for (x = 0; x < 32; x++)
		if (test_bit(x, &gh->gh_iflags))
			printk(" %u", x);
	printk(" \n");
	print_symbol(KERN_INFO "    initialized at: %s\n", gh->gh_ip);

	error = 0;

	return error;
}

/**
 * dump_inode - print information about an inode
 * @ip: the inode
 *
 * Returns: 0 on success, -ENOBUFS when we run out of space
 */

static int dump_inode(struct gfs2_inode *ip)
{
	unsigned int x;
	int error = -ENOBUFS;

	printk(KERN_INFO "  Inode:\n");
	printk(KERN_INFO "    num = %llu %llu\n",
		    ip->i_num.no_formal_ino, ip->i_num.no_addr);
	printk(KERN_INFO "    type = %u\n", IF2DT(ip->i_di.di_mode));
	printk(KERN_INFO "    i_count = %d\n", atomic_read(&ip->i_count));
	printk(KERN_INFO "    i_flags =");
	for (x = 0; x < 32; x++)
		if (test_bit(x, &ip->i_flags))
			printk(" %u", x);
	printk(" \n");
	printk(KERN_INFO "    vnode = %s\n", (ip->i_vnode) ? "yes" : "no");

	error = 0;

	return error;
}

/**
 * dump_glock - print information about a glock
 * @gl: the glock
 * @count: where we are in the buffer
 *
 * Returns: 0 on success, -ENOBUFS when we run out of space
 */

static int dump_glock(struct gfs2_glock *gl)
{
	struct gfs2_holder *gh;
	unsigned int x;
	int error = -ENOBUFS;

	spin_lock(&gl->gl_spin);

	printk(KERN_INFO "Glock (%u, %llu)\n",
		    gl->gl_name.ln_type,
		    gl->gl_name.ln_number);
	printk(KERN_INFO "  gl_flags =");
	for (x = 0; x < 32; x++)
		if (test_bit(x, &gl->gl_flags))
			printk(" %u", x);
	printk(" \n");
	printk(KERN_INFO "  gl_ref = %d\n", atomic_read(&gl->gl_ref.refcount));
	printk(KERN_INFO "  gl_state = %u\n", gl->gl_state);
	printk(KERN_INFO "  req_gh = %s\n", (gl->gl_req_gh) ? "yes" : "no");
	printk(KERN_INFO "  req_bh = %s\n", (gl->gl_req_bh) ? "yes" : "no");
	printk(KERN_INFO "  lvb_count = %d\n", atomic_read(&gl->gl_lvb_count));
	printk(KERN_INFO "  object = %s\n", (gl->gl_object) ? "yes" : "no");
	printk(KERN_INFO "  le = %s\n",
		   (list_empty(&gl->gl_le.le_list)) ? "no" : "yes");
	printk(KERN_INFO "  reclaim = %s\n",
		    (list_empty(&gl->gl_reclaim)) ? "no" : "yes");
	if (gl->gl_aspace)
		printk(KERN_INFO "  aspace = %lu\n",
			    gl->gl_aspace->i_mapping->nrpages);
	else
		printk(KERN_INFO "  aspace = no\n");
	printk(KERN_INFO "  ail = %d\n", atomic_read(&gl->gl_ail_count));
	if (gl->gl_req_gh) {
		error = dump_holder("Request", gl->gl_req_gh);
		if (error)
			goto out;
	}
	list_for_each_entry(gh, &gl->gl_holders, gh_list) {
		error = dump_holder("Holder", gh);
		if (error)
			goto out;
	}
	list_for_each_entry(gh, &gl->gl_waiters1, gh_list) {
		error = dump_holder("Waiter1", gh);
		if (error)
			goto out;
	}
	list_for_each_entry(gh, &gl->gl_waiters2, gh_list) {
		error = dump_holder("Waiter2", gh);
		if (error)
			goto out;
	}
	list_for_each_entry(gh, &gl->gl_waiters3, gh_list) {
		error = dump_holder("Waiter3", gh);
		if (error)
			goto out;
	}
	if (gl->gl_ops == &gfs2_inode_glops && gl->gl_object) {
		if (!test_bit(GLF_LOCK, &gl->gl_flags) &&
		    list_empty(&gl->gl_holders)) {
			error = dump_inode(gl->gl_object);
			if (error)
				goto out;
		} else {
			error = -ENOBUFS;
			printk(KERN_INFO "  Inode: busy\n");
		}
	}

	error = 0;

 out:
	spin_unlock(&gl->gl_spin);

	return error;
}

/**
 * gfs2_dump_lockstate - print out the current lockstate
 * @sdp: the filesystem
 * @ub: the buffer to copy the information into
 *
 * If @ub is NULL, dump the lockstate to the console.
 *
 */

static int gfs2_dump_lockstate(struct gfs2_sbd *sdp)
{
	struct gfs2_gl_hash_bucket *bucket;
	struct gfs2_glock *gl;
	unsigned int x;
	int error = 0;

	for (x = 0; x < GFS2_GL_HASH_SIZE; x++) {
		bucket = &sdp->sd_gl_hash[x];

		read_lock(&bucket->hb_lock);

		list_for_each_entry(gl, &bucket->hb_list, gl_list) {
			if (test_bit(GLF_PLUG, &gl->gl_flags))
				continue;

			error = dump_glock(gl);
			if (error)
				break;
		}

		read_unlock(&bucket->hb_lock);

		if (error)
			break;
	}


	return error;
}

