/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "bfad_drv.h"
#include "bfa_plog.h"
#include "bfa_cs.h"
#include "bfa_modules.h"

BFA_TRC_FILE(HAL, FCXP);
BFA_MODULE(fcxp);
BFA_MODULE(sgpg);
BFA_MODULE(lps);
BFA_MODULE(fcport);
BFA_MODULE(rport);
BFA_MODULE(uf);

/*
 * LPS related definitions
 */
#define BFA_LPS_MIN_LPORTS      (1)
#define BFA_LPS_MAX_LPORTS      (256)

/*
 * Maximum Vports supported per physical port or vf.
 */
#define BFA_LPS_MAX_VPORTS_SUPP_CB  255
#define BFA_LPS_MAX_VPORTS_SUPP_CT  190


/*
 * FC PORT related definitions
 */
/*
 * The port is considered disabled if corresponding physical port or IOC are
 * disabled explicitly
 */
#define BFA_PORT_IS_DISABLED(bfa) \
	((bfa_fcport_is_disabled(bfa) == BFA_TRUE) || \
	(bfa_ioc_is_disabled(&bfa->ioc) == BFA_TRUE))

/*
 * BFA port state machine events
 */
enum bfa_fcport_sm_event {
	BFA_FCPORT_SM_START	= 1,	/*  start port state machine	*/
	BFA_FCPORT_SM_STOP	= 2,	/*  stop port state machine	*/
	BFA_FCPORT_SM_ENABLE	= 3,	/*  enable port		*/
	BFA_FCPORT_SM_DISABLE	= 4,	/*  disable port state machine */
	BFA_FCPORT_SM_FWRSP	= 5,	/*  firmware enable/disable rsp */
	BFA_FCPORT_SM_LINKUP	= 6,	/*  firmware linkup event	*/
	BFA_FCPORT_SM_LINKDOWN	= 7,	/*  firmware linkup down	*/
	BFA_FCPORT_SM_QRESUME	= 8,	/*  CQ space available	*/
	BFA_FCPORT_SM_HWFAIL	= 9,	/*  IOC h/w failure		*/
};

/*
 * BFA port link notification state machine events
 */

enum bfa_fcport_ln_sm_event {
	BFA_FCPORT_LN_SM_LINKUP		= 1,	/*  linkup event	*/
	BFA_FCPORT_LN_SM_LINKDOWN	= 2,	/*  linkdown event	*/
	BFA_FCPORT_LN_SM_NOTIFICATION	= 3	/*  done notification	*/
};

/*
 * RPORT related definitions
 */
#define bfa_rport_offline_cb(__rp) do {					\
	if ((__rp)->bfa->fcs)						\
		bfa_cb_rport_offline((__rp)->rport_drv);      \
	else {								\
		bfa_cb_queue((__rp)->bfa, &(__rp)->hcb_qe,		\
				__bfa_cb_rport_offline, (__rp));      \
	}								\
} while (0)

#define bfa_rport_online_cb(__rp) do {					\
	if ((__rp)->bfa->fcs)						\
		bfa_cb_rport_online((__rp)->rport_drv);      \
	else {								\
		bfa_cb_queue((__rp)->bfa, &(__rp)->hcb_qe,		\
				  __bfa_cb_rport_online, (__rp));      \
		}							\
} while (0)

/*
 * forward declarations FCXP related functions
 */
static void	__bfa_fcxp_send_cbfn(void *cbarg, bfa_boolean_t complete);
static void	hal_fcxp_rx_plog(struct bfa_s *bfa, struct bfa_fcxp_s *fcxp,
				struct bfi_fcxp_send_rsp_s *fcxp_rsp);
static void	hal_fcxp_tx_plog(struct bfa_s *bfa, u32 reqlen,
				struct bfa_fcxp_s *fcxp, struct fchs_s *fchs);
static void	bfa_fcxp_qresume(void *cbarg);
static void	bfa_fcxp_queue(struct bfa_fcxp_s *fcxp,
				struct bfi_fcxp_send_req_s *send_req);

/*
 * forward declarations for LPS functions
 */
static void bfa_lps_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *ndm_len,
				u32 *dm_len);
static void bfa_lps_attach(struct bfa_s *bfa, void *bfad,
				struct bfa_iocfc_cfg_s *cfg,
				struct bfa_meminfo_s *meminfo,
				struct bfa_pcidev_s *pcidev);
static void bfa_lps_detach(struct bfa_s *bfa);
static void bfa_lps_start(struct bfa_s *bfa);
static void bfa_lps_stop(struct bfa_s *bfa);
static void bfa_lps_iocdisable(struct bfa_s *bfa);
static void bfa_lps_login_rsp(struct bfa_s *bfa,
				struct bfi_lps_login_rsp_s *rsp);
static void bfa_lps_logout_rsp(struct bfa_s *bfa,
				struct bfi_lps_logout_rsp_s *rsp);
static void bfa_lps_reqq_resume(void *lps_arg);
static void bfa_lps_free(struct bfa_lps_s *lps);
static void bfa_lps_send_login(struct bfa_lps_s *lps);
static void bfa_lps_send_logout(struct bfa_lps_s *lps);
static void bfa_lps_login_comp(struct bfa_lps_s *lps);
static void bfa_lps_logout_comp(struct bfa_lps_s *lps);
static void bfa_lps_cvl_event(struct bfa_lps_s *lps);

/*
 * forward declaration for LPS state machine
 */
static void bfa_lps_sm_init(struct bfa_lps_s *lps, enum bfa_lps_event event);
static void bfa_lps_sm_login(struct bfa_lps_s *lps, enum bfa_lps_event event);
static void bfa_lps_sm_loginwait(struct bfa_lps_s *lps, enum bfa_lps_event
					event);
static void bfa_lps_sm_online(struct bfa_lps_s *lps, enum bfa_lps_event event);
static void bfa_lps_sm_logout(struct bfa_lps_s *lps, enum bfa_lps_event event);
static void bfa_lps_sm_logowait(struct bfa_lps_s *lps, enum bfa_lps_event
					event);

/*
 * forward declaration for FC Port functions
 */
static bfa_boolean_t bfa_fcport_send_enable(struct bfa_fcport_s *fcport);
static bfa_boolean_t bfa_fcport_send_disable(struct bfa_fcport_s *fcport);
static void bfa_fcport_update_linkinfo(struct bfa_fcport_s *fcport);
static void bfa_fcport_reset_linkinfo(struct bfa_fcport_s *fcport);
static void bfa_fcport_set_wwns(struct bfa_fcport_s *fcport);
static void __bfa_cb_fcport_event(void *cbarg, bfa_boolean_t complete);
static void bfa_fcport_scn(struct bfa_fcport_s *fcport,
			enum bfa_port_linkstate event, bfa_boolean_t trunk);
static void bfa_fcport_queue_cb(struct bfa_fcport_ln_s *ln,
				enum bfa_port_linkstate event);
static void __bfa_cb_fcport_stats_clr(void *cbarg, bfa_boolean_t complete);
static void bfa_fcport_stats_get_timeout(void *cbarg);
static void bfa_fcport_stats_clr_timeout(void *cbarg);
static void bfa_trunk_iocdisable(struct bfa_s *bfa);

/*
 * forward declaration for FC PORT state machine
 */
