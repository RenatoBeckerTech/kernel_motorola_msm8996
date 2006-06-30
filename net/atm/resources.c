/* net/atm/resources.c - Statically allocated resources */

/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */

/* Fixes
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 * 2002/01 - don't free the whole struct sock on sk->destruct time,
 * 	     use the default destruct function initialized by sock_init_data */


#include <linux/config.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/sonet.h>
#include <linux/kernel.h> /* for barrier */
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/capability.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <net/sock.h>	 /* for struct sock */

#include "common.h"
#include "resources.h"
#include "addr.h"


LIST_HEAD(atm_devs);
DEFINE_MUTEX(atm_dev_mutex);

static struct atm_dev *__alloc_atm_dev(const char *type)
{
	struct atm_dev *dev;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;
	memset(dev, 0, sizeof(*dev));
	dev->type = type;
	dev->signal = ATM_PHY_SIG_UNKNOWN;
	dev->link_rate = ATM_OC3_PCR;
	spin_lock_init(&dev->lock);
	INIT_LIST_HEAD(&dev->local);
	INIT_LIST_HEAD(&dev->lecs);

	return dev;
}

static struct atm_dev *__atm_dev_lookup(int number)
{
	struct atm_dev *dev;
	struct list_head *p;

	list_for_each(p, &atm_devs) {
		dev = list_entry(p, struct atm_dev, dev_list);
		if (dev->number == number) {
			atm_dev_hold(dev);
			return dev;
		}
	}
	return NULL;
}

struct atm_dev *atm_dev_lookup(int number)
{
	struct atm_dev *dev;

	mutex_lock(&atm_dev_mutex);
	dev = __atm_dev_lookup(number);
	mutex_unlock(&atm_dev_mutex);
	return dev;
}


struct atm_dev *atm_dev_register(const char *type, const struct atmdev_ops *ops,
				 int number, unsigned long *flags)
{
	struct atm_dev *dev, *inuse;

	dev = __alloc_atm_dev(type);
	if (!dev) {
		printk(KERN_ERR "atm_dev_register: no space for dev %s\n",
		    type);
		return NULL;
	}
	mutex_lock(&atm_dev_mutex);
	if (number != -1) {
		if ((inuse = __atm_dev_lookup(number))) {
			atm_dev_put(inuse);
			mutex_unlock(&atm_dev_mutex);
			kfree(dev);
			return NULL;
		}
		dev->number = number;
	} else {
		dev->number = 0;
		while ((inuse = __atm_dev_lookup(dev->number))) {
			atm_dev_put(inuse);
			dev->number++;
		}
	}

	dev->ops = ops;
	if (flags)
		dev->flags = *flags;
	else
		memset(&dev->flags, 0, sizeof(dev->flags));
	memset(&dev->stats, 0, sizeof(dev->stats));
	atomic_set(&dev->refcnt, 1);

	if (atm_proc_dev_register(dev) < 0) {
		printk(KERN_ERR "atm_dev_register: "
		       "atm_proc_dev_register failed for dev %s\n",
		       type);
		goto out_fail;
	}

	if (atm_register_sysfs(dev) < 0) {
		printk(KERN_ERR "atm_dev_register: "
		       "atm_register_sysfs failed for dev %s\n",
		       type);
		atm_proc_dev_deregister(dev);
		goto out_fail;
	}

	list_add_tail(&dev->dev_list, &atm_devs);

out:
	mutex_unlock(&atm_dev_mutex);
	return dev;

out_fail:
	kfree(dev);
	dev = NULL;
	goto out;
}


void atm_dev_deregister(struct atm_dev *dev)
{
	BUG_ON(test_bit(ATM_DF_REMOVED, &dev->flags));
	set_bit(ATM_DF_REMOVED, &dev->flags);

	/*
	 * if we remove current device from atm_devs list, new device 
	 * with same number can appear, such we need deregister proc, 
	 * release async all vccs and remove them from vccs list too
	 */
	mutex_lock(&atm_dev_mutex);
	list_del(&dev->dev_list);
	mutex_unlock(&atm_dev_mutex);

	atm_dev_release_vccs(dev);
	atm_unregister_sysfs(dev);
	atm_proc_dev_deregister(dev);

	atm_dev_put(dev);
}


