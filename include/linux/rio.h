/*
 * RapidIO interconnect services
 * (RapidIO Interconnect Specification, http://www.rapidio.org)
 *
 * Copyright 2005 MontaVista Software, Inc.
 * Matt Porter <mporter@kernel.crashing.org>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef LINUX_RIO_H
#define LINUX_RIO_H

#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/rio_regs.h>
#ifdef CONFIG_RAPIDIO_DMA_ENGINE
#include <linux/dmaengine.h>
#endif

#define RIO_NO_HOPCOUNT		-1
#define RIO_INVALID_DESTID	0xffff

#define RIO_MAX_MPORTS		8
#define RIO_MAX_MPORT_RESOURCES	16
#define RIO_MAX_DEV_RESOURCES	16

#define RIO_GLOBAL_TABLE	0xff	/* Indicates access of a switch's
					   global routing table if it
					   has multiple (or per port)
					   tables */

#define RIO_INVALID_ROUTE	0xff	/* Indicates that a route table
					   entry is invalid (no route
					   exists for the device ID) */

#define RIO_MAX_ROUTE_ENTRIES(size)	(size ? (1 << 16) : (1 << 8))
#define RIO_ANY_DESTID(size)		(size ? 0xffff : 0xff)

#define RIO_MAX_MBOX		4
#define RIO_MAX_MSG_SIZE	0x1000

/*
 * Error values that may be returned by RIO functions.
 */
#define RIO_SUCCESSFUL			0x00
#define RIO_BAD_SIZE			0x81

/*
 * For RIO devices, the region numbers are assigned this way:
 *
 *	0	RapidIO outbound doorbells
 *      1-15	RapidIO memory regions
 *
 * For RIO master ports, the region number are assigned this way:
 *
 *	0	RapidIO inbound doorbells
 *	1	RapidIO inbound mailboxes
 *	1	RapidIO outbound mailboxes
 */
#define RIO_DOORBELL_RESOURCE	0
#define RIO_INB_MBOX_RESOURCE	1
#define RIO_OUTB_MBOX_RESOURCE	2

#define RIO_PW_MSG_SIZE		64

/*
 * A component tag value (stored in the component tag CSR) is used as device's
 * unique identifier assigned during enumeration. Besides being used for
 * identifying switches (which do not have device ID register), it also is used
 * by error management notification and therefore has to be assigned
 * to endpoints as well.
 */
#define RIO_CTAG_RESRVD	0xfffe0000 /* Reserved */
#define RIO_CTAG_UDEVID	0x0001ffff /* Unique device identifier */

extern struct bus_type rio_bus_type;
extern struct device rio_bus;
extern struct list_head rio_devices;	/* list of all devices */

struct rio_mport;
struct rio_dev;
union rio_pw_msg;

/**
 * struct rio_switch - RIO switch info
 * @node: Node in global list of switches
 * @switchid: Switch ID that is unique across a network
 * @route_table: Copy of switch routing table
 * @port_ok: Status of each port (one bit per port) - OK=1 or UNINIT=0
 * @add_entry: Callback for switch-specific route add function
 * @get_entry: Callback for switch-specific route get function
 * @clr_table: Callback for switch-specific clear route table function
 * @set_domain: Callback for switch-specific domain setting function
 * @get_domain: Callback for switch-specific domain get function
 * @em_init: Callback for switch-specific error management init function
 * @em_handle: Callback for switch-specific error management handler function
 * @sw_sysfs: Callback that initializes switch-specific sysfs attributes
 * @nextdev: Array of per-port pointers to the next attached device
 */
struct rio_switch {
	struct list_head node;
	u16 switchid;
	u8 *route_table;
	u32 port_ok;
	int (*add_entry) (struct rio_mport *mport, u16 destid, u8 hopcount,
			  u16 table, u16 route_destid, u8 route_port);
	int (*get_entry) (struct rio_mport *mport, u16 destid, u8 hopcount,
			  u16 table, u16 route_destid, u8 *route_port);
	int (*clr_table) (struct rio_mport *mport, u16 destid, u8 hopcount,
			  u16 table);
	int (*set_domain) (struct rio_mport *mport, u16 destid, u8 hopcount,
			   u8 sw_domain);
	int (*get_domain) (struct rio_mport *mport, u16 destid, u8 hopcount,
			   u8 *sw_domain);
	int (*em_init) (struct rio_dev *dev);
	int (*em_handle) (struct rio_dev *dev, u8 swport);
	int (*sw_sysfs) (struct rio_dev *dev, int create);
	struct rio_dev *nextdev[0];
};

