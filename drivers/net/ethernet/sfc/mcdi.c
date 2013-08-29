/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2008-2011 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <linux/delay.h>
#include "net_driver.h"
#include "nic.h"
#include "io.h"
#include "farch_regs.h"
#include "mcdi_pcol.h"
#include "phy.h"

/**************************************************************************
 *
 * Management-Controller-to-Driver Interface
 *
 **************************************************************************
 */

#define MCDI_RPC_TIMEOUT       (10 * HZ)

/* A reboot/assertion causes the MCDI status word to be set after the
 * command word is set or a REBOOT event is sent. If we notice a reboot
 * via these mechanisms then wait 20ms for the status word to be set.
 */
#define MCDI_STATUS_DELAY_US		100
#define MCDI_STATUS_DELAY_COUNT		200
#define MCDI_STATUS_SLEEP_MS						\
	(MCDI_STATUS_DELAY_US * MCDI_STATUS_DELAY_COUNT / 1000)

#define SEQ_MASK							\
	EFX_MASK32(EFX_WIDTH(MCDI_HEADER_SEQ))

static inline struct efx_mcdi_iface *efx_mcdi(struct efx_nic *efx)
{
	EFX_BUG_ON_PARANOID(!efx->mcdi);
	return &efx->mcdi->iface;
}

int efx_mcdi_init(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi;

	efx->mcdi = kzalloc(sizeof(*efx->mcdi), GFP_KERNEL);
	if (!efx->mcdi)
		return -ENOMEM;

	mcdi = efx_mcdi(efx);
	init_waitqueue_head(&mcdi->wq);
	spin_lock_init(&mcdi->iface_lock);
	atomic_set(&mcdi->state, MCDI_STATE_QUIESCENT);
	mcdi->mode = MCDI_MODE_POLL;

	(void) efx_mcdi_poll_reboot(efx);
	mcdi->new_epoch = true;

	/* Recover from a failed assertion before probing */
	return efx_mcdi_handle_assertion(efx);
}

void efx_mcdi_fini(struct efx_nic *efx)
{
	BUG_ON(efx->mcdi &&
	       atomic_read(&efx->mcdi->iface.state) != MCDI_STATE_QUIESCENT);
	kfree(efx->mcdi);
}

static void efx_mcdi_copyin(struct efx_nic *efx, unsigned cmd,
			    const efx_dword_t *inbuf, size_t inlen)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	efx_dword_t hdr[2];
	size_t hdr_len;
	u32 xflags, seqno;

	BUG_ON(atomic_read(&mcdi->state) == MCDI_STATE_QUIESCENT);

	seqno = mcdi->seqno & SEQ_MASK;
	xflags = 0;
	if (mcdi->mode == MCDI_MODE_EVENTS)
		xflags |= MCDI_HEADER_XFLAGS_EVREQ;

	if (efx->type->mcdi_max_ver == 1) {
		/* MCDI v1 */
		EFX_POPULATE_DWORD_7(hdr[0],
				     MCDI_HEADER_RESPONSE, 0,
				     MCDI_HEADER_RESYNC, 1,
				     MCDI_HEADER_CODE, cmd,
				     MCDI_HEADER_DATALEN, inlen,
				     MCDI_HEADER_SEQ, seqno,
				     MCDI_HEADER_XFLAGS, xflags,
				     MCDI_HEADER_NOT_EPOCH, !mcdi->new_epoch);
		hdr_len = 4;
	} else {
		/* MCDI v2 */
		BUG_ON(inlen > MCDI_CTL_SDU_LEN_MAX_V2);
		EFX_POPULATE_DWORD_7(hdr[0],
				     MCDI_HEADER_RESPONSE, 0,
				     MCDI_HEADER_RESYNC, 1,
				     MCDI_HEADER_CODE, MC_CMD_V2_EXTN,
				     MCDI_HEADER_DATALEN, 0,
				     MCDI_HEADER_SEQ, seqno,
				     MCDI_HEADER_XFLAGS, xflags,
				     MCDI_HEADER_NOT_EPOCH, !mcdi->new_epoch);
		EFX_POPULATE_DWORD_2(hdr[1],
				     MC_CMD_V2_EXTN_IN_EXTENDED_CMD, cmd,
				     MC_CMD_V2_EXTN_IN_ACTUAL_LEN, inlen);
		hdr_len = 8;
	}

	efx->type->mcdi_request(efx, hdr, hdr_len, inbuf, inlen);
}

static int efx_mcdi_errno(unsigned int mcdi_err)
{
	switch (mcdi_err) {
	case 0:
		return 0;
#define TRANSLATE_ERROR(name)					\
	case MC_CMD_ERR_ ## name:				\
		return -name;
	TRANSLATE_ERROR(EPERM);
	TRANSLATE_ERROR(ENOENT);
	TRANSLATE_ERROR(EINTR);
	TRANSLATE_ERROR(EAGAIN);
	TRANSLATE_ERROR(EACCES);
	TRANSLATE_ERROR(EBUSY);
	TRANSLATE_ERROR(EINVAL);
	TRANSLATE_ERROR(EDEADLK);
	TRANSLATE_ERROR(ENOSYS);
	TRANSLATE_ERROR(ETIME);
	TRANSLATE_ERROR(EALREADY);
	TRANSLATE_ERROR(ENOSPC);
#undef TRANSLATE_ERROR
	case MC_CMD_ERR_ALLOC_FAIL:
		return -ENOBUFS;
	case MC_CMD_ERR_MAC_EXIST:
		return -EADDRINUSE;
	default:
		return -EPROTO;
	}
}

