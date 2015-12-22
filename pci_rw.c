/*
 Copyright 2008 Google Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
 * Quick PCI access, requires linux /proc/bus/pci
 * Tim Hockin <thockin@google.com>
 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>
#include "commands.h"
#include "platform.h"

#define PROCFS_BASE_DIR	"/proc/bus/pci"
#define SYSFS_BASE_DIR	"/sys/bus/pci/devices"

static int
open_device(int segment, int bus, int device, int function, int mode)
{
	static char filename[FILENAME_MAX];
	int fd;

	/* Try sysfs first, but fall back on the proc filesystem. */
	snprintf(filename, sizeof(filename), "%s/%04x:%02x:%02x.%x/config",
		 SYSFS_BASE_DIR, segment, bus, device, function);
	fd = open(filename, mode);

	/* If sysfs failed, try the proc filesystem. */
	if (fd < 0) {
		if (segment == 0) {
			snprintf(filename, sizeof(filename), "%s/%02x/%02x.%x",
				 PROCFS_BASE_DIR, bus, device, function);
		} else {
			snprintf(filename, sizeof(filename),
			         "%s/%04x:%02x/%02x.%x", PROCFS_BASE_DIR,
			         segment, bus, device, function);
		}
		fd = open(filename, mode);
	}

	if (fd < 0) {
		fprintf(stderr,
		        "Unable to open file to access PCI device "
		        "'%04x:%02x:%02x.%x': %s\n",
		        segment, bus, device, function, strerror(errno));
	}

	return fd;
}