/**
 * struct rio_dev - RIO device info
 * @global_list: Node in list of all RIO devices
 * @net_list: Node in list of RIO devices in a network
 * @net: Network this device is a part of
 * @did: Device ID
 * @vid: Vendor ID
 * @device_rev: Device revision
 * @asm_did: Assembly device ID
 * @asm_vid: Assembly vendor ID
 * @asm_rev: Assembly revision
 * @efptr: Extended feature pointer
 * @pef: Processing element features
 * @swpinfo: Switch port info
 * @src_ops: Source operation capabilities
 * @dst_ops: Destination operation capabilities
 * @comp_tag: RIO component tag
 * @phys_efptr: RIO device extended features pointer
 * @em_efptr: RIO Error Management features pointer
 * @dma_mask: Mask of bits of RIO address this device implements
 * @driver: Driver claiming this device
 * @dev: Device model device
 * @riores: RIO resources this device owns
 * @pwcback: port-write callback function for this device
 * @destid: Network destination ID (or associated destid for switch)
 * @hopcount: Hopcount to this device
 * @prev: Previous RIO device connected to the current one
 * @rswitch: struct rio_switch (if valid for this device)
 */
struct rio_dev {
	struct list_head global_list;	/* node in list of all RIO devices */
	struct list_head net_list;	/* node in per net list */
	struct rio_net *net;	/* RIO net this device resides in */
	u16 did;
	u16 vid;
	u32 device_rev;
	u16 asm_did;
	u16 asm_vid;
	u16 asm_rev;
	u16 efptr;
	u32 pef;
	u32 swpinfo;
	u32 src_ops;
	u32 dst_ops;
	u32 comp_tag;
	u32 phys_efptr;
	u32 em_efptr;
	u64 dma_mask;
	struct rio_driver *driver;	/* RIO driver claiming this device */
	struct device dev;	/* LDM device structure */
	struct resource riores[RIO_MAX_DEV_RESOURCES];
	int (*pwcback) (struct rio_dev *rdev, union rio_pw_msg *msg, int step);
	u16 destid;
	u8 hopcount;
	struct rio_dev *prev;
	struct rio_switch rswitch[0];	/* RIO switch info */
};

#define rio_dev_g(n) list_entry(n, struct rio_dev, global_list)
#define rio_dev_f(n) list_entry(n, struct rio_dev, net_list)
#define	to_rio_dev(n) container_of(n, struct rio_dev, dev)
#define sw_to_rio_dev(n) container_of(n, struct rio_dev, rswitch[0])

/**
 * struct rio_msg - RIO message event
 * @res: Mailbox resource
 * @mcback: Message event callback
 */
struct rio_msg {
	struct resource *res;
	void (*mcback) (struct rio_mport * mport, void *dev_id, int mbox, int slot);
};

/**
 * struct rio_dbell - RIO doorbell event
 * @node: Node in list of doorbell events
 * @res: Doorbell resource
 * @dinb: Doorbell event callback
 * @dev_id: Device specific pointer to pass on event
 */
struct rio_dbell {
	struct list_head node;
	struct resource *res;
	void (*dinb) (struct rio_mport *mport, void *dev_id, u16 src, u16 dst, u16 info);
	void *dev_id;
};

enum rio_phy_type {
	RIO_PHY_PARALLEL,
	RIO_PHY_SERIAL,
};

/**
 * struct rio_mport - RIO master port info
 * @dbells: List of doorbell events
 * @node: Node in global list of master ports
 * @nnode: Node in network list of master ports
 * @iores: I/O mem resource that this master port interface owns
 * @riores: RIO resources that this master port interfaces owns
 * @inb_msg: RIO inbound message event descriptors
 * @outb_msg: RIO outbound message event descriptors
 * @host_deviceid: Host device ID associated with this master port
 * @ops: configuration space functions
 * @id: Port ID, unique among all ports
 * @index: Port index, unique among all port interfaces of the same type
 * @sys_size: RapidIO common transport system size
 * @phy_type: RapidIO phy type
 * @phys_efptr: RIO port extended features pointer
 * @name: Port name string
 * @priv: Master port private data
 */