static void efx_mcdi_read_response_header(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	unsigned int respseq, respcmd, error;
	efx_dword_t hdr;

	efx->type->mcdi_read_response(efx, &hdr, 0, 4);
	respseq = EFX_DWORD_FIELD(hdr, MCDI_HEADER_SEQ);
	respcmd = EFX_DWORD_FIELD(hdr, MCDI_HEADER_CODE);
	error = EFX_DWORD_FIELD(hdr, MCDI_HEADER_ERROR);

	if (respcmd != MC_CMD_V2_EXTN) {
		mcdi->resp_hdr_len = 4;
		mcdi->resp_data_len = EFX_DWORD_FIELD(hdr, MCDI_HEADER_DATALEN);
	} else {
		efx->type->mcdi_read_response(efx, &hdr, 4, 4);
		mcdi->resp_hdr_len = 8;
		mcdi->resp_data_len =
			EFX_DWORD_FIELD(hdr, MC_CMD_V2_EXTN_IN_ACTUAL_LEN);
	}

	if (error && mcdi->resp_data_len == 0) {
		netif_err(efx, hw, efx->net_dev, "MC rebooted\n");
		mcdi->resprc = -EIO;
	} else if ((respseq ^ mcdi->seqno) & SEQ_MASK) {
		netif_err(efx, hw, efx->net_dev,
			  "MC response mismatch tx seq 0x%x rx seq 0x%x\n",
			  respseq, mcdi->seqno);
		mcdi->resprc = -EIO;
	} else if (error) {
		efx->type->mcdi_read_response(efx, &hdr, mcdi->resp_hdr_len, 4);
		mcdi->resprc =
			efx_mcdi_errno(EFX_DWORD_FIELD(hdr, EFX_DWORD_0));
	} else {
		mcdi->resprc = 0;
	}
}

static int efx_mcdi_poll(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	unsigned long time, finish;
	unsigned int spins;
	int rc;

	/* Check for a reboot atomically with respect to efx_mcdi_copyout() */
	rc = efx_mcdi_poll_reboot(efx);
	if (rc) {
		spin_lock_bh(&mcdi->iface_lock);
		mcdi->resprc = rc;
		mcdi->resp_hdr_len = 0;
		mcdi->resp_data_len = 0;
		spin_unlock_bh(&mcdi->iface_lock);
		return 0;
	}

	/* Poll for completion. Poll quickly (once a us) for the 1st jiffy,
	 * because generally mcdi responses are fast. After that, back off
	 * and poll once a jiffy (approximately)
	 */
	spins = TICK_USEC;
	finish = jiffies + MCDI_RPC_TIMEOUT;

	while (1) {
		if (spins != 0) {
			--spins;
			udelay(1);
		} else {
			schedule_timeout_uninterruptible(1);
		}

		time = jiffies;

		rmb();
		if (efx->type->mcdi_poll_response(efx))
			break;

		if (time_after(time, finish))
			return -ETIMEDOUT;
	}

	spin_lock_bh(&mcdi->iface_lock);
	efx_mcdi_read_response_header(efx);
	spin_unlock_bh(&mcdi->iface_lock);

	/* Return rc=0 like wait_event_timeout() */
	return 0;
}

/* Test and clear MC-rebooted flag for this port/function; reset
 * software state as necessary.
 */
int efx_mcdi_poll_reboot(struct efx_nic *efx)
{
	if (!efx->mcdi)
		return 0;

	return efx->type->mcdi_poll_reboot(efx);
}

static void efx_mcdi_acquire(struct efx_mcdi_iface *mcdi)
{
	/* Wait until the interface becomes QUIESCENT and we win the race
	 * to mark it RUNNING. */
	wait_event(mcdi->wq,
		   atomic_cmpxchg(&mcdi->state,
				  MCDI_STATE_QUIESCENT,
				  MCDI_STATE_RUNNING)
		   == MCDI_STATE_QUIESCENT);
}

static int efx_mcdi_await_completion(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);

	if (wait_event_timeout(
		    mcdi->wq,
		    atomic_read(&mcdi->state) == MCDI_STATE_COMPLETED,
		    MCDI_RPC_TIMEOUT) == 0)
		return -ETIMEDOUT;

	/* Check if efx_mcdi_set_mode() switched us back to polled completions.
	 * In which case, poll for completions directly. If efx_mcdi_ev_cpl()
	 * completed the request first, then we'll just end up completing the
	 * request again, which is safe.
	 *
	 * We need an smp_rmb() to synchronise with efx_mcdi_mode_poll(), which
	 * wait_event_timeout() implicitly provides.
	 */
	if (mcdi->mode == MCDI_MODE_POLL)
		return efx_mcdi_poll(efx);

	return 0;
}

static bool efx_mcdi_complete(struct efx_mcdi_iface *mcdi)
{
	/* If the interface is RUNNING, then move to COMPLETED and wake any
	 * waiters. If the interface isn't in RUNNING then we've received a
	 * duplicate completion after we've already transitioned back to
	 * QUIESCENT. [A subsequent invocation would increment seqno, so would
	 * have failed the seqno check].
	 */
	if (atomic_cmpxchg(&mcdi->state,
			   MCDI_STATE_RUNNING,
			   MCDI_STATE_COMPLETED) == MCDI_STATE_RUNNING) {
		wake_up(&mcdi->wq);
		return true;
	}

	return false;
}

static void efx_mcdi_release(struct efx_mcdi_iface *mcdi)
{
	atomic_set(&mcdi->state, MCDI_STATE_QUIESCENT);
	wake_up(&mcdi->wq);
}

static void efx_mcdi_ev_cpl(struct efx_nic *efx, unsigned int seqno,
			    unsigned int datalen, unsigned int mcdi_err)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	bool wake = false;

	spin_lock(&mcdi->iface_lock);

	if ((seqno ^ mcdi->seqno) & SEQ_MASK) {
		if (mcdi->credits)
			/* The request has been cancelled */
			--mcdi->credits;
		else
			netif_err(efx, hw, efx->net_dev,
				  "MC response mismatch tx seq 0x%x rx "
				  "seq 0x%x\n", seqno, mcdi->seqno);
	} else {
		if (efx->type->mcdi_max_ver >= 2) {
			/* MCDI v2 responses don't fit in an event */
			efx_mcdi_read_response_header(efx);
		} else {
			mcdi->resprc = efx_mcdi_errno(mcdi_err);
			mcdi->resp_hdr_len = 4;
			mcdi->resp_data_len = datalen;
		}

		wake = true;
	}

	spin_unlock(&mcdi->iface_lock);

	if (wake)
		efx_mcdi_complete(mcdi);
}

