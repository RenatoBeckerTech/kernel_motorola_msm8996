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

/**========================================================================

  \file     wma.c
  \brief    Implementation of WMA

  ========================================================================*/
/**=========================================================================
  EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$   $DateTime: $ $Author: $


  when              who           what, where, why
  --------          ---           -----------------------------------------
  12/03/2013        Ganesh        Implementation of wma function for initialization
                    Kondabattini
  ==========================================================================*/
#ifndef WMA_API_H
#define WMA_API_H

#include "osdep.h"
#include "vos_mq.h"
#include "aniGlobal.h"
#include "a_types.h"
#include "wmi_unified.h"
#ifndef QCA_WIFI_ISOC
#include "wlan_hdd_tgt_cfg.h"
#endif
#ifdef NOT_YET
#include "htc_api.h"
#endif
#include "limGlobal.h"

typedef v_VOID_t* WMA_HANDLE;

typedef enum {
    /* Set ampdu size */
    GEN_VDEV_PARAM_AMPDU = 0x1,
    /* Set amsdu size */
    GEN_VDEV_PARAM_AMSDU,
    GEN_PARAM_DUMP_AGC_START,
    GEN_PARAM_DUMP_AGC,
    GEN_PARAM_DUMP_CHANINFO_START,
    GEN_PARAM_DUMP_CHANINFO,
    GEN_PARAM_DUMP_WATCHDOG,
} GEN_PARAM;

#define VDEV_CMD 1
#define PDEV_CMD 2
#define GEN_CMD  3
#define DBG_CMD  4

#ifdef QCA_WIFI_ISOC
VOS_STATUS wma_nv_download_start(v_VOID_t *vos_context);
#endif

VOS_STATUS wma_pre_start(v_VOID_t *vos_context);

VOS_STATUS wma_mc_process_msg( v_VOID_t *vos_context, vos_msg_t *msg );

VOS_STATUS wma_start(v_VOID_t *vos_context);

VOS_STATUS wma_stop(v_VOID_t *vos_context, tANI_U8 reason);

VOS_STATUS wma_close(v_VOID_t *vos_context);

v_VOID_t wma_rx_ready_event(WMA_HANDLE handle, v_VOID_t *ev);

v_VOID_t wma_rx_service_ready_event(WMA_HANDLE handle, 
				v_VOID_t *ev);

v_VOID_t wma_setneedshutdown(v_VOID_t *vos_context);

v_BOOL_t wma_needshutdown(v_VOID_t *vos_context);

VOS_STATUS wma_wait_for_ready_event(WMA_HANDLE handle);

int wma_cli_get_command(void *wmapvosContext, int vdev_id,
			int param_id, int vpdev);
eHalStatus wma_set_htconfig(tANI_U8 vdev_id, tANI_U16 ht_capab, int value);

#ifndef QCA_WIFI_ISOC
int wma_suspend_target(WMA_HANDLE handle, int disable_target_intr);
void wma_target_suspend_complete(void *context);
int wma_resume_target(WMA_HANDLE handle);
int wma_is_wow_enabled(WMA_HANDLE handle);
#endif
int wma_set_peer_param(void *wma_ctx, u_int8_t *peer_addr, u_int32_t param_id,
			u_int32_t param_value, u_int32_t vdev_id);
#ifdef NOT_YET
VOS_STATUS wma_update_channel_list(WMA_HANDLE handle, void *scan_chan_info);
#endif

u_int8_t *wma_get_vdev_address_by_vdev_id(u_int8_t vdev_id);

#ifndef QCA_WIFI_ISOC
void *wma_get_beacon_buffer_by_vdev_id(u_int8_t vdev_id,
				       u_int32_t *buffer_size);
#endif	/* QCA_WIFI_ISOC */
tANI_U8 wma_getFwWlanFeatCaps(tANI_U8 featEnumValue);
#endif