static void     bfa_fcport_sm_uninit(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_enabling_qwait(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_enabling(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_linkdown(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_linkup(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_disabling(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_disabling_qwait(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_toggling_qwait(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_disabled(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_stopped(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_iocdown(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);
static void     bfa_fcport_sm_iocfail(struct bfa_fcport_s *fcport,
					enum bfa_fcport_sm_event event);

static void     bfa_fcport_ln_sm_dn(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_dn_nf(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_dn_up_nf(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_up(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_up_nf(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_up_dn_nf(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);
static void     bfa_fcport_ln_sm_up_dn_up_nf(struct bfa_fcport_ln_s *ln,
					enum bfa_fcport_ln_sm_event event);

static struct bfa_sm_table_s hal_port_sm_table[] = {
	{BFA_SM(bfa_fcport_sm_uninit), BFA_PORT_ST_UNINIT},
	{BFA_SM(bfa_fcport_sm_enabling_qwait), BFA_PORT_ST_ENABLING_QWAIT},
	{BFA_SM(bfa_fcport_sm_enabling), BFA_PORT_ST_ENABLING},
	{BFA_SM(bfa_fcport_sm_linkdown), BFA_PORT_ST_LINKDOWN},
	{BFA_SM(bfa_fcport_sm_linkup), BFA_PORT_ST_LINKUP},
	{BFA_SM(bfa_fcport_sm_disabling_qwait), BFA_PORT_ST_DISABLING_QWAIT},
	{BFA_SM(bfa_fcport_sm_toggling_qwait), BFA_PORT_ST_TOGGLING_QWAIT},
	{BFA_SM(bfa_fcport_sm_disabling), BFA_PORT_ST_DISABLING},
	{BFA_SM(bfa_fcport_sm_disabled), BFA_PORT_ST_DISABLED},
	{BFA_SM(bfa_fcport_sm_stopped), BFA_PORT_ST_STOPPED},
	{BFA_SM(bfa_fcport_sm_iocdown), BFA_PORT_ST_IOCDOWN},
	{BFA_SM(bfa_fcport_sm_iocfail), BFA_PORT_ST_IOCDOWN},
};


/*
 * forward declaration for RPORT related functions
 */
static struct bfa_rport_s *bfa_rport_alloc(struct bfa_rport_mod_s *rp_mod);
static void		bfa_rport_free(struct bfa_rport_s *rport);
static bfa_boolean_t	bfa_rport_send_fwcreate(struct bfa_rport_s *rp);
static bfa_boolean_t	bfa_rport_send_fwdelete(struct bfa_rport_s *rp);
static bfa_boolean_t	bfa_rport_send_fwspeed(struct bfa_rport_s *rp);
static void		__bfa_cb_rport_online(void *cbarg,
						bfa_boolean_t complete);
static void		__bfa_cb_rport_offline(void *cbarg,
						bfa_boolean_t complete);

/*
 * forward declaration for RPORT state machine
 */
static void     bfa_rport_sm_uninit(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_created(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_fwcreate(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_online(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_fwdelete(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_offline(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_deleting(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_offline_pending(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_delete_pending(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_iocdisable(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_fwcreate_qfull(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_fwdelete_qfull(struct bfa_rport_s *rp,
					enum bfa_rport_event event);
static void     bfa_rport_sm_deleting_qfull(struct bfa_rport_s *rp,
					enum bfa_rport_event event);

/*
 * PLOG related definitions
 */
static int
plkd_validate_logrec(struct bfa_plog_rec_s *pl_rec)
{
	if ((pl_rec->log_type != BFA_PL_LOG_TYPE_INT) &&
		(pl_rec->log_type != BFA_PL_LOG_TYPE_STRING))
		return 1;

	if ((pl_rec->log_type != BFA_PL_LOG_TYPE_INT) &&
		(pl_rec->log_num_ints > BFA_PL_INT_LOG_SZ))
		return 1;

	return 0;
}

static u64
bfa_get_log_time(void)
{
	u64 system_time = 0;
	struct timeval tv;
	do_gettimeofday(&tv);

	/* We are interested in seconds only. */
	system_time = tv.tv_sec;
	return system_time;
}

static void
bfa_plog_add(struct bfa_plog_s *plog, struct bfa_plog_rec_s *pl_rec)
{
	u16 tail;
	struct bfa_plog_rec_s *pl_recp;

	if (plog->plog_enabled == 0)
		return;

	if (plkd_validate_logrec(pl_rec)) {
		bfa_assert(0);
		return;
	}

	tail = plog->tail;

	pl_recp = &(plog->plog_recs[tail]);

	memcpy(pl_recp, pl_rec, sizeof(struct bfa_plog_rec_s));

	pl_recp->tv = bfa_get_log_time();
	BFA_PL_LOG_REC_INCR(plog->tail);

	if (plog->head == plog->tail)
		BFA_PL_LOG_REC_INCR(plog->head);
}

void
bfa_plog_init(struct bfa_plog_s *plog)
{
	memset((char *)plog, 0, sizeof(struct bfa_plog_s));

	memcpy(plog->plog_sig, BFA_PL_SIG_STR, BFA_PL_SIG_LEN);
	plog->head = plog->tail = 0;
	plog->plog_enabled = 1;
}

void
bfa_plog_str(struct bfa_plog_s *plog, enum bfa_plog_mid mid,
		enum bfa_plog_eid event,
		u16 misc, char *log_str)
{
	struct bfa_plog_rec_s  lp;

	if (plog->plog_enabled) {
		memset(&lp, 0, sizeof(struct bfa_plog_rec_s));
		lp.mid = mid;
		lp.eid = event;
		lp.log_type = BFA_PL_LOG_TYPE_STRING;
		lp.misc = misc;
		strncpy(lp.log_entry.string_log, log_str,
			BFA_PL_STRING_LOG_SZ - 1);
		lp.log_entry.string_log[BFA_PL_STRING_LOG_SZ - 1] = '\0';
		bfa_plog_add(plog, &lp);
	}
}

void
bfa_plog_intarr(struct bfa_plog_s *plog, enum bfa_plog_mid mid,
		enum bfa_plog_eid event,
		u16 misc, u32 *intarr, u32 num_ints)
{
	struct bfa_plog_rec_s  lp;
	u32 i;

	if (num_ints > BFA_PL_INT_LOG_SZ)
		num_ints = BFA_PL_INT_LOG_SZ;

	if (plog->plog_enabled) {
		memset(&lp, 0, sizeof(struct bfa_plog_rec_s));
		lp.mid = mid;
		lp.eid = event;
		lp.log_type = BFA_PL_LOG_TYPE_INT;
		lp.misc = misc;

		for (i = 0; i < num_ints; i++)
			lp.log_entry.int_log[i] = intarr[i];

		lp.log_num_ints = (u8) num_ints;

		bfa_plog_add(plog, &lp);
	}
}

void
bfa_plog_fchdr(struct bfa_plog_s *plog, enum bfa_plog_mid mid,
			enum bfa_plog_eid event,
			u16 misc, struct fchs_s *fchdr)
{
	struct bfa_plog_rec_s  lp;
	u32	*tmp_int = (u32 *) fchdr;
	u32	ints[BFA_PL_INT_LOG_SZ];

	if (plog->plog_enabled) {
		memset(&lp, 0, sizeof(struct bfa_plog_rec_s));

		ints[0] = tmp_int[0];
		ints[1] = tmp_int[1];
		ints[2] = tmp_int[4];

		bfa_plog_intarr(plog, mid, event, misc, ints, 3);
	}
}

void
bfa_plog_fchdr_and_pl(struct bfa_plog_s *plog, enum bfa_plog_mid mid,
		      enum bfa_plog_eid event, u16 misc, struct fchs_s *fchdr,
		      u32 pld_w0)
{
	struct bfa_plog_rec_s  lp;
	u32	*tmp_int = (u32 *) fchdr;
	u32	ints[BFA_PL_INT_LOG_SZ];

	if (plog->plog_enabled) {
		memset(&lp, 0, sizeof(struct bfa_plog_rec_s));

		ints[0] = tmp_int[0];
		ints[1] = tmp_int[1];
		ints[2] = tmp_int[4];
		ints[3] = pld_w0;

		bfa_plog_intarr(plog, mid, event, misc, ints, 4);
	}
}


/*
 *  fcxp_pvt BFA FCXP private functions
 */

static void
claim_fcxp_req_rsp_mem(struct bfa_fcxp_mod_s *mod, struct bfa_meminfo_s *mi)
{
	u8	       *dm_kva = NULL;
	u64	dm_pa;
	u32	buf_pool_sz;

	dm_kva = bfa_meminfo_dma_virt(mi);
	dm_pa = bfa_meminfo_dma_phys(mi);

	buf_pool_sz = mod->req_pld_sz * mod->num_fcxps;

	/*
	 * Initialize the fcxp req payload list
	 */
	mod->req_pld_list_kva = dm_kva;
	mod->req_pld_list_pa = dm_pa;
	dm_kva += buf_pool_sz;
	dm_pa += buf_pool_sz;
	memset(mod->req_pld_list_kva, 0, buf_pool_sz);

	/*
	 * Initialize the fcxp rsp payload list
	 */
	buf_pool_sz = mod->rsp_pld_sz * mod->num_fcxps;
	mod->rsp_pld_list_kva = dm_kva;
	mod->rsp_pld_list_pa = dm_pa;
	dm_kva += buf_pool_sz;
	dm_pa += buf_pool_sz;
	memset(mod->rsp_pld_list_kva, 0, buf_pool_sz);

	bfa_meminfo_dma_virt(mi) = dm_kva;
	bfa_meminfo_dma_phys(mi) = dm_pa;
}

static void
claim_fcxps_mem(struct bfa_fcxp_mod_s *mod, struct bfa_meminfo_s *mi)
{
	u16	i;
	struct bfa_fcxp_s *fcxp;

	fcxp = (struct bfa_fcxp_s *) bfa_meminfo_kva(mi);
	memset(fcxp, 0, sizeof(struct bfa_fcxp_s) * mod->num_fcxps);

	INIT_LIST_HEAD(&mod->fcxp_free_q);
	INIT_LIST_HEAD(&mod->fcxp_active_q);

	mod->fcxp_list = fcxp;

	for (i = 0; i < mod->num_fcxps; i++) {
		fcxp->fcxp_mod = mod;
		fcxp->fcxp_tag = i;

		list_add_tail(&fcxp->qe, &mod->fcxp_free_q);
		bfa_reqq_winit(&fcxp->reqq_wqe, bfa_fcxp_qresume, fcxp);
		fcxp->reqq_waiting = BFA_FALSE;

		fcxp = fcxp + 1;
	}

	bfa_meminfo_kva(mi) = (void *)fcxp;
}

static void
bfa_fcxp_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *ndm_len,
		 u32 *dm_len)
{
	u16	num_fcxp_reqs = cfg->fwcfg.num_fcxp_reqs;

	if (num_fcxp_reqs == 0)
		return;

	/*
	 * Account for req/rsp payload
	 */
	*dm_len += BFA_FCXP_MAX_IBUF_SZ * num_fcxp_reqs;
	if (cfg->drvcfg.min_cfg)
		*dm_len += BFA_FCXP_MAX_IBUF_SZ * num_fcxp_reqs;
	else
		*dm_len += BFA_FCXP_MAX_LBUF_SZ * num_fcxp_reqs;

	/*
	 * Account for fcxp structs
	 */
	*ndm_len += sizeof(struct bfa_fcxp_s) * num_fcxp_reqs;
}

static void
bfa_fcxp_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_fcxp_mod_s *mod = BFA_FCXP_MOD(bfa);

	memset(mod, 0, sizeof(struct bfa_fcxp_mod_s));
	mod->bfa = bfa;
	mod->num_fcxps = cfg->fwcfg.num_fcxp_reqs;

	/*
	 * Initialize FCXP request and response payload sizes.
	 */
	mod->req_pld_sz = mod->rsp_pld_sz = BFA_FCXP_MAX_IBUF_SZ;
	if (!cfg->drvcfg.min_cfg)
		mod->rsp_pld_sz = BFA_FCXP_MAX_LBUF_SZ;

	INIT_LIST_HEAD(&mod->wait_q);

	claim_fcxp_req_rsp_mem(mod, meminfo);
	claim_fcxps_mem(mod, meminfo);
}

static void
bfa_fcxp_detach(struct bfa_s *bfa)
{
}

static void
bfa_fcxp_start(struct bfa_s *bfa)
{
}

static void
bfa_fcxp_stop(struct bfa_s *bfa)
{
}

static void
bfa_fcxp_iocdisable(struct bfa_s *bfa)
{
	struct bfa_fcxp_mod_s *mod = BFA_FCXP_MOD(bfa);
	struct bfa_fcxp_s *fcxp;
	struct list_head	      *qe, *qen;

	list_for_each_safe(qe, qen, &mod->fcxp_active_q) {
		fcxp = (struct bfa_fcxp_s *) qe;
		if (fcxp->caller == NULL) {
			fcxp->send_cbfn(fcxp->caller, fcxp, fcxp->send_cbarg,
					BFA_STATUS_IOC_FAILURE, 0, 0, NULL);
			bfa_fcxp_free(fcxp);
		} else {
			fcxp->rsp_status = BFA_STATUS_IOC_FAILURE;
			bfa_cb_queue(bfa, &fcxp->hcb_qe,
				     __bfa_fcxp_send_cbfn, fcxp);
		}
	}
}

static struct bfa_fcxp_s *
bfa_fcxp_get(struct bfa_fcxp_mod_s *fm)
{
	struct bfa_fcxp_s *fcxp;

	bfa_q_deq(&fm->fcxp_free_q, &fcxp);

	if (fcxp)
		list_add_tail(&fcxp->qe, &fm->fcxp_active_q);

	return fcxp;
}

static void
bfa_fcxp_init_reqrsp(struct bfa_fcxp_s *fcxp,
	       struct bfa_s *bfa,
	       u8 *use_ibuf,
	       u32 *nr_sgles,
	       bfa_fcxp_get_sgaddr_t *r_sga_cbfn,
	       bfa_fcxp_get_sglen_t *r_sglen_cbfn,
	       struct list_head *r_sgpg_q,
	       int n_sgles,
	       bfa_fcxp_get_sgaddr_t sga_cbfn,
	       bfa_fcxp_get_sglen_t sglen_cbfn)
{

	bfa_assert(bfa != NULL);

	bfa_trc(bfa, fcxp->fcxp_tag);

	if (n_sgles == 0) {
		*use_ibuf = 1;
	} else {
		bfa_assert(*sga_cbfn != NULL);
		bfa_assert(*sglen_cbfn != NULL);

		*use_ibuf = 0;
		*r_sga_cbfn = sga_cbfn;
		*r_sglen_cbfn = sglen_cbfn;

		*nr_sgles = n_sgles;

		/*
		 * alloc required sgpgs
		 */
		if (n_sgles > BFI_SGE_INLINE)
			bfa_assert(0);
	}

}

static void
bfa_fcxp_init(struct bfa_fcxp_s *fcxp,
	       void *caller, struct bfa_s *bfa, int nreq_sgles,
	       int nrsp_sgles, bfa_fcxp_get_sgaddr_t req_sga_cbfn,
	       bfa_fcxp_get_sglen_t req_sglen_cbfn,
	       bfa_fcxp_get_sgaddr_t rsp_sga_cbfn,
	       bfa_fcxp_get_sglen_t rsp_sglen_cbfn)
{

	bfa_assert(bfa != NULL);

	bfa_trc(bfa, fcxp->fcxp_tag);

	fcxp->caller = caller;

	bfa_fcxp_init_reqrsp(fcxp, bfa,
		&fcxp->use_ireqbuf, &fcxp->nreq_sgles, &fcxp->req_sga_cbfn,
		&fcxp->req_sglen_cbfn, &fcxp->req_sgpg_q,
		nreq_sgles, req_sga_cbfn, req_sglen_cbfn);

	bfa_fcxp_init_reqrsp(fcxp, bfa,
		&fcxp->use_irspbuf, &fcxp->nrsp_sgles, &fcxp->rsp_sga_cbfn,
		&fcxp->rsp_sglen_cbfn, &fcxp->rsp_sgpg_q,
		nrsp_sgles, rsp_sga_cbfn, rsp_sglen_cbfn);

}

static void
bfa_fcxp_put(struct bfa_fcxp_s *fcxp)
{
	struct bfa_fcxp_mod_s *mod = fcxp->fcxp_mod;
	struct bfa_fcxp_wqe_s *wqe;

	bfa_q_deq(&mod->wait_q, &wqe);
	if (wqe) {
		bfa_trc(mod->bfa, fcxp->fcxp_tag);

		bfa_fcxp_init(fcxp, wqe->caller, wqe->bfa, wqe->nreq_sgles,
			wqe->nrsp_sgles, wqe->req_sga_cbfn,
			wqe->req_sglen_cbfn, wqe->rsp_sga_cbfn,
			wqe->rsp_sglen_cbfn);

		wqe->alloc_cbfn(wqe->alloc_cbarg, fcxp);
		return;
	}

	bfa_assert(bfa_q_is_on_q(&mod->fcxp_active_q, fcxp));
	list_del(&fcxp->qe);
	list_add_tail(&fcxp->qe, &mod->fcxp_free_q);
}

static void
bfa_fcxp_null_comp(void *bfad_fcxp, struct bfa_fcxp_s *fcxp, void *cbarg,
		   bfa_status_t req_status, u32 rsp_len,
		   u32 resid_len, struct fchs_s *rsp_fchs)
{
	/* discarded fcxp completion */
}

static void
__bfa_fcxp_send_cbfn(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_fcxp_s *fcxp = cbarg;

	if (complete) {
		fcxp->send_cbfn(fcxp->caller, fcxp, fcxp->send_cbarg,
				fcxp->rsp_status, fcxp->rsp_len,
				fcxp->residue_len, &fcxp->rsp_fchs);
	} else {
		bfa_fcxp_free(fcxp);
	}
}

static void
hal_fcxp_send_comp(struct bfa_s *bfa, struct bfi_fcxp_send_rsp_s *fcxp_rsp)
{
	struct bfa_fcxp_mod_s	*mod = BFA_FCXP_MOD(bfa);
	struct bfa_fcxp_s	*fcxp;
	u16		fcxp_tag = be16_to_cpu(fcxp_rsp->fcxp_tag);

	bfa_trc(bfa, fcxp_tag);

	fcxp_rsp->rsp_len = be32_to_cpu(fcxp_rsp->rsp_len);

	/*
	 * @todo f/w should not set residue to non-0 when everything
	 *	 is received.
	 */
	if (fcxp_rsp->req_status == BFA_STATUS_OK)
		fcxp_rsp->residue_len = 0;
	else
		fcxp_rsp->residue_len = be32_to_cpu(fcxp_rsp->residue_len);

	fcxp = BFA_FCXP_FROM_TAG(mod, fcxp_tag);

	bfa_assert(fcxp->send_cbfn != NULL);

	hal_fcxp_rx_plog(mod->bfa, fcxp, fcxp_rsp);

	if (fcxp->send_cbfn != NULL) {
		bfa_trc(mod->bfa, (NULL == fcxp->caller));
		if (fcxp->caller == NULL) {
			fcxp->send_cbfn(fcxp->caller, fcxp, fcxp->send_cbarg,
					fcxp_rsp->req_status, fcxp_rsp->rsp_len,
					fcxp_rsp->residue_len, &fcxp_rsp->fchs);
			/*
			 * fcxp automatically freed on return from the callback
			 */
			bfa_fcxp_free(fcxp);
		} else {
			fcxp->rsp_status = fcxp_rsp->req_status;
			fcxp->rsp_len = fcxp_rsp->rsp_len;
			fcxp->residue_len = fcxp_rsp->residue_len;
			fcxp->rsp_fchs = fcxp_rsp->fchs;

			bfa_cb_queue(bfa, &fcxp->hcb_qe,
					__bfa_fcxp_send_cbfn, fcxp);
		}
	} else {
		bfa_trc(bfa, (NULL == fcxp->send_cbfn));
	}
}

static void
hal_fcxp_set_local_sges(struct bfi_sge_s *sge, u32 reqlen, u64 req_pa)
{
	union bfi_addr_u      sga_zero = { {0} };

	sge->sg_len = reqlen;
	sge->flags = BFI_SGE_DATA_LAST;
	bfa_dma_addr_set(sge[0].sga, req_pa);
	bfa_sge_to_be(sge);
	sge++;

	sge->sga = sga_zero;
	sge->sg_len = reqlen;
	sge->flags = BFI_SGE_PGDLEN;
	bfa_sge_to_be(sge);
}

static void
hal_fcxp_tx_plog(struct bfa_s *bfa, u32 reqlen, struct bfa_fcxp_s *fcxp,
		 struct fchs_s *fchs)
{
	/*
	 * TODO: TX ox_id
	 */
	if (reqlen > 0) {
		if (fcxp->use_ireqbuf) {
			u32	pld_w0 =
				*((u32 *) BFA_FCXP_REQ_PLD(fcxp));

			bfa_plog_fchdr_and_pl(bfa->plog, BFA_PL_MID_HAL_FCXP,
					BFA_PL_EID_TX,
					reqlen + sizeof(struct fchs_s), fchs,
					pld_w0);
		} else {
			bfa_plog_fchdr(bfa->plog, BFA_PL_MID_HAL_FCXP,
					BFA_PL_EID_TX,
					reqlen + sizeof(struct fchs_s),
					fchs);
		}
	} else {
		bfa_plog_fchdr(bfa->plog, BFA_PL_MID_HAL_FCXP, BFA_PL_EID_TX,
			       reqlen + sizeof(struct fchs_s), fchs);
	}
}

static void
hal_fcxp_rx_plog(struct bfa_s *bfa, struct bfa_fcxp_s *fcxp,
		 struct bfi_fcxp_send_rsp_s *fcxp_rsp)
{
	if (fcxp_rsp->rsp_len > 0) {
		if (fcxp->use_irspbuf) {
			u32	pld_w0 =
				*((u32 *) BFA_FCXP_RSP_PLD(fcxp));

			bfa_plog_fchdr_and_pl(bfa->plog, BFA_PL_MID_HAL_FCXP,
					      BFA_PL_EID_RX,
					      (u16) fcxp_rsp->rsp_len,
					      &fcxp_rsp->fchs, pld_w0);
		} else {
			bfa_plog_fchdr(bfa->plog, BFA_PL_MID_HAL_FCXP,
				       BFA_PL_EID_RX,
				       (u16) fcxp_rsp->rsp_len,
				       &fcxp_rsp->fchs);
		}
	} else {
		bfa_plog_fchdr(bfa->plog, BFA_PL_MID_HAL_FCXP, BFA_PL_EID_RX,
			       (u16) fcxp_rsp->rsp_len, &fcxp_rsp->fchs);
	}
}

/*
 * Handler to resume sending fcxp when space in available in cpe queue.
 */
static void
bfa_fcxp_qresume(void *cbarg)
{
	struct bfa_fcxp_s		*fcxp = cbarg;
	struct bfa_s			*bfa = fcxp->fcxp_mod->bfa;
	struct bfi_fcxp_send_req_s	*send_req;

	fcxp->reqq_waiting = BFA_FALSE;
	send_req = bfa_reqq_next(bfa, BFA_REQQ_FCXP);
	bfa_fcxp_queue(fcxp, send_req);
}

/*
 * Queue fcxp send request to foimrware.
 */
static void
bfa_fcxp_queue(struct bfa_fcxp_s *fcxp, struct bfi_fcxp_send_req_s *send_req)
{
	struct bfa_s			*bfa = fcxp->fcxp_mod->bfa;
	struct bfa_fcxp_req_info_s	*reqi = &fcxp->req_info;
	struct bfa_fcxp_rsp_info_s	*rspi = &fcxp->rsp_info;
	struct bfa_rport_s		*rport = reqi->bfa_rport;

	bfi_h2i_set(send_req->mh, BFI_MC_FCXP, BFI_FCXP_H2I_SEND_REQ,
		    bfa_lpuid(bfa));

	send_req->fcxp_tag = cpu_to_be16(fcxp->fcxp_tag);
	if (rport) {
		send_req->rport_fw_hndl = rport->fw_handle;
		send_req->max_frmsz = cpu_to_be16(rport->rport_info.max_frmsz);
		if (send_req->max_frmsz == 0)
			send_req->max_frmsz = cpu_to_be16(FC_MAX_PDUSZ);
	} else {
		send_req->rport_fw_hndl = 0;
		send_req->max_frmsz = cpu_to_be16(FC_MAX_PDUSZ);
	}

	send_req->vf_id = cpu_to_be16(reqi->vf_id);
	send_req->lp_tag = reqi->lp_tag;
	send_req->class = reqi->class;
	send_req->rsp_timeout = rspi->rsp_timeout;
	send_req->cts = reqi->cts;
	send_req->fchs = reqi->fchs;

	send_req->req_len = cpu_to_be32(reqi->req_tot_len);
	send_req->rsp_maxlen = cpu_to_be32(rspi->rsp_maxlen);

	/*
	 * setup req sgles
	 */
	if (fcxp->use_ireqbuf == 1) {
		hal_fcxp_set_local_sges(send_req->req_sge, reqi->req_tot_len,
					BFA_FCXP_REQ_PLD_PA(fcxp));
	} else {
		if (fcxp->nreq_sgles > 0) {
			bfa_assert(fcxp->nreq_sgles == 1);
			hal_fcxp_set_local_sges(send_req->req_sge,
						reqi->req_tot_len,
						fcxp->req_sga_cbfn(fcxp->caller,
								   0));
		} else {
			bfa_assert(reqi->req_tot_len == 0);
			hal_fcxp_set_local_sges(send_req->rsp_sge, 0, 0);
		}
	}

	/*
	 * setup rsp sgles
	 */
	if (fcxp->use_irspbuf == 1) {
		bfa_assert(rspi->rsp_maxlen <= BFA_FCXP_MAX_LBUF_SZ);

		hal_fcxp_set_local_sges(send_req->rsp_sge, rspi->rsp_maxlen,
					BFA_FCXP_RSP_PLD_PA(fcxp));

	} else {
		if (fcxp->nrsp_sgles > 0) {
			bfa_assert(fcxp->nrsp_sgles == 1);
			hal_fcxp_set_local_sges(send_req->rsp_sge,
						rspi->rsp_maxlen,
						fcxp->rsp_sga_cbfn(fcxp->caller,
								   0));
		} else {
			bfa_assert(rspi->rsp_maxlen == 0);
			hal_fcxp_set_local_sges(send_req->rsp_sge, 0, 0);
		}
	}

	hal_fcxp_tx_plog(bfa, reqi->req_tot_len, fcxp, &reqi->fchs);

	bfa_reqq_produce(bfa, BFA_REQQ_FCXP);

	bfa_trc(bfa, bfa_reqq_pi(bfa, BFA_REQQ_FCXP));
	bfa_trc(bfa, bfa_reqq_ci(bfa, BFA_REQQ_FCXP));
}

/*
 * Allocate an FCXP instance to send a response or to send a request
 * that has a response. Request/response buffers are allocated by caller.
 *
 * @param[in]	bfa		BFA bfa instance
 * @param[in]	nreq_sgles	Number of SG elements required for request
 *				buffer. 0, if fcxp internal buffers are	used.
 *				Use bfa_fcxp_get_reqbuf() to get the
 *				internal req buffer.
 * @param[in]	req_sgles	SG elements describing request buffer. Will be
 *				copied in by BFA and hence can be freed on
 *				return from this function.
 * @param[in]	get_req_sga	function ptr to be called to get a request SG
 *				Address (given the sge index).
 * @param[in]	get_req_sglen	function ptr to be called to get a request SG
 *				len (given the sge index).
 * @param[in]	get_rsp_sga	function ptr to be called to get a response SG
 *				Address (given the sge index).
 * @param[in]	get_rsp_sglen	function ptr to be called to get a response SG
 *				len (given the sge index).
 *
 * @return FCXP instance. NULL on failure.
 */
struct bfa_fcxp_s *
bfa_fcxp_alloc(void *caller, struct bfa_s *bfa, int nreq_sgles,
	       int nrsp_sgles, bfa_fcxp_get_sgaddr_t req_sga_cbfn,
	       bfa_fcxp_get_sglen_t req_sglen_cbfn,
	       bfa_fcxp_get_sgaddr_t rsp_sga_cbfn,
	       bfa_fcxp_get_sglen_t rsp_sglen_cbfn)
{
	struct bfa_fcxp_s *fcxp = NULL;

	bfa_assert(bfa != NULL);

	fcxp = bfa_fcxp_get(BFA_FCXP_MOD(bfa));
	if (fcxp == NULL)
		return NULL;

	bfa_trc(bfa, fcxp->fcxp_tag);

	bfa_fcxp_init(fcxp, caller, bfa, nreq_sgles, nrsp_sgles, req_sga_cbfn,
			req_sglen_cbfn, rsp_sga_cbfn, rsp_sglen_cbfn);

	return fcxp;
}

/*
 * Get the internal request buffer pointer
 *
 * @param[in]	fcxp	BFA fcxp pointer
 *
 * @return		pointer to the internal request buffer
 */
void *
bfa_fcxp_get_reqbuf(struct bfa_fcxp_s *fcxp)
{
	struct bfa_fcxp_mod_s *mod = fcxp->fcxp_mod;
	void	*reqbuf;

	bfa_assert(fcxp->use_ireqbuf == 1);
	reqbuf = ((u8 *)mod->req_pld_list_kva) +
		fcxp->fcxp_tag * mod->req_pld_sz;
	return reqbuf;
}

u32
bfa_fcxp_get_reqbufsz(struct bfa_fcxp_s *fcxp)
{
	struct bfa_fcxp_mod_s *mod = fcxp->fcxp_mod;

	return mod->req_pld_sz;
}

/*
 * Get the internal response buffer pointer
 *
 * @param[in]	fcxp	BFA fcxp pointer
 *
 * @return		pointer to the internal request buffer
 */
void *
bfa_fcxp_get_rspbuf(struct bfa_fcxp_s *fcxp)
{
	struct bfa_fcxp_mod_s *mod = fcxp->fcxp_mod;
	void	*rspbuf;

	bfa_assert(fcxp->use_irspbuf == 1);

	rspbuf = ((u8 *)mod->rsp_pld_list_kva) +
		fcxp->fcxp_tag * mod->rsp_pld_sz;
	return rspbuf;
}

/*
 * Free the BFA FCXP
 *
 * @param[in]	fcxp			BFA fcxp pointer
 *
 * @return		void
 */
void
bfa_fcxp_free(struct bfa_fcxp_s *fcxp)
{
	struct bfa_fcxp_mod_s *mod = fcxp->fcxp_mod;

	bfa_assert(fcxp != NULL);
	bfa_trc(mod->bfa, fcxp->fcxp_tag);
	bfa_fcxp_put(fcxp);
}

/*
 * Send a FCXP request
 *
 * @param[in]	fcxp	BFA fcxp pointer
 * @param[in]	rport	BFA rport pointer. Could be left NULL for WKA rports
 * @param[in]	vf_id	virtual Fabric ID
 * @param[in]	lp_tag	lport tag
 * @param[in]	cts	use Continous sequence
 * @param[in]	cos	fc Class of Service
 * @param[in]	reqlen	request length, does not include FCHS length
 * @param[in]	fchs	fc Header Pointer. The header content will be copied
 *			in by BFA.
 *
 * @param[in]	cbfn	call back function to be called on receiving
 *								the response
 * @param[in]	cbarg	arg for cbfn
 * @param[in]	rsp_timeout
 *			response timeout
 *
 * @return		bfa_status_t
 */
void
bfa_fcxp_send(struct bfa_fcxp_s *fcxp, struct bfa_rport_s *rport,
	      u16 vf_id, u8 lp_tag, bfa_boolean_t cts, enum fc_cos cos,
	      u32 reqlen, struct fchs_s *fchs, bfa_cb_fcxp_send_t cbfn,
	      void *cbarg, u32 rsp_maxlen, u8 rsp_timeout)
{
	struct bfa_s			*bfa  = fcxp->fcxp_mod->bfa;
	struct bfa_fcxp_req_info_s	*reqi = &fcxp->req_info;
	struct bfa_fcxp_rsp_info_s	*rspi = &fcxp->rsp_info;
	struct bfi_fcxp_send_req_s	*send_req;

	bfa_trc(bfa, fcxp->fcxp_tag);

	/*
	 * setup request/response info
	 */
	reqi->bfa_rport = rport;
	reqi->vf_id = vf_id;
	reqi->lp_tag = lp_tag;
	reqi->class = cos;
	rspi->rsp_timeout = rsp_timeout;
	reqi->cts = cts;
	reqi->fchs = *fchs;
	reqi->req_tot_len = reqlen;
	rspi->rsp_maxlen = rsp_maxlen;
	fcxp->send_cbfn = cbfn ? cbfn : bfa_fcxp_null_comp;
	fcxp->send_cbarg = cbarg;

	/*
	 * If no room in CPE queue, wait for space in request queue
	 */
	send_req = bfa_reqq_next(bfa, BFA_REQQ_FCXP);
	if (!send_req) {
		bfa_trc(bfa, fcxp->fcxp_tag);
		fcxp->reqq_waiting = BFA_TRUE;
		bfa_reqq_wait(bfa, BFA_REQQ_FCXP, &fcxp->reqq_wqe);
		return;
	}

	bfa_fcxp_queue(fcxp, send_req);
}

/*
 * Abort a BFA FCXP
 *
 * @param[in]	fcxp	BFA fcxp pointer
 *
 * @return		void
 */
bfa_status_t
bfa_fcxp_abort(struct bfa_fcxp_s *fcxp)
{
	bfa_trc(fcxp->fcxp_mod->bfa, fcxp->fcxp_tag);
	bfa_assert(0);
	return BFA_STATUS_OK;
}

void
bfa_fcxp_alloc_wait(struct bfa_s *bfa, struct bfa_fcxp_wqe_s *wqe,
	       bfa_fcxp_alloc_cbfn_t alloc_cbfn, void *alloc_cbarg,
	       void *caller, int nreq_sgles,
	       int nrsp_sgles, bfa_fcxp_get_sgaddr_t req_sga_cbfn,
	       bfa_fcxp_get_sglen_t req_sglen_cbfn,
	       bfa_fcxp_get_sgaddr_t rsp_sga_cbfn,
	       bfa_fcxp_get_sglen_t rsp_sglen_cbfn)
{
	struct bfa_fcxp_mod_s *mod = BFA_FCXP_MOD(bfa);

	bfa_assert(list_empty(&mod->fcxp_free_q));

	wqe->alloc_cbfn = alloc_cbfn;
	wqe->alloc_cbarg = alloc_cbarg;
	wqe->caller = caller;
	wqe->bfa = bfa;
	wqe->nreq_sgles = nreq_sgles;
	wqe->nrsp_sgles = nrsp_sgles;
	wqe->req_sga_cbfn = req_sga_cbfn;
	wqe->req_sglen_cbfn = req_sglen_cbfn;
	wqe->rsp_sga_cbfn = rsp_sga_cbfn;
	wqe->rsp_sglen_cbfn = rsp_sglen_cbfn;

	list_add_tail(&wqe->qe, &mod->wait_q);
}

void
bfa_fcxp_walloc_cancel(struct bfa_s *bfa, struct bfa_fcxp_wqe_s *wqe)
{
	struct bfa_fcxp_mod_s *mod = BFA_FCXP_MOD(bfa);

	bfa_assert(bfa_q_is_on_q(&mod->wait_q, wqe));
	list_del(&wqe->qe);
}

void
bfa_fcxp_discard(struct bfa_fcxp_s *fcxp)
{
	/*
	 * If waiting for room in request queue, cancel reqq wait
	 * and free fcxp.
	 */
	if (fcxp->reqq_waiting) {
		fcxp->reqq_waiting = BFA_FALSE;
		bfa_reqq_wcancel(&fcxp->reqq_wqe);
		bfa_fcxp_free(fcxp);
		return;
	}

	fcxp->send_cbfn = bfa_fcxp_null_comp;
}

void
bfa_fcxp_isr(struct bfa_s *bfa, struct bfi_msg_s *msg)
{
	switch (msg->mhdr.msg_id) {
	case BFI_FCXP_I2H_SEND_RSP:
		hal_fcxp_send_comp(bfa, (struct bfi_fcxp_send_rsp_s *) msg);
		break;

	default:
		bfa_trc(bfa, msg->mhdr.msg_id);
		bfa_assert(0);
	}
}

u32
bfa_fcxp_get_maxrsp(struct bfa_s *bfa)
{
	struct bfa_fcxp_mod_s *mod = BFA_FCXP_MOD(bfa);

	return mod->rsp_pld_sz;
}


/*
 *  BFA LPS state machine functions
 */

/*
 * Init state -- no login
 */
static void
bfa_lps_sm_init(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_LOGIN:
		if (bfa_reqq_full(lps->bfa, lps->reqq)) {
			bfa_sm_set_state(lps, bfa_lps_sm_loginwait);
			bfa_reqq_wait(lps->bfa, lps->reqq, &lps->wqe);
		} else {
			bfa_sm_set_state(lps, bfa_lps_sm_login);
			bfa_lps_send_login(lps);
		}

		if (lps->fdisc)
			bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
				BFA_PL_EID_LOGIN, 0, "FDISC Request");
		else
			bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
				BFA_PL_EID_LOGIN, 0, "FLOGI Request");
		break;

	case BFA_LPS_SM_LOGOUT:
		bfa_lps_logout_comp(lps);
		break;

	case BFA_LPS_SM_DELETE:
		bfa_lps_free(lps);
		break;

	case BFA_LPS_SM_RX_CVL:
	case BFA_LPS_SM_OFFLINE:
		break;

	case BFA_LPS_SM_FWRSP:
		/*
		 * Could happen when fabric detects loopback and discards
		 * the lps request. Fw will eventually sent out the timeout
		 * Just ignore
		 */
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}

/*
 * login is in progress -- awaiting response from firmware
 */
static void
bfa_lps_sm_login(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_FWRSP:
		if (lps->status == BFA_STATUS_OK) {
			bfa_sm_set_state(lps, bfa_lps_sm_online);
			if (lps->fdisc)
				bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
					BFA_PL_EID_LOGIN, 0, "FDISC Accept");
			else
				bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
					BFA_PL_EID_LOGIN, 0, "FLOGI Accept");
		} else {
			bfa_sm_set_state(lps, bfa_lps_sm_init);
			if (lps->fdisc)
				bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
					BFA_PL_EID_LOGIN, 0,
					"FDISC Fail (RJT or timeout)");
			else
				bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
					BFA_PL_EID_LOGIN, 0,
					"FLOGI Fail (RJT or timeout)");
		}
		bfa_lps_login_comp(lps);
		break;

	case BFA_LPS_SM_OFFLINE:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}

/*
 * login pending - awaiting space in request queue
 */
static void
bfa_lps_sm_loginwait(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_RESUME:
		bfa_sm_set_state(lps, bfa_lps_sm_login);
		break;

	case BFA_LPS_SM_OFFLINE:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		bfa_reqq_wcancel(&lps->wqe);
		break;

	case BFA_LPS_SM_RX_CVL:
		/*
		 * Login was not even sent out; so when getting out
		 * of this state, it will appear like a login retry
		 * after Clear virtual link
		 */
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}

/*
 * login complete
 */
static void
bfa_lps_sm_online(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_LOGOUT:
		if (bfa_reqq_full(lps->bfa, lps->reqq)) {
			bfa_sm_set_state(lps, bfa_lps_sm_logowait);
			bfa_reqq_wait(lps->bfa, lps->reqq, &lps->wqe);
		} else {
			bfa_sm_set_state(lps, bfa_lps_sm_logout);
			bfa_lps_send_logout(lps);
		}
		bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
			BFA_PL_EID_LOGO, 0, "Logout");
		break;

	case BFA_LPS_SM_RX_CVL:
		bfa_sm_set_state(lps, bfa_lps_sm_init);

		/* Let the vport module know about this event */
		bfa_lps_cvl_event(lps);
		bfa_plog_str(lps->bfa->plog, BFA_PL_MID_LPS,
			BFA_PL_EID_FIP_FCF_CVL, 0, "FCF Clear Virt. Link Rx");
		break;

	case BFA_LPS_SM_OFFLINE:
	case BFA_LPS_SM_DELETE:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}

/*
 * logout in progress - awaiting firmware response
 */
static void
bfa_lps_sm_logout(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_FWRSP:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		bfa_lps_logout_comp(lps);
		break;

	case BFA_LPS_SM_OFFLINE:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}

/*
 * logout pending -- awaiting space in request queue
 */
static void
bfa_lps_sm_logowait(struct bfa_lps_s *lps, enum bfa_lps_event event)
{
	bfa_trc(lps->bfa, lps->lp_tag);
	bfa_trc(lps->bfa, event);

	switch (event) {
	case BFA_LPS_SM_RESUME:
		bfa_sm_set_state(lps, bfa_lps_sm_logout);
		bfa_lps_send_logout(lps);
		break;

	case BFA_LPS_SM_OFFLINE:
		bfa_sm_set_state(lps, bfa_lps_sm_init);
		bfa_reqq_wcancel(&lps->wqe);
		break;

	default:
		bfa_sm_fault(lps->bfa, event);
	}
}



/*
 *  lps_pvt BFA LPS private functions
 */

/*
 * return memory requirement
 */
static void
bfa_lps_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *ndm_len,
	u32 *dm_len)
{
	if (cfg->drvcfg.min_cfg)
		*ndm_len += sizeof(struct bfa_lps_s) * BFA_LPS_MIN_LPORTS;
	else
		*ndm_len += sizeof(struct bfa_lps_s) * BFA_LPS_MAX_LPORTS;
}

/*
 * bfa module attach at initialization time
 */
static void
bfa_lps_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
	struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;
	int			i;

	memset(mod, 0, sizeof(struct bfa_lps_mod_s));
	mod->num_lps = BFA_LPS_MAX_LPORTS;
	if (cfg->drvcfg.min_cfg)
		mod->num_lps = BFA_LPS_MIN_LPORTS;
	else
		mod->num_lps = BFA_LPS_MAX_LPORTS;
	mod->lps_arr = lps = (struct bfa_lps_s *) bfa_meminfo_kva(meminfo);

	bfa_meminfo_kva(meminfo) += mod->num_lps * sizeof(struct bfa_lps_s);

	INIT_LIST_HEAD(&mod->lps_free_q);
	INIT_LIST_HEAD(&mod->lps_active_q);

	for (i = 0; i < mod->num_lps; i++, lps++) {
		lps->bfa	= bfa;
		lps->lp_tag	= (u8) i;
		lps->reqq	= BFA_REQQ_LPS;
		bfa_reqq_winit(&lps->wqe, bfa_lps_reqq_resume, lps);
		list_add_tail(&lps->qe, &mod->lps_free_q);
	}
}

static void
bfa_lps_detach(struct bfa_s *bfa)
{
}

static void
bfa_lps_start(struct bfa_s *bfa)
{
}

static void
bfa_lps_stop(struct bfa_s *bfa)
{
}

/*
 * IOC in disabled state -- consider all lps offline
 */
static void
bfa_lps_iocdisable(struct bfa_s *bfa)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;
	struct list_head		*qe, *qen;

	list_for_each_safe(qe, qen, &mod->lps_active_q) {
		lps = (struct bfa_lps_s *) qe;
		bfa_sm_send_event(lps, BFA_LPS_SM_OFFLINE);
	}
}

/*
 * Firmware login response
 */
static void
bfa_lps_login_rsp(struct bfa_s *bfa, struct bfi_lps_login_rsp_s *rsp)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;

	bfa_assert(rsp->lp_tag < mod->num_lps);
	lps = BFA_LPS_FROM_TAG(mod, rsp->lp_tag);

	lps->status = rsp->status;
	switch (rsp->status) {
	case BFA_STATUS_OK:
		lps->fport	= rsp->f_port;
		lps->npiv_en	= rsp->npiv_en;
		lps->lp_pid	= rsp->lp_pid;
		lps->pr_bbcred	= be16_to_cpu(rsp->bb_credit);
		lps->pr_pwwn	= rsp->port_name;
		lps->pr_nwwn	= rsp->node_name;
		lps->auth_req	= rsp->auth_req;
		lps->lp_mac	= rsp->lp_mac;
		lps->brcd_switch = rsp->brcd_switch;
		lps->fcf_mac	= rsp->fcf_mac;

		break;

	case BFA_STATUS_FABRIC_RJT:
		lps->lsrjt_rsn = rsp->lsrjt_rsn;
		lps->lsrjt_expl = rsp->lsrjt_expl;

		break;

	case BFA_STATUS_EPROTOCOL:
		lps->ext_status = rsp->ext_status;

		break;

	default:
		/* Nothing to do with other status */
		break;
	}

	bfa_sm_send_event(lps, BFA_LPS_SM_FWRSP);
}

/*
 * Firmware logout response
 */
static void
bfa_lps_logout_rsp(struct bfa_s *bfa, struct bfi_lps_logout_rsp_s *rsp)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;

	bfa_assert(rsp->lp_tag < mod->num_lps);
	lps = BFA_LPS_FROM_TAG(mod, rsp->lp_tag);

	bfa_sm_send_event(lps, BFA_LPS_SM_FWRSP);
}

/*
 * Firmware received a Clear virtual link request (for FCoE)
 */
static void
bfa_lps_rx_cvl_event(struct bfa_s *bfa, struct bfi_lps_cvl_event_s *cvl)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;

	lps = BFA_LPS_FROM_TAG(mod, cvl->lp_tag);

	bfa_sm_send_event(lps, BFA_LPS_SM_RX_CVL);
}

/*
 * Space is available in request queue, resume queueing request to firmware.
 */
static void
bfa_lps_reqq_resume(void *lps_arg)
{
	struct bfa_lps_s	*lps = lps_arg;

	bfa_sm_send_event(lps, BFA_LPS_SM_RESUME);
}

/*
 * lps is freed -- triggered by vport delete
 */
static void
bfa_lps_free(struct bfa_lps_s *lps)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(lps->bfa);

	lps->lp_pid = 0;
	list_del(&lps->qe);
	list_add_tail(&lps->qe, &mod->lps_free_q);
}

/*
 * send login request to firmware
 */
static void
bfa_lps_send_login(struct bfa_lps_s *lps)
{
	struct bfi_lps_login_req_s	*m;

	m = bfa_reqq_next(lps->bfa, lps->reqq);
	bfa_assert(m);

	bfi_h2i_set(m->mh, BFI_MC_LPS, BFI_LPS_H2I_LOGIN_REQ,
		bfa_lpuid(lps->bfa));

	m->lp_tag	= lps->lp_tag;
	m->alpa		= lps->alpa;
	m->pdu_size	= cpu_to_be16(lps->pdusz);
	m->pwwn		= lps->pwwn;
	m->nwwn		= lps->nwwn;
	m->fdisc	= lps->fdisc;
	m->auth_en	= lps->auth_en;

	bfa_reqq_produce(lps->bfa, lps->reqq);
}

/*
 * send logout request to firmware
 */
static void
bfa_lps_send_logout(struct bfa_lps_s *lps)
{
	struct bfi_lps_logout_req_s *m;

	m = bfa_reqq_next(lps->bfa, lps->reqq);
	bfa_assert(m);

	bfi_h2i_set(m->mh, BFI_MC_LPS, BFI_LPS_H2I_LOGOUT_REQ,
		bfa_lpuid(lps->bfa));

	m->lp_tag    = lps->lp_tag;
	m->port_name = lps->pwwn;
	bfa_reqq_produce(lps->bfa, lps->reqq);
}

/*
 * Indirect login completion handler for non-fcs
 */
static void
bfa_lps_login_comp_cb(void *arg, bfa_boolean_t complete)
{
	struct bfa_lps_s *lps	= arg;

	if (!complete)
		return;

	if (lps->fdisc)
		bfa_cb_lps_fdisc_comp(lps->bfa->bfad, lps->uarg, lps->status);
	else
		bfa_cb_lps_flogi_comp(lps->bfa->bfad, lps->uarg, lps->status);
}

/*
 * Login completion handler -- direct call for fcs, queue for others
 */
static void
bfa_lps_login_comp(struct bfa_lps_s *lps)
{
	if (!lps->bfa->fcs) {
		bfa_cb_queue(lps->bfa, &lps->hcb_qe, bfa_lps_login_comp_cb,
			lps);
		return;
	}

	if (lps->fdisc)
		bfa_cb_lps_fdisc_comp(lps->bfa->bfad, lps->uarg, lps->status);
	else
		bfa_cb_lps_flogi_comp(lps->bfa->bfad, lps->uarg, lps->status);
}

/*
 * Indirect logout completion handler for non-fcs
 */
static void
bfa_lps_logout_comp_cb(void *arg, bfa_boolean_t complete)
{
	struct bfa_lps_s *lps	= arg;

	if (!complete)
		return;

	if (lps->fdisc)
		bfa_cb_lps_fdisclogo_comp(lps->bfa->bfad, lps->uarg);
}

/*
 * Logout completion handler -- direct call for fcs, queue for others
 */
static void
bfa_lps_logout_comp(struct bfa_lps_s *lps)
{
	if (!lps->bfa->fcs) {
		bfa_cb_queue(lps->bfa, &lps->hcb_qe, bfa_lps_logout_comp_cb,
			lps);
		return;
	}
	if (lps->fdisc)
		bfa_cb_lps_fdisclogo_comp(lps->bfa->bfad, lps->uarg);
}

/*
 * Clear virtual link completion handler for non-fcs
 */
static void
bfa_lps_cvl_event_cb(void *arg, bfa_boolean_t complete)
{
	struct bfa_lps_s *lps	= arg;

	if (!complete)
		return;

	/* Clear virtual link to base port will result in link down */
	if (lps->fdisc)
		bfa_cb_lps_cvl_event(lps->bfa->bfad, lps->uarg);
}

/*
 * Received Clear virtual link event --direct call for fcs,
 * queue for others
 */
static void
bfa_lps_cvl_event(struct bfa_lps_s *lps)
{
	if (!lps->bfa->fcs) {
		bfa_cb_queue(lps->bfa, &lps->hcb_qe, bfa_lps_cvl_event_cb,
			lps);
		return;
	}

	/* Clear virtual link to base port will result in link down */
	if (lps->fdisc)
		bfa_cb_lps_cvl_event(lps->bfa->bfad, lps->uarg);
}



/*
 *  lps_public BFA LPS public functions
 */

u32
bfa_lps_get_max_vport(struct bfa_s *bfa)
{
	if (bfa_ioc_devid(&bfa->ioc) == BFA_PCI_DEVICE_ID_CT)
		return BFA_LPS_MAX_VPORTS_SUPP_CT;
	else
		return BFA_LPS_MAX_VPORTS_SUPP_CB;
}

/*
 * Allocate a lport srvice tag.
 */
struct bfa_lps_s  *
bfa_lps_alloc(struct bfa_s *bfa)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps = NULL;

	bfa_q_deq(&mod->lps_free_q, &lps);

	if (lps == NULL)
		return NULL;

	list_add_tail(&lps->qe, &mod->lps_active_q);

	bfa_sm_set_state(lps, bfa_lps_sm_init);
	return lps;
}

/*
 * Free lport service tag. This can be called anytime after an alloc.
 * No need to wait for any pending login/logout completions.
 */
void
bfa_lps_delete(struct bfa_lps_s *lps)
{
	bfa_sm_send_event(lps, BFA_LPS_SM_DELETE);
}

/*
 * Initiate a lport login.
 */
void
bfa_lps_flogi(struct bfa_lps_s *lps, void *uarg, u8 alpa, u16 pdusz,
	wwn_t pwwn, wwn_t nwwn, bfa_boolean_t auth_en)
{
	lps->uarg	= uarg;
	lps->alpa	= alpa;
	lps->pdusz	= pdusz;
	lps->pwwn	= pwwn;
	lps->nwwn	= nwwn;
	lps->fdisc	= BFA_FALSE;
	lps->auth_en	= auth_en;
	bfa_sm_send_event(lps, BFA_LPS_SM_LOGIN);
}

/*
 * Initiate a lport fdisc login.
 */
void
bfa_lps_fdisc(struct bfa_lps_s *lps, void *uarg, u16 pdusz, wwn_t pwwn,
	wwn_t nwwn)
{
	lps->uarg	= uarg;
	lps->alpa	= 0;
	lps->pdusz	= pdusz;
	lps->pwwn	= pwwn;
	lps->nwwn	= nwwn;
	lps->fdisc	= BFA_TRUE;
	lps->auth_en	= BFA_FALSE;
	bfa_sm_send_event(lps, BFA_LPS_SM_LOGIN);
}


/*
 * Initiate a lport FDSIC logout.
 */
void
bfa_lps_fdisclogo(struct bfa_lps_s *lps)
{
	bfa_sm_send_event(lps, BFA_LPS_SM_LOGOUT);
}


/*
 * Return lport services tag given the pid
 */
u8
bfa_lps_get_tag_from_pid(struct bfa_s *bfa, u32 pid)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);
	struct bfa_lps_s	*lps;
	int			i;

	for (i = 0, lps = mod->lps_arr; i < mod->num_lps; i++, lps++) {
		if (lps->lp_pid == pid)
			return lps->lp_tag;
	}

	/* Return base port tag anyway */
	return 0;
}


/*
 * return port id assigned to the base lport
 */
u32
bfa_lps_get_base_pid(struct bfa_s *bfa)
{
	struct bfa_lps_mod_s	*mod = BFA_LPS_MOD(bfa);

	return BFA_LPS_FROM_TAG(mod, 0)->lp_pid;
}

/*
 * LPS firmware message class handler.
 */
void
bfa_lps_isr(struct bfa_s *bfa, struct bfi_msg_s *m)
{
	union bfi_lps_i2h_msg_u	msg;

	bfa_trc(bfa, m->mhdr.msg_id);
	msg.msg = m;

	switch (m->mhdr.msg_id) {
	case BFI_LPS_H2I_LOGIN_RSP:
		bfa_lps_login_rsp(bfa, msg.login_rsp);
		break;

	case BFI_LPS_H2I_LOGOUT_RSP:
		bfa_lps_logout_rsp(bfa, msg.logout_rsp);
		break;

	case BFI_LPS_H2I_CVL_EVENT:
		bfa_lps_rx_cvl_event(bfa, msg.cvl_event);
		break;

	default:
		bfa_trc(bfa, m->mhdr.msg_id);
		bfa_assert(0);
	}
}

/*
 * FC PORT state machine functions
 */
static void
bfa_fcport_sm_uninit(struct bfa_fcport_s *fcport,
			enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_START:
		/*
		 * Start event after IOC is configured and BFA is started.
		 */
		if (bfa_fcport_send_enable(fcport)) {
			bfa_trc(fcport->bfa, BFA_TRUE);
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		} else {
			bfa_trc(fcport->bfa, BFA_FALSE);
			bfa_sm_set_state(fcport,
					bfa_fcport_sm_enabling_qwait);
		}
		break;

	case BFA_FCPORT_SM_ENABLE:
		/*
		 * Port is persistently configured to be in enabled state. Do
		 * not change state. Port enabling is done when START event is
		 * received.
		 */
		break;

	case BFA_FCPORT_SM_DISABLE:
		/*
		 * If a port is persistently configured to be disabled, the
		 * first event will a port disable request.
		 */
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabled);
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_enabling_qwait(struct bfa_fcport_s *fcport,
				enum bfa_fcport_sm_event event)
{
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_QRESUME:
		bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		bfa_fcport_send_enable(fcport);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_reqq_wcancel(&fcport->reqq_wait);
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		break;

	case BFA_FCPORT_SM_ENABLE:
		/*
		 * Already enable is in progress.
		 */
		break;

	case BFA_FCPORT_SM_DISABLE:
		/*
		 * Just send disable request to firmware when room becomes
		 * available in request queue.
		 */
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabled);
		bfa_reqq_wcancel(&fcport->reqq_wait);
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_DISABLE, 0, "Port Disable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port disabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_LINKUP:
	case BFA_FCPORT_SM_LINKDOWN:
		/*
		 * Possible to get link events when doing back-to-back
		 * enable/disables.
		 */
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_reqq_wcancel(&fcport->reqq_wait);
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_enabling(struct bfa_fcport_s *fcport,
						enum bfa_fcport_sm_event event)
{
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_FWRSP:
	case BFA_FCPORT_SM_LINKDOWN:
		bfa_sm_set_state(fcport, bfa_fcport_sm_linkdown);
		break;

	case BFA_FCPORT_SM_LINKUP:
		bfa_fcport_update_linkinfo(fcport);
		bfa_sm_set_state(fcport, bfa_fcport_sm_linkup);

		bfa_assert(fcport->event_cbfn);
		bfa_fcport_scn(fcport, BFA_PORT_LINKUP, BFA_FALSE);
		break;

	case BFA_FCPORT_SM_ENABLE:
		/*
		 * Already being enabled.
		 */
		break;

	case BFA_FCPORT_SM_DISABLE:
		if (bfa_fcport_send_disable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_disabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_disabling_qwait);

		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_DISABLE, 0, "Port Disable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port disabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_linkdown(struct bfa_fcport_s *fcport,
						enum bfa_fcport_sm_event event)
{
	struct bfi_fcport_event_s *pevent = fcport->event_arg.i2hmsg.event;
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;

	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_LINKUP:
		bfa_fcport_update_linkinfo(fcport);
		bfa_sm_set_state(fcport, bfa_fcport_sm_linkup);
		bfa_assert(fcport->event_cbfn);
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_ST_CHANGE, 0, "Port Linkup");
		if (!bfa_ioc_get_fcmode(&fcport->bfa->ioc)) {

			bfa_trc(fcport->bfa,
				pevent->link_state.vc_fcf.fcf.fipenabled);
			bfa_trc(fcport->bfa,
				pevent->link_state.vc_fcf.fcf.fipfailed);

			if (pevent->link_state.vc_fcf.fcf.fipfailed)
				bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
					BFA_PL_EID_FIP_FCF_DISC, 0,
					"FIP FCF Discovery Failed");
			else
				bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
					BFA_PL_EID_FIP_FCF_DISC, 0,
					"FIP FCF Discovered");
		}

		bfa_fcport_scn(fcport, BFA_PORT_LINKUP, BFA_FALSE);
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port online: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_LINKDOWN:
		/*
		 * Possible to get link down event.
		 */
		break;

	case BFA_FCPORT_SM_ENABLE:
		/*
		 * Already enabled.
		 */
		break;

	case BFA_FCPORT_SM_DISABLE:
		if (bfa_fcport_send_disable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_disabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_disabling_qwait);

		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_DISABLE, 0, "Port Disable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port disabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_linkup(struct bfa_fcport_s *fcport,
	enum bfa_fcport_sm_event event)
{
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;

	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_ENABLE:
		/*
		 * Already enabled.
		 */
		break;

	case BFA_FCPORT_SM_DISABLE:
		if (bfa_fcport_send_disable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_disabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_disabling_qwait);

		bfa_fcport_reset_linkinfo(fcport);
		bfa_fcport_scn(fcport, BFA_PORT_LINKDOWN, BFA_FALSE);
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_DISABLE, 0, "Port Disable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port offline: WWN = %s\n", pwwn_buf);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port disabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_LINKDOWN:
		bfa_sm_set_state(fcport, bfa_fcport_sm_linkdown);
		bfa_fcport_reset_linkinfo(fcport);
		bfa_fcport_scn(fcport, BFA_PORT_LINKDOWN, BFA_FALSE);
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_ST_CHANGE, 0, "Port Linkdown");
		wwn2str(pwwn_buf, fcport->pwwn);
		if (BFA_PORT_IS_DISABLED(fcport->bfa))
			BFA_LOG(KERN_INFO, bfad, bfa_log_level,
				"Base port offline: WWN = %s\n", pwwn_buf);
		else
			BFA_LOG(KERN_ERR, bfad, bfa_log_level,
				"Base port (WWN = %s) "
				"lost fabric connectivity\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		bfa_fcport_reset_linkinfo(fcport);
		wwn2str(pwwn_buf, fcport->pwwn);
		if (BFA_PORT_IS_DISABLED(fcport->bfa))
			BFA_LOG(KERN_INFO, bfad, bfa_log_level,
				"Base port offline: WWN = %s\n", pwwn_buf);
		else
			BFA_LOG(KERN_ERR, bfad, bfa_log_level,
				"Base port (WWN = %s) "
				"lost fabric connectivity\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		bfa_fcport_reset_linkinfo(fcport);
		bfa_fcport_scn(fcport, BFA_PORT_LINKDOWN, BFA_FALSE);
		wwn2str(pwwn_buf, fcport->pwwn);
		if (BFA_PORT_IS_DISABLED(fcport->bfa))
			BFA_LOG(KERN_INFO, bfad, bfa_log_level,
				"Base port offline: WWN = %s\n", pwwn_buf);
		else
			BFA_LOG(KERN_ERR, bfad, bfa_log_level,
				"Base port (WWN = %s) "
				"lost fabric connectivity\n", pwwn_buf);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_disabling_qwait(struct bfa_fcport_s *fcport,
				 enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_QRESUME:
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabling);
		bfa_fcport_send_disable(fcport);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		bfa_reqq_wcancel(&fcport->reqq_wait);
		break;

	case BFA_FCPORT_SM_ENABLE:
		bfa_sm_set_state(fcport, bfa_fcport_sm_toggling_qwait);
		break;

	case BFA_FCPORT_SM_DISABLE:
		/*
		 * Already being disabled.
		 */
		break;

	case BFA_FCPORT_SM_LINKUP:
	case BFA_FCPORT_SM_LINKDOWN:
		/*
		 * Possible to get link events when doing back-to-back
		 * enable/disables.
		 */
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocfail);
		bfa_reqq_wcancel(&fcport->reqq_wait);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_toggling_qwait(struct bfa_fcport_s *fcport,
				 enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_QRESUME:
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabling);
		bfa_fcport_send_disable(fcport);
		if (bfa_fcport_send_enable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_enabling_qwait);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		bfa_reqq_wcancel(&fcport->reqq_wait);
		break;

	case BFA_FCPORT_SM_ENABLE:
		break;

	case BFA_FCPORT_SM_DISABLE:
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabling_qwait);
		break;

	case BFA_FCPORT_SM_LINKUP:
	case BFA_FCPORT_SM_LINKDOWN:
		/*
		 * Possible to get link events when doing back-to-back
		 * enable/disables.
		 */
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocfail);
		bfa_reqq_wcancel(&fcport->reqq_wait);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_disabling(struct bfa_fcport_s *fcport,
						enum bfa_fcport_sm_event event)
{
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_FWRSP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabled);
		break;

	case BFA_FCPORT_SM_DISABLE:
		/*
		 * Already being disabled.
		 */
		break;

	case BFA_FCPORT_SM_ENABLE:
		if (bfa_fcport_send_enable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_enabling_qwait);

		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_ENABLE, 0, "Port Enable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port enabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		break;

	case BFA_FCPORT_SM_LINKUP:
	case BFA_FCPORT_SM_LINKDOWN:
		/*
		 * Possible to get link events when doing back-to-back
		 * enable/disables.
		 */
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocfail);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_disabled(struct bfa_fcport_s *fcport,
						enum bfa_fcport_sm_event event)
{
	char pwwn_buf[BFA_STRING_32];
	struct bfad_s *bfad = (struct bfad_s *)fcport->bfa->bfad;
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_START:
		/*
		 * Ignore start event for a port that is disabled.
		 */
		break;

	case BFA_FCPORT_SM_STOP:
		bfa_sm_set_state(fcport, bfa_fcport_sm_stopped);
		break;

	case BFA_FCPORT_SM_ENABLE:
		if (bfa_fcport_send_enable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_enabling_qwait);

		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
				BFA_PL_EID_PORT_ENABLE, 0, "Port Enable");
		wwn2str(pwwn_buf, fcport->pwwn);
		BFA_LOG(KERN_INFO, bfad, bfa_log_level,
			"Base port enabled: WWN = %s\n", pwwn_buf);
		break;

	case BFA_FCPORT_SM_DISABLE:
		/*
		 * Already disabled.
		 */
		break;

	case BFA_FCPORT_SM_HWFAIL:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocfail);
		break;

	default:
		bfa_sm_fault(fcport->bfa, event);
	}
}

