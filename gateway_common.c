/*
 * Copyright (C) 2009 B.A.T.M.A.N. contributors:
 * Marek Lindner
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

#include "main.h"
#include "gateway_common.h"
#include "gateway_client.h"

atomic_t gw_mode;
atomic_t gw_srv_class;

/* calculates the gateway class from kbit */
void kbit_to_gw_srv_class(int down, int up, long *gw_srv_class)
{
	int mdown = 0, tdown, tup, difference;
	uint8_t sbit, part;

	*gw_srv_class = 0;
	difference = 0x0FFFFFFF;

	/* test all downspeeds */
	for (sbit = 0; sbit < 2; sbit++) {
		for (part = 0; part < 16; part++) {
			tdown = 32 * (sbit + 2) * (1 << part);

			if (abs(tdown - down) < difference) {
				*gw_srv_class = (sbit << 7) + (part << 3);
				difference = abs(tdown - down);
				mdown = tdown;
			}
		}
	}

	/* test all upspeeds */
	difference = 0x0FFFFFFF;

	for (part = 0; part < 8; part++) {
		tup = ((part + 1) * (mdown)) / 8;

		if (abs(tup - up) < difference) {
			*gw_srv_class = (*gw_srv_class & 0xF8) | part;
			difference = abs(tup - up);
		}
	}
}

/* returns the up and downspeeds in kbit, calculated from the class */
void gw_srv_class_to_kbit(uint8_t gw_srv_class, int *down, int *up)
{
	char sbit = (gw_srv_class & 0x80) >> 7;
	char dpart = (gw_srv_class & 0x7C) >> 3;
	char upart = (gw_srv_class & 0x07);

	if (!gw_srv_class) {
		*down = 0;
		*up = 0;
		return;
	}

	*down = 32 * (sbit + 2) * (1 << dpart);
	*up = ((upart + 1) * (*down)) / 8;
}

static bool parse_gw_mode_tok(char *tokptr, long *gw_mode_tmp,
			      char **gw_mode_tmp_str, long *gw_clnt_class_tmp,
			      long *up, long *down)
{
	int ret;
	char *slash_ptr, *tmp_ptr;

	switch (*gw_mode_tmp) {
	case GW_MODE_CLIENT:
		ret = strict_strtol(tokptr, 10, gw_clnt_class_tmp);
		if (ret) {
			printk(KERN_ERR "Client class of gateway mode invalid: %s\n",
			       tokptr);
			return false;
		}

		if (*gw_clnt_class_tmp > TQ_MAX_VALUE) {
			printk(KERN_ERR "Client class of gateway mode greater than %i: %ld\n",
					TQ_MAX_VALUE, *gw_clnt_class_tmp);
			return false;
		}

		break;
	case GW_MODE_SERVER:
		slash_ptr = strchr(tokptr, '/');
		if (slash_ptr)
			*slash_ptr = 0;

		ret = strict_strtol(tokptr, 10, down);
		if (ret) {
			printk(KERN_ERR "Download speed of gateway mode invalid: %s\n",
			       tokptr);
			return false;
		}

		tmp_ptr = tokptr + strlen(tokptr) - 4;

		if ((strlen(tokptr) > 4) &&
			((strncmp(tmp_ptr, "MBit", 4) == 0) ||
			(strncmp(tmp_ptr, "mbit", 4) == 0) ||
			(strncmp(tmp_ptr, "Mbit", 4) == 0)))
			*down *= 1024;

		/* we also got some upload info */
		if (slash_ptr) {
			ret = strict_strtol(slash_ptr + 1, 10, up);
			if (ret) {
				printk(KERN_ERR "Upload speed of gateway mode invalid: %s\n",
				       slash_ptr + 1);
				return false;
			}

			tmp_ptr = slash_ptr + 1 + strlen(slash_ptr + 1) - 4;

			if ((strlen(slash_ptr + 1) > 4) &&
				((strncmp(tmp_ptr, "MBit", 4) == 0) ||
				(strncmp(tmp_ptr, "mbit", 4) == 0) ||
				(strncmp(tmp_ptr, "Mbit", 4) == 0)))
				*up *= 1024;

			*slash_ptr = '/';
		}

		break;
	default:
		if (strcmp(tokptr, GW_MODE_OFF_NAME) == 0) {
			*gw_mode_tmp = GW_MODE_OFF;
			*gw_mode_tmp_str = GW_MODE_OFF_NAME;
		}

		if (strcmp(tokptr, GW_MODE_CLIENT_NAME) == 0) {
			*gw_mode_tmp = GW_MODE_CLIENT;
			*gw_mode_tmp_str = GW_MODE_CLIENT_NAME;
		}

		if (strcmp(tokptr, GW_MODE_SERVER_NAME) == 0) {
			*gw_mode_tmp = GW_MODE_SERVER;
			*gw_mode_tmp_str = GW_MODE_SERVER_NAME;
		}
	}

	return true;
}