static void copy_aal_stats(struct k_atm_aal_stats *from,
    struct atm_aal_stats *to)
{
#define __HANDLE_ITEM(i) to->i = atomic_read(&from->i)
	__AAL_STAT_ITEMS
#undef __HANDLE_ITEM
}


static void subtract_aal_stats(struct k_atm_aal_stats *from,
    struct atm_aal_stats *to)
{
#define __HANDLE_ITEM(i) atomic_sub(to->i, &from->i)
	__AAL_STAT_ITEMS
#undef __HANDLE_ITEM
}


static int fetch_stats(struct atm_dev *dev, struct atm_dev_stats __user *arg, int zero)
{
	struct atm_dev_stats tmp;
	int error = 0;

	copy_aal_stats(&dev->stats.aal0, &tmp.aal0);
	copy_aal_stats(&dev->stats.aal34, &tmp.aal34);
	copy_aal_stats(&dev->stats.aal5, &tmp.aal5);
	if (arg)
		error = copy_to_user(arg, &tmp, sizeof(tmp));
	if (zero && !error) {
		subtract_aal_stats(&dev->stats.aal0, &tmp.aal0);
		subtract_aal_stats(&dev->stats.aal34, &tmp.aal34);
		subtract_aal_stats(&dev->stats.aal5, &tmp.aal5);
	}
	return error ? -EFAULT : 0;
}