int efx_mcdi_rpc(struct efx_nic *efx, unsigned cmd,
		 const efx_dword_t *inbuf, size_t inlen,
		 efx_dword_t *outbuf, size_t outlen,
		 size_t *outlen_actual)
{
	int rc;

	rc = efx_mcdi_rpc_start(efx, cmd, inbuf, inlen);
	if (rc)
		return rc;
	return efx_mcdi_rpc_finish(efx, cmd, inlen,
				   outbuf, outlen, outlen_actual);
}

int efx_mcdi_rpc_start(struct efx_nic *efx, unsigned cmd,
		       const efx_dword_t *inbuf, size_t inlen)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);

	if (efx->type->mcdi_max_ver < 0 ||
	     (efx->type->mcdi_max_ver < 2 &&
	      cmd > MC_CMD_CMD_SPACE_ESCAPE_7))
		return -EINVAL;

	if (inlen > MCDI_CTL_SDU_LEN_MAX_V2 ||
	    (efx->type->mcdi_max_ver < 2 &&
	     inlen > MCDI_CTL_SDU_LEN_MAX_V1))
		return -EMSGSIZE;

	efx_mcdi_acquire(mcdi);

	/* Serialise with efx_mcdi_ev_cpl() and efx_mcdi_ev_death() */
	spin_lock_bh(&mcdi->iface_lock);
	++mcdi->seqno;
	spin_unlock_bh(&mcdi->iface_lock);

	efx_mcdi_copyin(efx, cmd, inbuf, inlen);
	mcdi->new_epoch = false;
	return 0;
}

int efx_mcdi_rpc_finish(struct efx_nic *efx, unsigned cmd, size_t inlen,
			efx_dword_t *outbuf, size_t outlen,
			size_t *outlen_actual)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);
	int rc;

	if (mcdi->mode == MCDI_MODE_POLL)
		rc = efx_mcdi_poll(efx);
	else
		rc = efx_mcdi_await_completion(efx);

	if (rc != 0) {
		/* Close the race with efx_mcdi_ev_cpl() executing just too late
		 * and completing a request we've just cancelled, by ensuring
		 * that the seqno check therein fails.
		 */
		spin_lock_bh(&mcdi->iface_lock);
		++mcdi->seqno;
		++mcdi->credits;
		spin_unlock_bh(&mcdi->iface_lock);

		netif_err(efx, hw, efx->net_dev,
			  "MC command 0x%x inlen %d mode %d timed out\n",
			  cmd, (int)inlen, mcdi->mode);
	} else {
		size_t hdr_len, data_len;

		/* At the very least we need a memory barrier here to ensure
		 * we pick up changes from efx_mcdi_ev_cpl(). Protect against
		 * a spurious efx_mcdi_ev_cpl() running concurrently by
		 * acquiring the iface_lock. */
		spin_lock_bh(&mcdi->iface_lock);
		rc = mcdi->resprc;
		hdr_len = mcdi->resp_hdr_len;
		data_len = mcdi->resp_data_len;
		spin_unlock_bh(&mcdi->iface_lock);

		BUG_ON(rc > 0);

		if (rc == 0) {
			efx->type->mcdi_read_response(efx, outbuf, hdr_len,
						      min(outlen, data_len));
			if (outlen_actual != NULL)
				*outlen_actual = data_len;
		} else if (cmd == MC_CMD_REBOOT && rc == -EIO)
			; /* Don't reset if MC_CMD_REBOOT returns EIO */
		else if (rc == -EIO || rc == -EINTR) {
			netif_err(efx, hw, efx->net_dev, "MC fatal error %d\n",
				  -rc);
			efx_schedule_reset(efx, RESET_TYPE_MC_FAILURE);
		} else
			netif_dbg(efx, hw, efx->net_dev,
				  "MC command 0x%x inlen %d failed rc=%d\n",
				  cmd, (int)inlen, -rc);

		if (rc == -EIO || rc == -EINTR) {
			msleep(MCDI_STATUS_SLEEP_MS);
			efx_mcdi_poll_reboot(efx);
			mcdi->new_epoch = true;
		}
	}

	efx_mcdi_release(mcdi);
	return rc;
}

void efx_mcdi_mode_poll(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi;

	if (!efx->mcdi)
		return;

	mcdi = efx_mcdi(efx);
	if (mcdi->mode == MCDI_MODE_POLL)
		return;

	/* We can switch from event completion to polled completion, because
	 * mcdi requests are always completed in shared memory. We do this by
	 * switching the mode to POLL'd then completing the request.
	 * efx_mcdi_await_completion() will then call efx_mcdi_poll().
	 *
	 * We need an smp_wmb() to synchronise with efx_mcdi_await_completion(),
	 * which efx_mcdi_complete() provides for us.
	 */
	mcdi->mode = MCDI_MODE_POLL;

	efx_mcdi_complete(mcdi);
}

void efx_mcdi_mode_event(struct efx_nic *efx)
{
	struct efx_mcdi_iface *mcdi;

	if (!efx->mcdi)
		return;

	mcdi = efx_mcdi(efx);

	if (mcdi->mode == MCDI_MODE_EVENTS)
		return;

	/* We can't switch from polled to event completion in the middle of a
	 * request, because the completion method is specified in the request.
	 * So acquire the interface to serialise the requestors. We don't need
	 * to acquire the iface_lock to change the mode here, but we do need a
	 * write memory barrier ensure that efx_mcdi_rpc() sees it, which
	 * efx_mcdi_acquire() provides.
	 */
	efx_mcdi_acquire(mcdi);
	mcdi->mode = MCDI_MODE_EVENTS;
	efx_mcdi_release(mcdi);
}

