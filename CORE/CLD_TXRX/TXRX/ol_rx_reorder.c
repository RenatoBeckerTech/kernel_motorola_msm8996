/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*=== header file includes ===*/
/* generic utilities */
#include <adf_nbuf.h>          /* adf_nbuf_t, etc. */
#include <adf_os_mem.h>        /* adf_os_mem_alloc */

#include <ieee80211.h>         /* IEEE80211_SEQ_MAX */

/* external interfaces */
#include <ol_txrx_api.h>       /* ol_txrx_pdev_handle */
#include <ol_txrx_htt_api.h>   /* ol_rx_addba_handler, etc. */
#include <ol_ctrl_txrx_api.h>  /* ol_ctrl_rx_addba_complete */
#include <ol_htt_rx_api.h>     /* htt_rx_desc_frame_free */
#include <ol_ctrl_txrx_api.h> /* ol_rx_err */

/* datapath internal interfaces */
#include <ol_txrx_peer_find.h>     /* ol_txrx_peer_find_by_id */
#include <ol_txrx_internal.h>      /* TXRX_ASSERT */
#include <ol_rx_reorder_timeout.h> /* OL_RX_REORDER_TIMEOUT_REMOVE, etc. */
#include <ol_rx_reorder.h>
#include <ol_rx_defrag.h>


/*=== data types and defines ===*/
#define OL_RX_REORDER_ROUND_PWR2(value) g_log2ceil[value]

/*=== global variables ===*/

static char g_log2ceil[] = {
    1, // 0 -> 1
    1, // 1 -> 1
    2, // 2 -> 2
    4, 4, // 3-4 -> 4
    8, 8, 8, 8, // 5-8 -> 8
    16, 16, 16, 16, 16, 16, 16, 16, // 9-16 -> 16
    32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, // 17-32 -> 32
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, // 33-64 -> 64
};


/*=== function definitions ===*/

/*---*/
#ifdef QCA_WIFI_ISOC

#define QCA_SUPPORT_RX_REORDER_RELEASE_CHECK 1

static inline void
OL_RX_REORDER_IDX_START_SELF_SELECT(
    struct ol_txrx_peer_t *peer, unsigned tid, unsigned *idx_start)
{
    /* for simplicity, always use the next_rel_idx value */
    //if (*idx_start == ~0)
    {
        *idx_start = peer->tids_next_rel_idx[tid];
    }
}
#define OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask) \
   do { \
       if (idx >= win_sz) { \
           idx = 0; \
       } \
   } while (0)
#define OL_RX_REORDER_IDX_MAX(win_sz, win_sz_mask) (win_sz - 1)
/*
 * For "integrated SoC", the reorder index represents the offset from the
 * start of the block ack window.
 * Hence, the initial value for this index is zero.
 */
#define OL_RX_REORDER_IDX_INIT(seq_num, win_sz, win_sz_mask) 0
#define OL_RX_REORDER_NO_HOLES(rx_reorder) ((rx_reorder)->num_mpdus == 0)
#define OL_RX_REORDER_MPDU_CNT_INCR(rx_reorder, incr) \
    ((rx_reorder)->num_mpdus += (incr))
#define OL_RX_REORDER_MPDU_CNT_DECR(rx_reorder, decr) \
    ((rx_reorder)->num_mpdus -= (decr))

#else

#define QCA_SUPPORT_RX_REORDER_RELEASE_CHECK 0
#define OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, idx_start) /* no-op */
#define OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask) idx &= win_sz_mask;
#define OL_RX_REORDER_IDX_MAX(win_sz, win_sz_mask) win_sz_mask
#define OL_RX_REORDER_IDX_INIT(seq_num, win_sz, win_sz_mask) 0 /* n/a */
#define OL_RX_REORDER_NO_HOLES(rx_reorder) 0
#define OL_RX_REORDER_MPDU_CNT_INCR(rx_reorder, incr) /* n/a */
#define OL_RX_REORDER_MPDU_CNT_DECR(rx_reorder, decr) /* n/a */

#endif /* QCA_WIFI_ISOC */

/*---*/
#if defined (QCA_WIFI_ISOC) && defined (QCA_SUPPORT_RX_REORDER_RELEASE_CHECK)

