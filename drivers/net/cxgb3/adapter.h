/*
 * This file is part of the Chelsio T3 Ethernet driver for Linux.
 *
 * Copyright (C) 2003-2006 Chelsio Communications.  All rights reserved.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the LICENSE file included in this
 * release for licensing terms and conditions.
 */

/* This file should not be included directly.  Include common.h instead. */

#ifndef __T3_ADAPTER_H__
#define __T3_ADAPTER_H__

#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/cache.h>
#include "t3cdev.h"
#include <asm/semaphore.h>
#include <asm/bitops.h>
#include <asm/io.h>

typedef irqreturn_t(*intr_handler_t) (int, void *);

struct vlan_group;

struct port_info {
	struct vlan_group *vlan_grp;
	const struct port_type_info *port_type;
	u8 port_id;
	u8 rx_csum_offload;
	u8 nqsets;
	u8 first_qset;
	struct cphy phy;
	struct cmac mac;
	struct link_config link_config;
	struct net_device_stats netstats;
	int activity;
};

enum {				/* adapter flags */
	FULL_INIT_DONE = (1 << 0),
	USING_MSI = (1 << 1),
	USING_MSIX = (1 << 2),
	QUEUES_BOUND = (1 << 3),
};

struct rx_desc;
struct rx_sw_desc;

struct sge_fl {			/* SGE per free-buffer list state */
	unsigned int buf_size;	/* size of each Rx buffer */
	unsigned int credits;	/* # of available Rx buffers */
	unsigned int size;	/* capacity of free list */
	unsigned int cidx;	/* consumer index */
	unsigned int pidx;	/* producer index */
	unsigned int gen;	/* free list generation */
	struct rx_desc *desc;	/* address of HW Rx descriptor ring */
	struct rx_sw_desc *sdesc;	/* address of SW Rx descriptor ring */
	dma_addr_t phys_addr;	/* physical address of HW ring start */
	unsigned int cntxt_id;	/* SGE context id for the free list */
	unsigned long empty;	/* # of times queue ran out of buffers */
};

/*
 * Bundle size for grouping offload RX packets for delivery to the stack.
 * Don't make this too big as we do prefetch on each packet in a bundle.
 */
# define RX_BUNDLE_SIZE 8

struct rsp_desc;

struct sge_rspq {		/* state for an SGE response queue */
	unsigned int credits;	/* # of pending response credits */
	unsigned int size;	/* capacity of response queue */
	unsigned int cidx;	/* consumer index */
	unsigned int gen;	/* current generation bit */
	unsigned int polling;	/* is the queue serviced through NAPI? */
	unsigned int holdoff_tmr;	/* interrupt holdoff timer in 100ns */
	unsigned int next_holdoff;	/* holdoff time for next interrupt */
	struct rsp_desc *desc;	/* address of HW response ring */
	dma_addr_t phys_addr;	/* physical address of the ring */
	unsigned int cntxt_id;	/* SGE context id for the response q */
	spinlock_t lock;	/* guards response processing */
	struct sk_buff *rx_head;	/* offload packet receive queue head */
	struct sk_buff *rx_tail;	/* offload packet receive queue tail */

	unsigned long offload_pkts;
	unsigned long offload_bundles;
	unsigned long eth_pkts;	/* # of ethernet packets */
	unsigned long pure_rsps;	/* # of pure (non-data) responses */
	unsigned long imm_data;	/* responses with immediate data */
	unsigned long rx_drops;	/* # of packets dropped due to no mem */
	unsigned long async_notif; /* # of asynchronous notification events */
	unsigned long empty;	/* # of times queue ran out of credits */
	unsigned long nomem;	/* # of responses deferred due to no mem */
	unsigned long unhandled_irqs;	/* # of spurious intrs */
};

struct tx_desc;
struct tx_sw_desc;

struct sge_txq {		/* state for an SGE Tx queue */
	unsigned long flags;	/* HW DMA fetch status */
	unsigned int in_use;	/* # of in-use Tx descriptors */
	unsigned int size;	/* # of descriptors */
	unsigned int processed;	/* total # of descs HW has processed */
	unsigned int cleaned;	/* total # of descs SW has reclaimed */
	unsigned int stop_thres;	/* SW TX queue suspend threshold */
	unsigned int cidx;	/* consumer index */
	unsigned int pidx;	/* producer index */
	unsigned int gen;	/* current value of generation bit */
	unsigned int unacked;	/* Tx descriptors used since last COMPL */
	struct tx_desc *desc;	/* address of HW Tx descriptor ring */
	struct tx_sw_desc *sdesc;	/* address of SW Tx descriptor ring */
	spinlock_t lock;	/* guards enqueueing of new packets */
	unsigned int token;	/* WR token */
	dma_addr_t phys_addr;	/* physical address of the ring */
	struct sk_buff_head sendq;	/* List of backpressured offload packets */
	struct tasklet_struct qresume_tsk;	/* restarts the queue */
	unsigned int cntxt_id;	/* SGE context id for the Tx q */
	unsigned long stops;	/* # of times q has been stopped */
	unsigned long restarts;	/* # of queue restarts */
};

