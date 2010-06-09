/*
 * Copyright (C) 2005, 2006
 * Avishay Traeger (avishay@gmail.com)
 * Copyright (C) 2008, 2009
 * Boaz Harrosh <bharrosh@panasas.com>
 *
 * Copyrights for code taken from ext2:
 *     Copyright (C) 1992, 1993, 1994, 1995
 *     Remy Card (card@masi.ibp.fr)
 *     Laboratoire MASI - Institut Blaise Pascal
 *     Universite Pierre et Marie Curie (Paris VI)
 *     from
 *     linux/fs/minix/inode.c
 *     Copyright (C) 1991, 1992  Linus Torvalds
 *
 * This file is part of exofs.
 *
 * exofs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.  Since it is based on ext2, and the only
 * valid version of GPL for the Linux kernel is version 2, the only valid
 * version of GPL for exofs is version 2.
 *
 * exofs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with exofs; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/buffer_head.h>

#include "exofs.h"

static int exofs_release_file(struct inode *inode, struct file *filp)
{
	return 0;
}

static int exofs_file_fsync(struct file *filp, int datasync)
{
	int ret;
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct super_block *sb;

	ret = filemap_write_and_wait(mapping);
	if (ret)
		return ret;

	/* sync the inode attributes */
	ret = write_inode_now(inode, 1);

	/* This is a good place to write the sb */
	/* TODO: Sechedule an sb-sync on create */
	sb = inode->i_sb;
	if (sb->s_dirt)
		exofs_sync_fs(sb, 1);

	return ret;
}

static int exofs_flush(struct file *file, fl_owner_t id)
{
	exofs_file_fsync(file, 1);
	/* TODO: Flush the OSD target */
	return 0;
}

const struct file_operations exofs_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
	.open		= generic_file_open,
	.release	= exofs_release_file,
	.fsync		= exofs_file_fsync,
	.flush		= exofs_flush,
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
};

const struct inode_operations exofs_file_inode_operations = {
	.setattr	= exofs_setattr,
};