static void
bfa_fcport_sm_stopped(struct bfa_fcport_s *fcport,
			 enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_START:
		if (bfa_fcport_send_enable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_enabling_qwait);
		break;

	default:
		/*
		 * Ignore all other events.
		 */
		;
	}
}

/*
 * Port is enabled. IOC is down/failed.
 */
static void
bfa_fcport_sm_iocdown(struct bfa_fcport_s *fcport,
			 enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_START:
		if (bfa_fcport_send_enable(fcport))
			bfa_sm_set_state(fcport, bfa_fcport_sm_enabling);
		else
			bfa_sm_set_state(fcport,
					 bfa_fcport_sm_enabling_qwait);
		break;

	default:
		/*
		 * Ignore all events.
		 */
		;
	}
}

/*
 * Port is disabled. IOC is down/failed.
 */
static void
bfa_fcport_sm_iocfail(struct bfa_fcport_s *fcport,
			 enum bfa_fcport_sm_event event)
{
	bfa_trc(fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_SM_START:
		bfa_sm_set_state(fcport, bfa_fcport_sm_disabled);
		break;

	case BFA_FCPORT_SM_ENABLE:
		bfa_sm_set_state(fcport, bfa_fcport_sm_iocdown);
		break;

	default:
		/*
		 * Ignore all events.
		 */
		;
	}
}