static void efx_mcdi_ev_death(struct efx_nic *efx, int rc)
{
	struct efx_mcdi_iface *mcdi = efx_mcdi(efx);

	/* If there is an outstanding MCDI request, it has been terminated
	 * either by a BADASSERT or REBOOT event. If the mcdi interface is
	 * in polled mode, then do nothing because the MC reboot handler will
	 * set the header correctly. However, if the mcdi interface is waiting
	 * for a CMDDONE event it won't receive it [and since all MCDI events
	 * are sent to the same queue, we can't be racing with
	 * efx_mcdi_ev_cpl()]
	 *
	 * There's a race here with efx_mcdi_rpc(), because we might receive
	 * a REBOOT event *before* the request has been copied out. In polled
	 * mode (during startup) this is irrelevant, because efx_mcdi_complete()
	 * is ignored. In event mode, this condition is just an edge-case of
	 * receiving a REBOOT event after posting the MCDI request. Did the mc
	 * reboot before or after the copyout? The best we can do always is
	 * just return failure.
	 */
	spin_lock(&mcdi->iface_lock);
	if (efx_mcdi_complete(mcdi)) {
		if (mcdi->mode == MCDI_MODE_EVENTS) {
			mcdi->resprc = rc;
			mcdi->resp_hdr_len = 0;
			mcdi->resp_data_len = 0;
			++mcdi->credits;
		}
	} else {
		int count;

		/* Nobody was waiting for an MCDI request, so trigger a reset */
		efx_schedule_reset(efx, RESET_TYPE_MC_FAILURE);

		/* Consume the status word since efx_mcdi_rpc_finish() won't */
		for (count = 0; count < MCDI_STATUS_DELAY_COUNT; ++count) {
			if (efx_mcdi_poll_reboot(efx))
				break;
			udelay(MCDI_STATUS_DELAY_US);
		}
		mcdi->new_epoch = true;
	}

	spin_unlock(&mcdi->iface_lock);
}

/* Called from  falcon_process_eventq for MCDI events */
void efx_mcdi_process_event(struct efx_channel *channel,
			    efx_qword_t *event)
{
	struct efx_nic *efx = channel->efx;
	int code = EFX_QWORD_FIELD(*event, MCDI_EVENT_CODE);
	u32 data = EFX_QWORD_FIELD(*event, MCDI_EVENT_DATA);

	switch (code) {
	case MCDI_EVENT_CODE_BADSSERT:
		netif_err(efx, hw, efx->net_dev,
			  "MC watchdog or assertion failure at 0x%x\n", data);
		efx_mcdi_ev_death(efx, -EINTR);
		break;

	case MCDI_EVENT_CODE_PMNOTICE:
		netif_info(efx, wol, efx->net_dev, "MCDI PM event.\n");
		break;

	case MCDI_EVENT_CODE_CMDDONE:
		efx_mcdi_ev_cpl(efx,
				MCDI_EVENT_FIELD(*event, CMDDONE_SEQ),
				MCDI_EVENT_FIELD(*event, CMDDONE_DATALEN),
				MCDI_EVENT_FIELD(*event, CMDDONE_ERRNO));
		break;

	case MCDI_EVENT_CODE_LINKCHANGE:
		efx_mcdi_process_link_change(efx, event);
		break;
	case MCDI_EVENT_CODE_SENSOREVT:
		efx_mcdi_sensor_event(efx, event);
		break;
	case MCDI_EVENT_CODE_SCHEDERR:
		netif_info(efx, hw, efx->net_dev,
			   "MC Scheduler error address=0x%x\n", data);
		break;
	case MCDI_EVENT_CODE_REBOOT:
		netif_info(efx, hw, efx->net_dev, "MC Reboot\n");
		efx_mcdi_ev_death(efx, -EIO);
		break;
	case MCDI_EVENT_CODE_MAC_STATS_DMA:
		/* MAC stats are gather lazily.  We can ignore this. */
		break;
	case MCDI_EVENT_CODE_FLR:
		efx_sriov_flr(efx, MCDI_EVENT_FIELD(*event, FLR_VF));
		break;
	case MCDI_EVENT_CODE_PTP_RX:
	case MCDI_EVENT_CODE_PTP_FAULT:
	case MCDI_EVENT_CODE_PTP_PPS:
		efx_ptp_event(efx, event);
		break;

	case MCDI_EVENT_CODE_TX_ERR:
	case MCDI_EVENT_CODE_RX_ERR:
		netif_err(efx, hw, efx->net_dev,
			  "%s DMA error (event: "EFX_QWORD_FMT")\n",
			  code == MCDI_EVENT_CODE_TX_ERR ? "TX" : "RX",
			  EFX_QWORD_VAL(*event));
		efx_schedule_reset(efx, RESET_TYPE_DMA_ERROR);
		break;
	default:
		netif_err(efx, hw, efx->net_dev, "Unknown MCDI event 0x%x\n",
			  code);
	}
}

/**************************************************************************
 *
 * Specific request functions
 *
 **************************************************************************
 */

