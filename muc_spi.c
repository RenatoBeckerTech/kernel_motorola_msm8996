/*
 * Copyright (C) 2015 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>

#include "crc.h"
#include "muc_attach.h"
#include "mods_nw.h"
#include "muc_svc.h"

/* Default payload size of a SPI packet (in bytes) */
#define DEFAULT_PAYLOAD_SZ  (32)

/* SPI packet header bit definitions */
#define HDR_BIT_VALID  (0x01 << 7)  /* 1 = valid packet, 0 = dummy packet */
#define HDR_BIT_RSVD   (0x03 << 5)  /* Reserved */
#define HDR_BIT_PKTS   (0x1F << 0)  /* How many additional packets to expect */

/* SPI packet CRC size (in bytes) */
#define CRC_SIZE       (2)

/* Macro to determine the payload size from the packet size */
#define PL_SIZE(pkt_size)  (pkt_size - sizeof(struct spi_msg_hdr) - CRC_SIZE)

/* Macro to determine the packet size from the payload size */
#define PKT_SIZE(pl_size)  (pl_size + sizeof(struct spi_msg_hdr) + CRC_SIZE)

/* Macro to determine the location of the CRC in the packet */
#define CRC_NDX(pkt_size)  (pkt_size - CRC_SIZE)

#define RDY_TIMEOUT_JIFFIES     (1 * HZ) /* 1 sec */

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

struct muc_spi_data {
	struct spi_device *spi;
	struct mods_dl_device *dld;
	bool present;
	struct notifier_block attach_nb;   /* attach/detach notifications */
	struct mutex mutex;
	wait_queue_head_t rdy_wq;

	int gpio_wake_n;
	int gpio_rdy_n;

	size_t pkt_size;                   /* Size of hdr + pl + CRC in bytes */
	__u8 *tx_pkt;                      /* Buffer for transmit packets */
	__u8 *rx_pkt;                      /* Buffer for received packets */

	/*
	 * Buffer to hold incoming payload (which could be spread across
	 * multiple packets)
	 */
	__u8 rcvd_payload[MUC_MSG_SIZE_MAX];
	int rcvd_payload_idx;
};

#pragma pack(push, 1)
struct spi_msg_hdr
{
	__u8 bits;
};
#pragma pack(pop)

static void parse_rx_pkt(struct muc_spi_data *dd);

static inline struct muc_spi_data *dld_to_dd(struct mods_dl_device *dld)
{
	return (struct muc_spi_data *)dld->dl_priv;
}

static int set_packet_size(struct muc_spi_data *dd, size_t pkt_size)
{
	size_t pl_size = PL_SIZE(pkt_size);
	__u8 *tx_pkt_new;
	__u8 *rx_pkt_new;

	/* Immediately return if packet size is not changing */
	if (pkt_size == dd->pkt_size)
		return 0;

	/*
	 * Verify new packet size is valid. The payload must be no smaller than
	 * the default packet size and must be a power of two.
	 */
	if (!is_power_of_2(pl_size) || (pl_size < DEFAULT_PAYLOAD_SZ))
		return -EINVAL;

	/* Allocate new TX packet buffer */
	tx_pkt_new = devm_kzalloc(&dd->spi->dev, pkt_size, GFP_KERNEL);
	if (!tx_pkt_new)
		return -ENOMEM;

	/* Allocate new RX packet buffer */
	rx_pkt_new = devm_kzalloc(&dd->spi->dev, pkt_size, GFP_KERNEL);
	if (!rx_pkt_new) {
		devm_kfree(&dd->spi->dev, tx_pkt_new);
		return -ENOMEM;
	}

	/* Save new packet size */
	dd->pkt_size = pkt_size;

	/* Free existing packet buffers (if any) */
	if (dd->tx_pkt)
		devm_kfree(&dd->spi->dev, dd->tx_pkt);
	if (dd->rx_pkt)
		devm_kfree(&dd->spi->dev, dd->rx_pkt);

	/* Save pointers to new packet buffers */
	dd->tx_pkt = tx_pkt_new;
	dd->rx_pkt = rx_pkt_new;

	dev_info(&dd->spi->dev, "Packet size is %zu bytes\n", pkt_size);

	return 0;
}