/*
 * Link state is down
 */
static void
bfa_fcport_ln_sm_dn(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKUP:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up_nf);
		bfa_fcport_queue_cb(ln, BFA_PORT_LINKUP);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is waiting for down notification
 */
static void
bfa_fcport_ln_sm_dn_nf(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKUP:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn_up_nf);
		break;

	case BFA_FCPORT_LN_SM_NOTIFICATION:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is waiting for down notification and there is a pending up
 */
static void
bfa_fcport_ln_sm_dn_up_nf(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKDOWN:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn_nf);
		break;

	case BFA_FCPORT_LN_SM_NOTIFICATION:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up_nf);
		bfa_fcport_queue_cb(ln, BFA_PORT_LINKUP);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is up
 */
static void
bfa_fcport_ln_sm_up(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKDOWN:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn_nf);
		bfa_fcport_queue_cb(ln, BFA_PORT_LINKDOWN);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is waiting for up notification
 */
static void
bfa_fcport_ln_sm_up_nf(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKDOWN:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up_dn_nf);
		break;

	case BFA_FCPORT_LN_SM_NOTIFICATION:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is waiting for up notification and there is a pending down
 */
static void
bfa_fcport_ln_sm_up_dn_nf(struct bfa_fcport_ln_s *ln,
		enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKUP:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up_dn_up_nf);
		break;

	case BFA_FCPORT_LN_SM_NOTIFICATION:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn_nf);
		bfa_fcport_queue_cb(ln, BFA_PORT_LINKDOWN);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

