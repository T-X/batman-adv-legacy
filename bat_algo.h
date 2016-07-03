/* Copyright (C) 2011-2013 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner
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
 */

#ifndef _NET_BATMAN_ADV_BAT_ALGO_H_
#define _NET_BATMAN_ADV_BAT_ALGO_H_

struct netlink_callback;
struct sk_buff;

int batadv_iv_init(void);
int batadv_algo_dump(struct sk_buff *msg, struct netlink_callback *cb);

#endif /* _NET_BATMAN_ADV_BAT_ALGO_H_ */