int atm_dev_ioctl(unsigned int cmd, void __user *arg)
{
	void __user *buf;
	int error, len, number, size = 0;
	struct atm_dev *dev;
	struct list_head *p;
	int *tmp_buf, *tmp_p;
	struct atm_iobuf __user *iobuf = arg;
	struct atmif_sioc __user *sioc = arg;
	switch (cmd) {
		case ATM_GETNAMES:
			if (get_user(buf, &iobuf->buffer))
				return -EFAULT;
			if (get_user(len, &iobuf->length))
				return -EFAULT;
			mutex_lock(&atm_dev_mutex);
			list_for_each(p, &atm_devs)
				size += sizeof(int);
			if (size > len) {
				mutex_unlock(&atm_dev_mutex);
				return -E2BIG;
			}
			tmp_buf = kmalloc(size, GFP_ATOMIC);
			if (!tmp_buf) {
				mutex_unlock(&atm_dev_mutex);
				return -ENOMEM;
			}
			tmp_p = tmp_buf;
			list_for_each(p, &atm_devs) {
				dev = list_entry(p, struct atm_dev, dev_list);
				*tmp_p++ = dev->number;
			}
			mutex_unlock(&atm_dev_mutex);
		        error = ((copy_to_user(buf, tmp_buf, size)) ||
					put_user(size, &iobuf->length))
						? -EFAULT : 0;
			kfree(tmp_buf);
			return error;
		default:
			break;
	}

	if (get_user(buf, &sioc->arg))
		return -EFAULT;
	if (get_user(len, &sioc->length))
		return -EFAULT;
	if (get_user(number, &sioc->number))
		return -EFAULT;

	if (!(dev = try_then_request_module(atm_dev_lookup(number),
					    "atm-device-%d", number)))
		return -ENODEV;
	
	switch (cmd) {
		case ATM_GETTYPE:
			size = strlen(dev->type) + 1;
			if (copy_to_user(buf, dev->type, size)) {
				error = -EFAULT;
				goto done;
			}
			break;
		case ATM_GETESI:
			size = ESI_LEN;
			if (copy_to_user(buf, dev->esi, size)) {
				error = -EFAULT;
				goto done;
			}
			break;
		case ATM_SETESI:
			{
				int i;

				for (i = 0; i < ESI_LEN; i++)
					if (dev->esi[i]) {
						error = -EEXIST;
						goto done;
					}
			}
			/* fall through */
		case ATM_SETESIF:
			{
				unsigned char esi[ESI_LEN];

				if (!capable(CAP_NET_ADMIN)) {
					error = -EPERM;
					goto done;
				}
				if (copy_from_user(esi, buf, ESI_LEN)) {
					error = -EFAULT;
					goto done;
				}
				memcpy(dev->esi, esi, ESI_LEN);
				error =  ESI_LEN;
				goto done;
			}
		case ATM_GETSTATZ:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			/* fall through */
		case ATM_GETSTAT:
			size = sizeof(struct atm_dev_stats);
			error = fetch_stats(dev, buf, cmd == ATM_GETSTATZ);
			if (error)
				goto done;
			break;
		case ATM_GETCIRANGE:
			size = sizeof(struct atm_cirange);
			if (copy_to_user(buf, &dev->ci_range, size)) {
				error = -EFAULT;
				goto done;
			}
			break;
		case ATM_GETLINKRATE:
			size = sizeof(int);
			if (copy_to_user(buf, &dev->link_rate, size)) {
				error = -EFAULT;
				goto done;
			}
			break;
		case ATM_RSTADDR:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			atm_reset_addr(dev, ATM_ADDR_LOCAL);
			break;
		case ATM_ADDADDR:
		case ATM_DELADDR:
		case ATM_ADDLECSADDR:
		case ATM_DELLECSADDR:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			{
				struct sockaddr_atmsvc addr;

				if (copy_from_user(&addr, buf, sizeof(addr))) {
					error = -EFAULT;
					goto done;
				}
				if (cmd == ATM_ADDADDR || cmd == ATM_ADDLECSADDR)
					error = atm_add_addr(dev, &addr,
							     (cmd == ATM_ADDADDR ?
							      ATM_ADDR_LOCAL : ATM_ADDR_LECS));
				else
					error = atm_del_addr(dev, &addr,
							     (cmd == ATM_DELADDR ?
							      ATM_ADDR_LOCAL : ATM_ADDR_LECS));
				goto done;
			}
		case ATM_GETADDR:
		case ATM_GETLECSADDR:
			error = atm_get_addr(dev, buf, len,
					     (cmd == ATM_GETADDR ?
					      ATM_ADDR_LOCAL : ATM_ADDR_LECS));
			if (error < 0)
				goto done;
			size = error;
			/* may return 0, but later on size == 0 means "don't
			   write the length" */
			error = put_user(size, &sioc->length)
				? -EFAULT : 0;
			goto done;
		case ATM_SETLOOP:
			if (__ATM_LM_XTRMT((int) (unsigned long) buf) &&
			    __ATM_LM_XTLOC((int) (unsigned long) buf) >
			    __ATM_LM_XTRMT((int) (unsigned long) buf)) {
				error = -EINVAL;
				goto done;
			}
			/* fall through */
		case ATM_SETCIRANGE:
		case SONET_GETSTATZ:
		case SONET_SETDIAG:
		case SONET_CLRDIAG:
		case SONET_SETFRAMING:
			if (!capable(CAP_NET_ADMIN)) {
				error = -EPERM;
				goto done;
			}
			/* fall through */
		default:
			if (!dev->ops->ioctl) {
				error = -EINVAL;
				goto done;
			}
			size = dev->ops->ioctl(dev, cmd, buf);
			if (size < 0) {
				error = (size == -ENOIOCTLCMD ? -EINVAL : size);
				goto done;
			}
	}
	
	if (size)
		error = put_user(size, &sioc->length)
			? -EFAULT : 0;
	else
		error = 0;
done:
	atm_dev_put(dev);
	return error;
}

static __inline__ void *dev_get_idx(loff_t left)
{
	struct list_head *p;

	list_for_each(p, &atm_devs) {
		if (!--left)
			break;
	}
	return (p != &atm_devs) ? p : NULL;
}

void *atm_dev_seq_start(struct seq_file *seq, loff_t *pos)
{
 	mutex_lock(&atm_dev_mutex);
	return *pos ? dev_get_idx(*pos) : (void *) 1;
}

void atm_dev_seq_stop(struct seq_file *seq, void *v)
{
 	mutex_unlock(&atm_dev_mutex);
}
 
void *atm_dev_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	++*pos;
	v = (v == (void *)1) ? atm_devs.next : ((struct list_head *)v)->next;
	return (v == &atm_devs) ? NULL : v;
}


EXPORT_SYMBOL(atm_dev_register);
EXPORT_SYMBOL(atm_dev_deregister);
EXPORT_SYMBOL(atm_dev_lookup);