void efx_mcdi_print_fwver(struct efx_nic *efx, char *buf, size_t len)
{
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_VERSION_OUT_LEN);
	size_t outlength;
	const __le16 *ver_words;
	int rc;

	BUILD_BUG_ON(MC_CMD_GET_VERSION_IN_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_GET_VERSION, NULL, 0,
			  outbuf, sizeof(outbuf), &outlength);
	if (rc)
		goto fail;

	if (outlength < MC_CMD_GET_VERSION_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	ver_words = (__le16 *)MCDI_PTR(outbuf, GET_VERSION_OUT_VERSION);
	snprintf(buf, len, "%u.%u.%u.%u",
		 le16_to_cpu(ver_words[0]), le16_to_cpu(ver_words[1]),
		 le16_to_cpu(ver_words[2]), le16_to_cpu(ver_words[3]));
	return;

fail:
	netif_err(efx, probe, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	buf[0] = 0;
}

int efx_mcdi_drv_attach(struct efx_nic *efx, bool driver_operating,
			bool *was_attached)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_DRV_ATTACH_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_DRV_ATTACH_OUT_LEN);
	size_t outlen;
	int rc;

	MCDI_SET_DWORD(inbuf, DRV_ATTACH_IN_NEW_STATE,
		       driver_operating ? 1 : 0);
	MCDI_SET_DWORD(inbuf, DRV_ATTACH_IN_UPDATE, 1);
	MCDI_SET_DWORD(inbuf, DRV_ATTACH_IN_FIRMWARE_ID, MC_CMD_FW_LOW_LATENCY);

	rc = efx_mcdi_rpc(efx, MC_CMD_DRV_ATTACH, inbuf, sizeof(inbuf),
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;
	if (outlen < MC_CMD_DRV_ATTACH_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	if (was_attached != NULL)
		*was_attached = MCDI_DWORD(outbuf, DRV_ATTACH_OUT_OLD_STATE);
	return 0;

fail:
	netif_err(efx, probe, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

int efx_mcdi_get_board_cfg(struct efx_nic *efx, u8 *mac_address,
			   u16 *fw_subtype_list, u32 *capabilities)
{
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_BOARD_CFG_OUT_LENMAX);
	size_t outlen, i;
	int port_num = efx_port_num(efx);
	int rc;

	BUILD_BUG_ON(MC_CMD_GET_BOARD_CFG_IN_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_GET_BOARD_CFG, NULL, 0,
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;

	if (outlen < MC_CMD_GET_BOARD_CFG_OUT_LENMIN) {
		rc = -EIO;
		goto fail;
	}

	if (mac_address)
		memcpy(mac_address,
		       port_num ?
		       MCDI_PTR(outbuf, GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT1) :
		       MCDI_PTR(outbuf, GET_BOARD_CFG_OUT_MAC_ADDR_BASE_PORT0),
		       ETH_ALEN);
	if (fw_subtype_list) {
		for (i = 0;
		     i < MCDI_VAR_ARRAY_LEN(outlen,
					    GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST);
		     i++)
			fw_subtype_list[i] = MCDI_ARRAY_WORD(
				outbuf, GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST, i);
		for (; i < MC_CMD_GET_BOARD_CFG_OUT_FW_SUBTYPE_LIST_MAXNUM; i++)
			fw_subtype_list[i] = 0;
	}
	if (capabilities) {
		if (port_num)
			*capabilities = MCDI_DWORD(outbuf,
					GET_BOARD_CFG_OUT_CAPABILITIES_PORT1);
		else
			*capabilities = MCDI_DWORD(outbuf,
					GET_BOARD_CFG_OUT_CAPABILITIES_PORT0);
	}

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d len=%d\n",
		  __func__, rc, (int)outlen);

	return rc;
}

int efx_mcdi_log_ctrl(struct efx_nic *efx, bool evq, bool uart, u32 dest_evq)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_LOG_CTRL_IN_LEN);
	u32 dest = 0;
	int rc;

	if (uart)
		dest |= MC_CMD_LOG_CTRL_IN_LOG_DEST_UART;
	if (evq)
		dest |= MC_CMD_LOG_CTRL_IN_LOG_DEST_EVQ;

	MCDI_SET_DWORD(inbuf, LOG_CTRL_IN_LOG_DEST, dest);
	MCDI_SET_DWORD(inbuf, LOG_CTRL_IN_LOG_DEST_EVQ, dest_evq);

	BUILD_BUG_ON(MC_CMD_LOG_CTRL_OUT_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_LOG_CTRL, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

int efx_mcdi_nvram_types(struct efx_nic *efx, u32 *nvram_types_out)
{
	MCDI_DECLARE_BUF(outbuf, MC_CMD_NVRAM_TYPES_OUT_LEN);
	size_t outlen;
	int rc;

	BUILD_BUG_ON(MC_CMD_NVRAM_TYPES_IN_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_TYPES, NULL, 0,
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;
	if (outlen < MC_CMD_NVRAM_TYPES_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	*nvram_types_out = MCDI_DWORD(outbuf, NVRAM_TYPES_OUT_TYPES);
	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n",
		  __func__, rc);
	return rc;
}

int efx_mcdi_nvram_info(struct efx_nic *efx, unsigned int type,
			size_t *size_out, size_t *erase_size_out,
			bool *protected_out)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_INFO_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_NVRAM_INFO_OUT_LEN);
	size_t outlen;
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_INFO_IN_TYPE, type);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_INFO, inbuf, sizeof(inbuf),
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;
	if (outlen < MC_CMD_NVRAM_INFO_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	*size_out = MCDI_DWORD(outbuf, NVRAM_INFO_OUT_SIZE);
	*erase_size_out = MCDI_DWORD(outbuf, NVRAM_INFO_OUT_ERASESIZE);
	*protected_out = !!(MCDI_DWORD(outbuf, NVRAM_INFO_OUT_FLAGS) &
				(1 << MC_CMD_NVRAM_INFO_OUT_PROTECTED_LBN));
	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_nvram_test(struct efx_nic *efx, unsigned int type)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_TEST_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_NVRAM_TEST_OUT_LEN);
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_TEST_IN_TYPE, type);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_TEST, inbuf, sizeof(inbuf),
			  outbuf, sizeof(outbuf), NULL);
	if (rc)
		return rc;

	switch (MCDI_DWORD(outbuf, NVRAM_TEST_OUT_RESULT)) {
	case MC_CMD_NVRAM_TEST_PASS:
	case MC_CMD_NVRAM_TEST_NOTSUPP:
		return 0;
	default:
		return -EIO;
	}
}

