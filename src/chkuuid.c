/*
	Copyright (C) 2018, 2020 daltomi <daltomi@disroot.org>

	This file is part of chkuuid.

	chkuuid is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	chkuuid is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with chkuuid.  If not, see <http://www.gnu.org/licenses/>.
*/

/* C99 Comp. GNU/Linux */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include <unistd.h>
#include <fstab.h>
#include <libudev.h>
#include <blkid.h>
#include <libmount.h>

#include "config.h"

#ifdef DEBUG
#define TITLE "    chkuuid " VERSION " - Debug"
#else
#define TITLE "chkuuid " VERSION
#endif


/*
 * Colores.
 * */
#ifdef NOCOLOR
#define CLR_ERROR
#define CLR_OK
#define CLR_NONE
#define CLR_TITLE
#define CLR_INFORMATION
#define CLR_NORMAL
#else
#define CLR_ERROR			"\e[0;31m"
#define CLR_OK				"\e[0;32m"
#define CLR_NONE			"\e[1;33m"
#define CLR_TITLE			"\e[30;46m"
#define CLR_INFORMATION		"\e[1;37m"
#define CLR_NORMAL			"\e[0;0m"
#endif


enum UUID_STATE {
	OK,
	BAD,
	OK_NOT_MOUNT,
	NOTFOUND
};

static struct partition {
	enum UUID_STATE state;
	char const* name;
	char const* uuid;
	char const* type;
	char const* mntpoint;
} partition;



static void print_state(void)
{
	printf(CLR_NORMAL "%-12s", partition.name);

	switch (partition.state) {
	case  OK:
		printf("%-40s%-18s", partition.uuid, "OK");
		break;
	case BAD:
		printf(CLR_OK "%-40s" CLR_ERROR "%-18s", partition.uuid, "BAD");
		break;
	case OK_NOT_MOUNT:
		printf("%-40s%-18s", partition.uuid, "OK, NOT MOUNT");
		break;
	case NOTFOUND:
		printf("%-40s%-18s", partition.uuid, "NOT FOUND");
		break;
	default:
		assert(0);
	}

	printf(CLR_NORMAL "%-18s%-18s\n", partition.type, partition.mntpoint);
}


static void read_fstab(void)
{
	char const* const STR_UUID = "UUID=";
	char const* const STR_PARTUUID = "PARTUUID=";

	size_t const LEN_STR_UUID = strlen(STR_UUID);
	size_t const LEN_STR_PARTUUID = strlen(STR_PARTUUID);

	size_t const LEN_STR_PARTITION_UUID = strlen(partition.uuid);

	struct fstab* fs = 0;

	setfsent();

	partition.state = NOTFOUND;

	while ((fs = getfsent())) {

		char const* const spec = fs->fs_spec;

		if (strstr(spec, "LABEL=") || strstr(spec, "/dev/")) {
			continue;
		}

		char const* uuid = 0L;
		char const* partuuid = 0;
		bool const find_uuid = (uuid = strstr(spec, STR_UUID)) || (partuuid = strstr(spec, STR_PARTUUID));

		if (!find_uuid) {
			continue;
		}

		char const* const file = fs->fs_file;
		char const* cmp_uuid = 0;

		if (uuid) {
			cmp_uuid = (uuid + LEN_STR_UUID);

			// remove double quotes
			if (strchr(uuid, '\"')) {
					cmp_uuid = (uuid + LEN_STR_UUID + 1);
					*((char*)strrchr(uuid, '\"')) = '\0';
			}

		} else if (partuuid) {
			cmp_uuid = (partuuid + LEN_STR_PARTUUID);

			// remove double quotes
			if (strchr(partuuid, '\"')) {
					cmp_uuid = (partuuid + LEN_STR_PARTUUID + 1);
					*((char*)strrchr(partuuid, '\"')) = '\0';
			}
		}

		assert(cmp_uuid);
		assert(*cmp_uuid != '\0');

		if (strcmp(partition.mntpoint, "NONE")) {

			if (!strcmp(file, partition.mntpoint) || (!strcmp(file, "none") &&  !strcmp(partition.mntpoint, "swap"))) {

				if (!strncmp(cmp_uuid, partition.uuid, LEN_STR_PARTITION_UUID)) {
					partition.state = OK;
				} else {
					partition.state = BAD;
				}
				break;
			}

		} else {

			if (!strncmp(cmp_uuid, partition.uuid, LEN_STR_PARTITION_UUID)) {
				partition.state = OK_NOT_MOUNT;
				partition.mntpoint = file;
				break;
			}
		}
	}

	endfsent();
}