static int
pci_read_x(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned int seg;
	unsigned int bus;
	unsigned int dev;
	unsigned int func;
	unsigned int reg;
	data_store data;
	int fd;
	int arg;

	arg = 1;
	if (argc > 5) {
		seg = strtol(argv[arg++], NULL, 0);
	} else {
		seg = 0;
	}
	bus = strtol(argv[arg++], NULL, 0);
	dev = strtoul(argv[arg++], NULL, 0);
	func = strtoul(argv[arg++], NULL, 0);
	reg = strtoul(argv[arg++], NULL, 0);
	

	fd = open_device(seg, bus, dev, func, O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	if (lseek(fd, reg, SEEK_SET) < 0) {
		fprintf(stderr, "lseek(%u): %s\n", reg, strerror(errno));
		return -1;
	}


	#define DO_READ(size_) \
		if (read(fd, &data.u ##size_, sizeof(data.u ##size_)) != \
		    sizeof(data.u ##size_)) { \
		    fprintf(stderr, "read(): %s\n", strerror(errno)); \
		    return -1; \
		} \
		data.u ##size_ = le_to_host_##size_(data.u ##size_); \
		fprintf(stdout, "0x%0*x\n", (int)sizeof(data.u ##size_)*2, \
		       data.u ##size_)

	switch(get_command_size(info)) {
	case SIZE8:
		DO_READ(8);
		break;
	case SIZE16:
		DO_READ(16);
		break;
	case SIZE32:
		DO_READ(32);
		break;
	default:
		fprintf(stderr, "invalid pci_read entry\n");
		return -1;
	}

	close(fd);

	return 0;
}

static int
pci_write_x(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned int seg;
	unsigned int bus;
	unsigned int dev;
	unsigned int func;
	unsigned int reg;
	unsigned long ldata;
	data_store data;
	int fd;
	int arg;

	arg = 1;
	if (argc > 6) {
		seg = strtol(argv[arg++], NULL, 0);
	} else {
		seg = 0;
	}
	bus = strtol(argv[arg++], NULL, 0);
	dev = strtoul(argv[arg++], NULL, 0);
	func = strtoul(argv[arg++], NULL, 0);
	reg = strtoul(argv[arg++], NULL, 0);
	ldata = strtoul(argv[arg++], NULL, 0);

	if ((fd = open_device(seg, bus, dev, func, O_WRONLY)) < 0 ) {
		return -1;
	}

	if (lseek(fd, reg, SEEK_SET) < 0) {
		fprintf(stderr, "lseek(%u): %s\n", reg, strerror(errno));
		return -1;
	}

#define DO_WRITE(size_) \
	do { \
	data.u ##size_ = host_to_le_##size_((typeof(data.u ##size_))ldata); \
	if (write(fd, &data.u ##size_, sizeof(data.u ##size_)) != \
	    sizeof(data.u ##size_)) { \
		fprintf(stderr, "write(): %s\n", strerror(errno)); \
		return -1;\
	} \
	} while (0)

	switch(get_command_size(info)) {
	case SIZE8:
		DO_WRITE(8);
		break;
	case SIZE16:
		DO_WRITE(16);
		break;
	case SIZE32:
		DO_WRITE(32);
		break;
	default:
		fprintf(stderr, "invalid pci_read entry\n");
		return -1;
	}

	close(fd);

	return 0;
}

static int
pci_list_sysfs(void)
{
	DIR *dir = opendir(SYSFS_BASE_DIR);
	struct dirent *de;

	if (!dir) {
		fprintf(stderr, "opendir(%s): %s\n",
		        SYSFS_BASE_DIR, strerror(errno));
		return -1;
	}
	while ((de = readdir(dir))) {
		int seg, bus, dev, fun;
		int r = sscanf(de->d_name, "%04x:%02x:%02x.%x",
		               &seg, &bus, &dev, &fun);
		if (r == 4) {
			printf("%d %d %d\n", bus, dev, fun);
		}
	}
	closedir(dir);
	return 0;
}

static int
pci_list_procfs(void)
{
	DIR *dir = opendir(PROCFS_BASE_DIR);
	struct dirent *de;

	if (!dir) {
		fprintf(stderr, "opendir(%s): %s\n",
		        PROCFS_BASE_DIR, strerror(errno));
		return -1;
	}
	while ((de = readdir(dir))) {
		int bus;
		int r = sscanf(de->d_name, "%02x", &bus);
		if (r == 1 && de->d_type == DT_DIR) {
			char buf[1024];
			snprintf(buf, sizeof(buf), "%s/%s",
			         PROCFS_BASE_DIR, de->d_name);
			DIR *subdir = opendir(buf);
			struct dirent *subde;

			if (!subdir) {
				fprintf(stderr, "opendir(%s): %s\n",
				        buf, strerror(errno));
				return -1;
			}
			while ((subde = readdir(subdir))) {
				int dev, fun;
				r = sscanf(subde->d_name, "%02x.%x",
				           &dev, &fun);
				if (r == 2) {
					printf("%d %d %d\n", bus, dev, fun);
				}
			}
		}
	}
	return 0;
}

static int
pci_list(int argc, const char *argv[], const struct cmd_info *info)
{
	int ret = pci_list_sysfs();
	if (ret < 0) {
		ret = pci_list_procfs();
	}
	return ret;
}

MAKE_PREREQ_PARAMS_VAR_ARGS(rd_params, 5, 6,
                            "[segment] <bus> <dev> <func> <reg>", 0);
MAKE_PREREQ_PARAMS_VAR_ARGS(wr_params, 6, 7,
                            "[segment] <bus> <dev> <func> <reg> <data>", 0);

#define MAKE_PCI_READ_CMD(size_) \
	MAKE_CMD_WITH_PARAMS_SIZE(pci_read ##size_, &pci_read_x, NULL, \
	                         &rd_params, &size ##size_)
#define MAKE_PCI_WRITE_CMD(size_) \
	MAKE_CMD_WITH_PARAMS_SIZE(pci_write ##size_, &pci_write_x, NULL, \
	                          &wr_params, &size ##size_)
#define MAKE_PCI_RW_CMD_PAIR(size_) \
	MAKE_PCI_READ_CMD(size_), \
	MAKE_PCI_WRITE_CMD(size_)

static const struct cmd_info pci_cmds[] = {
	MAKE_PCI_RW_CMD_PAIR(8),
	MAKE_PCI_RW_CMD_PAIR(16),
	MAKE_PCI_RW_CMD_PAIR(32),
	MAKE_CMD(pci_list, pci_list, NULL),
};

MAKE_CMD_GROUP(PCI, "commands to access PCI registers", pci_cmds);
REGISTER_CMD_GROUP(PCI);