int efx_mcdi_nvram_test_all(struct efx_nic *efx)
{
	u32 nvram_types;
	unsigned int type;
	int rc;

	rc = efx_mcdi_nvram_types(efx, &nvram_types);
	if (rc)
		goto fail1;

	type = 0;
	while (nvram_types != 0) {
		if (nvram_types & 1) {
			rc = efx_mcdi_nvram_test(efx, type);
			if (rc)
				goto fail2;
		}
		type++;
		nvram_types >>= 1;
	}

	return 0;

fail2:
	netif_err(efx, hw, efx->net_dev, "%s: failed type=%u\n",
		  __func__, type);
fail1:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_read_assertion(struct efx_nic *efx)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_GET_ASSERTS_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_GET_ASSERTS_OUT_LEN);
	unsigned int flags, index;
	const char *reason;
	size_t outlen;
	int retry;
	int rc;

	/* Attempt to read any stored assertion state before we reboot
	 * the mcfw out of the assertion handler. Retry twice, once
	 * because a boot-time assertion might cause this command to fail
	 * with EINTR. And once again because GET_ASSERTS can race with
	 * MC_CMD_REBOOT running on the other port. */
	retry = 2;
	do {
		MCDI_SET_DWORD(inbuf, GET_ASSERTS_IN_CLEAR, 1);
		rc = efx_mcdi_rpc(efx, MC_CMD_GET_ASSERTS,
				  inbuf, MC_CMD_GET_ASSERTS_IN_LEN,
				  outbuf, sizeof(outbuf), &outlen);
	} while ((rc == -EINTR || rc == -EIO) && retry-- > 0);

	if (rc)
		return rc;
	if (outlen < MC_CMD_GET_ASSERTS_OUT_LEN)
		return -EIO;

	/* Print out any recorded assertion state */
	flags = MCDI_DWORD(outbuf, GET_ASSERTS_OUT_GLOBAL_FLAGS);
	if (flags == MC_CMD_GET_ASSERTS_FLAGS_NO_FAILS)
		return 0;

	reason = (flags == MC_CMD_GET_ASSERTS_FLAGS_SYS_FAIL)
		? "system-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_THR_FAIL)
		? "thread-level assertion"
		: (flags == MC_CMD_GET_ASSERTS_FLAGS_WDOG_FIRED)
		? "watchdog reset"
		: "unknown assertion";
	netif_err(efx, hw, efx->net_dev,
		  "MCPU %s at PC = 0x%.8x in thread 0x%.8x\n", reason,
		  MCDI_DWORD(outbuf, GET_ASSERTS_OUT_SAVED_PC_OFFS),
		  MCDI_DWORD(outbuf, GET_ASSERTS_OUT_THREAD_OFFS));

	/* Print out the registers */
	for (index = 0;
	     index < MC_CMD_GET_ASSERTS_OUT_GP_REGS_OFFS_NUM;
	     index++)
		netif_err(efx, hw, efx->net_dev, "R%.2d (?): 0x%.8x\n",
			  1 + index,
			  MCDI_ARRAY_DWORD(outbuf, GET_ASSERTS_OUT_GP_REGS_OFFS,
					   index));

	return 0;
}

static void efx_mcdi_exit_assertion(struct efx_nic *efx)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_REBOOT_IN_LEN);

	/* If the MC is running debug firmware, it might now be
	 * waiting for a debugger to attach, but we just want it to
	 * reboot.  We set a flag that makes the command a no-op if it
	 * has already done so.  We don't know what return code to
	 * expect (0 or -EIO), so ignore it.
	 */
	BUILD_BUG_ON(MC_CMD_REBOOT_OUT_LEN != 0);
	MCDI_SET_DWORD(inbuf, REBOOT_IN_FLAGS,
		       MC_CMD_REBOOT_FLAGS_AFTER_ASSERTION);
	(void) efx_mcdi_rpc(efx, MC_CMD_REBOOT, inbuf, MC_CMD_REBOOT_IN_LEN,
			    NULL, 0, NULL);
}

int efx_mcdi_handle_assertion(struct efx_nic *efx)
{
	int rc;

	rc = efx_mcdi_read_assertion(efx);
	if (rc)
		return rc;

	efx_mcdi_exit_assertion(efx);

	return 0;
}

void efx_mcdi_set_id_led(struct efx_nic *efx, enum efx_led_mode mode)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_SET_ID_LED_IN_LEN);
	int rc;

	BUILD_BUG_ON(EFX_LED_OFF != MC_CMD_LED_OFF);
	BUILD_BUG_ON(EFX_LED_ON != MC_CMD_LED_ON);
	BUILD_BUG_ON(EFX_LED_DEFAULT != MC_CMD_LED_DEFAULT);

	BUILD_BUG_ON(MC_CMD_SET_ID_LED_OUT_LEN != 0);

	MCDI_SET_DWORD(inbuf, SET_ID_LED_IN_STATE, mode);

	rc = efx_mcdi_rpc(efx, MC_CMD_SET_ID_LED, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n",
			  __func__, rc);
}

static int efx_mcdi_reset_port(struct efx_nic *efx)
{
	int rc = efx_mcdi_rpc(efx, MC_CMD_ENTITY_RESET, NULL, 0, NULL, 0, NULL);
	if (rc)
		netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n",
			  __func__, rc);
	return rc;
}

