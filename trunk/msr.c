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
 * Quick MSR access, requires linux msr driver
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
#include <inttypes.h>
#include "commands.h"
#include "platform.h"

#ifdef ARCH_X86

static int
open_and_seek(int cpu, unsigned long msr, int mode, int *fd)
{
	char dev[512];

	snprintf(dev, sizeof(dev), "/dev/cpu/%d/msr", cpu);
	*fd = open(dev, mode);
	if (*fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", dev, strerror(errno));
		return -1;
	}

	if (lseek(*fd, msr, SEEK_SET) == (off_t)-1) {
		fprintf(stderr, "lseek(%lu): %s\n", msr, strerror(errno));
		close(*fd);
		return -1;
	}

	return 0;
}

static int
rd_msr(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long msr;
	int cpu;
	uint64_t data;
	int fd;

	cpu = strtol(argv[1], NULL, 0);
	msr = strtoul(argv[2], NULL, 0);

	if (open_and_seek(cpu, msr, O_RDONLY, &fd) < 0) {
		return -1;
	}

	if (read(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "read(): %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);

	printf("0x%016" PRIx64 "\n", data);

	return 0;
}

static int
wr_msr(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long msr;
	int cpu;
	uint64_t data;
	int fd;
	int ret = 0;

	cpu = strtol(argv[1], NULL, 0);
	msr = strtoul(argv[2], NULL, 0);
	data = strtoull(argv[3], NULL, 0);

	if (open_and_seek(cpu, msr, O_WRONLY, &fd) < 0) {
		return -1;
	}

	if (write(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "write(): %s\n", strerror(errno));
		ret = -1;
	}

	close(fd);

	return ret;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(rd_params, 3, "<cpu> <msr>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(wr_params, 4, "<cpu> <msr> <data>", 0);

static const struct cmd_info msr_cmds[] = {
	MAKE_CMD_WITH_PARAMS(rdmsr, &rd_msr, NULL, &rd_params),
	MAKE_CMD_WITH_PARAMS(wrmsr, &wr_msr, NULL, &wr_params),
};

MAKE_CMD_GROUP(MSR, "commands to access CPU model specific registers",
               msr_cmds);
REGISTER_CMD_GROUP(MSR);

#endif /* #ifdef ARCH_X86 */