struct rio_mport {
	struct list_head dbells;	/* list of doorbell events */
	struct list_head node;	/* node in global list of ports */
	struct list_head nnode;	/* node in net list of ports */
	struct resource iores;
	struct resource riores[RIO_MAX_MPORT_RESOURCES];
	struct rio_msg inb_msg[RIO_MAX_MBOX];
	struct rio_msg outb_msg[RIO_MAX_MBOX];
	int host_deviceid;	/* Host device ID */
	struct rio_ops *ops;	/* low-level architecture-dependent routines */
	unsigned char id;	/* port ID, unique among all ports */
	unsigned char index;	/* port index, unique among all port
				   interfaces of the same type */
	unsigned int sys_size;	/* RapidIO common transport system size.
				 * 0 - Small size. 256 devices.
				 * 1 - Large size, 65536 devices.
				 */
	enum rio_phy_type phy_type;	/* RapidIO phy type */
	u32 phys_efptr;
	unsigned char name[40];
	void *priv;		/* Master port private data */
#ifdef CONFIG_RAPIDIO_DMA_ENGINE
	struct dma_device	dma;
#endif
};

/**
 * struct rio_net - RIO network info
 * @node: Node in global list of RIO networks
 * @devices: List of devices in this network
 * @mports: List of master ports accessing this network
 * @hport: Default port for accessing this network
 * @id: RIO network ID
 */
struct rio_net {
	struct list_head node;	/* node in list of networks */
	struct list_head devices;	/* list of devices in this net */
	struct list_head mports;	/* list of ports accessing net */
	struct rio_mport *hport;	/* primary port for accessing net */
	unsigned char id;	/* RIO network ID */
};

/* Definitions used by switch sysfs initialization callback */
#define RIO_SW_SYSFS_CREATE	1	/* Create switch attributes */
#define RIO_SW_SYSFS_REMOVE	0	/* Remove switch attributes */

/* Low-level architecture-dependent routines */

/**
 * struct rio_ops - Low-level RIO configuration space operations
 * @lcread: Callback to perform local (master port) read of config space.
 * @lcwrite: Callback to perform local (master port) write of config space.
 * @cread: Callback to perform network read of config space.
 * @cwrite: Callback to perform network write of config space.
 * @dsend: Callback to send a doorbell message.
 * @pwenable: Callback to enable/disable port-write message handling.
 * @open_outb_mbox: Callback to initialize outbound mailbox.
 * @close_outb_mbox: Callback to shut down outbound mailbox.
 * @open_inb_mbox: Callback to initialize inbound mailbox.
 * @close_inb_mbox: Callback to	shut down inbound mailbox.
 * @add_outb_message: Callback to add a message to an outbound mailbox queue.
 * @add_inb_buffer: Callback to	add a buffer to an inbound mailbox queue.
 * @get_inb_message: Callback to get a message from an inbound mailbox queue.
 */
struct rio_ops {
	int (*lcread) (struct rio_mport *mport, int index, u32 offset, int len,
			u32 *data);
	int (*lcwrite) (struct rio_mport *mport, int index, u32 offset, int len,
			u32 data);
	int (*cread) (struct rio_mport *mport, int index, u16 destid,
			u8 hopcount, u32 offset, int len, u32 *data);
	int (*cwrite) (struct rio_mport *mport, int index, u16 destid,
			u8 hopcount, u32 offset, int len, u32 data);
	int (*dsend) (struct rio_mport *mport, int index, u16 destid, u16 data);
	int (*pwenable) (struct rio_mport *mport, int enable);
	int (*open_outb_mbox)(struct rio_mport *mport, void *dev_id,
			      int mbox, int entries);
	void (*close_outb_mbox)(struct rio_mport *mport, int mbox);
	int  (*open_inb_mbox)(struct rio_mport *mport, void *dev_id,
			     int mbox, int entries);
	void (*close_inb_mbox)(struct rio_mport *mport, int mbox);
	int  (*add_outb_message)(struct rio_mport *mport, struct rio_dev *rdev,
				 int mbox, void *buffer, size_t len);
	int (*add_inb_buffer)(struct rio_mport *mport, int mbox, void *buf);
	void *(*get_inb_message)(struct rio_mport *mport, int mbox);
};

#define RIO_RESOURCE_MEM	0x00000100
#define RIO_RESOURCE_DOORBELL	0x00000200
#define RIO_RESOURCE_MAILBOX	0x00000400

#define RIO_RESOURCE_CACHEABLE	0x00010000
#define RIO_RESOURCE_PCI	0x00020000

#define RIO_RESOURCE_BUSY	0x80000000