/* reorder array elements could be NULL */
#define OL_RX_REORDER_PTR_CHECK(ptr) if (ptr)
#define OL_RX_REORDER_LIST_APPEND(head_msdu, tail_msdu, rx_reorder_array_elem) \
    do { \
        if (tail_msdu) { \
            adf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head); \
        } else { \
            head_msdu = rx_reorder_array_elem->head; \
            tail_msdu = rx_reorder_array_elem->tail; \
        } \
    } while (0)
    
#else

/* reorder array elements are known to be non-NULL */
#define OL_RX_REORDER_PTR_CHECK(ptr) /* no-op */
#define OL_RX_REORDER_LIST_APPEND(head_msdu, tail_msdu, rx_reorder_array_elem) \
    adf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head)

#endif /* QCA_SUPPORT_RX_REORDER_RELEASE_CHECK */

/* functions called by txrx components */

void ol_rx_reorder_init(struct ol_rx_reorder_t *rx_reorder, u_int8_t tid)
{
    rx_reorder->win_sz = 1;
    rx_reorder->win_sz_mask = 0;
    rx_reorder->array = &rx_reorder->base;
    rx_reorder->base.head = rx_reorder->base.tail = NULL;
    rx_reorder->tid = tid;
    rx_reorder->defrag_timeout_ms = 0;

    rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
    rx_reorder->defrag_waitlist_elem.tqe_prev = NULL;
}


enum htt_rx_status
ol_rx_reorder_seq_num_check(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num)
{
    unsigned seq_num_delta;

    /* don't check the new seq_num against last_seq if last_seq is not valid */
    if (peer->tids_last_seq[tid] == IEEE80211_SEQ_MAX) {
        return htt_rx_status_ok;
    }
    /*
     * For duplicate detection, it might be helpful to also check
     * whether the retry bit is set or not - a strict duplicate packet
     * should be the one with retry bit set.
     * However, since many implementations do not set the retry bit,
     * and since this same function is also used for filtering out
     * late-arriving frames (frames that arive after their rx reorder
     * timeout has expired) which are not retries, don't bother checking
     * the retry bit for now.
     */
    /* note: if new seq_num == old seq_num, seq_num_delta = 4095 */
    seq_num_delta = (seq_num - 1 - peer->tids_last_seq[tid]) &
        (IEEE80211_SEQ_MAX-1); /* account for wraparound */

    if (seq_num_delta > (IEEE80211_SEQ_MAX >> 1)) {
         return htt_rx_status_err_replay; /* or maybe htt_rx_status_err_dup */
    } 
    return htt_rx_status_ok;
}

void
ol_rx_reorder_store(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned idx,
    adf_nbuf_t head_msdu,
    adf_nbuf_t tail_msdu)
{
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;

    idx &= peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
    if (rx_reorder_array_elem->head) {
#ifdef QCA_WIFI_ISOC
        /* This should not happen in Riva/Pronto case. Because
         * A-MSDU within a MPDU is indicated together. Defragmentation
         * is handled by RPE. So add an Assert here to catch potential
         * rx reorder issues.
         */
        TXRX_ASSERT2(0);
#endif
        adf_nbuf_set_next(rx_reorder_array_elem->tail, head_msdu);
    } else {
        rx_reorder_array_elem->head = head_msdu;
	OL_RX_REORDER_MPDU_CNT_INCR(&peer->tids_rx_reorder[tid], 1);
    }
    rx_reorder_array_elem->tail = tail_msdu;
}