static int efx_mcdi_reset_mc(struct efx_nic *efx)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_REBOOT_IN_LEN);
	int rc;

	BUILD_BUG_ON(MC_CMD_REBOOT_OUT_LEN != 0);
	MCDI_SET_DWORD(inbuf, REBOOT_IN_FLAGS, 0);
	rc = efx_mcdi_rpc(efx, MC_CMD_REBOOT, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	/* White is black, and up is down */
	if (rc == -EIO)
		return 0;
	if (rc == 0)
		rc = -EIO;
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

enum reset_type efx_mcdi_map_reset_reason(enum reset_type reason)
{
	return RESET_TYPE_RECOVER_OR_ALL;
}

int efx_mcdi_reset(struct efx_nic *efx, enum reset_type method)
{
	int rc;

	/* Recover from a failed assertion pre-reset */
	rc = efx_mcdi_handle_assertion(efx);
	if (rc)
		return rc;

	if (method == RESET_TYPE_WORLD)
		return efx_mcdi_reset_mc(efx);
	else
		return efx_mcdi_reset_port(efx);
}

static int efx_mcdi_wol_filter_set(struct efx_nic *efx, u32 type,
				   const u8 *mac, int *id_out)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_WOL_FILTER_SET_IN_LEN);
	MCDI_DECLARE_BUF(outbuf, MC_CMD_WOL_FILTER_SET_OUT_LEN);
	size_t outlen;
	int rc;

	MCDI_SET_DWORD(inbuf, WOL_FILTER_SET_IN_WOL_TYPE, type);
	MCDI_SET_DWORD(inbuf, WOL_FILTER_SET_IN_FILTER_MODE,
		       MC_CMD_FILTER_MODE_SIMPLE);
	memcpy(MCDI_PTR(inbuf, WOL_FILTER_SET_IN_MAGIC_MAC), mac, ETH_ALEN);

	rc = efx_mcdi_rpc(efx, MC_CMD_WOL_FILTER_SET, inbuf, sizeof(inbuf),
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;

	if (outlen < MC_CMD_WOL_FILTER_SET_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	*id_out = (int)MCDI_DWORD(outbuf, WOL_FILTER_SET_OUT_FILTER_ID);

	return 0;

fail:
	*id_out = -1;
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;

}


int
efx_mcdi_wol_filter_set_magic(struct efx_nic *efx,  const u8 *mac, int *id_out)
{
	return efx_mcdi_wol_filter_set(efx, MC_CMD_WOL_TYPE_MAGIC, mac, id_out);
}


int efx_mcdi_wol_filter_get_magic(struct efx_nic *efx, int *id_out)
{
	MCDI_DECLARE_BUF(outbuf, MC_CMD_WOL_FILTER_GET_OUT_LEN);
	size_t outlen;
	int rc;

	rc = efx_mcdi_rpc(efx, MC_CMD_WOL_FILTER_GET, NULL, 0,
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;

	if (outlen < MC_CMD_WOL_FILTER_GET_OUT_LEN) {
		rc = -EIO;
		goto fail;
	}

	*id_out = (int)MCDI_DWORD(outbuf, WOL_FILTER_GET_OUT_FILTER_ID);

	return 0;

fail:
	*id_out = -1;
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}


int efx_mcdi_wol_filter_remove(struct efx_nic *efx, int id)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_WOL_FILTER_REMOVE_IN_LEN);
	int rc;

	MCDI_SET_DWORD(inbuf, WOL_FILTER_REMOVE_IN_FILTER_ID, (u32)id);

	rc = efx_mcdi_rpc(efx, MC_CMD_WOL_FILTER_REMOVE, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

int efx_mcdi_flush_rxqs(struct efx_nic *efx)
{
	struct efx_channel *channel;
	struct efx_rx_queue *rx_queue;
	MCDI_DECLARE_BUF(inbuf,
			 MC_CMD_FLUSH_RX_QUEUES_IN_LEN(EFX_MAX_CHANNELS));
	int rc, count;

	BUILD_BUG_ON(EFX_MAX_CHANNELS >
		     MC_CMD_FLUSH_RX_QUEUES_IN_QID_OFST_MAXNUM);

	count = 0;
	efx_for_each_channel(channel, efx) {
		efx_for_each_channel_rx_queue(rx_queue, channel) {
			if (rx_queue->flush_pending) {
				rx_queue->flush_pending = false;
				atomic_dec(&efx->rxq_flush_pending);
				MCDI_SET_ARRAY_DWORD(
					inbuf, FLUSH_RX_QUEUES_IN_QID_OFST,
					count, efx_rx_queue_index(rx_queue));
				count++;
			}
		}
	}

	rc = efx_mcdi_rpc(efx, MC_CMD_FLUSH_RX_QUEUES, inbuf,
			  MC_CMD_FLUSH_RX_QUEUES_IN_LEN(count), NULL, 0, NULL);
	WARN_ON(rc < 0);

	return rc;
}

int efx_mcdi_wol_filter_reset(struct efx_nic *efx)
{
	int rc;

	rc = efx_mcdi_rpc(efx, MC_CMD_WOL_FILTER_RESET, NULL, 0, NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

#ifdef CONFIG_SFC_MTD

#define EFX_MCDI_NVRAM_LEN_MAX 128

static int efx_mcdi_nvram_update_start(struct efx_nic *efx, unsigned int type)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_UPDATE_START_IN_LEN);
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_UPDATE_START_IN_TYPE, type);

	BUILD_BUG_ON(MC_CMD_NVRAM_UPDATE_START_OUT_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_UPDATE_START, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_nvram_read(struct efx_nic *efx, unsigned int type,
			       loff_t offset, u8 *buffer, size_t length)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_READ_IN_LEN);
	MCDI_DECLARE_BUF(outbuf,
			 MC_CMD_NVRAM_READ_OUT_LEN(EFX_MCDI_NVRAM_LEN_MAX));
	size_t outlen;
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_READ_IN_TYPE, type);
	MCDI_SET_DWORD(inbuf, NVRAM_READ_IN_OFFSET, offset);
	MCDI_SET_DWORD(inbuf, NVRAM_READ_IN_LENGTH, length);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_READ, inbuf, sizeof(inbuf),
			  outbuf, sizeof(outbuf), &outlen);
	if (rc)
		goto fail;

	memcpy(buffer, MCDI_PTR(outbuf, NVRAM_READ_OUT_READ_BUFFER), length);
	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_nvram_write(struct efx_nic *efx, unsigned int type,
				loff_t offset, const u8 *buffer, size_t length)
{
	MCDI_DECLARE_BUF(inbuf,
			 MC_CMD_NVRAM_WRITE_IN_LEN(EFX_MCDI_NVRAM_LEN_MAX));
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_WRITE_IN_TYPE, type);
	MCDI_SET_DWORD(inbuf, NVRAM_WRITE_IN_OFFSET, offset);
	MCDI_SET_DWORD(inbuf, NVRAM_WRITE_IN_LENGTH, length);
	memcpy(MCDI_PTR(inbuf, NVRAM_WRITE_IN_WRITE_BUFFER), buffer, length);

	BUILD_BUG_ON(MC_CMD_NVRAM_WRITE_OUT_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_WRITE, inbuf,
			  ALIGN(MC_CMD_NVRAM_WRITE_IN_LEN(length), 4),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_nvram_erase(struct efx_nic *efx, unsigned int type,
				loff_t offset, size_t length)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_ERASE_IN_LEN);
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_ERASE_IN_TYPE, type);
	MCDI_SET_DWORD(inbuf, NVRAM_ERASE_IN_OFFSET, offset);
	MCDI_SET_DWORD(inbuf, NVRAM_ERASE_IN_LENGTH, length);

	BUILD_BUG_ON(MC_CMD_NVRAM_ERASE_OUT_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_ERASE, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

static int efx_mcdi_nvram_update_finish(struct efx_nic *efx, unsigned int type)
{
	MCDI_DECLARE_BUF(inbuf, MC_CMD_NVRAM_UPDATE_FINISH_IN_LEN);
	int rc;

	MCDI_SET_DWORD(inbuf, NVRAM_UPDATE_FINISH_IN_TYPE, type);

	BUILD_BUG_ON(MC_CMD_NVRAM_UPDATE_FINISH_OUT_LEN != 0);

	rc = efx_mcdi_rpc(efx, MC_CMD_NVRAM_UPDATE_FINISH, inbuf, sizeof(inbuf),
			  NULL, 0, NULL);
	if (rc)
		goto fail;

	return 0;

fail:
	netif_err(efx, hw, efx->net_dev, "%s: failed rc=%d\n", __func__, rc);
	return rc;
}

int efx_mcdi_mtd_read(struct mtd_info *mtd, loff_t start,
		      size_t len, size_t *retlen, u8 *buffer)
{
	struct efx_mcdi_mtd_partition *part = to_efx_mcdi_mtd_partition(mtd);
	struct efx_nic *efx = mtd->priv;
	loff_t offset = start;
	loff_t end = min_t(loff_t, start + len, mtd->size);
	size_t chunk;
	int rc = 0;

	while (offset < end) {
		chunk = min_t(size_t, end - offset, EFX_MCDI_NVRAM_LEN_MAX);
		rc = efx_mcdi_nvram_read(efx, part->nvram_type, offset,
					 buffer, chunk);
		if (rc)
			goto out;
		offset += chunk;
		buffer += chunk;
	}
out:
	*retlen = offset - start;
	return rc;
}

int efx_mcdi_mtd_erase(struct mtd_info *mtd, loff_t start, size_t len)
{
	struct efx_mcdi_mtd_partition *part = to_efx_mcdi_mtd_partition(mtd);
	struct efx_nic *efx = mtd->priv;
	loff_t offset = start & ~((loff_t)(mtd->erasesize - 1));
	loff_t end = min_t(loff_t, start + len, mtd->size);
	size_t chunk = part->common.mtd.erasesize;
	int rc = 0;

	if (!part->updating) {
		rc = efx_mcdi_nvram_update_start(efx, part->nvram_type);
		if (rc)
			goto out;
		part->updating = true;
	}

	/* The MCDI interface can in fact do multiple erase blocks at once;
	 * but erasing may be slow, so we make multiple calls here to avoid
	 * tripping the MCDI RPC timeout. */
	while (offset < end) {
		rc = efx_mcdi_nvram_erase(efx, part->nvram_type, offset,
					  chunk);
		if (rc)
			goto out;
		offset += chunk;
	}
out:
	return rc;
}

int efx_mcdi_mtd_write(struct mtd_info *mtd, loff_t start,
		       size_t len, size_t *retlen, const u8 *buffer)
{
	struct efx_mcdi_mtd_partition *part = to_efx_mcdi_mtd_partition(mtd);
	struct efx_nic *efx = mtd->priv;
	loff_t offset = start;
	loff_t end = min_t(loff_t, start + len, mtd->size);
	size_t chunk;
	int rc = 0;

	if (!part->updating) {
		rc = efx_mcdi_nvram_update_start(efx, part->nvram_type);
		if (rc)
			goto out;
		part->updating = true;
	}

	while (offset < end) {
		chunk = min_t(size_t, end - offset, EFX_MCDI_NVRAM_LEN_MAX);
		rc = efx_mcdi_nvram_write(efx, part->nvram_type, offset,
					  buffer, chunk);
		if (rc)
			goto out;
		offset += chunk;
		buffer += chunk;
	}
out:
	*retlen = offset - start;
	return rc;
}

int efx_mcdi_mtd_sync(struct mtd_info *mtd)
{
	struct efx_mcdi_mtd_partition *part = to_efx_mcdi_mtd_partition(mtd);
	struct efx_nic *efx = mtd->priv;
	int rc = 0;

	if (part->updating) {
		part->updating = false;
		rc = efx_mcdi_nvram_update_finish(efx, part->nvram_type);
	}

	return rc;
}

void efx_mcdi_mtd_rename(struct efx_mtd_partition *part)
{
	struct efx_mcdi_mtd_partition *mcdi_part =
		container_of(part, struct efx_mcdi_mtd_partition, common);
	struct efx_nic *efx = part->mtd.priv;

	snprintf(part->name, sizeof(part->name), "%s %s:%02x",
		 efx->name, part->type_name, mcdi_part->fw_subtype);
}

#endif /* CONFIG_SFC_MTD */
