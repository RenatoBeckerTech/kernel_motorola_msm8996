/*
 * Copyright (C) 2007-2009 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include "types.h"

int hna_local_init(void);
void hna_local_add(uint8_t *addr);
int hna_local_fill_buffer(unsigned char *buff, int buff_len);
int hna_local_fill_buffer_text(unsigned char *buff, int buff_len);
void hna_local_purge(struct work_struct *work);
void hna_local_free(void);
int hna_global_init(void);
void hna_global_add_orig(struct orig_node *orig_node, unsigned char *hna_buff,
			 int hna_buff_len);
int hna_global_fill_buffer_text(unsigned char *buff, int buff_len);
void _hna_global_del_orig(struct hna_global_entry *hna_global_entry,
			  char *orig_str);
void hna_global_del_orig(struct orig_node *orig_node, char *message);
void hna_global_free(void);
struct orig_node *transtable_search(uint8_t *addr);

extern spinlock_t hna_local_hash_lock;
extern struct hashtable_t *hna_local_hash;
extern atomic_t hna_local_changed;