void
ol_rx_reorder_release(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned idx_start,
    unsigned idx_end)
{
    unsigned idx;
    unsigned win_sz, win_sz_mask;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    adf_nbuf_t head_msdu;
    adf_nbuf_t tail_msdu;

    OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
    peer->tids_next_rel_idx[tid] = (u_int16_t)idx_end; /* may get reset below */

    win_sz = peer->tids_rx_reorder[tid].win_sz;
    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
    idx_start &= win_sz_mask;
    idx_end   &= win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx_start];

    head_msdu = rx_reorder_array_elem->head;
    tail_msdu = rx_reorder_array_elem->tail;
    rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
    OL_RX_REORDER_PTR_CHECK(head_msdu) {
        OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid], 1);
    }

    idx = (idx_start + 1);
    OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask);
    while (idx != idx_end) {
        rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
        OL_RX_REORDER_PTR_CHECK(rx_reorder_array_elem->head) {
            OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid], 1);
            OL_RX_REORDER_LIST_APPEND(
                head_msdu, tail_msdu, rx_reorder_array_elem);
            tail_msdu = rx_reorder_array_elem->tail;
        }
        rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
        idx++;
        OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask);
    }
    OL_RX_REORDER_PTR_CHECK(head_msdu) {
        u_int16_t seq_num;
        htt_pdev_handle htt_pdev = vdev->pdev->htt_pdev;

        /*
         * This logic is not quite correct - the last_seq value should be
         * the sequence number of the final MPDU released rather than the
         * initial MPDU released.
         * However, tracking the sequence number of the first MPDU in the
         * released batch works well enough:
         * For Peregrine and Rome, the last_seq is checked only for
         * non-aggregate cases, where only one MPDU at a time is released.
         * For Riva, Pronto, and Northstar, the last_seq is checked to
         * filter out late-arriving rx frames, whose sequence number will
         * be less than the first MPDU in this release batch.
         */
        seq_num = htt_rx_mpdu_desc_seq_num(
            htt_pdev, htt_rx_msdu_desc_retrieve(htt_pdev, head_msdu));
        peer->tids_last_seq[tid] = seq_num;
        /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
        adf_nbuf_set_next(tail_msdu, NULL);
        peer->rx_opt_proc(vdev, peer, tid, head_msdu);
    }
    /*
     * If the rx reorder timeout is handled by host SW rather than the
     * target's rx reorder logic, then stop the timer here.
     * (If there are remaining rx holes, then the timer will be restarted.)
     */
    OL_RX_REORDER_TIMEOUT_REMOVE(peer, tid);
}

void
ol_rx_reorder_flush(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned idx_start,
    unsigned idx_end,
    enum htt_rx_flush_action action)
{
    struct ol_txrx_pdev_t *pdev;
    unsigned win_sz;
    u_int8_t win_sz_mask;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    adf_nbuf_t head_msdu = NULL;
    adf_nbuf_t tail_msdu = NULL;

    pdev = vdev->pdev;
    win_sz = peer->tids_rx_reorder[tid].win_sz;
    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;

    OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
    /* a idx_end value of 0xffff means to flush the entire array */
    if (idx_end == 0xffff) {
        idx_end = idx_start;
        /*
         * The array is being flushed in entirety because the block
         * ack window has been shifted to a new position that does not
         * overlap with the old position.  (Or due to reception of a
         * DELBA.)
         * Thus, since the block ack window is essentially being reset,
         * reset the "next release index".
         */
        peer->tids_next_rel_idx[tid] = OL_RX_REORDER_IDX_INIT(
            0/*n/a*/, win_sz, win_sz_mask);
    } else {
        peer->tids_next_rel_idx[tid] = (u_int16_t)idx_end;
    }

    idx_start &= win_sz_mask;
    idx_end   &= win_sz_mask;

    do {
        rx_reorder_array_elem = 
            &peer->tids_rx_reorder[tid].array[idx_start];
        idx_start = (idx_start + 1);
        OL_RX_REORDER_IDX_WRAP(idx_start, win_sz, win_sz_mask);

        if (rx_reorder_array_elem->head) {
            OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid], 1);
            if (head_msdu == NULL) {
                head_msdu = rx_reorder_array_elem->head;
                tail_msdu = rx_reorder_array_elem->tail;
                rx_reorder_array_elem->head = NULL;
                rx_reorder_array_elem->tail = NULL;
                continue;
            }
            adf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head);
            tail_msdu = rx_reorder_array_elem->tail;
            rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
        }
    } while (idx_start != idx_end);

    ol_rx_defrag_waitlist_remove(peer, tid);

    if (head_msdu) {
        u_int16_t seq_num;
        htt_pdev_handle htt_pdev = vdev->pdev->htt_pdev;

        seq_num = htt_rx_mpdu_desc_seq_num(
            htt_pdev, htt_rx_msdu_desc_retrieve(htt_pdev, head_msdu));
        peer->tids_last_seq[tid] = seq_num;
        /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
        adf_nbuf_set_next(tail_msdu, NULL);
        if (action == htt_rx_flush_release) {
            peer->rx_opt_proc(vdev, peer, tid, head_msdu);
        } else {
            do {
                adf_nbuf_t next;
                next = adf_nbuf_next(head_msdu);
                htt_rx_desc_frame_free(pdev->htt_pdev, head_msdu);
                head_msdu = next;
            } while (head_msdu);
        }
    }
    /*
     * If the rx reorder array is empty, then reset the last_seq value -
     * it is likely that a BAR or a sequence number shift caused the
     * sequence number to jump, so the old last_seq value is not relevant.
     */
    if (OL_RX_REORDER_NO_HOLES(&peer->tids_rx_reorder[tid])) {
        peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX; /* invalid */
    }

    OL_RX_REORDER_TIMEOUT_REMOVE(peer, tid);
}

