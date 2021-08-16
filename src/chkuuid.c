/*
	Copyright (C) 2018, 2021 daltomi <daltomi@disroot.org>

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


static bool read_fstab(void)
{
	char const* const STR_UUID = "UUID=";
	char const* const STR_PARTUUID = "PARTUUID=";

	size_t const LEN_STR_UUID = strlen(STR_UUID);
	size_t const LEN_STR_PARTUUID = strlen(STR_PARTUUID);

	size_t const LEN_STR_PARTITION_UUID = strlen(partition.uuid);

	struct fstab* fs = NULL;

	if (0 == setfsent()) {
		fprintf(stderr, CLR_ERROR "Could not open the 'fstab' file.\n" CLR_NORMAL);
		return false;
	}


	partition.state = NOTFOUND;

	while ((fs = getfsent())) {

		char const* const spec = fs->fs_spec;

		assert(NULL != spec);

		if ((NULL != strstr(spec, "LABEL=")) || (NULL != strstr(spec, "/dev/"))) {
			continue;
		}

		char const* uuid = NULL;
		char const* partuuid = NULL;
		bool const find_uuid = (NULL != (uuid = strstr(spec, STR_UUID))) ||
				(NULL != (partuuid = strstr(spec, STR_PARTUUID)));

		if (!find_uuid) {
			continue;
		}

		char const* const file = fs->fs_file;
		assert(NULL != file);
		char const* cmp_uuid = NULL;

		if (NULL != uuid) {
			cmp_uuid = (uuid + LEN_STR_UUID);

			// remove double quotes
			if (NULL != strchr(uuid, '\"')) {
					cmp_uuid = (uuid + LEN_STR_UUID + 1);
					*((char*)strrchr(uuid, '\"')) = '\0';
			}

		} else if (NULL != partuuid) {
			cmp_uuid = (partuuid + LEN_STR_PARTUUID);

			// remove double quotes
			if (NULL != strchr(partuuid, '\"')) {
					cmp_uuid = (partuuid + LEN_STR_PARTUUID + 1);
					*((char*)strrchr(partuuid, '\"')) = '\0';
			}
		}

		assert(NULL != cmp_uuid);
		assert(*cmp_uuid != '\0');

		if (0 != strcmp(partition.mntpoint, "NONE")) {

			if (0 == strcmp(file, partition.mntpoint) ||
					(0 == strcmp(file, "none") &&  0 == strcmp(partition.mntpoint, "swap"))) {

				if (0 == strncmp(cmp_uuid, partition.uuid, LEN_STR_PARTITION_UUID)) {
					partition.state = OK;
				} else {
					partition.state = BAD;
				}
				break;
			}

		} else {

			if (0 == strncmp(cmp_uuid, partition.uuid, LEN_STR_PARTITION_UUID)) {
				partition.state = OK_NOT_MOUNT;
				partition.mntpoint = file;
				break;
			}
		}
	}

	endfsent();
	return true;
}


static bool exists_fstab(void)
{
	bool ret = true;

	if (0 == setfsent()) {
		ret = false;
	}

	endfsent();
	return ret;
}


static void enumerate_uuid_partitions(void)
{
	struct udev* udev = NULL;

	if (!(udev = udev_new())) {
		fprintf(stderr, CLR_ERROR "Failed to start udev.\n" CLR_NORMAL);
		exit(EXIT_FAILURE);
	}

	bool error = false;
	struct udev_list_entry* entry = NULL;
	struct udev_enumerate* enumerate = udev_enumerate_new(udev);

	if (NULL == enumerate) {
		fprintf(stderr, CLR_ERROR "Failed to start udev enumerate.\n" CLR_NORMAL);
		(void)udev_unref(udev);
		exit(EXIT_FAILURE);
	}

#ifdef DEBUG
	int udev_ret = 0;
	udev_ret = udev_enumerate_add_match_subsystem(enumerate, "block");
	assert(0 == udev_ret);
	udev_ret = udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
	assert(0 == udev_ret);
	udev_ret = udev_enumerate_add_match_property(enumerate, "DEVTYPE", "partition");
	assert(0 == udev_ret);
#else
	udev_enumerate_add_match_subsystem(enumerate, "block");
	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "disk");
	udev_enumerate_add_match_property(enumerate, "DEVTYPE", "partition");
#endif

	if (udev_enumerate_scan_devices(enumerate) < 0) {
		fprintf(stderr, CLR_ERROR "Failed to scan devices.\n" CLR_NORMAL);
		udev_enumerate_unref(enumerate);
		(void)udev_unref(udev);
		exit(EXIT_FAILURE);
	}

	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {

		blkid_probe pr = NULL;
		struct libmnt_fs* fs = NULL;
		struct libmnt_iter* iter = NULL;
		struct libmnt_table* table = NULL;
		struct udev_device* dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));

		if (NULL == (partition.name = udev_device_get_devnode(dev))) {
			goto clean;
		}

		if (NULL == (pr = blkid_new_probe_from_filename(partition.name))) {
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
		if (NULL == partition.uuid) {
			goto clean;
		}

		blkid_probe_lookup_value(pr, "TYPE", &partition.type, 0);

		/* La partition swap no se lista en mtab.*/
		if (0 == strcmp(partition.type, "swap")) {
			partition.mntpoint = "swap";
			goto swap;
		}


		if (NULL == (table = mnt_new_table())) {
			fprintf(stderr, CLR_ERROR "Error: mnt_new_table.\n" CLR_NORMAL);
			error = true;
			goto clean;
		}

		if (NULL == (iter = mnt_new_iter(MNT_ITER_FORWARD))) {
			fprintf(stderr, CLR_ERROR "Error: mnt_new_iter.\n" CLR_NORMAL);
			error = true;
			goto clean;
		}

		mnt_table_parse_mtab(table, 0);

		while (0 == mnt_table_next_fs(table, iter, &fs)) {
			/* Sólo particiones.
			 * Esto discrimina entre '/dev/sda1 on /home' y
			 * 'tmpfs on /run/user'.
			 * */
			if (0 == strcmp(partition.name, mnt_fs_get_source(fs))) {
				partition.mntpoint = mnt_fs_get_target(fs);
				assert(NULL != partition.mntpoint);
				break;
			} else {
				partition.mntpoint = "NONE";
			}
		}
swap:
		error = !read_fstab();

		if (!error) {
			print_state();
		}
clean:
		if (NULL != fs) {
			mnt_unref_fs(fs);
		}

		if (NULL != table) {
			mnt_unref_table(table);
		}

		if (NULL != iter) {
			mnt_free_iter(iter);
		}

		if (NULL != pr) {
			blkid_free_probe(pr);
		}

		if (NULL != dev) {
			udev_device_unref(dev);
		}

		partition.name = NULL;
		partition.uuid = NULL;
		partition.mntpoint = NULL;
		partition.type = NULL;

		if (error) {
			break;
		}

	} /* udev_list_entry_foreach */


	if (NULL != enumerate) {
		udev_enumerate_unref(enumerate);
	}

	if (NULL != udev) {
		(void)udev_unref(udev);
	}
}


int main(void)
{
	printf("\n%s\n", "-------------------------------");
	printf("%20s\n", TITLE);
	printf("%s\n\n", "-------------------------------");

	if (0 != geteuid()) {
		fprintf(stderr, CLR_ERROR "Administrator permission are needed.\n" CLR_NORMAL);
		exit(EXIT_FAILURE);
	}

	if (!exists_fstab()) {
		fprintf(stderr, CLR_ERROR "Could not open the fstab file." CLR_NORMAL);
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
