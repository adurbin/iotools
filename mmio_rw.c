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
 * Quick access to physical memory, requires /dev/mem
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
#include <sys/mman.h>
#include "commands.h"

struct mmap_info {
	int fd;
	volatile void *mem;
	int off;
	size_t pgsize;
	size_t length;
	uint64_t addr;
};

struct mmap_file_flags {
	int flags;
};

/* open /dev/mem and mmap the address specified in mmap_addr. return 1 on
 * success, 0 on failure. */
static int
open_mapping(struct mmap_info *mmap_addr, int flags, unsigned bytes)
{
	int prot;

	/* align addr */
	mmap_addr->pgsize = getpagesize();
	mmap_addr->off = mmap_addr->addr & (mmap_addr->pgsize - 1);
	mmap_addr->addr &= ~ ((uint64_t)mmap_addr->pgsize - 1);

	mmap_addr->fd = open("/dev/mem", flags);
	if (mmap_addr->fd < 0) {
		fprintf(stderr, "open(/dev/mem): %s\n", strerror(errno));
		return -1;
	}

	prot = 0;
	if ((flags&O_ACCMODE) == O_RDWR) {
		prot = PROT_READ | PROT_WRITE;
	} else if ((flags & O_ACCMODE) == O_RDONLY) {
		prot = PROT_READ;
	} else if  ((flags & O_ACCMODE) == O_WRONLY) {
		prot = PROT_WRITE;
	}

	/* The length is comprised of the bytes requested including the partial
	 * number of bytes that do not make up a full page. */
	mmap_addr->length = bytes + mmap_addr->off;

	mmap_addr->mem = mmap(NULL, mmap_addr->length, prot, MAP_SHARED,
	           mmap_addr->fd, mmap_addr->addr);

	if (!mmap_addr->mem || mmap_addr->mem == MAP_FAILED) {
		fprintf(stderr, "mmap(/dev/mem): %s\n", strerror(errno));
		close(mmap_addr->fd);
		return -1;
	}

	return 0;
}

static void
close_mapping(struct mmap_info *mmap_addr)
{
	munmap((void *)mmap_addr->mem, mmap_addr->length);
	close(mmap_addr->fd);
}