/*
 * Link state is waiting for up notification and there are pending down and up
 */
static void
bfa_fcport_ln_sm_up_dn_up_nf(struct bfa_fcport_ln_s *ln,
			enum bfa_fcport_ln_sm_event event)
{
	bfa_trc(ln->fcport->bfa, event);

	switch (event) {
	case BFA_FCPORT_LN_SM_LINKDOWN:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_up_dn_nf);
		break;

	case BFA_FCPORT_LN_SM_NOTIFICATION:
		bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn_up_nf);
		bfa_fcport_queue_cb(ln, BFA_PORT_LINKDOWN);
		break;

	default:
		bfa_sm_fault(ln->fcport->bfa, event);
	}
}

static void
__bfa_cb_fcport_event(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_fcport_ln_s *ln = cbarg;

	if (complete)
		ln->fcport->event_cbfn(ln->fcport->event_cbarg, ln->ln_event);
	else
		bfa_sm_send_event(ln, BFA_FCPORT_LN_SM_NOTIFICATION);
}

/*
 * Send SCN notification to upper layers.
 * trunk - false if caller is fcport to ignore fcport event in trunked mode
 */
static void
bfa_fcport_scn(struct bfa_fcport_s *fcport, enum bfa_port_linkstate event,
	bfa_boolean_t trunk)
{
	if (fcport->cfg.trunked && !trunk)
		return;

	switch (event) {
	case BFA_PORT_LINKUP:
		bfa_sm_send_event(&fcport->ln, BFA_FCPORT_LN_SM_LINKUP);
		break;
	case BFA_PORT_LINKDOWN:
		bfa_sm_send_event(&fcport->ln, BFA_FCPORT_LN_SM_LINKDOWN);
		break;
	default:
		bfa_assert(0);
	}
}

static void
bfa_fcport_queue_cb(struct bfa_fcport_ln_s *ln, enum bfa_port_linkstate event)
{
	struct bfa_fcport_s *fcport = ln->fcport;

	if (fcport->bfa->fcs) {
		fcport->event_cbfn(fcport->event_cbarg, event);
		bfa_sm_send_event(ln, BFA_FCPORT_LN_SM_NOTIFICATION);
	} else {
		ln->ln_event = event;
		bfa_cb_queue(fcport->bfa, &ln->ln_qe,
			__bfa_cb_fcport_event, ln);
	}
}

#define FCPORT_STATS_DMA_SZ (BFA_ROUNDUP(sizeof(union bfa_fcport_stats_u), \
							BFA_CACHELINE_SZ))

static void
bfa_fcport_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *ndm_len,
		u32 *dm_len)
{
	*dm_len += FCPORT_STATS_DMA_SZ;
}

static void
bfa_fcport_qresume(void *cbarg)
{
	struct bfa_fcport_s *fcport = cbarg;

	bfa_sm_send_event(fcport, BFA_FCPORT_SM_QRESUME);
}

static void
bfa_fcport_mem_claim(struct bfa_fcport_s *fcport, struct bfa_meminfo_s *meminfo)
{
	u8		*dm_kva;
	u64	dm_pa;

	dm_kva = bfa_meminfo_dma_virt(meminfo);
	dm_pa  = bfa_meminfo_dma_phys(meminfo);

	fcport->stats_kva = dm_kva;
	fcport->stats_pa  = dm_pa;
	fcport->stats	  = (union bfa_fcport_stats_u *) dm_kva;

	dm_kva += FCPORT_STATS_DMA_SZ;
	dm_pa  += FCPORT_STATS_DMA_SZ;

	bfa_meminfo_dma_virt(meminfo) = dm_kva;
	bfa_meminfo_dma_phys(meminfo) = dm_pa;
}

/*
 * Memory initialization.
 */
static void
bfa_fcport_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);
	struct bfa_port_cfg_s *port_cfg = &fcport->cfg;
	struct bfa_fcport_ln_s *ln = &fcport->ln;
	struct timeval tv;

	memset(fcport, 0, sizeof(struct bfa_fcport_s));
	fcport->bfa = bfa;
	ln->fcport = fcport;

	bfa_fcport_mem_claim(fcport, meminfo);

	bfa_sm_set_state(fcport, bfa_fcport_sm_uninit);
	bfa_sm_set_state(ln, bfa_fcport_ln_sm_dn);

	/*
	 * initialize time stamp for stats reset
	 */
	do_gettimeofday(&tv);
	fcport->stats_reset_time = tv.tv_sec;

	/*
	 * initialize and set default configuration
	 */
	port_cfg->topology = BFA_PORT_TOPOLOGY_P2P;
	port_cfg->speed = BFA_PORT_SPEED_AUTO;
	port_cfg->trunked = BFA_FALSE;
	port_cfg->maxfrsize = 0;

	port_cfg->trl_def_speed = BFA_PORT_SPEED_1GBPS;

	bfa_reqq_winit(&fcport->reqq_wait, bfa_fcport_qresume, fcport);
}

static void
bfa_fcport_detach(struct bfa_s *bfa)
{
}

/*
 * Called when IOC is ready.
 */
static void
bfa_fcport_start(struct bfa_s *bfa)
{
	bfa_sm_send_event(BFA_FCPORT_MOD(bfa), BFA_FCPORT_SM_START);
}

/*
 * Called before IOC is stopped.
 */
static void
bfa_fcport_stop(struct bfa_s *bfa)
{
	bfa_sm_send_event(BFA_FCPORT_MOD(bfa), BFA_FCPORT_SM_STOP);
	bfa_trunk_iocdisable(bfa);
}

/*
 * Called when IOC failure is detected.
 */
static void
bfa_fcport_iocdisable(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_sm_send_event(fcport, BFA_FCPORT_SM_HWFAIL);
	bfa_trunk_iocdisable(bfa);
}

static void
bfa_fcport_update_linkinfo(struct bfa_fcport_s *fcport)
{
	struct bfi_fcport_event_s *pevent = fcport->event_arg.i2hmsg.event;
	struct bfa_fcport_trunk_s *trunk = &fcport->trunk;

	fcport->speed = pevent->link_state.speed;
	fcport->topology = pevent->link_state.topology;

	if (fcport->topology == BFA_PORT_TOPOLOGY_LOOP)
		fcport->myalpa = 0;

	/* QoS Details */
	fcport->qos_attr = pevent->link_state.qos_attr;
	fcport->qos_vc_attr = pevent->link_state.vc_fcf.qos_vc_attr;

	/*
	 * update trunk state if applicable
	 */
	if (!fcport->cfg.trunked)
		trunk->attr.state = BFA_TRUNK_DISABLED;

	/* update FCoE specific */
	fcport->fcoe_vlan = be16_to_cpu(pevent->link_state.vc_fcf.fcf.vlan);

	bfa_trc(fcport->bfa, fcport->speed);
	bfa_trc(fcport->bfa, fcport->topology);
}

static void
bfa_fcport_reset_linkinfo(struct bfa_fcport_s *fcport)
{
	fcport->speed = BFA_PORT_SPEED_UNKNOWN;
	fcport->topology = BFA_PORT_TOPOLOGY_NONE;
}

/*
 * Send port enable message to firmware.
 */
static bfa_boolean_t
bfa_fcport_send_enable(struct bfa_fcport_s *fcport)
{
	struct bfi_fcport_enable_req_s *m;

	/*
	 * Increment message tag before queue check, so that responses to old
	 * requests are discarded.
	 */
	fcport->msgtag++;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(fcport->bfa, BFA_REQQ_PORT);
	if (!m) {
		bfa_reqq_wait(fcport->bfa, BFA_REQQ_PORT,
							&fcport->reqq_wait);
		return BFA_FALSE;
	}

	bfi_h2i_set(m->mh, BFI_MC_FCPORT, BFI_FCPORT_H2I_ENABLE_REQ,
			bfa_lpuid(fcport->bfa));
	m->nwwn = fcport->nwwn;
	m->pwwn = fcport->pwwn;
	m->port_cfg = fcport->cfg;
	m->msgtag = fcport->msgtag;
	m->port_cfg.maxfrsize = cpu_to_be16(fcport->cfg.maxfrsize);
	bfa_dma_be_addr_set(m->stats_dma_addr, fcport->stats_pa);
	bfa_trc(fcport->bfa, m->stats_dma_addr.a32.addr_lo);
	bfa_trc(fcport->bfa, m->stats_dma_addr.a32.addr_hi);

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(fcport->bfa, BFA_REQQ_PORT);
	return BFA_TRUE;
}

/*
 * Send port disable message to firmware.
 */
static	bfa_boolean_t
bfa_fcport_send_disable(struct bfa_fcport_s *fcport)
{
	struct bfi_fcport_req_s *m;

	/*
	 * Increment message tag before queue check, so that responses to old
	 * requests are discarded.
	 */
	fcport->msgtag++;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(fcport->bfa, BFA_REQQ_PORT);
	if (!m) {
		bfa_reqq_wait(fcport->bfa, BFA_REQQ_PORT,
							&fcport->reqq_wait);
		return BFA_FALSE;
	}

	bfi_h2i_set(m->mh, BFI_MC_FCPORT, BFI_FCPORT_H2I_DISABLE_REQ,
			bfa_lpuid(fcport->bfa));
	m->msgtag = fcport->msgtag;

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(fcport->bfa, BFA_REQQ_PORT);

	return BFA_TRUE;
}

static void
bfa_fcport_set_wwns(struct bfa_fcport_s *fcport)
{
	fcport->pwwn = fcport->bfa->ioc.attr->pwwn;
	fcport->nwwn = fcport->bfa->ioc.attr->nwwn;

	bfa_trc(fcport->bfa, fcport->pwwn);
	bfa_trc(fcport->bfa, fcport->nwwn);
}

static void
bfa_fcport_send_txcredit(void *port_cbarg)
{

	struct bfa_fcport_s *fcport = port_cbarg;
	struct bfi_fcport_set_svc_params_req_s *m;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(fcport->bfa, BFA_REQQ_PORT);
	if (!m) {
		bfa_trc(fcport->bfa, fcport->cfg.tx_bbcredit);
		return;
	}

	bfi_h2i_set(m->mh, BFI_MC_FCPORT, BFI_FCPORT_H2I_SET_SVC_PARAMS_REQ,
			bfa_lpuid(fcport->bfa));
	m->tx_bbcredit = cpu_to_be16((u16)fcport->cfg.tx_bbcredit);

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(fcport->bfa, BFA_REQQ_PORT);
}

static void
bfa_fcport_qos_stats_swap(struct bfa_qos_stats_s *d,
	struct bfa_qos_stats_s *s)
{
	u32	*dip = (u32 *) d;
	__be32	*sip = (__be32 *) s;
	int		i;

	/* Now swap the 32 bit fields */
	for (i = 0; i < (sizeof(struct bfa_qos_stats_s)/sizeof(u32)); ++i)
		dip[i] = be32_to_cpu(sip[i]);
}

static void
bfa_fcport_fcoe_stats_swap(struct bfa_fcoe_stats_s *d,
	struct bfa_fcoe_stats_s *s)
{
	u32	*dip = (u32 *) d;
	__be32	*sip = (__be32 *) s;
	int		i;

	for (i = 0; i < ((sizeof(struct bfa_fcoe_stats_s))/sizeof(u32));
	     i = i + 2) {
#ifdef __BIG_ENDIAN
		dip[i] = be32_to_cpu(sip[i]);
		dip[i + 1] = be32_to_cpu(sip[i + 1]);
#else
		dip[i] = be32_to_cpu(sip[i + 1]);
		dip[i + 1] = be32_to_cpu(sip[i]);
#endif
	}
}

static void
__bfa_cb_fcport_stats_get(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_fcport_s *fcport = cbarg;

	if (complete) {
		if (fcport->stats_status == BFA_STATUS_OK) {
			struct timeval tv;

			/* Swap FC QoS or FCoE stats */
			if (bfa_ioc_get_fcmode(&fcport->bfa->ioc)) {
				bfa_fcport_qos_stats_swap(
					&fcport->stats_ret->fcqos,
					&fcport->stats->fcqos);
			} else {
				bfa_fcport_fcoe_stats_swap(
					&fcport->stats_ret->fcoe,
					&fcport->stats->fcoe);

				do_gettimeofday(&tv);
				fcport->stats_ret->fcoe.secs_reset =
					tv.tv_sec - fcport->stats_reset_time;
			}
		}
		fcport->stats_cbfn(fcport->stats_cbarg, fcport->stats_status);
	} else {
		fcport->stats_busy = BFA_FALSE;
		fcport->stats_status = BFA_STATUS_OK;
	}
}

static void
bfa_fcport_stats_get_timeout(void *cbarg)
{
	struct bfa_fcport_s *fcport = (struct bfa_fcport_s *) cbarg;

	bfa_trc(fcport->bfa, fcport->stats_qfull);

	if (fcport->stats_qfull) {
		bfa_reqq_wcancel(&fcport->stats_reqq_wait);
		fcport->stats_qfull = BFA_FALSE;
	}

	fcport->stats_status = BFA_STATUS_ETIMER;
	bfa_cb_queue(fcport->bfa, &fcport->hcb_qe, __bfa_cb_fcport_stats_get,
		fcport);
}

static void
bfa_fcport_send_stats_get(void *cbarg)
{
	struct bfa_fcport_s *fcport = (struct bfa_fcport_s *) cbarg;
	struct bfi_fcport_req_s *msg;

	msg = bfa_reqq_next(fcport->bfa, BFA_REQQ_PORT);

	if (!msg) {
		fcport->stats_qfull = BFA_TRUE;
		bfa_reqq_winit(&fcport->stats_reqq_wait,
				bfa_fcport_send_stats_get, fcport);
		bfa_reqq_wait(fcport->bfa, BFA_REQQ_PORT,
				&fcport->stats_reqq_wait);
		return;
	}
	fcport->stats_qfull = BFA_FALSE;

	memset(msg, 0, sizeof(struct bfi_fcport_req_s));
	bfi_h2i_set(msg->mh, BFI_MC_FCPORT, BFI_FCPORT_H2I_STATS_GET_REQ,
			bfa_lpuid(fcport->bfa));
	bfa_reqq_produce(fcport->bfa, BFA_REQQ_PORT);
}

static void
__bfa_cb_fcport_stats_clr(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_fcport_s *fcport = cbarg;

	if (complete) {
		struct timeval tv;

		/*
		 * re-initialize time stamp for stats reset
		 */
		do_gettimeofday(&tv);
		fcport->stats_reset_time = tv.tv_sec;

		fcport->stats_cbfn(fcport->stats_cbarg, fcport->stats_status);
	} else {
		fcport->stats_busy = BFA_FALSE;
		fcport->stats_status = BFA_STATUS_OK;
	}
}

static void
bfa_fcport_stats_clr_timeout(void *cbarg)
{
	struct bfa_fcport_s *fcport = (struct bfa_fcport_s *) cbarg;

	bfa_trc(fcport->bfa, fcport->stats_qfull);

	if (fcport->stats_qfull) {
		bfa_reqq_wcancel(&fcport->stats_reqq_wait);
		fcport->stats_qfull = BFA_FALSE;
	}

	fcport->stats_status = BFA_STATUS_ETIMER;
	bfa_cb_queue(fcport->bfa, &fcport->hcb_qe,
			__bfa_cb_fcport_stats_clr, fcport);
}

