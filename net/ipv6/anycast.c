/*
 *	Anycast support for IPv6
 *	Linux INET6 implementation
 *
 *	Authors:
 *	David L Stevens (dlstevens@us.ibm.com)
 *
 *	based heavily on net/ipv6/mcast.c
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/capability.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/route.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <net/net_namespace.h>
#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/if_inet6.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>

#include <net/checksum.h>

static int ipv6_dev_ac_dec(struct net_device *dev, struct in6_addr *addr);

/* Big ac list lock for all the sockets */
static DEFINE_RWLOCK(ipv6_sk_ac_lock);


/*
 *	socket join an anycast group
 */

int ipv6_sock_ac_join(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct net_device *dev = NULL;
	struct inet6_dev *idev;
	struct ipv6_ac_socklist *pac;
	struct net *net = sock_net(sk);
	int	ishost = !net->ipv6.devconf_all->forwarding;
	int	err = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	if (ipv6_addr_is_multicast(addr))
		return -EINVAL;
	if (ipv6_chk_addr(net, addr, NULL, 0))
		return -EINVAL;

	pac = sock_kmalloc(sk, sizeof(struct ipv6_ac_socklist), GFP_KERNEL);
	if (pac == NULL)
		return -ENOMEM;
	pac->acl_next = NULL;
	ipv6_addr_copy(&pac->acl_addr, addr);

	if (ifindex == 0) {
		struct rt6_info *rt;

		rt = rt6_lookup(net, addr, NULL, 0, 0);
		if (rt) {
			dev = rt->rt6i_dev;
			dev_hold(dev);
			dst_release(&rt->u.dst);
		} else if (ishost) {
			err = -EADDRNOTAVAIL;
			goto out_free_pac;
		} else {
			/* router, no matching interface: just pick one */

			dev = dev_get_by_flags(net, IFF_UP, IFF_UP|IFF_LOOPBACK);
		}
	} else
		dev = dev_get_by_index(net, ifindex);

	if (dev == NULL) {
		err = -ENODEV;
		goto out_free_pac;
	}

	idev = in6_dev_get(dev);
	if (!idev) {
		if (ifindex)
			err = -ENODEV;
		else
			err = -EADDRNOTAVAIL;
		goto out_dev_put;
	}
	/* reset ishost, now that we have a specific device */
	ishost = !idev->cnf.forwarding;
	in6_dev_put(idev);

	pac->acl_ifindex = dev->ifindex;

	/* XXX
	 * For hosts, allow link-local or matching prefix anycasts.
	 * This obviates the need for propagating anycast routes while
	 * still allowing some non-router anycast participation.
	 */
	if (!ipv6_chk_prefix(addr, dev)) {
		if (ishost)
			err = -EADDRNOTAVAIL;
		if (err)
			goto out_dev_put;
	}

	err = ipv6_dev_ac_inc(dev, addr);
	if (err)
		goto out_dev_put;

	write_lock_bh(&ipv6_sk_ac_lock);
	pac->acl_next = np->ipv6_ac_list;
	np->ipv6_ac_list = pac;
	write_unlock_bh(&ipv6_sk_ac_lock);

	dev_put(dev);

	return 0;

out_dev_put:
	dev_put(dev);
out_free_pac:
	sock_kfree_s(sk, pac, sizeof(*pac));
	return err;
}

/*
 *	socket leave an anycast group
 */
int ipv6_sock_ac_drop(struct sock *sk, int ifindex, struct in6_addr *addr)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct net_device *dev;
	struct ipv6_ac_socklist *pac, *prev_pac;
	struct net *net = sock_net(sk);

	write_lock_bh(&ipv6_sk_ac_lock);
	prev_pac = NULL;
	for (pac = np->ipv6_ac_list; pac; pac = pac->acl_next) {
		if ((ifindex == 0 || pac->acl_ifindex == ifindex) &&
		     ipv6_addr_equal(&pac->acl_addr, addr))
			break;
		prev_pac = pac;
	}
	if (!pac) {
		write_unlock_bh(&ipv6_sk_ac_lock);
		return -ENOENT;
	}
	if (prev_pac)
		prev_pac->acl_next = pac->acl_next;
	else
		np->ipv6_ac_list = pac->acl_next;

	write_unlock_bh(&ipv6_sk_ac_lock);

	dev = dev_get_by_index(net, pac->acl_ifindex);
	if (dev) {
		ipv6_dev_ac_dec(dev, &pac->acl_addr);
		dev_put(dev);
	}
	sock_kfree_s(sk, pac, sizeof(*pac));
	return 0;
}