void
ol_rx_reorder_first_hole(
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned *idx_end)
{
    unsigned win_sz, win_sz_mask;
    unsigned idx_start = 0, tmp_idx = 0;

    win_sz = peer->tids_rx_reorder[tid].win_sz;
    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;

    OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
    tmp_idx++;
    OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
    /* bypass the initial hole */
    while (tmp_idx != idx_start &&
           !peer->tids_rx_reorder[tid].array[tmp_idx].head)
    {
        tmp_idx++;
        OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
    }
    /* bypass the present frames following the initial hole */
    while (tmp_idx != idx_start &&
           peer->tids_rx_reorder[tid].array[tmp_idx].head)
    {
        tmp_idx++;
        OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
    }
    /*
     * idx_end is exclusive rather than inclusive.
     * In other words, it is the index of the first slot of the second
     * hole, rather than the index of the final present frame following
     * the first hole.
     */
    *idx_end = tmp_idx;
}

void
ol_rx_reorder_peer_cleanup(
    struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer)
{
    int tid;
    for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
        ol_rx_reorder_flush(vdev, peer, tid, 0, 0, htt_rx_flush_discard);
    }
    OL_RX_REORDER_TIMEOUT_PEER_CLEANUP(peer);
}

#ifdef QCA_WIFI_ISOC
/*
 * (Responder Role) TXRX Data Path will invoke this API after
 * Aggregation has been enabled for this peer-tid combination
 * with appropriate status code. Send ADDBA Response to Peer with
 * status.
 */
void ol_ctrl_rx_addba_complete(ol_pdev_handle pdev,
			       u_int8_t *peer_mac_addr,
			       int tid, int failed)
{
	/*
	 * TODO: Send Rx AddBA response to umac for the devices
	 * which does addba processing on host (pronto).
	 */
}
#endif

/* functions called by HTT */

void
ol_rx_addba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t win_sz,
    u_int16_t start_seq_num,
    u_int8_t failed)
{
    u_int8_t round_pwr2_win_sz;
    unsigned array_size;
    struct ol_txrx_peer_t *peer;
    struct ol_rx_reorder_t *rx_reorder;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer == NULL) {
        return;
    }

    if (pdev->cfg.host_addba) {
        ol_ctrl_rx_addba_complete(
            pdev->ctrl_pdev, &peer->mac_addr.raw[0], tid, failed);
    }
    if (failed) {
        return;
    }

    peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX; /* invalid */
    rx_reorder = &peer->tids_rx_reorder[tid];

    TXRX_ASSERT2(win_sz <= 64);
    rx_reorder->win_sz = win_sz;
    round_pwr2_win_sz = OL_RX_REORDER_ROUND_PWR2(win_sz);
    array_size = round_pwr2_win_sz * sizeof(struct ol_rx_reorder_array_elem_t);
    rx_reorder->array = adf_os_mem_alloc(pdev->osdev, array_size);
    TXRX_ASSERT1(rx_reorder->array);
    adf_os_mem_set(rx_reorder->array, 0x0, array_size);

    rx_reorder->win_sz_mask = round_pwr2_win_sz - 1;
    rx_reorder->num_mpdus = 0;

    peer->tids_next_rel_idx[tid] = OL_RX_REORDER_IDX_INIT(
        start_seq_num, rx_reorder->win_sz, rx_reorder->win_sz_mask);
}