static void
bfa_fcport_send_stats_clear(void *cbarg)
{
	struct bfa_fcport_s *fcport = (struct bfa_fcport_s *) cbarg;
	struct bfi_fcport_req_s *msg;

	msg = bfa_reqq_next(fcport->bfa, BFA_REQQ_PORT);

	if (!msg) {
		fcport->stats_qfull = BFA_TRUE;
		bfa_reqq_winit(&fcport->stats_reqq_wait,
				bfa_fcport_send_stats_clear, fcport);
		bfa_reqq_wait(fcport->bfa, BFA_REQQ_PORT,
						&fcport->stats_reqq_wait);
		return;
	}
	fcport->stats_qfull = BFA_FALSE;

	memset(msg, 0, sizeof(struct bfi_fcport_req_s));
	bfi_h2i_set(msg->mh, BFI_MC_FCPORT, BFI_FCPORT_H2I_STATS_CLEAR_REQ,
			bfa_lpuid(fcport->bfa));
	bfa_reqq_produce(fcport->bfa, BFA_REQQ_PORT);
}

/*
 * Handle trunk SCN event from firmware.
 */
static void
bfa_trunk_scn(struct bfa_fcport_s *fcport, struct bfi_fcport_trunk_scn_s *scn)
{
	struct bfa_fcport_trunk_s *trunk = &fcport->trunk;
	struct bfi_fcport_trunk_link_s *tlink;
	struct bfa_trunk_link_attr_s *lattr;
	enum bfa_trunk_state state_prev;
	int i;
	int link_bm = 0;

	bfa_trc(fcport->bfa, fcport->cfg.trunked);
	bfa_assert(scn->trunk_state == BFA_TRUNK_ONLINE ||
		   scn->trunk_state == BFA_TRUNK_OFFLINE);

	bfa_trc(fcport->bfa, trunk->attr.state);
	bfa_trc(fcport->bfa, scn->trunk_state);
	bfa_trc(fcport->bfa, scn->trunk_speed);

	/*
	 * Save off new state for trunk attribute query
	 */
	state_prev = trunk->attr.state;
	if (fcport->cfg.trunked && (trunk->attr.state != BFA_TRUNK_DISABLED))
		trunk->attr.state = scn->trunk_state;
	trunk->attr.speed = scn->trunk_speed;
	for (i = 0; i < BFA_TRUNK_MAX_PORTS; i++) {
		lattr = &trunk->attr.link_attr[i];
		tlink = &scn->tlink[i];

		lattr->link_state = tlink->state;
		lattr->trunk_wwn  = tlink->trunk_wwn;
		lattr->fctl	  = tlink->fctl;
		lattr->speed	  = tlink->speed;
		lattr->deskew	  = be32_to_cpu(tlink->deskew);

		if (tlink->state == BFA_TRUNK_LINK_STATE_UP) {
			fcport->speed	 = tlink->speed;
			fcport->topology = BFA_PORT_TOPOLOGY_P2P;
			link_bm |= 1 << i;
		}

		bfa_trc(fcport->bfa, lattr->link_state);
		bfa_trc(fcport->bfa, lattr->trunk_wwn);
		bfa_trc(fcport->bfa, lattr->fctl);
		bfa_trc(fcport->bfa, lattr->speed);
		bfa_trc(fcport->bfa, lattr->deskew);
	}

	switch (link_bm) {
	case 3:
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
			BFA_PL_EID_TRUNK_SCN, 0, "Trunk up(0,1)");
		break;
	case 2:
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
			BFA_PL_EID_TRUNK_SCN, 0, "Trunk up(-,1)");
		break;
	case 1:
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
			BFA_PL_EID_TRUNK_SCN, 0, "Trunk up(0,-)");
		break;
	default:
		bfa_plog_str(fcport->bfa->plog, BFA_PL_MID_HAL,
			BFA_PL_EID_TRUNK_SCN, 0, "Trunk down");
	}

	/*
	 * Notify upper layers if trunk state changed.
	 */
	if ((state_prev != trunk->attr.state) ||
		(scn->trunk_state == BFA_TRUNK_OFFLINE)) {
		bfa_fcport_scn(fcport, (scn->trunk_state == BFA_TRUNK_ONLINE) ?
			BFA_PORT_LINKUP : BFA_PORT_LINKDOWN, BFA_TRUE);
	}
}

static void
bfa_trunk_iocdisable(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);
	int i = 0;

	/*
	 * In trunked mode, notify upper layers that link is down
	 */
	if (fcport->cfg.trunked) {
		if (fcport->trunk.attr.state == BFA_TRUNK_ONLINE)
			bfa_fcport_scn(fcport, BFA_PORT_LINKDOWN, BFA_TRUE);

		fcport->trunk.attr.state = BFA_TRUNK_OFFLINE;
		fcport->trunk.attr.speed = BFA_PORT_SPEED_UNKNOWN;
		for (i = 0; i < BFA_TRUNK_MAX_PORTS; i++) {
			fcport->trunk.attr.link_attr[i].trunk_wwn = 0;
			fcport->trunk.attr.link_attr[i].fctl =
						BFA_TRUNK_LINK_FCTL_NORMAL;
			fcport->trunk.attr.link_attr[i].link_state =
						BFA_TRUNK_LINK_STATE_DN_LINKDN;
			fcport->trunk.attr.link_attr[i].speed =
						BFA_PORT_SPEED_UNKNOWN;
			fcport->trunk.attr.link_attr[i].deskew = 0;
		}
	}
}

/*
 * Called to initialize port attributes
 */
void
bfa_fcport_init(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	/*
	 * Initialize port attributes from IOC hardware data.
	 */
	bfa_fcport_set_wwns(fcport);
	if (fcport->cfg.maxfrsize == 0)
		fcport->cfg.maxfrsize = bfa_ioc_maxfrsize(&bfa->ioc);
	fcport->cfg.rx_bbcredit = bfa_ioc_rx_bbcredit(&bfa->ioc);
	fcport->speed_sup = bfa_ioc_speed_sup(&bfa->ioc);

	bfa_assert(fcport->cfg.maxfrsize);
	bfa_assert(fcport->cfg.rx_bbcredit);
	bfa_assert(fcport->speed_sup);
}

/*
 * Firmware message handler.
 */
void
bfa_fcport_isr(struct bfa_s *bfa, struct bfi_msg_s *msg)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);
	union bfi_fcport_i2h_msg_u i2hmsg;

	i2hmsg.msg = msg;
	fcport->event_arg.i2hmsg = i2hmsg;

	bfa_trc(bfa, msg->mhdr.msg_id);
	bfa_trc(bfa, bfa_sm_to_state(hal_port_sm_table, fcport->sm));

	switch (msg->mhdr.msg_id) {
	case BFI_FCPORT_I2H_ENABLE_RSP:
		if (fcport->msgtag == i2hmsg.penable_rsp->msgtag)
			bfa_sm_send_event(fcport, BFA_FCPORT_SM_FWRSP);
		break;

	case BFI_FCPORT_I2H_DISABLE_RSP:
		if (fcport->msgtag == i2hmsg.penable_rsp->msgtag)
			bfa_sm_send_event(fcport, BFA_FCPORT_SM_FWRSP);
		break;

	case BFI_FCPORT_I2H_EVENT:
		if (i2hmsg.event->link_state.linkstate == BFA_PORT_LINKUP)
			bfa_sm_send_event(fcport, BFA_FCPORT_SM_LINKUP);
		else
			bfa_sm_send_event(fcport, BFA_FCPORT_SM_LINKDOWN);
		break;

	case BFI_FCPORT_I2H_TRUNK_SCN:
		bfa_trunk_scn(fcport, i2hmsg.trunk_scn);
		break;

	case BFI_FCPORT_I2H_STATS_GET_RSP:
		/*
		 * check for timer pop before processing the rsp
		 */
		if (fcport->stats_busy == BFA_FALSE ||
		    fcport->stats_status == BFA_STATUS_ETIMER)
			break;

		bfa_timer_stop(&fcport->timer);
		fcport->stats_status = i2hmsg.pstatsget_rsp->status;
		bfa_cb_queue(fcport->bfa, &fcport->hcb_qe,
				__bfa_cb_fcport_stats_get, fcport);
		break;

	case BFI_FCPORT_I2H_STATS_CLEAR_RSP:
		/*
		 * check for timer pop before processing the rsp
		 */
		if (fcport->stats_busy == BFA_FALSE ||
		    fcport->stats_status == BFA_STATUS_ETIMER)
			break;

		bfa_timer_stop(&fcport->timer);
		fcport->stats_status = BFA_STATUS_OK;
		bfa_cb_queue(fcport->bfa, &fcport->hcb_qe,
				__bfa_cb_fcport_stats_clr, fcport);
		break;

	case BFI_FCPORT_I2H_ENABLE_AEN:
		bfa_sm_send_event(fcport, BFA_FCPORT_SM_ENABLE);
		break;

	case BFI_FCPORT_I2H_DISABLE_AEN:
		bfa_sm_send_event(fcport, BFA_FCPORT_SM_DISABLE);
		break;

	default:
		bfa_assert(0);
	break;
	}
}

/*
 * Registered callback for port events.
 */
void
bfa_fcport_event_register(struct bfa_s *bfa,
				void (*cbfn) (void *cbarg,
				enum bfa_port_linkstate event),
				void *cbarg)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	fcport->event_cbfn = cbfn;
	fcport->event_cbarg = cbarg;
}

bfa_status_t
bfa_fcport_enable(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	if (bfa_ioc_is_disabled(&bfa->ioc))
		return BFA_STATUS_IOC_DISABLED;

	if (fcport->diag_busy)
		return BFA_STATUS_DIAG_BUSY;

	bfa_sm_send_event(BFA_FCPORT_MOD(bfa), BFA_FCPORT_SM_ENABLE);
	return BFA_STATUS_OK;
}

bfa_status_t
bfa_fcport_disable(struct bfa_s *bfa)
{

	if (bfa_ioc_is_disabled(&bfa->ioc))
		return BFA_STATUS_IOC_DISABLED;

	bfa_sm_send_event(BFA_FCPORT_MOD(bfa), BFA_FCPORT_SM_DISABLE);
	return BFA_STATUS_OK;
}

/*
 * Configure port speed.
 */
bfa_status_t
bfa_fcport_cfg_speed(struct bfa_s *bfa, enum bfa_port_speed speed)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, speed);

	if (fcport->cfg.trunked == BFA_TRUE)
		return BFA_STATUS_TRUNK_ENABLED;
	if ((speed != BFA_PORT_SPEED_AUTO) && (speed > fcport->speed_sup)) {
		bfa_trc(bfa, fcport->speed_sup);
		return BFA_STATUS_UNSUPP_SPEED;
	}

	fcport->cfg.speed = speed;

	return BFA_STATUS_OK;
}

/*
 * Get current speed.
 */
enum bfa_port_speed
bfa_fcport_get_speed(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->speed;
}

/*
 * Configure port topology.
 */
bfa_status_t
bfa_fcport_cfg_topology(struct bfa_s *bfa, enum bfa_port_topology topology)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, topology);
	bfa_trc(bfa, fcport->cfg.topology);

	switch (topology) {
	case BFA_PORT_TOPOLOGY_P2P:
	case BFA_PORT_TOPOLOGY_LOOP:
	case BFA_PORT_TOPOLOGY_AUTO:
		break;

	default:
		return BFA_STATUS_EINVAL;
	}

	fcport->cfg.topology = topology;
	return BFA_STATUS_OK;
}

/*
 * Get current topology.
 */
enum bfa_port_topology
bfa_fcport_get_topology(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->topology;
}

bfa_status_t
bfa_fcport_cfg_hardalpa(struct bfa_s *bfa, u8 alpa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, alpa);
	bfa_trc(bfa, fcport->cfg.cfg_hardalpa);
	bfa_trc(bfa, fcport->cfg.hardalpa);

	fcport->cfg.cfg_hardalpa = BFA_TRUE;
	fcport->cfg.hardalpa = alpa;

	return BFA_STATUS_OK;
}

bfa_status_t
bfa_fcport_clr_hardalpa(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, fcport->cfg.cfg_hardalpa);
	bfa_trc(bfa, fcport->cfg.hardalpa);

	fcport->cfg.cfg_hardalpa = BFA_FALSE;
	return BFA_STATUS_OK;
}

bfa_boolean_t
bfa_fcport_get_hardalpa(struct bfa_s *bfa, u8 *alpa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	*alpa = fcport->cfg.hardalpa;
	return fcport->cfg.cfg_hardalpa;
}

u8
bfa_fcport_get_myalpa(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->myalpa;
}

bfa_status_t
bfa_fcport_cfg_maxfrsize(struct bfa_s *bfa, u16 maxfrsize)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, maxfrsize);
	bfa_trc(bfa, fcport->cfg.maxfrsize);

	/* with in range */
	if ((maxfrsize > FC_MAX_PDUSZ) || (maxfrsize < FC_MIN_PDUSZ))
		return BFA_STATUS_INVLD_DFSZ;

	/* power of 2, if not the max frame size of 2112 */
	if ((maxfrsize != FC_MAX_PDUSZ) && (maxfrsize & (maxfrsize - 1)))
		return BFA_STATUS_INVLD_DFSZ;

	fcport->cfg.maxfrsize = maxfrsize;
	return BFA_STATUS_OK;
}

u16
bfa_fcport_get_maxfrsize(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->cfg.maxfrsize;
}

u8
bfa_fcport_get_rx_bbcredit(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->cfg.rx_bbcredit;
}

void
bfa_fcport_set_tx_bbcredit(struct bfa_s *bfa, u16 tx_bbcredit)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	fcport->cfg.tx_bbcredit = (u8)tx_bbcredit;
	bfa_fcport_send_txcredit(fcport);
}

/*
 * Get port attributes.
 */

wwn_t
bfa_fcport_get_wwn(struct bfa_s *bfa, bfa_boolean_t node)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);
	if (node)
		return fcport->nwwn;
	else
		return fcport->pwwn;
}

void
bfa_fcport_get_attr(struct bfa_s *bfa, struct bfa_port_attr_s *attr)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	memset(attr, 0, sizeof(struct bfa_port_attr_s));

	attr->nwwn = fcport->nwwn;
	attr->pwwn = fcport->pwwn;

	attr->factorypwwn =  bfa->ioc.attr->mfg_pwwn;
	attr->factorynwwn =  bfa->ioc.attr->mfg_nwwn;

	memcpy(&attr->pport_cfg, &fcport->cfg,
		sizeof(struct bfa_port_cfg_s));
	/* speed attributes */
	attr->pport_cfg.speed = fcport->cfg.speed;
	attr->speed_supported = fcport->speed_sup;
	attr->speed = fcport->speed;
	attr->cos_supported = FC_CLASS_3;

	/* topology attributes */
	attr->pport_cfg.topology = fcport->cfg.topology;
	attr->topology = fcport->topology;
	attr->pport_cfg.trunked = fcport->cfg.trunked;

	/* beacon attributes */
	attr->beacon = fcport->beacon;
	attr->link_e2e_beacon = fcport->link_e2e_beacon;
	attr->plog_enabled = (bfa_boolean_t)fcport->bfa->plog->plog_enabled;
	attr->io_profile = bfa_fcpim_get_io_profile(fcport->bfa);

	attr->pport_cfg.path_tov  = bfa_fcpim_path_tov_get(bfa);
	attr->pport_cfg.q_depth  = bfa_fcpim_qdepth_get(bfa);
	attr->port_state = bfa_sm_to_state(hal_port_sm_table, fcport->sm);
	if (bfa_ioc_is_disabled(&fcport->bfa->ioc))
		attr->port_state = BFA_PORT_ST_IOCDIS;
	else if (bfa_ioc_fw_mismatch(&fcport->bfa->ioc))
		attr->port_state = BFA_PORT_ST_FWMISMATCH;

	/* FCoE vlan */
	attr->fcoe_vlan = fcport->fcoe_vlan;
}

#define BFA_FCPORT_STATS_TOV	1000

/*
 * Fetch port statistics (FCQoS or FCoE).
 */
bfa_status_t
bfa_fcport_get_stats(struct bfa_s *bfa, union bfa_fcport_stats_u *stats,
	bfa_cb_port_t cbfn, void *cbarg)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	if (fcport->stats_busy) {
		bfa_trc(bfa, fcport->stats_busy);
		return BFA_STATUS_DEVBUSY;
	}

	fcport->stats_busy  = BFA_TRUE;
	fcport->stats_ret   = stats;
	fcport->stats_cbfn  = cbfn;
	fcport->stats_cbarg = cbarg;

	bfa_fcport_send_stats_get(fcport);

	bfa_timer_start(bfa, &fcport->timer, bfa_fcport_stats_get_timeout,
			fcport, BFA_FCPORT_STATS_TOV);
	return BFA_STATUS_OK;
}

/*
 * Reset port statistics (FCQoS or FCoE).
 */
bfa_status_t
bfa_fcport_clear_stats(struct bfa_s *bfa, bfa_cb_port_t cbfn, void *cbarg)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	if (fcport->stats_busy) {
		bfa_trc(bfa, fcport->stats_busy);
		return BFA_STATUS_DEVBUSY;
	}

	fcport->stats_busy  = BFA_TRUE;
	fcport->stats_cbfn  = cbfn;
	fcport->stats_cbarg = cbarg;

	bfa_fcport_send_stats_clear(fcport);

	bfa_timer_start(bfa, &fcport->timer, bfa_fcport_stats_clr_timeout,
			fcport, BFA_FCPORT_STATS_TOV);
	return BFA_STATUS_OK;
}


/*
 * Fetch port attributes.
 */
bfa_boolean_t
bfa_fcport_is_disabled(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return bfa_sm_to_state(hal_port_sm_table, fcport->sm) ==
		BFA_PORT_ST_DISABLED;

}

bfa_boolean_t
bfa_fcport_is_ratelim(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->cfg.ratelimit ? BFA_TRUE : BFA_FALSE;

}

/*
 * Get default minimum ratelim speed
 */
enum bfa_port_speed
bfa_fcport_get_ratelim_speed(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	bfa_trc(bfa, fcport->cfg.trl_def_speed);
	return fcport->cfg.trl_def_speed;

}