static int
mmio_read_x(int argc, const char *argv[], const struct cmd_info *info)
{
	int ret;
	data_store data;
	struct mmap_info mmap_addr;
	const struct mmap_file_flags *mmf;

	mmf = info->privdata;

	mmap_addr.addr = strtoull(argv[1], NULL, 0);

	if (open_mapping(&mmap_addr, O_RDONLY | mmf->flags, sizeof(data)) < 0) {
		return -1;
	}

	ret = 0;

	#define DO_READ(mem_, off_, size_)\
		data.u ##size_ = *(typeof(data.u ##size_) *)(mem_ + off_); \
		fprintf(stdout, "0x%0*llx\n", (int)sizeof(data.u ##size_)*2, \
		        (unsigned long long)data.u ##size_)

	switch (get_command_size(info)) {
	case SIZE8:
		DO_READ(mmap_addr.mem, mmap_addr.off, 8);
		break;
	case SIZE16:
		DO_READ(mmap_addr.mem, mmap_addr.off, 16);
		break;
	case SIZE32:
		DO_READ(mmap_addr.mem, mmap_addr.off, 32);
		break;
	case SIZE64:
		if (sizeof(void *) != sizeof(uint64_t)) {
			fprintf(stderr, "warning: 64 bit operations might "
			        "not be atomic on 32 bit builds\n");
		}
		DO_READ(mmap_addr.mem, mmap_addr.off, 64);
		break;
	default:
		fprintf(stderr, "invalid mmio_read parameter\n");
		ret = -1;
	}

	close_mapping(&mmap_addr);

	return ret;
}

static int
mmio_write_x(int argc, const char *argv[], const struct cmd_info *info)
{
	int ret;
	unsigned long ldata;
	data_store data;
	struct mmap_info mmap_addr;

	mmap_addr.addr = strtoull(argv[1], NULL, 0);
	ldata = strtoul(argv[2], NULL, 0);

	if (open_mapping(&mmap_addr, O_RDWR, sizeof(data)) < 0) {
		return -1;
	}

	ret = 0;

	#define DO_WRITE(mem_, off_, size_)\
		data.u ##size_ = (typeof(data.u ##size_))ldata;\
		*(typeof(data.u ##size_) *)(mem_+off_) = data.u ##size_

	switch (get_command_size(info)) {
	case SIZE8:
		DO_WRITE(mmap_addr.mem, mmap_addr.off, 8);
		break;
	case SIZE16:
		DO_WRITE(mmap_addr.mem, mmap_addr.off, 16);
		break;
	case SIZE32:
		DO_WRITE(mmap_addr.mem, mmap_addr.off, 32);
		break;
	case SIZE64:
		if (sizeof(void *) != sizeof(uint64_t)) {
			fprintf(stderr, "warning: 64 bit operations might "
			        "not be atomic on 32 bit builds\n");
		}
		DO_WRITE(mmap_addr.mem, mmap_addr.off, 64);
		break;
	default:
		fprintf(stderr, "invalid mmio_write parameter\n");
		ret = -1;
	}

	close_mapping(&mmap_addr);

	return ret;
}

static int
mmio_dump(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long bytes_to_dump;
	unsigned long bytes_left;
	struct mmap_info mmap_addr;
	uint32_t *addr;
	uint64_t desired_addr;
	int fields_on_line;
	int write_binary;
	const struct mmap_file_flags *mmf = info->privdata;

	desired_addr = strtoull(argv[1], NULL, 0);
	bytes_to_dump = strtoul(argv[2], NULL, 0);

	write_binary = 0;
	if (argc == 4) {
		if (!strcmp(argv[3], "-b")) {
			write_binary = 1;
		} else {
			return -1;
		}
	}

	mmap_addr.addr = desired_addr;
	if (open_mapping(&mmap_addr, O_RDONLY | mmf->flags, bytes_to_dump) < 0) {
		return -1;
	}

	if (write_binary) {
		void *buffer_to_write = (void *)mmap_addr.mem + mmap_addr.off;
		int ret = fwrite(buffer_to_write, bytes_to_dump, 1, stdout);
		close_mapping(&mmap_addr);
		return ret == 1 ? 0 : -1;
	}

	addr = (void *)mmap_addr.mem + mmap_addr.off;
	bytes_left = bytes_to_dump;

	fields_on_line = 0;
	while (bytes_left) {
		int bytes_printed = sizeof(*addr);
		/* Print out the current address. */
		if (!fields_on_line) {
			fprintf(stdout, "0x%016llx:",
			        (unsigned long long)desired_addr);
		}

		/* Print out the leftover bytes. */
		if (bytes_left < sizeof(*addr)) {
			unsigned char *ptr = (unsigned char *)addr;
			fprintf(stdout, " 0x%02x", *ptr);
			/* Adjust the working pointer and the bytes_printed */
			addr = (typeof(addr))++ptr;
			bytes_printed = sizeof(*ptr);
		} else {
			fprintf(stdout, " 0x%08x", *addr);
			addr++;
		}

		/* Keep track of statistics. */
		bytes_left -= bytes_printed;
		desired_addr += bytes_printed;

		/* Default to printing out 4 sets of 32-bit values. */
		fields_on_line = (fields_on_line + 1) % 4;

		/* Handle the new line once we are field 0 again. */
		if (!fields_on_line) {
			fprintf(stdout, "\n");
		}
	}

	/* Print newline if we stopped printing in the middle of a line. */
	if (fields_on_line) {
		fprintf(stdout, "\n");
	}

	close_mapping(&mmap_addr);

	return 0;
}

static struct mmap_file_flags cacheable_access = {};
static struct mmap_file_flags uncacheable_access = { O_SYNC };

MAKE_PREREQ_PARAMS_FIXED_ARGS(rd_params, 2, "<addr>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(wr_params, 3, "<addr> <value>", 0);
MAKE_PREREQ_PARAMS_VAR_ARGS(dump_params, 3, 4, "<addr> <num_bytes> [-b]", 0);

#define MAKE_MMIO_READ_CMD(prefix_, size_, access_) \
	MAKE_CMD_WITH_PARAMS_SIZE(prefix_ ## _read ##size_, &mmio_read_x, \
	                          &access_, &rd_params, &size ##size_)
#define MAKE_MMIO_WRITE_CMD(prefix_, size_, access_) \
	MAKE_CMD_WITH_PARAMS_SIZE(prefix_ ## _write ##size_, &mmio_write_x, \
	                          &access_, &wr_params, &size ##size_)
#define MAKE_MMIO_RW_CMD_PAIR(prefix_, size_, access_) \
	MAKE_MMIO_READ_CMD(prefix_, size_, access_), \
	MAKE_MMIO_WRITE_CMD(prefix_, size_, access_)

#define MAKE_UC_MMIO_RW_CMD_PAIR(size_) \
	MAKE_MMIO_RW_CMD_PAIR(mmio, size_, uncacheable_access)
#define MAKE_WB_MMIO_RW_CMD_PAIR(size_) \
	MAKE_MMIO_RW_CMD_PAIR(mem, size_, cacheable_access)

static const struct cmd_info mmio_cmds[] = {
	MAKE_UC_MMIO_RW_CMD_PAIR(8),
	MAKE_UC_MMIO_RW_CMD_PAIR(16),
	MAKE_UC_MMIO_RW_CMD_PAIR(32),
	MAKE_UC_MMIO_RW_CMD_PAIR(64),
	MAKE_CMD_WITH_PARAMS(mmio_dump, &mmio_dump, &uncacheable_access,
	                     &dump_params)
};

MAKE_CMD_GROUP(MMIO,
               "commands to access uncacheable memory mapped address spaces",
               mmio_cmds);
REGISTER_CMD_GROUP(MMIO);

static const struct cmd_info cacheable_mmio_cmds[] = {
	MAKE_WB_MMIO_RW_CMD_PAIR(8),
	MAKE_WB_MMIO_RW_CMD_PAIR(16),
	MAKE_WB_MMIO_RW_CMD_PAIR(32),
	MAKE_WB_MMIO_RW_CMD_PAIR(64),
	MAKE_CMD_WITH_PARAMS(mem_dump, &mmio_dump, &cacheable_access,
	                     &dump_params)
};

MAKE_CMD_GROUP(MEM,
               "commands to access cacheable memory mapped address spaces",
               cacheable_mmio_cmds);
REGISTER_CMD_GROUP(MEM);