static int muc_spi_transfer_locked(struct muc_spi_data *dd,
				   uint8_t *tx_buf, bool keep_wake)
{
	struct spi_transfer t[] = {
		{
			.tx_buf = tx_buf,
			.rx_buf = dd->rx_pkt,
			.len = dd->pkt_size,
		},
	};
	int ret;

	/* Check if WAKE is not asserted */
	if (gpio_get_value(dd->gpio_wake_n)) {
		/* Assert WAKE */
		gpio_set_value(dd->gpio_wake_n, 0);

		/* Wait for ADC enable */
		udelay(300);
	}

	/* Wait for RDY to be asserted */
	ret = wait_event_timeout(dd->rdy_wq, !gpio_get_value(dd->gpio_rdy_n),
				 RDY_TIMEOUT_JIFFIES);

	if (!keep_wake) {
		/* Deassert WAKE */
		gpio_set_value(dd->gpio_wake_n, 1);
	}

	/*
	 * Check that RDY successfully was asserted after wake deassert to ensure
	 * wake line is deasserted if requested.
	 */
	if (ret <= 0) {
		dev_err(&dd->spi->dev, "Timeout waiting for rdy to assert\n");
		return ret;
	}

	ret = spi_sync_transfer(dd->spi, t, 1);

	if (!ret)
		parse_rx_pkt(dd);

	return ret;
}

static int muc_spi_transfer(struct muc_spi_data *dd, uint8_t *tx_buf,
			    bool keep_wake)
{
	int ret;

	mutex_lock(&dd->mutex);
	ret = muc_spi_transfer_locked(dd, tx_buf, keep_wake);
	mutex_unlock(&dd->mutex);

	return ret;
}

static void parse_rx_pkt(struct muc_spi_data *dd)
{
	struct spi_msg_hdr *hdr = (struct spi_msg_hdr *)dd->rx_pkt;
	struct spi_device *spi = dd->spi;
	uint16_t *rcvcrc_p;
	uint16_t calcrc;
	size_t pl_size = PL_SIZE(dd->pkt_size);

	if (!(hdr->bits & HDR_BIT_VALID)) {
		/* Received a dummy packet - nothing to do! */
		return;
	}

	rcvcrc_p = (uint16_t *)&dd->rx_pkt[CRC_NDX(dd->pkt_size)];
	calcrc = gen_crc16(dd->rx_pkt, CRC_NDX(dd->pkt_size));
	if (le16_to_cpu(*rcvcrc_p) != calcrc) {
		dev_err(&spi->dev, "CRC mismatch, received: 0x%x,"
			"calculated: 0x%x\n", le16_to_cpu(*rcvcrc_p), calcrc);
		return;
	}

	/* Check if un-packetizing is required */
	if (MUC_MSG_SIZE_MAX != pl_size) {
		if (unlikely(dd->rcvd_payload_idx >= MUC_MSG_SIZE_MAX)) {
			dev_err(&spi->dev, "Too many packets received!\n");
			dd->rcvd_payload_idx = 0;
			return;
		}

		memcpy(&dd->rcvd_payload[dd->rcvd_payload_idx],
		       &dd->rx_pkt[sizeof(struct spi_msg_hdr)],
		       pl_size);
		dd->rcvd_payload_idx += pl_size;

		if (hdr->bits & HDR_BIT_PKTS) {
			/* Need additional packets */
			muc_spi_transfer_locked(dd, NULL,
					((hdr->bits & HDR_BIT_PKTS) > 1));
			return;
		}

		mods_nw_switch(dd->dld, dd->rcvd_payload, dd->rcvd_payload_idx);
		dd->rcvd_payload_idx = 0;
	} else {
		/* Un-packetizing not required */
		mods_nw_switch(dd->dld, &dd->rx_pkt[sizeof(struct spi_msg_hdr)],
			       pl_size);
	}
}