void
ol_rx_delba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid)
{
    struct ol_txrx_peer_t *peer;
    struct ol_rx_reorder_t *rx_reorder;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer == NULL) {
        return;
    }
    peer->tids_next_rel_idx[tid] = 0xffff; /* invalid value */
    rx_reorder = &peer->tids_rx_reorder[tid];

    /* check that there really was a block ack agreement */
    TXRX_ASSERT1(rx_reorder->win_sz_mask != 0);
    /*
     * Deallocate the old rx reorder array.
     * The call to ol_rx_reorder_init below
     * will reset rx_reorder->array to point to
     * the single-element statically-allocated reorder array
     * used for non block-ack cases.
     */
    adf_os_mem_free(rx_reorder->array);

    /* set up the TID with default parameters (ARQ window size = 1) */
    ol_rx_reorder_init(rx_reorder, tid);
}

void
ol_rx_flush_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int16_t idx_start,
    u_int16_t idx_end,
    enum htt_rx_flush_action action)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    void *rx_desc;
    struct ol_txrx_peer_t *peer;
    int idx;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer) {
        vdev = peer->vdev;
    } else {
        return;
    } 

    OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev);

    idx = idx_start & peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
    if (rx_reorder_array_elem->head) {    
        rx_desc =
            htt_rx_msdu_desc_retrieve(htt_pdev, rx_reorder_array_elem->head);
        if (htt_rx_msdu_is_frag(htt_pdev, rx_desc)) {
            ol_rx_reorder_flush_frag(htt_pdev, peer, tid, idx_start);
            /*
             * Assuming flush message sent seperately for frags
             * and for normal frames
             */ 
            OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);
            return;
        }
    }
    ol_rx_reorder_flush(
        vdev, peer, tid, idx_start, idx_end, action);
    /*
     * If the rx reorder timeout is handled by host SW, see if there are
     * remaining rx holes that require the timer to be restarted.
     */
    OL_RX_REORDER_TIMEOUT_UPDATE(peer, tid);
    OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);
}

void
ol_rx_pn_ind_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    int seq_num_start,
    int seq_num_end,
    u_int8_t pn_ie_cnt,
    u_int8_t *pn_ie)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    void *rx_desc;
    struct ol_txrx_peer_t *peer;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    union htt_rx_pn_t pn;
    unsigned win_sz_mask;
    adf_nbuf_t head_msdu = NULL;
    adf_nbuf_t tail_msdu = NULL;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    int seq_num, i=0;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer) {
        vdev = peer->vdev;
    } else {
        return;
    }

    /*TODO: Fragmentation case*/
    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
    seq_num_start &= win_sz_mask;
    seq_num_end   &= win_sz_mask;
    seq_num = seq_num_start;

    do {
        rx_reorder_array_elem =
            &peer->tids_rx_reorder[tid].array[seq_num];

        if (rx_reorder_array_elem->head) {
            if (pn_ie_cnt && seq_num_start == (int)(pn_ie[i])) {
                adf_nbuf_t msdu, next_msdu, mpdu_head, mpdu_tail;

                mpdu_head = msdu = rx_reorder_array_elem->head;
                mpdu_tail = rx_reorder_array_elem->tail;

                pn_ie_cnt--;
                i++;
                rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, msdu);
                htt_rx_mpdu_desc_pn(htt_pdev, rx_desc, &pn, 16);

                TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                "Tgt PN check failed - TID %d, peer %p "
                "(%02x:%02x:%02x:%02x:%02x:%02x)\n"
                "    PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                "    new seq num = %d\n",
                tid, peer,
                peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                peer->mac_addr.raw[4], peer->mac_addr.raw[5],
                pn.pn128[1],
                pn.pn128[0],
                pn.pn128[0] & 0xffffffffffffULL,
                htt_rx_mpdu_desc_seq_num(htt_pdev, rx_desc));
                ol_rx_err(
                    pdev->ctrl_pdev,
                    vdev->vdev_id, peer->mac_addr.raw, tid,
                    htt_rx_mpdu_desc_tsf32(htt_pdev, rx_desc),
                    OL_RX_ERR_PN, mpdu_head);
                /* free all MSDUs within this MPDU */
                do {
                    next_msdu = adf_nbuf_next(msdu);
                    htt_rx_desc_frame_free(htt_pdev, msdu);
                    if (msdu == mpdu_tail) {
                        break;
                    } else {
                        msdu = next_msdu;
                    }
                }while(1);

            } else {
                if (head_msdu == NULL) {
                    head_msdu = rx_reorder_array_elem->head;
                    tail_msdu = rx_reorder_array_elem->tail;
                } else {
                    adf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head);
                    tail_msdu = rx_reorder_array_elem->tail;
                }
            }
            rx_reorder_array_elem->head = NULL;
            rx_reorder_array_elem->tail = NULL;
        } 
        seq_num = (seq_num + 1) & win_sz_mask;
    } while (seq_num != seq_num_end);

    if (head_msdu) {
        /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
        adf_nbuf_set_next(tail_msdu, NULL);
        peer->rx_opt_proc(vdev, peer, tid, head_msdu);
    }
}