static bool exists_fstab(void)
{
	bool ret = true;

	if (setfsent() == 0) {
		ret = false;
	}

	endfsent();
	return ret;
}


static void enumerate_uuid_partitions(void)
{
	struct udev* udev = 0;

	if (!(udev = udev_new())) {
		fprintf(stderr, CLR_ERROR "Failed to start udev.\n");
		exit(EXIT_FAILURE);
	}

	bool error = false;
	struct udev_list_entry* entry = 0;
	struct udev_enumerate* enumerate = udev_enumerate_new(udev);

	if (!enumerate) {
		fprintf(stderr, CLR_ERROR "Failed to start udev enumerate.\n");
		udev_unref(udev);
		exit(EXIT_FAILURE);
	}

	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "partition");

	if (udev_enumerate_scan_devices(enumerate) < 0) {
		fprintf(stderr, CLR_ERROR "Failed to scan devices.\n");
		udev_enumerate_unref(enumerate);
		udev_unref(udev);
		exit(EXIT_FAILURE);
	}

	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {

		blkid_probe pr = 0;
		struct libmnt_fs* fs = 0;
		struct libmnt_iter* iter = 0;
		struct libmnt_table* table = 0;
		struct udev_device* dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));

		if (!(partition.name = udev_device_get_devnode(dev))) {
			goto clean;
		}

		if (!(pr = blkid_new_probe_from_filename(partition.name))) {
			/* Error al abrir el dispositivo.
			 * Por ej. un lector SD sin la tarjeta.
			 * */
			goto clean;
		}

		blkid_do_probe(pr);
		blkid_probe_lookup_value(pr, "UUID", &partition.uuid, 0);

		/* Sólo las particiones con uuid.
		 * Esto discrimina entre /dev/sda y /dev/sda1.
		 * */
		if (!partition.uuid) {
			goto clean;
		}

		blkid_probe_lookup_value(pr, "TYPE", &partition.type, 0);

		/* La partition swap no se lista en mtab.*/
		if (!strcmp(partition.type, "swap")) {
			partition.mntpoint = "swap";
			goto swap;
		}


		if (!(table = mnt_new_table())) {
			fprintf(stderr, CLR_ERROR "Error: mnt_new_table.\n");
			error = true;
			goto clean;
		}

		if (!(iter = mnt_new_iter(MNT_ITER_FORWARD))) {
			fprintf(stderr, CLR_ERROR "Error: mnt_new_iter.\n");
			error = true;
			goto clean;
		}

		mnt_table_parse_mtab(table, 0);

		while (mnt_table_next_fs(table, iter, &fs) == 0) {
			/* Sólo particiones.
			 * Esto discrimina entre '/dev/sda1 on /home' y
			 * 'tmpfs on /run/user'.
			 * */
			if (!strcmp(partition.name, mnt_fs_get_source(fs))) {
				partition.mntpoint = mnt_fs_get_target(fs);
				break;
			} else {
				partition.mntpoint = "NONE";
			}
		}
swap:
		read_fstab();
		print_state();
clean:
		if (fs) {
			mnt_unref_fs(fs);
		}

		if (table) {
			mnt_unref_table(table);
		}

		if (iter) {
			mnt_free_iter(iter);
		}

		if (pr) {
			blkid_free_probe(pr);
		}

		if (dev) {
			udev_device_unref(dev);
		}

		partition.name = 0;
		partition.uuid = 0;
		partition.mntpoint = 0;
		partition.type = 0;

		if (error) {
			break;
		}

	} /* udev_list_entry_foreach */


	if (enumerate) {
		udev_enumerate_unref(enumerate);
	}

	if (udev) {
		udev_unref(udev);
	}
}


int main(void)
{
	printf("\n%s\n", "-------------------------------");
	printf("%20s\n", TITLE);
	printf("%s\n\n", "-------------------------------");

	if (geteuid() != 0) {
		fprintf(stderr, CLR_ERROR "Administrator permission are needed.\n");
		exit(EXIT_FAILURE);
	}

	if (!exists_fstab()) {
		fprintf(stderr, CLR_ERROR "Could not open the fstab file.");
		exit(EXIT_FAILURE);
	}

	printf(CLR_INFORMATION "%s\n", "* The following is a list of partitions, with the UUID tag, \n"
					"  which is tried to be found in the fstab file.\n");

	printf(CLR_NORMAL "%-12s%-40s%-18s%-18s%-18s\n", "PART", "UUID", "FSTAB", "TYPE", "MOUNT");
	printf("------------------------------------------------------------------------------------------------\n");

	enumerate_uuid_partitions();

	printf(CLR_NORMAL "%s\n", "End list.");

	return EXIT_SUCCESS;
}