ssize_t gw_mode_set(const char __user *userbuffer, size_t count)
{
	char *gw_mode_string, *tokptr, *cp;
	char *gw_mode_curr_str, *gw_mode_tmp_str = NULL;
	int finished, not_copied = 0;
	long gw_mode_curr, gw_mode_tmp = GW_MODE_OFF;
	long gw_srv_class_tmp = 0, gw_clnt_class_tmp = 0, up = 0, down = 0;
	bool ret;

	gw_mode_string = kmalloc(count, GFP_KERNEL);

	if (!gw_mode_string)
		return -ENOMEM;

	not_copied = copy_from_user(gw_mode_string, userbuffer, count);
	gw_mode_string[count - not_copied - 1] = 0;

	tokptr = gw_mode_string;
	gw_mode_curr = atomic_read(&gw_mode);

	for (cp = gw_mode_string, finished = 0; !finished; cp++) {
		switch (*cp) {
		case 0:
			finished = 1;
		case ' ':
		case '\n':
		case '\t':
			*cp = 0;
			ret = parse_gw_mode_tok(tokptr, &gw_mode_tmp,
						&gw_mode_tmp_str,
						&gw_clnt_class_tmp,
						&up, &down);

			if (!ret)
				goto end;

			tokptr = cp + 1;
			break;
		default:
			break;
		}
	}

	if (!gw_mode_tmp_str) {
		printk(KERN_INFO "Gateway mode can only be set to: '%s', '%s' or '%s' - given value: %s\n",
			GW_MODE_OFF_NAME, GW_MODE_CLIENT_NAME,
			GW_MODE_SERVER_NAME, gw_mode_string);
		goto end;
	}

	switch (gw_mode_curr) {
	case GW_MODE_CLIENT:
		gw_mode_curr_str = GW_MODE_CLIENT_NAME;
		break;
	case GW_MODE_SERVER:
		gw_mode_curr_str = GW_MODE_SERVER_NAME;
		break;
	default:
		gw_mode_curr_str = GW_MODE_OFF_NAME;
		break;
	}

	switch (gw_mode_tmp) {
	case GW_MODE_CLIENT:
		if ((gw_mode_tmp == GW_MODE_CLIENT) && (!gw_clnt_class_tmp))
			gw_clnt_class_tmp = 20;

		printk(KERN_INFO "Changing gateway mode from: '%s' to: '%s' (gw_clnt_class: %ld)\n",
			  gw_mode_curr_str, gw_mode_tmp_str,
			  gw_clnt_class_tmp);
		break;
	case GW_MODE_SERVER:
		if (!down)
			down = 2000;

		if (!up)
			up = down / 5;

		kbit_to_gw_srv_class(down, up, &gw_srv_class_tmp);

		/**
		 * the gw class we guessed above might not match the given
		 * speeds, hence we need to calculate it back to show the
		 * number that is going to be propagated
		 **/
		gw_srv_class_to_kbit((uint8_t)gw_srv_class_tmp,
				     (int *)&down, (int *)&up);

		printk(KERN_INFO
			  "Changing gateway mode from: '%s' to: '%s' (gw_srv_class: %ld -> propagating: %ld%s/%ld%s)\n",
			  gw_mode_curr_str, gw_mode_tmp_str,
			  gw_srv_class_tmp,
			  (down > 2048 ? down / 1024 : down),
			  (down > 2048 ? "MBit" : "KBit"),
			  (up > 2048 ? up / 1024 : up),
			  (up > 2048 ? "MBit" : "KBit"));
		break;
	default:
		printk(KERN_INFO "Changing gateway mode from: '%s' to: '%s'\n",
			  gw_mode_curr_str, gw_mode_tmp_str);
		break;
	}

	atomic_set(&gw_mode, gw_mode_tmp);
	atomic_set(&gw_srv_class, gw_srv_class_tmp);
	atomic_set(&gw_clnt_class, gw_clnt_class_tmp);

	if (gw_clnt_class_tmp == 0)
		gw_deselect();

end:
	kfree(gw_mode_string);
	return count;
}