#if defined(ENABLE_RX_REORDER_TRACE)

A_STATUS
ol_rx_reorder_trace_attach(ol_txrx_pdev_handle pdev)
{
    int num_elems;

    num_elems = 1 << TXRX_RX_REORDER_TRACE_SIZE_LOG2;
    pdev->rx_reorder_trace.idx = 0;
    pdev->rx_reorder_trace.cnt = 0;
    pdev->rx_reorder_trace.mask = num_elems - 1;
    pdev->rx_reorder_trace.data = adf_os_mem_alloc(
        pdev->osdev, sizeof(*pdev->rx_reorder_trace.data) * num_elems);
    if (! pdev->rx_reorder_trace.data) {
        return A_ERROR;
    }
    while (--num_elems >= 0) {
        pdev->rx_reorder_trace.data[num_elems].seq_num = 0xffff;
    }

    return A_OK;
}

void
ol_rx_reorder_trace_detach(ol_txrx_pdev_handle pdev)
{
    adf_os_mem_free(pdev->rx_reorder_trace.data);
}

void
ol_rx_reorder_trace_add(
    ol_txrx_pdev_handle pdev,
    u_int8_t tid,
    u_int16_t reorder_idx,
    u_int16_t seq_num,
    int num_mpdus)
{
    u_int32_t idx = pdev->rx_reorder_trace.idx;

    pdev->rx_reorder_trace.data[idx].tid = tid;
    pdev->rx_reorder_trace.data[idx].reorder_idx = reorder_idx;
    pdev->rx_reorder_trace.data[idx].seq_num = seq_num;
    pdev->rx_reorder_trace.data[idx].num_mpdus = num_mpdus;
    pdev->rx_reorder_trace.cnt++;
    idx++;
    pdev->rx_reorder_trace.idx = idx & pdev->rx_reorder_trace.mask;
}

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle pdev, int just_once, int limit)
{
    static int print_count = 0;
    u_int32_t i, start, end;
    u_int64_t cnt;
    int elems;

    if (print_count != 0 && just_once) {
        return;
    }
    print_count++;

    end = pdev->rx_reorder_trace.idx;
    if (pdev->rx_reorder_trace.data[end].seq_num == 0xffff) {
        /* trace log has not yet wrapped around - start at the top */
        start = 0;
        cnt = 0;
    } else {
        start = end;
        cnt = pdev->rx_reorder_trace.cnt - (pdev->rx_reorder_trace.mask + 1);
    }
    elems = (end - 1 - start) & pdev->rx_reorder_trace.mask;
    if (limit > 0 && elems > limit) {
        int delta;
        delta = elems - limit;
        start += delta;
        start &= pdev->rx_reorder_trace.mask;
        cnt += delta;
    }

    i = start;
    adf_os_print("           log       array seq\n");
    adf_os_print("   count   idx  tid   idx  num (LSBs)\n");
    do {
        u_int16_t seq_num, reorder_idx;
        seq_num = pdev->rx_reorder_trace.data[i].seq_num;
        reorder_idx = pdev->rx_reorder_trace.data[i].reorder_idx;
        if (seq_num < (1 << 14)) {
            adf_os_print("  %6lld  %4d  %3d  %4d  %4d (%d)\n",
                cnt, i, pdev->rx_reorder_trace.data[i].tid,
                reorder_idx, seq_num, seq_num & 63);
        } else {
            int err = TXRX_SEQ_NUM_ERR(seq_num);
            adf_os_print("  %6lld  %4d err %d (%d MPDUs)\n",
                cnt, i, err, pdev->rx_reorder_trace.data[i].num_mpdus);
        }
        cnt++;
        i++;
        i &= pdev->rx_reorder_trace.mask;
    } while (i != end);
}

#endif /* ENABLE_RX_REORDER_TRACE */