static irqreturn_t muc_spi_isr(int irq, void *data)
{
	struct muc_spi_data *dd = data;

	/* Any interrupt while the MuC is not attached would be spurious */
	if (!dd->present)
		return IRQ_HANDLED;

	muc_spi_transfer(dd, NULL, false);
	return IRQ_HANDLED;
}

static irqreturn_t muc_spi_rdy_isr(int irq, void *data)
{
	struct muc_spi_data *dd = data;

	/* Wake up SPI transfer */
	wake_up(&dd->rdy_wq);

	return IRQ_HANDLED;
}

static int muc_attach(struct notifier_block *nb,
		      unsigned long now_present, void *not_used)
{
	struct muc_spi_data *dd = container_of(nb, struct muc_spi_data, attach_nb);
	struct spi_device *spi = dd->spi;
	int err;

	if (now_present != dd->present) {
		dev_info(&spi->dev, "%s: state = %lu\n", __func__, now_present);

		dd->present = now_present;

		if (now_present) {
			err = devm_request_threaded_irq(&spi->dev, spi->irq,
							NULL, muc_spi_isr,
							IRQF_TRIGGER_LOW |
							IRQF_ONESHOT,
							"muc_spi", dd);
			if (err) {
				dev_err(&spi->dev, "Unable to request irq.\n");
				goto set_missing;
			}

			err = devm_request_irq(&spi->dev,
					       gpio_to_irq(dd->gpio_rdy_n),
					       muc_spi_rdy_isr,
					       IRQF_TRIGGER_RISING |
					       IRQF_TRIGGER_FALLING,
					       "muc_spi_rdy", dd);
			if (err) {
				dev_err(&spi->dev, "Unable to request rdy.\n");
				goto free_irq;
			}

			err = mods_dl_dev_attached(dd->dld);
			if (err) {
				dev_err(&spi->dev, "Error attaching to SVC\n");
				goto free_rdy;
			}
		} else {
			devm_free_irq(&spi->dev, gpio_to_irq(dd->gpio_rdy_n), dd);
			devm_free_irq(&spi->dev, spi->irq, dd);
			mods_dl_dev_detached(dd->dld);
		}
	}
	return NOTIFY_OK;

free_rdy:
	devm_free_irq(&spi->dev, gpio_to_irq(dd->gpio_rdy_n), dd);
free_irq:
	devm_free_irq(&spi->dev, spi->irq, dd);
set_missing:
	dd->present = 0;

	return NOTIFY_OK;
}

/* send message from switch to muc */
static int muc_spi_message_send(struct mods_dl_device *dld,
				   uint8_t *buf, size_t len)
{
	struct spi_msg_hdr *hdr;
	uint16_t *crc;
	struct muc_spi_data *dd = dld_to_dd(dld);
	int remaining = len;
	size_t pl_size = PL_SIZE(dd->pkt_size);
	int packets;

	if (!dd->present)
		return -ENODEV;

	/* Calculate how many packets are required to send whole payload */
	packets = (remaining + pl_size - 1) / pl_size;

	hdr = (struct spi_msg_hdr *)dd->tx_pkt;
	crc = (uint16_t *)&dd->tx_pkt[CRC_NDX(dd->pkt_size)];

	while ((remaining > 0) && (packets > 0)) {
		int this_pl;

		/* Determine the payload size of this packet */
		this_pl = MIN(remaining, pl_size);

		/* Populate the SPI message */
		hdr->bits = HDR_BIT_VALID;
		hdr->bits |= (--packets & HDR_BIT_PKTS);
		memcpy((dd->tx_pkt + sizeof(*hdr)), buf, this_pl);

		*crc = gen_crc16(dd->tx_pkt, CRC_NDX(dd->pkt_size));
		*crc = cpu_to_le16(*crc);

		muc_spi_transfer(dd, dd->tx_pkt, (packets > 0));

		remaining -= this_pl;
		buf += this_pl;
	}

	return 0;
}