bfa_boolean_t
bfa_fcport_is_linkup(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return	(!fcport->cfg.trunked &&
		 bfa_sm_cmp_state(fcport, bfa_fcport_sm_linkup)) ||
		(fcport->cfg.trunked &&
		 fcport->trunk.attr.state == BFA_TRUNK_ONLINE);
}

bfa_boolean_t
bfa_fcport_is_qos_enabled(struct bfa_s *bfa)
{
	struct bfa_fcport_s *fcport = BFA_FCPORT_MOD(bfa);

	return fcport->cfg.qos_enabled;
}

/*
 * Rport State machine functions
 */
/*
 * Beginning state, only online event expected.
 */
static void
bfa_rport_sm_uninit(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_CREATE:
		bfa_stats(rp, sm_un_cr);
		bfa_sm_set_state(rp, bfa_rport_sm_created);
		break;

	default:
		bfa_stats(rp, sm_un_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

static void
bfa_rport_sm_created(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_ONLINE:
		bfa_stats(rp, sm_cr_on);
		if (bfa_rport_send_fwcreate(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate_qfull);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_cr_del);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_cr_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		break;

	default:
		bfa_stats(rp, sm_cr_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Waiting for rport create response from firmware.
 */
static void
bfa_rport_sm_fwcreate(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_FWRSP:
		bfa_stats(rp, sm_fwc_rsp);
		bfa_sm_set_state(rp, bfa_rport_sm_online);
		bfa_rport_online_cb(rp);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_fwc_del);
		bfa_sm_set_state(rp, bfa_rport_sm_delete_pending);
		break;

	case BFA_RPORT_SM_OFFLINE:
		bfa_stats(rp, sm_fwc_off);
		bfa_sm_set_state(rp, bfa_rport_sm_offline_pending);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_fwc_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		break;

	default:
		bfa_stats(rp, sm_fwc_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Request queue is full, awaiting queue resume to send create request.
 */
static void
bfa_rport_sm_fwcreate_qfull(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_QRESUME:
		bfa_sm_set_state(rp, bfa_rport_sm_fwcreate);
		bfa_rport_send_fwcreate(rp);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_fwc_del);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_reqq_wcancel(&rp->reqq_wait);
		bfa_rport_free(rp);
		break;

	case BFA_RPORT_SM_OFFLINE:
		bfa_stats(rp, sm_fwc_off);
		bfa_sm_set_state(rp, bfa_rport_sm_offline);
		bfa_reqq_wcancel(&rp->reqq_wait);
		bfa_rport_offline_cb(rp);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_fwc_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		bfa_reqq_wcancel(&rp->reqq_wait);
		break;

	default:
		bfa_stats(rp, sm_fwc_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Online state - normal parking state.
 */
static void
bfa_rport_sm_online(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	struct bfi_rport_qos_scn_s *qos_scn;

	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_OFFLINE:
		bfa_stats(rp, sm_on_off);
		if (bfa_rport_send_fwdelete(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_fwdelete);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_fwdelete_qfull);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_on_del);
		if (bfa_rport_send_fwdelete(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_deleting);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_deleting_qfull);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_on_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		break;

	case BFA_RPORT_SM_SET_SPEED:
		bfa_rport_send_fwspeed(rp);
		break;

	case BFA_RPORT_SM_QOS_SCN:
		qos_scn = (struct bfi_rport_qos_scn_s *) rp->event_arg.fw_msg;
		rp->qos_attr = qos_scn->new_qos_attr;
		bfa_trc(rp->bfa, qos_scn->old_qos_attr.qos_flow_id);
		bfa_trc(rp->bfa, qos_scn->new_qos_attr.qos_flow_id);
		bfa_trc(rp->bfa, qos_scn->old_qos_attr.qos_priority);
		bfa_trc(rp->bfa, qos_scn->new_qos_attr.qos_priority);

		qos_scn->old_qos_attr.qos_flow_id  =
			be32_to_cpu(qos_scn->old_qos_attr.qos_flow_id);
		qos_scn->new_qos_attr.qos_flow_id  =
			be32_to_cpu(qos_scn->new_qos_attr.qos_flow_id);

		if (qos_scn->old_qos_attr.qos_flow_id !=
			qos_scn->new_qos_attr.qos_flow_id)
			bfa_cb_rport_qos_scn_flowid(rp->rport_drv,
						    qos_scn->old_qos_attr,
						    qos_scn->new_qos_attr);
		if (qos_scn->old_qos_attr.qos_priority !=
			qos_scn->new_qos_attr.qos_priority)
			bfa_cb_rport_qos_scn_prio(rp->rport_drv,
						  qos_scn->old_qos_attr,
						  qos_scn->new_qos_attr);
		break;

	default:
		bfa_stats(rp, sm_on_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Firmware rport is being deleted - awaiting f/w response.
 */
static void
bfa_rport_sm_fwdelete(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_FWRSP:
		bfa_stats(rp, sm_fwd_rsp);
		bfa_sm_set_state(rp, bfa_rport_sm_offline);
		bfa_rport_offline_cb(rp);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_fwd_del);
		bfa_sm_set_state(rp, bfa_rport_sm_deleting);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_fwd_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		bfa_rport_offline_cb(rp);
		break;

	default:
		bfa_stats(rp, sm_fwd_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

static void
bfa_rport_sm_fwdelete_qfull(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_QRESUME:
		bfa_sm_set_state(rp, bfa_rport_sm_fwdelete);
		bfa_rport_send_fwdelete(rp);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_fwd_del);
		bfa_sm_set_state(rp, bfa_rport_sm_deleting_qfull);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_fwd_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		bfa_reqq_wcancel(&rp->reqq_wait);
		bfa_rport_offline_cb(rp);
		break;

	default:
		bfa_stats(rp, sm_fwd_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Offline state.
 */
static void
bfa_rport_sm_offline(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_off_del);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	case BFA_RPORT_SM_ONLINE:
		bfa_stats(rp, sm_off_on);
		if (bfa_rport_send_fwcreate(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate_qfull);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_off_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		break;

	default:
		bfa_stats(rp, sm_off_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Rport is deleted, waiting for firmware response to delete.
 */
static void
bfa_rport_sm_deleting(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_FWRSP:
		bfa_stats(rp, sm_del_fwrsp);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_del_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	default:
		bfa_sm_fault(rp->bfa, event);
	}
}

static void
bfa_rport_sm_deleting_qfull(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_QRESUME:
		bfa_stats(rp, sm_del_fwrsp);
		bfa_sm_set_state(rp, bfa_rport_sm_deleting);
		bfa_rport_send_fwdelete(rp);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_del_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_reqq_wcancel(&rp->reqq_wait);
		bfa_rport_free(rp);
		break;

	default:
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Waiting for rport create response from firmware. A delete is pending.
 */
static void
bfa_rport_sm_delete_pending(struct bfa_rport_s *rp,
				enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_FWRSP:
		bfa_stats(rp, sm_delp_fwrsp);
		if (bfa_rport_send_fwdelete(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_deleting);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_deleting_qfull);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_delp_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	default:
		bfa_stats(rp, sm_delp_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * Waiting for rport create response from firmware. Rport offline is pending.
 */
static void
bfa_rport_sm_offline_pending(struct bfa_rport_s *rp,
				 enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_FWRSP:
		bfa_stats(rp, sm_offp_fwrsp);
		if (bfa_rport_send_fwdelete(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_fwdelete);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_fwdelete_qfull);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_offp_del);
		bfa_sm_set_state(rp, bfa_rport_sm_delete_pending);
		break;

	case BFA_RPORT_SM_HWFAIL:
		bfa_stats(rp, sm_offp_hwf);
		bfa_sm_set_state(rp, bfa_rport_sm_iocdisable);
		break;

	default:
		bfa_stats(rp, sm_offp_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}

/*
 * IOC h/w failed.
 */
static void
bfa_rport_sm_iocdisable(struct bfa_rport_s *rp, enum bfa_rport_event event)
{
	bfa_trc(rp->bfa, rp->rport_tag);
	bfa_trc(rp->bfa, event);

	switch (event) {
	case BFA_RPORT_SM_OFFLINE:
		bfa_stats(rp, sm_iocd_off);
		bfa_rport_offline_cb(rp);
		break;

	case BFA_RPORT_SM_DELETE:
		bfa_stats(rp, sm_iocd_del);
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);
		bfa_rport_free(rp);
		break;

	case BFA_RPORT_SM_ONLINE:
		bfa_stats(rp, sm_iocd_on);
		if (bfa_rport_send_fwcreate(rp))
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate);
		else
			bfa_sm_set_state(rp, bfa_rport_sm_fwcreate_qfull);
		break;

	case BFA_RPORT_SM_HWFAIL:
		break;

	default:
		bfa_stats(rp, sm_iocd_unexp);
		bfa_sm_fault(rp->bfa, event);
	}
}



/*
 *  bfa_rport_private BFA rport private functions
 */

static void
__bfa_cb_rport_online(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_rport_s *rp = cbarg;

	if (complete)
		bfa_cb_rport_online(rp->rport_drv);
}

static void
__bfa_cb_rport_offline(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_rport_s *rp = cbarg;

	if (complete)
		bfa_cb_rport_offline(rp->rport_drv);
}

static void
bfa_rport_qresume(void *cbarg)
{
	struct bfa_rport_s	*rp = cbarg;

	bfa_sm_send_event(rp, BFA_RPORT_SM_QRESUME);
}

static void
bfa_rport_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *km_len,
		u32 *dm_len)
{
	if (cfg->fwcfg.num_rports < BFA_RPORT_MIN)
		cfg->fwcfg.num_rports = BFA_RPORT_MIN;

	*km_len += cfg->fwcfg.num_rports * sizeof(struct bfa_rport_s);
}

static void
bfa_rport_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		     struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_rport_mod_s *mod = BFA_RPORT_MOD(bfa);
	struct bfa_rport_s *rp;
	u16 i;

	INIT_LIST_HEAD(&mod->rp_free_q);
	INIT_LIST_HEAD(&mod->rp_active_q);

	rp = (struct bfa_rport_s *) bfa_meminfo_kva(meminfo);
	mod->rps_list = rp;
	mod->num_rports = cfg->fwcfg.num_rports;

	bfa_assert(mod->num_rports &&
		   !(mod->num_rports & (mod->num_rports - 1)));

	for (i = 0; i < mod->num_rports; i++, rp++) {
		memset(rp, 0, sizeof(struct bfa_rport_s));
		rp->bfa = bfa;
		rp->rport_tag = i;
		bfa_sm_set_state(rp, bfa_rport_sm_uninit);

		/*
		 *  - is unused
		 */
		if (i)
			list_add_tail(&rp->qe, &mod->rp_free_q);

		bfa_reqq_winit(&rp->reqq_wait, bfa_rport_qresume, rp);
	}

	/*
	 * consume memory
	 */
	bfa_meminfo_kva(meminfo) = (u8 *) rp;
}

static void
bfa_rport_detach(struct bfa_s *bfa)
{
}

static void
bfa_rport_start(struct bfa_s *bfa)
{
}

static void
bfa_rport_stop(struct bfa_s *bfa)
{
}

static void
bfa_rport_iocdisable(struct bfa_s *bfa)
{
	struct bfa_rport_mod_s *mod = BFA_RPORT_MOD(bfa);
	struct bfa_rport_s *rport;
	struct list_head *qe, *qen;

	list_for_each_safe(qe, qen, &mod->rp_active_q) {
		rport = (struct bfa_rport_s *) qe;
		bfa_sm_send_event(rport, BFA_RPORT_SM_HWFAIL);
	}
}

static struct bfa_rport_s *
bfa_rport_alloc(struct bfa_rport_mod_s *mod)
{
	struct bfa_rport_s *rport;

	bfa_q_deq(&mod->rp_free_q, &rport);
	if (rport)
		list_add_tail(&rport->qe, &mod->rp_active_q);

	return rport;
}

static void
bfa_rport_free(struct bfa_rport_s *rport)
{
	struct bfa_rport_mod_s *mod = BFA_RPORT_MOD(rport->bfa);

	bfa_assert(bfa_q_is_on_q(&mod->rp_active_q, rport));
	list_del(&rport->qe);
	list_add_tail(&rport->qe, &mod->rp_free_q);
}

static bfa_boolean_t
bfa_rport_send_fwcreate(struct bfa_rport_s *rp)
{
	struct bfi_rport_create_req_s *m;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(rp->bfa, BFA_REQQ_RPORT);
	if (!m) {
		bfa_reqq_wait(rp->bfa, BFA_REQQ_RPORT, &rp->reqq_wait);
		return BFA_FALSE;
	}

	bfi_h2i_set(m->mh, BFI_MC_RPORT, BFI_RPORT_H2I_CREATE_REQ,
			bfa_lpuid(rp->bfa));
	m->bfa_handle = rp->rport_tag;
	m->max_frmsz = cpu_to_be16(rp->rport_info.max_frmsz);
	m->pid = rp->rport_info.pid;
	m->lp_tag = rp->rport_info.lp_tag;
	m->local_pid = rp->rport_info.local_pid;
	m->fc_class = rp->rport_info.fc_class;
	m->vf_en = rp->rport_info.vf_en;
	m->vf_id = rp->rport_info.vf_id;
	m->cisc = rp->rport_info.cisc;

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(rp->bfa, BFA_REQQ_RPORT);
	return BFA_TRUE;
}

static bfa_boolean_t
bfa_rport_send_fwdelete(struct bfa_rport_s *rp)
{
	struct bfi_rport_delete_req_s *m;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(rp->bfa, BFA_REQQ_RPORT);
	if (!m) {
		bfa_reqq_wait(rp->bfa, BFA_REQQ_RPORT, &rp->reqq_wait);
		return BFA_FALSE;
	}

	bfi_h2i_set(m->mh, BFI_MC_RPORT, BFI_RPORT_H2I_DELETE_REQ,
			bfa_lpuid(rp->bfa));
	m->fw_handle = rp->fw_handle;

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(rp->bfa, BFA_REQQ_RPORT);
	return BFA_TRUE;
}

static bfa_boolean_t
bfa_rport_send_fwspeed(struct bfa_rport_s *rp)
{
	struct bfa_rport_speed_req_s *m;

	/*
	 * check for room in queue to send request now
	 */
	m = bfa_reqq_next(rp->bfa, BFA_REQQ_RPORT);
	if (!m) {
		bfa_trc(rp->bfa, rp->rport_info.speed);
		return BFA_FALSE;
	}

	bfi_h2i_set(m->mh, BFI_MC_RPORT, BFI_RPORT_H2I_SET_SPEED_REQ,
			bfa_lpuid(rp->bfa));
	m->fw_handle = rp->fw_handle;
	m->speed = (u8)rp->rport_info.speed;

	/*
	 * queue I/O message to firmware
	 */
	bfa_reqq_produce(rp->bfa, BFA_REQQ_RPORT);
	return BFA_TRUE;
}



/*
 *  bfa_rport_public
 */

/*
 * Rport interrupt processing.
 */
void
bfa_rport_isr(struct bfa_s *bfa, struct bfi_msg_s *m)
{
	union bfi_rport_i2h_msg_u msg;
	struct bfa_rport_s *rp;

	bfa_trc(bfa, m->mhdr.msg_id);

	msg.msg = m;

	switch (m->mhdr.msg_id) {
	case BFI_RPORT_I2H_CREATE_RSP:
		rp = BFA_RPORT_FROM_TAG(bfa, msg.create_rsp->bfa_handle);
		rp->fw_handle = msg.create_rsp->fw_handle;
		rp->qos_attr = msg.create_rsp->qos_attr;
		bfa_assert(msg.create_rsp->status == BFA_STATUS_OK);
		bfa_sm_send_event(rp, BFA_RPORT_SM_FWRSP);
		break;

	case BFI_RPORT_I2H_DELETE_RSP:
		rp = BFA_RPORT_FROM_TAG(bfa, msg.delete_rsp->bfa_handle);
		bfa_assert(msg.delete_rsp->status == BFA_STATUS_OK);
		bfa_sm_send_event(rp, BFA_RPORT_SM_FWRSP);
		break;

	case BFI_RPORT_I2H_QOS_SCN:
		rp = BFA_RPORT_FROM_TAG(bfa, msg.qos_scn_evt->bfa_handle);
		rp->event_arg.fw_msg = msg.qos_scn_evt;
		bfa_sm_send_event(rp, BFA_RPORT_SM_QOS_SCN);
		break;

	default:
		bfa_trc(bfa, m->mhdr.msg_id);
		bfa_assert(0);
	}
}



/*
 *  bfa_rport_api
 */

struct bfa_rport_s *
bfa_rport_create(struct bfa_s *bfa, void *rport_drv)
{
	struct bfa_rport_s *rp;

	rp = bfa_rport_alloc(BFA_RPORT_MOD(bfa));

	if (rp == NULL)
		return NULL;

	rp->bfa = bfa;
	rp->rport_drv = rport_drv;
	memset(&rp->stats, 0, sizeof(rp->stats));

	bfa_assert(bfa_sm_cmp_state(rp, bfa_rport_sm_uninit));
	bfa_sm_send_event(rp, BFA_RPORT_SM_CREATE);

	return rp;
}

void
bfa_rport_online(struct bfa_rport_s *rport, struct bfa_rport_info_s *rport_info)
{
	bfa_assert(rport_info->max_frmsz != 0);

	/*
	 * Some JBODs are seen to be not setting PDU size correctly in PLOGI
	 * responses. Default to minimum size.
	 */
	if (rport_info->max_frmsz == 0) {
		bfa_trc(rport->bfa, rport->rport_tag);
		rport_info->max_frmsz = FC_MIN_PDUSZ;
	}

	rport->rport_info = *rport_info;
	bfa_sm_send_event(rport, BFA_RPORT_SM_ONLINE);
}

void
bfa_rport_speed(struct bfa_rport_s *rport, enum bfa_port_speed speed)
{
	bfa_assert(speed != 0);
	bfa_assert(speed != BFA_PORT_SPEED_AUTO);

	rport->rport_info.speed = speed;
	bfa_sm_send_event(rport, BFA_RPORT_SM_SET_SPEED);
}


/*
 * SGPG related functions
 */

/*
 * Compute and return memory needed by FCP(im) module.
 */
static void
bfa_sgpg_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *km_len,
		u32 *dm_len)
{
	if (cfg->drvcfg.num_sgpgs < BFA_SGPG_MIN)
		cfg->drvcfg.num_sgpgs = BFA_SGPG_MIN;

	*km_len += (cfg->drvcfg.num_sgpgs + 1) * sizeof(struct bfa_sgpg_s);
	*dm_len += (cfg->drvcfg.num_sgpgs + 1) * sizeof(struct bfi_sgpg_s);
}


static void
bfa_sgpg_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		    struct bfa_meminfo_s *minfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_sgpg_mod_s *mod = BFA_SGPG_MOD(bfa);
	int i;
	struct bfa_sgpg_s *hsgpg;
	struct bfi_sgpg_s *sgpg;
	u64 align_len;

	union {
		u64 pa;
		union bfi_addr_u addr;
	} sgpg_pa, sgpg_pa_tmp;

	INIT_LIST_HEAD(&mod->sgpg_q);
	INIT_LIST_HEAD(&mod->sgpg_wait_q);

	bfa_trc(bfa, cfg->drvcfg.num_sgpgs);

	mod->num_sgpgs = cfg->drvcfg.num_sgpgs;
	mod->sgpg_arr_pa = bfa_meminfo_dma_phys(minfo);
	align_len = (BFA_SGPG_ROUNDUP(mod->sgpg_arr_pa) - mod->sgpg_arr_pa);
	mod->sgpg_arr_pa += align_len;
	mod->hsgpg_arr = (struct bfa_sgpg_s *) (bfa_meminfo_kva(minfo) +
						align_len);
	mod->sgpg_arr = (struct bfi_sgpg_s *) (bfa_meminfo_dma_virt(minfo) +
						align_len);

	hsgpg = mod->hsgpg_arr;
	sgpg = mod->sgpg_arr;
	sgpg_pa.pa = mod->sgpg_arr_pa;
	mod->free_sgpgs = mod->num_sgpgs;

	bfa_assert(!(sgpg_pa.pa & (sizeof(struct bfi_sgpg_s) - 1)));

	for (i = 0; i < mod->num_sgpgs; i++) {
		memset(hsgpg, 0, sizeof(*hsgpg));
		memset(sgpg, 0, sizeof(*sgpg));

		hsgpg->sgpg = sgpg;
		sgpg_pa_tmp.pa = bfa_sgaddr_le(sgpg_pa.pa);
		hsgpg->sgpg_pa = sgpg_pa_tmp.addr;
		list_add_tail(&hsgpg->qe, &mod->sgpg_q);

		hsgpg++;
		sgpg++;
		sgpg_pa.pa += sizeof(struct bfi_sgpg_s);
	}

	bfa_meminfo_kva(minfo) = (u8 *) hsgpg;
	bfa_meminfo_dma_virt(minfo) = (u8 *) sgpg;
	bfa_meminfo_dma_phys(minfo) = sgpg_pa.pa;
}

static void
bfa_sgpg_detach(struct bfa_s *bfa)
{
}

static void
bfa_sgpg_start(struct bfa_s *bfa)
{
}

static void
bfa_sgpg_stop(struct bfa_s *bfa)
{
}

static void
bfa_sgpg_iocdisable(struct bfa_s *bfa)
{
}

bfa_status_t
bfa_sgpg_malloc(struct bfa_s *bfa, struct list_head *sgpg_q, int nsgpgs)
{
	struct bfa_sgpg_mod_s *mod = BFA_SGPG_MOD(bfa);
	struct bfa_sgpg_s *hsgpg;
	int i;

	bfa_trc_fp(bfa, nsgpgs);

	if (mod->free_sgpgs < nsgpgs)
		return BFA_STATUS_ENOMEM;

	for (i = 0; i < nsgpgs; i++) {
		bfa_q_deq(&mod->sgpg_q, &hsgpg);
		bfa_assert(hsgpg);
		list_add_tail(&hsgpg->qe, sgpg_q);
	}

	mod->free_sgpgs -= nsgpgs;
	return BFA_STATUS_OK;
}

void
bfa_sgpg_mfree(struct bfa_s *bfa, struct list_head *sgpg_q, int nsgpg)
{
	struct bfa_sgpg_mod_s *mod = BFA_SGPG_MOD(bfa);
	struct bfa_sgpg_wqe_s *wqe;

	bfa_trc_fp(bfa, nsgpg);

	mod->free_sgpgs += nsgpg;
	bfa_assert(mod->free_sgpgs <= mod->num_sgpgs);

	list_splice_tail_init(sgpg_q, &mod->sgpg_q);

	if (list_empty(&mod->sgpg_wait_q))
		return;

	/*
	 * satisfy as many waiting requests as possible
	 */
	do {
		wqe = bfa_q_first(&mod->sgpg_wait_q);
		if (mod->free_sgpgs < wqe->nsgpg)
			nsgpg = mod->free_sgpgs;
		else
			nsgpg = wqe->nsgpg;
		bfa_sgpg_malloc(bfa, &wqe->sgpg_q, nsgpg);
		wqe->nsgpg -= nsgpg;
		if (wqe->nsgpg == 0) {
			list_del(&wqe->qe);
			wqe->cbfn(wqe->cbarg);
		}
	} while (mod->free_sgpgs && !list_empty(&mod->sgpg_wait_q));
}

void
bfa_sgpg_wait(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe, int nsgpg)
{
	struct bfa_sgpg_mod_s *mod = BFA_SGPG_MOD(bfa);

	bfa_assert(nsgpg > 0);
	bfa_assert(nsgpg > mod->free_sgpgs);

	wqe->nsgpg_total = wqe->nsgpg = nsgpg;

	/*
	 * allocate any left to this one first
	 */
	if (mod->free_sgpgs) {
		/*
		 * no one else is waiting for SGPG
		 */
		bfa_assert(list_empty(&mod->sgpg_wait_q));
		list_splice_tail_init(&mod->sgpg_q, &wqe->sgpg_q);
		wqe->nsgpg -= mod->free_sgpgs;
		mod->free_sgpgs = 0;
	}

	list_add_tail(&wqe->qe, &mod->sgpg_wait_q);
}

void
bfa_sgpg_wcancel(struct bfa_s *bfa, struct bfa_sgpg_wqe_s *wqe)
{
	struct bfa_sgpg_mod_s *mod = BFA_SGPG_MOD(bfa);

	bfa_assert(bfa_q_is_on_q(&mod->sgpg_wait_q, wqe));
	list_del(&wqe->qe);

	if (wqe->nsgpg_total != wqe->nsgpg)
		bfa_sgpg_mfree(bfa, &wqe->sgpg_q,
				   wqe->nsgpg_total - wqe->nsgpg);
}

void
bfa_sgpg_winit(struct bfa_sgpg_wqe_s *wqe, void (*cbfn) (void *cbarg),
		   void *cbarg)
{
	INIT_LIST_HEAD(&wqe->sgpg_q);
	wqe->cbfn = cbfn;
	wqe->cbarg = cbarg;
}

/*
 *  UF related functions
 */
/*
 *****************************************************************************
 * Internal functions
 *****************************************************************************
 */
static void
__bfa_cb_uf_recv(void *cbarg, bfa_boolean_t complete)
{
	struct bfa_uf_s   *uf = cbarg;
	struct bfa_uf_mod_s *ufm = BFA_UF_MOD(uf->bfa);

	if (complete)
		ufm->ufrecv(ufm->cbarg, uf);
}

static void
claim_uf_pbs(struct bfa_uf_mod_s *ufm, struct bfa_meminfo_s *mi)
{
	u32 uf_pb_tot_sz;

	ufm->uf_pbs_kva = (struct bfa_uf_buf_s *) bfa_meminfo_dma_virt(mi);
	ufm->uf_pbs_pa = bfa_meminfo_dma_phys(mi);
	uf_pb_tot_sz = BFA_ROUNDUP((sizeof(struct bfa_uf_buf_s) * ufm->num_ufs),
							BFA_DMA_ALIGN_SZ);

	bfa_meminfo_dma_virt(mi) += uf_pb_tot_sz;
	bfa_meminfo_dma_phys(mi) += uf_pb_tot_sz;

	memset((void *)ufm->uf_pbs_kva, 0, uf_pb_tot_sz);
}

static void
claim_uf_post_msgs(struct bfa_uf_mod_s *ufm, struct bfa_meminfo_s *mi)
{
	struct bfi_uf_buf_post_s *uf_bp_msg;
	struct bfi_sge_s      *sge;
	union bfi_addr_u      sga_zero = { {0} };
	u16 i;
	u16 buf_len;

	ufm->uf_buf_posts = (struct bfi_uf_buf_post_s *) bfa_meminfo_kva(mi);
	uf_bp_msg = ufm->uf_buf_posts;

	for (i = 0, uf_bp_msg = ufm->uf_buf_posts; i < ufm->num_ufs;
	     i++, uf_bp_msg++) {
		memset(uf_bp_msg, 0, sizeof(struct bfi_uf_buf_post_s));

		uf_bp_msg->buf_tag = i;
		buf_len = sizeof(struct bfa_uf_buf_s);
		uf_bp_msg->buf_len = cpu_to_be16(buf_len);
		bfi_h2i_set(uf_bp_msg->mh, BFI_MC_UF, BFI_UF_H2I_BUF_POST,
			    bfa_lpuid(ufm->bfa));

		sge = uf_bp_msg->sge;
		sge[0].sg_len = buf_len;
		sge[0].flags = BFI_SGE_DATA_LAST;
		bfa_dma_addr_set(sge[0].sga, ufm_pbs_pa(ufm, i));
		bfa_sge_to_be(sge);

		sge[1].sg_len = buf_len;
		sge[1].flags = BFI_SGE_PGDLEN;
		sge[1].sga = sga_zero;
		bfa_sge_to_be(&sge[1]);
	}

	/*
	 * advance pointer beyond consumed memory
	 */
	bfa_meminfo_kva(mi) = (u8 *) uf_bp_msg;
}

static void
claim_ufs(struct bfa_uf_mod_s *ufm, struct bfa_meminfo_s *mi)
{
	u16 i;
	struct bfa_uf_s   *uf;

	/*
	 * Claim block of memory for UF list
	 */
	ufm->uf_list = (struct bfa_uf_s *) bfa_meminfo_kva(mi);

	/*
	 * Initialize UFs and queue it in UF free queue
	 */
	for (i = 0, uf = ufm->uf_list; i < ufm->num_ufs; i++, uf++) {
		memset(uf, 0, sizeof(struct bfa_uf_s));
		uf->bfa = ufm->bfa;
		uf->uf_tag = i;
		uf->pb_len = sizeof(struct bfa_uf_buf_s);
		uf->buf_kva = (void *)&ufm->uf_pbs_kva[i];
		uf->buf_pa = ufm_pbs_pa(ufm, i);
		list_add_tail(&uf->qe, &ufm->uf_free_q);
	}

	/*
	 * advance memory pointer
	 */
	bfa_meminfo_kva(mi) = (u8 *) uf;
}

static void
uf_mem_claim(struct bfa_uf_mod_s *ufm, struct bfa_meminfo_s *mi)
{
	claim_uf_pbs(ufm, mi);
	claim_ufs(ufm, mi);
	claim_uf_post_msgs(ufm, mi);
}

static void
bfa_uf_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *ndm_len, u32 *dm_len)
{
	u32 num_ufs = cfg->fwcfg.num_uf_bufs;

	/*
	 * dma-able memory for UF posted bufs
	 */
	*dm_len += BFA_ROUNDUP((sizeof(struct bfa_uf_buf_s) * num_ufs),
							BFA_DMA_ALIGN_SZ);

	/*
	 * kernel Virtual memory for UFs and UF buf post msg copies
	 */
	*ndm_len += sizeof(struct bfa_uf_s) * num_ufs;
	*ndm_len += sizeof(struct bfi_uf_buf_post_s) * num_ufs;
}

static void
bfa_uf_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		  struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	struct bfa_uf_mod_s *ufm = BFA_UF_MOD(bfa);

	memset(ufm, 0, sizeof(struct bfa_uf_mod_s));
	ufm->bfa = bfa;
	ufm->num_ufs = cfg->fwcfg.num_uf_bufs;
	INIT_LIST_HEAD(&ufm->uf_free_q);
	INIT_LIST_HEAD(&ufm->uf_posted_q);

	uf_mem_claim(ufm, meminfo);
}

static void
bfa_uf_detach(struct bfa_s *bfa)
{
}

static struct bfa_uf_s *
bfa_uf_get(struct bfa_uf_mod_s *uf_mod)
{
	struct bfa_uf_s   *uf;

	bfa_q_deq(&uf_mod->uf_free_q, &uf);
	return uf;
}

static void
bfa_uf_put(struct bfa_uf_mod_s *uf_mod, struct bfa_uf_s *uf)
{
	list_add_tail(&uf->qe, &uf_mod->uf_free_q);
}

static bfa_status_t
bfa_uf_post(struct bfa_uf_mod_s *ufm, struct bfa_uf_s *uf)
{
	struct bfi_uf_buf_post_s *uf_post_msg;

	uf_post_msg = bfa_reqq_next(ufm->bfa, BFA_REQQ_FCXP);
	if (!uf_post_msg)
		return BFA_STATUS_FAILED;

	memcpy(uf_post_msg, &ufm->uf_buf_posts[uf->uf_tag],
		      sizeof(struct bfi_uf_buf_post_s));
	bfa_reqq_produce(ufm->bfa, BFA_REQQ_FCXP);

	bfa_trc(ufm->bfa, uf->uf_tag);

	list_add_tail(&uf->qe, &ufm->uf_posted_q);
	return BFA_STATUS_OK;
}

static void
bfa_uf_post_all(struct bfa_uf_mod_s *uf_mod)
{
	struct bfa_uf_s   *uf;

	while ((uf = bfa_uf_get(uf_mod)) != NULL) {
		if (bfa_uf_post(uf_mod, uf) != BFA_STATUS_OK)
			break;
	}
}

static void
uf_recv(struct bfa_s *bfa, struct bfi_uf_frm_rcvd_s *m)
{
	struct bfa_uf_mod_s *ufm = BFA_UF_MOD(bfa);
	u16 uf_tag = m->buf_tag;
	struct bfa_uf_buf_s *uf_buf = &ufm->uf_pbs_kva[uf_tag];
	struct bfa_uf_s *uf = &ufm->uf_list[uf_tag];
	u8 *buf = &uf_buf->d[0];
	struct fchs_s *fchs;

	m->frm_len = be16_to_cpu(m->frm_len);
	m->xfr_len = be16_to_cpu(m->xfr_len);

	fchs = (struct fchs_s *)uf_buf;

	list_del(&uf->qe);	/* dequeue from posted queue */

	uf->data_ptr = buf;
	uf->data_len = m->xfr_len;

	bfa_assert(uf->data_len >= sizeof(struct fchs_s));

	if (uf->data_len == sizeof(struct fchs_s)) {
		bfa_plog_fchdr(bfa->plog, BFA_PL_MID_HAL_UF, BFA_PL_EID_RX,
			       uf->data_len, (struct fchs_s *)buf);
	} else {
		u32 pld_w0 = *((u32 *) (buf + sizeof(struct fchs_s)));
		bfa_plog_fchdr_and_pl(bfa->plog, BFA_PL_MID_HAL_UF,
				      BFA_PL_EID_RX, uf->data_len,
				      (struct fchs_s *)buf, pld_w0);
	}

	if (bfa->fcs)
		__bfa_cb_uf_recv(uf, BFA_TRUE);
	else
		bfa_cb_queue(bfa, &uf->hcb_qe, __bfa_cb_uf_recv, uf);
}

static void
bfa_uf_stop(struct bfa_s *bfa)
{
}

static void
bfa_uf_iocdisable(struct bfa_s *bfa)
{
	struct bfa_uf_mod_s *ufm = BFA_UF_MOD(bfa);
	struct bfa_uf_s *uf;
	struct list_head *qe, *qen;

	list_for_each_safe(qe, qen, &ufm->uf_posted_q) {
		uf = (struct bfa_uf_s *) qe;
		list_del(&uf->qe);
		bfa_uf_put(ufm, uf);
	}
}

static void
bfa_uf_start(struct bfa_s *bfa)
{
	bfa_uf_post_all(BFA_UF_MOD(bfa));
}

/*
 * Register handler for all unsolicted recieve frames.
 *
 * @param[in]	bfa		BFA instance
 * @param[in]	ufrecv	receive handler function
 * @param[in]	cbarg	receive handler arg
 */
void
bfa_uf_recv_register(struct bfa_s *bfa, bfa_cb_uf_recv_t ufrecv, void *cbarg)
{
	struct bfa_uf_mod_s *ufm = BFA_UF_MOD(bfa);

	ufm->ufrecv = ufrecv;
	ufm->cbarg = cbarg;
}

/*
 *	Free an unsolicited frame back to BFA.
 *
 * @param[in]		uf		unsolicited frame to be freed
 *
 * @return None
 */
void
bfa_uf_free(struct bfa_uf_s *uf)
{
	bfa_uf_put(BFA_UF_MOD(uf->bfa), uf);
	bfa_uf_post_all(BFA_UF_MOD(uf->bfa));
}



/*
 *  uf_pub BFA uf module public functions
 */
void
bfa_uf_isr(struct bfa_s *bfa, struct bfi_msg_s *msg)
{
	bfa_trc(bfa, msg->mhdr.msg_id);

	switch (msg->mhdr.msg_id) {
	case BFI_UF_I2H_FRM_RCVD:
		uf_recv(bfa, (struct bfi_uf_frm_rcvd_s *) msg);
		break;

	default:
		bfa_trc(bfa, msg->mhdr.msg_id);
		bfa_assert(0);
	}
}


