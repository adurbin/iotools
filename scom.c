/*
 Copyright 2013 Google Inc.

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

#if defined(__powerpc64__) || defined(__powerpc__)

/*
 * Quick POWER CPU SCOM register access, requires linux SCOM debugfs support.
 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <glob.h>
#include "commands.h"
#include "platform.h"

static int
open_and_seek(int chip, uint64_t scom, int mode, int *fd)
{
	char dev[512];
	/* Shift scom address to align to 8 byte boundary, mask high bit */
	off64_t offset = (scom & ((1ULL << 63) - 1)) << 3;
	/* Handle high bit (indirect SCOM) by shifting the bit right one bit.
	   File offsets are signed, and this is how the kernel expects us to
	   mangle it.
	   Note we set bit 62 instead of bit 59 because of a bug in the kernel
	   scom.c that shifts the whole value right 3 before looking for
	   bit 59 set.
	*/
	if (scom & (1ULL << 63)) {
		offset |= 1ULL << 62;
	}

	snprintf(dev, sizeof(dev), "/sys/kernel/debug/powerpc/scom/%08x/access",
	         chip);
	*fd = open(dev, mode);
	if (*fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", dev, strerror(errno));
		return -1;
	}

	if (lseek64(*fd, offset, SEEK_SET) == (off_t)-1) {
		fprintf(stderr, "lseek(%jd): %s\n", offset, strerror(errno));
		close(*fd);
		return -1;
	}

	return 0;
}

static int
rd_scom(int argc, const char *argv[], const struct cmd_info *info)
{
	uint64_t scom;
	int chip;
	uint64_t data = 0;
	int fd;

	chip = strtol(argv[1], NULL, 0);
	scom = strtoull(argv[2], NULL, 0);

	if (open_and_seek(chip, scom, O_RDONLY, &fd) < 0) {
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
wr_scom(int argc, const char *argv[], const struct cmd_info *info)
{
	uint64_t scom;
	int chip;
	uint64_t data;
	int fd;
	int ret = 0;

	chip = strtol(argv[1], NULL, 0);
	scom = strtoull(argv[2], NULL, 0);
	data = strtoull(argv[3], NULL, 0);

	if (open_and_seek(chip, scom, O_WRONLY, &fd) < 0) {
		return -1;
	}

	if (write(fd, &data, sizeof(data)) != sizeof(data)) {
		fprintf(stderr, "write(): %s\n", strerror(errno));
		ret = -1;
	}

	close(fd);

	return ret;
}

/* Reads the Processor Identification Register (PIR) for a linux CPU number */
static int
cpu_to_pir(int cpu, uint32_t *pir)
{
	char pir_file[512], pir_str[64];
	int fd;

	snprintf(pir_file, sizeof(pir_file),
	         "/sys/devices/system/cpu/cpu%d/pir", cpu);
	fd = open(pir_file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", pir_file, strerror(errno));
		return -1;
	}

	if (read(fd, pir_str, sizeof(pir_str)) < 0) {
		fprintf(stderr, "read(): %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	*pir = strtoul(pir_str, NULL, 16);
	return 0;
}

/* Converts a PIR to a chip ID using the device tree */
static int
pir_to_chipid(uint32_t pir, uint32_t *chipid)
{
	char cpu_glob_str[512];
	glob_t globbuf;
	int fd;

	snprintf(cpu_glob_str, sizeof(cpu_glob_str),
	         "/proc/device-tree/cpus/*@%x/ibm,chip-id", pir);

	if (glob(cpu_glob_str, 0, NULL, &globbuf) != 0) {
		fprintf(stderr, "glob(\"%s\"): %s\n", cpu_glob_str,
		        strerror(errno));
		return -1;
	}

	fd = open(globbuf.gl_pathv[0], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", globbuf.gl_pathv[0],
		        strerror(errno));
		globfree(&globbuf);
		return -1;
	}
	globfree(&globbuf);

	if (read(fd, chipid, sizeof(*chipid)) < 0) {
		fprintf(stderr, "read(): %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int
cpu_to_chipid(int argc, const char *argv[], const struct cmd_info *info)
{
	int cpu;
	uint32_t chipid, pir;
	int ret = 0;

	cpu = strtoul(argv[1], NULL, 0);

	ret = cpu_to_pir(cpu, &pir);
	if (ret < 0) {
		return ret;
	}

	ret = pir_to_chipid(pir, &chipid);
	if (ret < 0) {
		return ret;
	}

	printf("0x%08x\n", chipid);
	return ret;
}

static int
cpu_to_ex(int argc, const char *argv[], const struct cmd_info *info)
{
	int cpu;
	uint32_t pir, ex;
	int ret = 0;

	cpu = strtoul(argv[1], NULL, 0);

	ret = cpu_to_pir(cpu, &pir);
	if (ret < 0) {
		return ret;
	}

	/* EX number is the 4-bit core ID part of PIR */
	ex = (pir >> 3) & 0xf;

	printf("%d\n", ex);
	return ret;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(rd_params, 3, "<chipid> <scom>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(wr_params, 4, "<chipid> <scom> <data>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(cpu_params, 2, "<cpu>", 0);

static const struct cmd_info scom_cmds[] = {
	MAKE_CMD_WITH_PARAMS(getscom, &rd_scom, NULL, &rd_params),
	MAKE_CMD_WITH_PARAMS(putscom, &wr_scom, NULL, &wr_params),
	MAKE_CMD_WITH_PARAMS(cputochipid, &cpu_to_chipid, NULL, &cpu_params),
	MAKE_CMD_WITH_PARAMS(cputoex, &cpu_to_ex, NULL, &cpu_params),
};

MAKE_CMD_GROUP(SCOM, "commands to access SCOM registers",
               scom_cmds);
REGISTER_CMD_GROUP(SCOM);

#endif  /* defined(__powerpc64__) || defined(__powerpc__) */