void ipv6_sock_ac_close(struct sock *sk)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct net_device *dev = NULL;
	struct ipv6_ac_socklist *pac;
	struct net *net = sock_net(sk);
	int	prev_index;

	write_lock_bh(&ipv6_sk_ac_lock);
	pac = np->ipv6_ac_list;
	np->ipv6_ac_list = NULL;
	write_unlock_bh(&ipv6_sk_ac_lock);

	prev_index = 0;
	while (pac) {
		struct ipv6_ac_socklist *next = pac->acl_next;

		if (pac->acl_ifindex != prev_index) {
			if (dev)
				dev_put(dev);
			dev = dev_get_by_index(net, pac->acl_ifindex);
			prev_index = pac->acl_ifindex;
		}
		if (dev)
			ipv6_dev_ac_dec(dev, &pac->acl_addr);
		sock_kfree_s(sk, pac, sizeof(*pac));
		pac = next;
	}
	if (dev)
		dev_put(dev);
}

#if 0
/* The function is not used, which is funny. Apparently, author
 * supposed to use it to filter out datagrams inside udp/raw but forgot.
 *
 * It is OK, anycasts are not special comparing to delivery to unicasts.
 */

int inet6_ac_check(struct sock *sk, struct in6_addr *addr, int ifindex)
{
	struct ipv6_ac_socklist *pac;
	struct ipv6_pinfo *np = inet6_sk(sk);
	int	found;

	found = 0;
	read_lock(&ipv6_sk_ac_lock);
	for (pac=np->ipv6_ac_list; pac; pac=pac->acl_next) {
		if (ifindex && pac->acl_ifindex != ifindex)
			continue;
		found = ipv6_addr_equal(&pac->acl_addr, addr);
		if (found)
			break;
	}
	read_unlock(&ipv6_sk_ac_lock);

	return found;
}

#endif

static void aca_put(struct ifacaddr6 *ac)
{
	if (atomic_dec_and_test(&ac->aca_refcnt)) {
		in6_dev_put(ac->aca_idev);
		dst_release(&ac->aca_rt->u.dst);
		kfree(ac);
	}
}

/*
 *	device anycast group inc (add if not found)
 */
int ipv6_dev_ac_inc(struct net_device *dev, struct in6_addr *addr)
{
	struct ifacaddr6 *aca;
	struct inet6_dev *idev;
	struct rt6_info *rt;
	int err;

	idev = in6_dev_get(dev);

	if (idev == NULL)
		return -EINVAL;

	write_lock_bh(&idev->lock);
	if (idev->dead) {
		err = -ENODEV;
		goto out;
	}

	for (aca = idev->ac_list; aca; aca = aca->aca_next) {
		if (ipv6_addr_equal(&aca->aca_addr, addr)) {
			aca->aca_users++;
			err = 0;
			goto out;
		}
	}

	/*
	 *	not found: create a new one.
	 */

	aca = kzalloc(sizeof(struct ifacaddr6), GFP_ATOMIC);

	if (aca == NULL) {
		err = -ENOMEM;
		goto out;
	}

	rt = addrconf_dst_alloc(idev, addr, 1);
	if (IS_ERR(rt)) {
		kfree(aca);
		err = PTR_ERR(rt);
		goto out;
	}

	ipv6_addr_copy(&aca->aca_addr, addr);
	aca->aca_idev = idev;
	aca->aca_rt = rt;
	aca->aca_users = 1;
	/* aca_tstamp should be updated upon changes */
	aca->aca_cstamp = aca->aca_tstamp = jiffies;
	atomic_set(&aca->aca_refcnt, 2);
	spin_lock_init(&aca->aca_lock);

	aca->aca_next = idev->ac_list;
	idev->ac_list = aca;
	write_unlock_bh(&idev->lock);

	ip6_ins_rt(rt);

	addrconf_join_solict(dev, &aca->aca_addr);

	aca_put(aca);
	return 0;
out:
	write_unlock_bh(&idev->lock);
	in6_dev_put(idev);
	return err;
}

/*
 *	device anycast group decrement
 */
int __ipv6_dev_ac_dec(struct inet6_dev *idev, struct in6_addr *addr)
{
	struct ifacaddr6 *aca, *prev_aca;

	write_lock_bh(&idev->lock);
	prev_aca = NULL;
	for (aca = idev->ac_list; aca; aca = aca->aca_next) {
		if (ipv6_addr_equal(&aca->aca_addr, addr))
			break;
		prev_aca = aca;
	}
	if (!aca) {
		write_unlock_bh(&idev->lock);
		return -ENOENT;
	}
	if (--aca->aca_users > 0) {
		write_unlock_bh(&idev->lock);
		return 0;
	}
	if (prev_aca)
		prev_aca->aca_next = aca->aca_next;
	else
		idev->ac_list = aca->aca_next;
	write_unlock_bh(&idev->lock);
	addrconf_leave_solict(idev, &aca->aca_addr);

	dst_hold(&aca->aca_rt->u.dst);
	ip6_del_rt(aca->aca_rt);

	aca_put(aca);
	return 0;
}

static int ipv6_dev_ac_dec(struct net_device *dev, struct in6_addr *addr)
{
	int ret;
	struct inet6_dev *idev = in6_dev_get(dev);
	if (idev == NULL)
		return -ENODEV;
	ret = __ipv6_dev_ac_dec(idev, addr);
	in6_dev_put(idev);
	return ret;
}

