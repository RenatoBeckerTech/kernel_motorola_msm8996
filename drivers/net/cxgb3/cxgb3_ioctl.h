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

#ifndef __CHIOCTL_H__
#define __CHIOCTL_H__

/*
 * Ioctl commands specific to this driver.
 */
enum {
	CHELSIO_SETREG = 1024,
	CHELSIO_GETREG,
	CHELSIO_SETTPI,
	CHELSIO_GETTPI,
	CHELSIO_GETMTUTAB,
	CHELSIO_SETMTUTAB,
	CHELSIO_GETMTU,
	CHELSIO_SET_PM,
	CHELSIO_GET_PM,
	CHELSIO_GET_TCAM,
	CHELSIO_SET_TCAM,
	CHELSIO_GET_TCB,
	CHELSIO_GET_MEM,
	CHELSIO_LOAD_FW,
	CHELSIO_GET_PROTO,
	CHELSIO_SET_PROTO,
	CHELSIO_SET_TRACE_FILTER,
	CHELSIO_SET_QSET_PARAMS,
	CHELSIO_GET_QSET_PARAMS,
	CHELSIO_SET_QSET_NUM,
	CHELSIO_GET_QSET_NUM,
	CHELSIO_SET_PKTSCHED,
};

struct ch_reg {
	uint32_t cmd;
	uint32_t addr;
	uint32_t val;
};

struct ch_cntxt {
	uint32_t cmd;
	uint32_t cntxt_type;
	uint32_t cntxt_id;
	uint32_t data[4];
};

/* context types */
enum { CNTXT_TYPE_EGRESS, CNTXT_TYPE_FL, CNTXT_TYPE_RSP, CNTXT_TYPE_CQ };

struct ch_desc {
	uint32_t cmd;
	uint32_t queue_num;
	uint32_t idx;
	uint32_t size;
	uint8_t data[128];
};

struct ch_mem_range {
	uint32_t cmd;
	uint32_t mem_id;
	uint32_t addr;
	uint32_t len;
	uint32_t version;
	uint8_t buf[0];
};

struct ch_qset_params {
	uint32_t cmd;
	uint32_t qset_idx;
	int32_t txq_size[3];
	int32_t rspq_size;
	int32_t fl_size[2];
	int32_t intr_lat;
	int32_t polling;
	int32_t cong_thres;
};

struct ch_pktsched_params {
	uint32_t cmd;
	uint8_t sched;
	uint8_t idx;
	uint8_t min;
	uint8_t max;
	uint8_t binding;
};

#ifndef TCB_SIZE
# define TCB_SIZE   128
#endif

/* TCB size in 32-bit words */
#define TCB_WORDS (TCB_SIZE / 4)

enum { MEM_CM, MEM_PMRX, MEM_PMTX };	/* ch_mem_range.mem_id values */

struct ch_mtus {
	uint32_t cmd;
	uint32_t nmtus;
	uint16_t mtus[NMTUS];
};

struct ch_pm {
	uint32_t cmd;
	uint32_t tx_pg_sz;
	uint32_t tx_num_pg;
	uint32_t rx_pg_sz;
	uint32_t rx_num_pg;
	uint32_t pm_total;
};

struct ch_tcam {
	uint32_t cmd;
	uint32_t tcam_size;
	uint32_t nservers;
	uint32_t nroutes;
	uint32_t nfilters;
};

struct ch_tcb {
	uint32_t cmd;
	uint32_t tcb_index;
	uint32_t tcb_data[TCB_WORDS];
};

struct ch_tcam_word {
	uint32_t cmd;
	uint32_t addr;
	uint32_t buf[3];
};

struct ch_trace {
	uint32_t cmd;
	uint32_t sip;
	uint32_t sip_mask;
	uint32_t dip;
	uint32_t dip_mask;
	uint16_t sport;
	uint16_t sport_mask;
	uint16_t dport;
	uint16_t dport_mask;
	uint32_t vlan:12;
	uint32_t vlan_mask:12;
	uint32_t intf:4;
	uint32_t intf_mask:4;
	uint8_t proto;
	uint8_t proto_mask;
	uint8_t invert_match:1;
	uint8_t config_tx:1;
	uint8_t config_rx:1;
	uint8_t trace_tx:1;
	uint8_t trace_rx:1;
};

#define SIOCCHIOCTL SIOCDEVPRIVATE

#endif