/**
 * struct rio_driver - RIO driver info
 * @node: Node in list of drivers
 * @name: RIO driver name
 * @id_table: RIO device ids to be associated with this driver
 * @probe: RIO device inserted
 * @remove: RIO device removed
 * @suspend: RIO device suspended
 * @resume: RIO device awakened
 * @enable_wake: RIO device enable wake event
 * @driver: LDM driver struct
 *
 * Provides info on a RIO device driver for insertion/removal and
 * power management purposes.
 */
struct rio_driver {
	struct list_head node;
	char *name;
	const struct rio_device_id *id_table;
	int (*probe) (struct rio_dev * dev, const struct rio_device_id * id);
	void (*remove) (struct rio_dev * dev);
	int (*suspend) (struct rio_dev * dev, u32 state);
	int (*resume) (struct rio_dev * dev);
	int (*enable_wake) (struct rio_dev * dev, u32 state, int enable);
	struct device_driver driver;
};

#define	to_rio_driver(drv) container_of(drv,struct rio_driver, driver)

/**
 * struct rio_device_id - RIO device identifier
 * @did: RIO device ID
 * @vid: RIO vendor ID
 * @asm_did: RIO assembly device ID
 * @asm_vid: RIO assembly vendor ID
 *
 * Identifies a RIO device based on both the device/vendor IDs and
 * the assembly device/vendor IDs.
 */
struct rio_device_id {
	u16 did, vid;
	u16 asm_did, asm_vid;
};

/**
 * struct rio_switch_ops - Per-switch operations
 * @vid: RIO vendor ID
 * @did: RIO device ID
 * @init_hook: Callback that performs switch device initialization
 *
 * Defines the operations that are necessary to initialize/control
 * a particular RIO switch device.
 */
struct rio_switch_ops {
	u16 vid, did;
	int (*init_hook) (struct rio_dev *rdev, int do_enum);
};

union rio_pw_msg {
	struct {
		u32 comptag;	/* Component Tag CSR */
		u32 errdetect;	/* Port N Error Detect CSR */
		u32 is_port;	/* Implementation specific + PortID */
		u32 ltlerrdet;	/* LTL Error Detect CSR */
		u32 padding[12];
	} em;
	u32 raw[RIO_PW_MSG_SIZE/sizeof(u32)];
};

#ifdef CONFIG_RAPIDIO_DMA_ENGINE

/**
 * enum rio_write_type - RIO write transaction types used in DMA transfers
 *
 * Note: RapidIO specification defines write (NWRITE) and
 * write-with-response (NWRITE_R) data transfer operations.
 * Existing DMA controllers that service RapidIO may use one of these operations
 * for entire data transfer or their combination with only the last data packet
 * requires response.
 */
enum rio_write_type {
	RDW_DEFAULT,		/* default method used by DMA driver */
	RDW_ALL_NWRITE,		/* all packets use NWRITE */
	RDW_ALL_NWRITE_R,	/* all packets use NWRITE_R */
	RDW_LAST_NWRITE_R,	/* last packet uses NWRITE_R, others - NWRITE */
};

struct rio_dma_ext {
	u16 destid;
	u64 rio_addr;	/* low 64-bits of 66-bit RapidIO address */
	u8  rio_addr_u;  /* upper 2-bits of 66-bit RapidIO address */
	enum rio_write_type wr_type; /* preferred RIO write operation type */
};

struct rio_dma_data {
	/* Local data (as scatterlist) */
	struct scatterlist	*sg;	/* I/O scatter list */
	unsigned int		sg_len;	/* size of scatter list */
	/* Remote device address (flat buffer) */
	u64 rio_addr;	/* low 64-bits of 66-bit RapidIO address */
	u8  rio_addr_u;  /* upper 2-bits of 66-bit RapidIO address */
	enum rio_write_type wr_type; /* preferred RIO write operation type */
};

static inline struct rio_mport *dma_to_mport(struct dma_device *ddev)
{
	return container_of(ddev, struct rio_mport, dma);
}
#endif /* CONFIG_RAPIDIO_DMA_ENGINE */

/* Architecture and hardware-specific functions */
extern int rio_register_mport(struct rio_mport *);
extern int rio_open_inb_mbox(struct rio_mport *, void *, int, int);
extern void rio_close_inb_mbox(struct rio_mport *, int);
extern int rio_open_outb_mbox(struct rio_mport *, void *, int, int);
extern void rio_close_outb_mbox(struct rio_mport *, int);

#endif				/* LINUX_RIO_H */