static struct mods_dl_driver muc_spi_dl_driver = {
	.message_send		= muc_spi_message_send,
};

static int muc_spi_gpio_init(struct muc_spi_data *dd)
{
	struct device_node *np = dd->spi->dev.of_node;
	int ret;

	dd->gpio_wake_n = of_get_gpio(np, 0);
	dd->gpio_rdy_n = of_get_gpio(np, 1);

	ret = gpio_request_one(dd->gpio_wake_n, GPIOF_OUT_INIT_HIGH,
			       "muc_wake_n");
	if (ret)
		return ret;
	gpio_export(dd->gpio_wake_n, false);

	ret = gpio_request_one(dd->gpio_rdy_n, GPIOF_IN, "muc_rdy_n");
	if (ret)
		return ret;
	gpio_export(dd->gpio_rdy_n, false);

	return 0;
}

static int muc_spi_probe(struct spi_device *spi)
{
	struct muc_spi_data *dd;
	u8 intf_id;
	int ret;

	dev_info(&spi->dev, "%s: enter\n", __func__);

	if (spi->irq < 0) {
		dev_err(&spi->dev, "%s: IRQ not defined\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u8(spi->dev.of_node, "mmi,intf-id", &intf_id)) {
		dev_err(&spi->dev, "Couldn't get mmi,intf-id\n");
		return -EINVAL;
	}

	dd = devm_kzalloc(&spi->dev, sizeof(*dd), GFP_KERNEL);
	if (!dd)
		return -ENOMEM;

	dd->dld = mods_create_dl_device(&muc_spi_dl_driver, &spi->dev, intf_id);
	if (IS_ERR(dd->dld)) {
		dev_err(&spi->dev, "%s: Unable to create greybus host driver.\n",
		        __func__);
		return PTR_ERR(dd->dld);
	}

	dd->dld->dl_priv = (void *)dd;
	dd->spi = spi;
	dd->attach_nb.notifier_call = muc_attach;

	ret = set_packet_size(dd, PKT_SIZE(DEFAULT_PAYLOAD_SZ));
	if (ret)
		goto remove_dl_device;

	muc_spi_gpio_init(dd);
	mutex_init(&dd->mutex);
	init_waitqueue_head(&dd->rdy_wq);

	spi_set_drvdata(spi, dd);

	register_muc_attach_notifier(&dd->attach_nb);

	return 0;

remove_dl_device:
	mods_remove_dl_device(dd->dld);

	return ret;
}

static int muc_spi_remove(struct spi_device *spi)
{
	struct muc_spi_data *dd = spi_get_drvdata(spi);

	dev_info(&spi->dev, "%s: enter\n", __func__);

	if (dd->present)
		mods_dl_dev_detached(dd->dld);

	gpio_free(dd->gpio_wake_n);
	gpio_free(dd->gpio_rdy_n);

	unregister_muc_attach_notifier(&dd->attach_nb);
	mods_remove_dl_device(dd->dld);
	spi_set_drvdata(spi, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_muc_spi_match[] = {
	{ .compatible = "moto,muc_spi", },
	{},
};
#endif

static const struct spi_device_id muc_spi_id[] = {
	{ "muc_spi", 0 },
	{ }
};

static struct spi_driver muc_spi_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "muc_spi",
		.of_match_table = of_match_ptr(of_muc_spi_match),
	},
	.id_table = muc_spi_id,
	.probe = muc_spi_probe,
	.remove  = muc_spi_remove,
};

int __init muc_spi_init(void)
{
	int err;

	err = spi_register_driver(&muc_spi_driver);
	if (err != 0)
		pr_err("muc_spi initialization failed\n");

	return err;
}

void __exit muc_spi_exit(void)
{
	spi_unregister_driver(&muc_spi_driver);
}