/*
 *	check if the interface has this anycast address
 */
static int ipv6_chk_acast_dev(struct net_device *dev, struct in6_addr *addr)
{
	struct inet6_dev *idev;
	struct ifacaddr6 *aca;

	idev = in6_dev_get(dev);
	if (idev) {
		read_lock_bh(&idev->lock);
		for (aca = idev->ac_list; aca; aca = aca->aca_next)
			if (ipv6_addr_equal(&aca->aca_addr, addr))
				break;
		read_unlock_bh(&idev->lock);
		in6_dev_put(idev);
		return aca != NULL;
	}
	return 0;
}

/*
 *	check if given interface (or any, if dev==0) has this anycast address
 */
int ipv6_chk_acast_addr(struct net *net, struct net_device *dev,
			struct in6_addr *addr)
{
	int found = 0;

	if (dev)
		return ipv6_chk_acast_dev(dev, addr);
	rcu_read_lock();
	for_each_netdev_rcu(net, dev)
		if (ipv6_chk_acast_dev(dev, addr)) {
			found = 1;
			break;
		}
	rcu_read_unlock();
	return found;
}


#ifdef CONFIG_PROC_FS
struct ac6_iter_state {
	struct seq_net_private p;
	struct net_device *dev;
	struct inet6_dev *idev;
};

#define ac6_seq_private(seq)	((struct ac6_iter_state *)(seq)->private)

static inline struct ifacaddr6 *ac6_get_first(struct seq_file *seq)
{
	struct ifacaddr6 *im = NULL;
	struct ac6_iter_state *state = ac6_seq_private(seq);
	struct net *net = seq_file_net(seq);

	state->idev = NULL;
	for_each_netdev_rcu(net, state->dev) {
		struct inet6_dev *idev;
		idev = __in6_dev_get(state->dev);
		if (!idev)
			continue;
		read_lock_bh(&idev->lock);
		im = idev->ac_list;
		if (im) {
			state->idev = idev;
			break;
		}
		read_unlock_bh(&idev->lock);
	}
	return im;
}

static struct ifacaddr6 *ac6_get_next(struct seq_file *seq, struct ifacaddr6 *im)
{
	struct ac6_iter_state *state = ac6_seq_private(seq);

	im = im->aca_next;
	while (!im) {
		if (likely(state->idev != NULL))
			read_unlock_bh(&state->idev->lock);

		state->dev = next_net_device_rcu(state->dev);
		if (!state->dev) {
			state->idev = NULL;
			break;
		}
		state->idev = __in6_dev_get(state->dev);
		if (!state->idev)
			continue;
		read_lock_bh(&state->idev->lock);
		im = state->idev->ac_list;
	}
	return im;
}

static struct ifacaddr6 *ac6_get_idx(struct seq_file *seq, loff_t pos)
{
	struct ifacaddr6 *im = ac6_get_first(seq);
	if (im)
		while (pos && (im = ac6_get_next(seq, im)) != NULL)
			--pos;
	return pos ? NULL : im;
}

static void *ac6_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return ac6_get_idx(seq, *pos);
}

static void *ac6_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct ifacaddr6 *im = ac6_get_next(seq, v);

	++*pos;
	return im;
}

static void ac6_seq_stop(struct seq_file *seq, void *v)
	__releases(RCU)
{
	struct ac6_iter_state *state = ac6_seq_private(seq);

	if (likely(state->idev != NULL)) {
		read_unlock_bh(&state->idev->lock);
		state->idev = NULL;
	}
	rcu_read_unlock();
}

static int ac6_seq_show(struct seq_file *seq, void *v)
{
	struct ifacaddr6 *im = (struct ifacaddr6 *)v;
	struct ac6_iter_state *state = ac6_seq_private(seq);

	seq_printf(seq, "%-4d %-15s %pi6 %5d\n",
		   state->dev->ifindex, state->dev->name,
		   &im->aca_addr, im->aca_users);
	return 0;
}

static const struct seq_operations ac6_seq_ops = {
	.start	=	ac6_seq_start,
	.next	=	ac6_seq_next,
	.stop	=	ac6_seq_stop,
	.show	=	ac6_seq_show,
};

static int ac6_seq_open(struct inode *inode, struct file *file)
{
	return seq_open_net(inode, file, &ac6_seq_ops,
			    sizeof(struct ac6_iter_state));
}

static const struct file_operations ac6_seq_fops = {
	.owner		=	THIS_MODULE,
	.open		=	ac6_seq_open,
	.read		=	seq_read,
	.llseek		=	seq_lseek,
	.release	=	seq_release_net,
};

int ac6_proc_init(struct net *net)
{
	if (!proc_net_fops_create(net, "anycast6", S_IRUGO, &ac6_seq_fops))
		return -ENOMEM;

	return 0;
}

void ac6_proc_exit(struct net *net)
{
	proc_net_remove(net, "anycast6");
}
#endif