enum {				/* per port SGE statistics */
	SGE_PSTAT_TSO,		/* # of TSO requests */
	SGE_PSTAT_RX_CSUM_GOOD,	/* # of successful RX csum offloads */
	SGE_PSTAT_TX_CSUM,	/* # of TX checksum offloads */
	SGE_PSTAT_VLANEX,	/* # of VLAN tag extractions */
	SGE_PSTAT_VLANINS,	/* # of VLAN tag insertions */

	SGE_PSTAT_MAX		/* must be last */
};

struct sge_qset {		/* an SGE queue set */
	struct sge_rspq rspq;
	struct sge_fl fl[SGE_RXQ_PER_SET];
	struct sge_txq txq[SGE_TXQ_PER_SET];
	struct net_device *netdev;	/* associated net device */
	unsigned long txq_stopped;	/* which Tx queues are stopped */
	struct timer_list tx_reclaim_timer;	/* reclaims TX buffers */
	unsigned long port_stats[SGE_PSTAT_MAX];
} ____cacheline_aligned;

struct sge {
	struct sge_qset qs[SGE_QSETS];
	spinlock_t reg_lock;	/* guards non-atomic SGE registers (eg context) */
};

struct adapter {
	struct t3cdev tdev;
	struct list_head adapter_list;
	void __iomem *regs;
	struct pci_dev *pdev;
	unsigned long registered_device_map;
	unsigned long open_device_map;
	unsigned long flags;

	const char *name;
	int msg_enable;
	unsigned int mmio_len;

	struct adapter_params params;
	unsigned int slow_intr_mask;
	unsigned long irq_stats[IRQ_NUM_STATS];

	struct {
		unsigned short vec;
		char desc[22];
	} msix_info[SGE_QSETS + 1];

	/* T3 modules */
	struct sge sge;
	struct mc7 pmrx;
	struct mc7 pmtx;
	struct mc7 cm;
	struct mc5 mc5;

	struct net_device *port[MAX_NPORTS];
	unsigned int check_task_cnt;
	struct delayed_work adap_check_task;
	struct work_struct ext_intr_handler_task;

	/*
	 * Dummy netdevices are needed when using multiple receive queues with
	 * NAPI as each netdevice can service only one queue.
	 */
	struct net_device *dummy_netdev[SGE_QSETS - 1];

	struct dentry *debugfs_root;

	struct mutex mdio_lock;
	spinlock_t stats_lock;
	spinlock_t work_lock;
};

static inline u32 t3_read_reg(struct adapter *adapter, u32 reg_addr)
{
	u32 val = readl(adapter->regs + reg_addr);

	CH_DBG(adapter, MMIO, "read register 0x%x value 0x%x\n", reg_addr, val);
	return val;
}

static inline void t3_write_reg(struct adapter *adapter, u32 reg_addr, u32 val)
{
	CH_DBG(adapter, MMIO, "setting register 0x%x to 0x%x\n", reg_addr, val);
	writel(val, adapter->regs + reg_addr);
}

static inline struct port_info *adap2pinfo(struct adapter *adap, int idx)
{
	return netdev_priv(adap->port[idx]);
}

/*
 * We use the spare atalk_ptr to map a net device to its SGE queue set.
 * This is a macro so it can be used as l-value.
 */
#define dev2qset(netdev) ((netdev)->atalk_ptr)

#define OFFLOAD_DEVMAP_BIT 15

#define tdev2adap(d) container_of(d, struct adapter, tdev)

static inline int offload_running(struct adapter *adapter)
{
	return test_bit(OFFLOAD_DEVMAP_BIT, &adapter->open_device_map);
}

int t3_offload_tx(struct t3cdev *tdev, struct sk_buff *skb);

void t3_os_ext_intr_handler(struct adapter *adapter);
void t3_os_link_changed(struct adapter *adapter, int port_id, int link_status,
			int speed, int duplex, int fc);

void t3_sge_start(struct adapter *adap);
void t3_sge_stop(struct adapter *adap);
void t3_free_sge_resources(struct adapter *adap);
void t3_sge_err_intr_handler(struct adapter *adapter);
intr_handler_t t3_intr_handler(struct adapter *adap, int polling);
int t3_eth_xmit(struct sk_buff *skb, struct net_device *dev);
int t3_mgmt_tx(struct adapter *adap, struct sk_buff *skb);
void t3_update_qset_coalesce(struct sge_qset *qs, const struct qset_params *p);
int t3_sge_alloc_qset(struct adapter *adapter, unsigned int id, int nports,
		      int irq_vec_idx, const struct qset_params *p,
		      int ntxq, struct net_device *netdev);
int t3_get_desc(const struct sge_qset *qs, unsigned int qnum, unsigned int idx,
		unsigned char *data);
irqreturn_t t3_sge_intr_msix(int irq, void *cookie);

#endif				/* __T3_ADAPTER_H__ */
