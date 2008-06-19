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
 * Quick CMOS access, requires linux nvram driver
 * Tim Hockin <thockin@google.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "commands.h"

#define DEVICE		"/dev/nvram"
#define NVRAM_OFFSET	14  /* From the kernel driver. bytes < 14 are RTC */

static int
cmos_rd(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long index;
	uint8_t data;
	int fd;

	index = strtoul(argv[1], NULL, 0);
	if (index < NVRAM_OFFSET) {
		fprintf(stderr, "can't read bytes below %d\n", NVRAM_OFFSET);
		return EXIT_FAILURE;
	}
	index -= NVRAM_OFFSET;

	fd = open(DEVICE, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", DEVICE, strerror(errno));
		return EXIT_FAILURE;
	}

	if (lseek(fd, index, SEEK_SET) < 0) {
		fprintf(stderr, "lseek(%lu): %s\n", index, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	if (read(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "read(): %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	printf("0x%02x\n", data);

	return EXIT_SUCCESS;
}

static int
cmos_wr(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long index;
	unsigned long ldata;
	uint8_t data;
	int fd;

	index = strtoul(argv[1], NULL, 0);
	if (index < NVRAM_OFFSET) {
		fprintf(stderr, "can't read bytes below %d\n", NVRAM_OFFSET);
		return EXIT_FAILURE;
	}
	index -= NVRAM_OFFSET;
	ldata = strtoul(argv[2], NULL, 0);
	data = (typeof(data))ldata;

	fd = open(DEVICE, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", DEVICE, strerror(errno));
		return EXIT_FAILURE;
	}

	if (lseek(fd, index, SEEK_SET) < 0) {
		fprintf(stderr, "lseek(%lu): %s\n", index, strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	if (write(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "write(): %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	return EXIT_SUCCESS;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(cmos_rd_params, 2, "<index>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(cmos_wr_params, 3, "<index> <data>", 0);

static const struct cmd_info cmos_cmds[] = {
	MAKE_CMD_WITH_PARAMS(cmos_read, cmos_rd, NULL, &cmos_rd_params),
	MAKE_CMD_WITH_PARAMS(cmos_write, cmos_wr, NULL, &cmos_wr_params),
};

MAKE_CMD_GROUP(CMOS, "commands to access the CMOS registers", cmos_cmds);
REGISTER_CMD_GROUP(CMOS);
