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

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "platform.h"

#ifdef ARCH_X86
#include <sys/io.h>
#else
/* Platform independent IO port access using /dev/port device node */
static const char dev_port[] = "/dev/port";

static int io_in(int iobase, int size, void *data)
{
	int ret, fd;

	fd = open(dev_port, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", dev_port, strerror(errno));
		return -1;
	}

	if (lseek(fd, iobase, SEEK_SET) == (off_t)-1) {
		fprintf(stderr, "lseek(%d): %s\n", iobase, strerror(errno));
		close(fd);
		return -1;
	}

	ret = read(fd, data, size);
	return ret;
}

static uint8_t inb(int iobase)
{
	uint8_t val = 0xff;
	io_in(iobase, sizeof(val), &val);
	return val;
}
static uint16_t inw(int iobase)
{
	uint16_t val = 0xffff;
	io_in(iobase, sizeof(val), &val);
	return val;
}
static uint32_t inl(int iobase)
{
	uint32_t val = 0xffff;
	io_in(iobase, sizeof(val), &val);
	return val;
}

static int io_out(int iobase, int size, void *data)
{
        int ret, fd;

	fd = open(dev_port, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "open(\"%s\"): %s\n", dev_port, strerror(errno));
		return -1;
	}

	if (lseek(fd, iobase, SEEK_SET) == (off_t)-1) {
		fprintf(stderr, "lseek(%d): %s\n", iobase, strerror(errno));
		close(fd);
		return -1;
	}

	ret = write(fd, data, size);
	return ret;
}

static void outb(uint8_t val, int iobase)
{
	io_out(iobase, sizeof(val), &val);
}
static void outw(uint16_t val, int iobase)
{
	io_out(iobase, sizeof(val), &val);
}
static void outl(uint32_t val, int iobase)
{
	io_out(iobase, sizeof(val), &val);
}
#endif  /* #ifdef ARCH_X86 */

static int
io_read_x(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long iobase;
	data_store data;
	int ret;

	ret = 0;
	iobase = strtoul(argv[1], NULL, 0);

	#define in8 inb
	#define in16 inw
	#define in32 inl
	#define DO_READ(addr_, size_) \
		data.u ##size_ = in ##size_(addr_); \
		fprintf(stdout, "0x%0*x\n", \
		        (int)sizeof(data.u ##size_)*2, data.u ##size_)

	switch (get_command_size(info)) {
	case SIZE8:
		DO_READ(iobase, 8);
		break;
	case SIZE16:
		DO_READ(iobase, 16);
		break;
	case SIZE32:
		DO_READ(iobase, 32);
		break;
	default:
		fprintf(stderr, "invalid io_read entry\n");
		ret = -1;
	}

	return ret;
}

static int
io_write_x(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long iobase;
	unsigned long ldata;
	int ret;
	data_store data;

	ret = 0;
	iobase = strtoul(argv[1], NULL, 0);
	ldata = strtoul(argv[2], NULL, 0);

	#define out8 outb
	#define out16 outw
	#define out32 outl
	#define DO_WRITE(addr_, size_) \
		data.u ##size_ = (typeof(data.u ##size_))ldata; \
		out ##size_(data.u ##size_, addr_)

	switch (get_command_size(info)) {
	case SIZE8:
		DO_WRITE(iobase, 8);
		break;
	case SIZE16:
		DO_WRITE(iobase, 16);
		break;
	case SIZE32:
		DO_WRITE(iobase, 32);
		break;
	default:
		fprintf(stderr, "invalid io_write entry\n");
		ret = -1;
	}

	return ret;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(rd_params, 2, "<io_addr>", 3);
MAKE_PREREQ_PARAMS_FIXED_ARGS(wr_params, 3, "<io_addr> <data>", 3);

#define MAKE_IO_READ_CMD(size_) \
	MAKE_CMD_WITH_PARAMS_SIZE(io_read ##size_, &io_read_x, NULL, \
	                          &rd_params, &size ##size_)
#define MAKE_IO_WRITE_CMD(size_) \
	MAKE_CMD_WITH_PARAMS_SIZE(io_write ##size_, &io_write_x, NULL, \
	                          &wr_params, &size ##size_)
#define MAKE_IO_RW_CMD_PAIR(size_) \
	MAKE_IO_READ_CMD(size_), \
	MAKE_IO_WRITE_CMD(size_)

static const struct cmd_info io_cmds[] = {
	MAKE_IO_RW_CMD_PAIR(8),
	MAKE_IO_RW_CMD_PAIR(16),
	MAKE_IO_RW_CMD_PAIR(32),
};

MAKE_CMD_GROUP(IO, "commands to access registers in the IO address space",
               io_cmds);
REGISTER_CMD_GROUP(IO);

