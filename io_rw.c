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

#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include "commands.h"

static int
io_read_x(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long iobase;
	data_store data;
	int ret;

	ret = EXIT_SUCCESS;
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
		ret = EXIT_FAILURE;
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

	ret = EXIT_SUCCESS;
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
		ret = EXIT_FAILURE;
	}

	return ret;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(rd_params, 2, "<io_addr>", 3);
MAKE_PREREQ_PARAMS_FIXED_ARGS(wr_params, 3, "<io_addr> <data>", 3);

#define MAKE_IO_READ_CMD(size_) \
	MAKE_CMD_WITH_PARAMS(io_read ##size_, &io_read_x, &size ##size_, \
	                     &rd_params)
#define MAKE_IO_WRITE_CMD(size_) \
	MAKE_CMD_WITH_PARAMS(io_write ##size_, &io_write_x, &size ##size_, \
	                     &wr_params)
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
