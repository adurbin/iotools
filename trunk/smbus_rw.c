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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "commands.h"
#include <linux/i2c-dev.h>

enum SMBUS_SIZE
{
	SMBUS_SIZE_8  = SIZE8,
	SMBUS_SIZE_16 = SIZE16,
	SMBUS_SIZE_BLOCK
};

typedef union {
	data_store fixed;
	uint8_t   array[32];
} SMBUS_DTYPE;

struct smbus_op_params {
	int fd;
	uint8_t reg;
	uint8_t i2c_bus;
	uint8_t address;
	int len;
	SMBUS_DTYPE data;
};

struct smbus_op {
	int size;
	int (*perform_op)(struct smbus_op_params *params, const struct smbus_op *op);
};

/* setup a file descriptor for i2c slave access */
static int
open_i2c_slave(unsigned char i2c_bus, unsigned char slave_address)
{
	char devfile[15];
	int fd;

	sprintf(devfile, "/dev/i2c-%d", i2c_bus);
	fd = open(devfile, O_RDWR);
	if (fd < 0) {
		printf("Couldn't open i2c device file: %s\n", strerror(errno));
		return -1;
	}

	if (ioctl(fd, I2C_SLAVE, slave_address) < 0) {
		printf("Could not attach to i2c bus %d slave address %d: %s\n",
		       i2c_bus, slave_address, strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

static int
parse_uint8(const char *arg, uint8_t *ret)
{
	unsigned long ldata;
	char *end;

	ldata = strtoul(arg, &end, 0);
	if (ldata == LONG_MAX || *end != '\0') {
		return -1;
	}
	*ret = (uint8_t)ldata;
	return 0;
}

/* smbus_prologue is responsible for doing the common bits for both smbus read
 * and write. It will parse the comand line arguments and open the appropriate
 * i2c device. It returns 1 on success, 0 on failure. */
static int
smbus_prologue(const char *argv[], struct smbus_op_params *params)
{
	if (parse_uint8(argv[1], &params->i2c_bus)) {
		fprintf(stderr, "invalid adapter value\n");
		return -1;
	}
	if (parse_uint8(argv[2], &params->address)) {
		fprintf(stderr, "invalid address value\n");
		return -1;
	}
	if (parse_uint8(argv[3], &params->reg)) {
		fprintf(stderr, "invalid register value\n");
		return -1;
	}

	params->fd = open_i2c_slave(params->i2c_bus, params->address);
	if (params->fd < 0) {
		fprintf(stderr, "can't get slave\n");
		return -1;
	}

	return 0;
}

static int
smbus_read(int argc, const char *argv[], const struct cmd_info *info)
{
	int ret;
	struct smbus_op_params params;
	const struct smbus_op *op =
		(const struct smbus_op *)info->privdata;

	if (smbus_prologue(argv, &params) < 0) {
		return -1;
	}

	ret = op->perform_op(&params, op);

	close(params.fd);

	return ret;
}

static int
smbus_read_op(struct smbus_op_params *params, const struct smbus_op *op)
{
	int result;

	switch (op->size) {
	case SMBUS_SIZE_8:
		result = i2c_smbus_read_byte_data(params->fd, params->reg);
		params->data.fixed.u8 = result;
		break;
	case SMBUS_SIZE_16:
		result = i2c_smbus_read_word_data(params->fd, params->reg);
		params->data.fixed.u16 = result;
		break;
	case SMBUS_SIZE_BLOCK:
		result = i2c_smbus_read_block_data(params->fd,
		             params->reg, params->data.array) - 1;
		break;
	default:
		fprintf(stderr, "Illegal SMBus size for read operation.\n");
		return -1;
	}

	/* if result contains the number of bytes read; make sure it is >= 1
	 * otherwise make sure result >= 0 */
	if (result < 0) {
		fprintf(stderr, "can't read register 0x%02X, %s\n", params->reg,
			strerror(errno));
		return -1;
	}

	/* print out the data read. */
	switch (op->size) {
	case SMBUS_SIZE_8:
		printf("0x%02X\n", params->data.fixed.u8);
		break;
	case SMBUS_SIZE_16:
		printf("0x%04X\n", params->data.fixed.u16);
		break;
	case SMBUS_SIZE_BLOCK:
		{
		int i;
		for (i=0; i <= result; i++)
			printf("%02X", params->data.array[i]);
		printf("\n");
		}
		break;
	}

	return 0;
}

static int
parse_io_width(const char *arg, struct smbus_op_params *params,
               const struct smbus_op *op)
{
	unsigned long ldata;
	char *end;

	switch (op->size) {
	case SMBUS_SIZE_8:
		ldata = strtoul(arg, &end, 0);
		if (ldata == LONG_MAX || *end != '\0') {
			return -1;
		}
		params->data.fixed.u8 = ldata;
		break;
	case SMBUS_SIZE_16:
		ldata = strtoul(arg, &end, 0);
		if (ldata == LONG_MAX || *end != '\0') {
			return -1;
		}
		params->data.fixed.u16 = ldata;
		break;
	case SMBUS_SIZE_BLOCK:
		{
		int len;
		int i;
		char *err;
		char str_nibble[3];

		len = strlen(arg);
		if ( (len <= 0) || (len > 64) || (len % 2 != 0) ) {
			return -1;
		}

		/* null terminate string. */
		str_nibble[2] = '\0';
		for (i = len - 2; i >= 0 ; i -= 2) {
			str_nibble[0] = arg[i];
			str_nibble[1] = arg[i+1];
			params->data.array[i/2] = strtol(str_nibble, &err, 16);
			if (err[0] != '\0') {
				return -1;
			}
		}
		params->len = len / 2;
		}
		break;
	default:
		return -1;
	}

	return 0;
}

static int
smbus_write(int argc, const char *argv[], const struct cmd_info *info)
{
	int ret;
	struct smbus_op_params params;
	const struct smbus_op *op =
		(const struct smbus_op *)info->privdata;

	if (smbus_prologue(argv, &params) < 0) {
		return -1;
	}

	if (parse_io_width(argv[4], &params, op) < 0 ) {
		fprintf(stderr, "invalid value to write\n");
		return -1;
	}

	ret = op->perform_op(&params, op);

	close(params.fd);

	return ret;
}

static int
smbus_write_op(struct smbus_op_params *params, const struct smbus_op *op)
{
	int result;

	/* FIXME: Why is this needed if the open_i2c_slave performs the ioctl
	 * with I2C_SLAVE? */
	if (ioctl(params->fd, I2C_SLAVE_FORCE, params->address) < 0) {
		fprintf(stderr, "can't set address 0x%02X, %s\n",
		        params->address, strerror(errno));
		return -1;
	}

	switch (op->size) {
	case SMBUS_SIZE_8:
		result = i2c_smbus_write_byte_data(params->fd, params->reg,
		             params->data.fixed.u8);
		break;
	case SMBUS_SIZE_16:
		result = i2c_smbus_write_byte_data(params->fd, params->reg,
		             params->data.fixed.u16);
		break;
	case SMBUS_SIZE_BLOCK:
		result = i2c_smbus_write_block_data(params->fd, params->reg,
		             params->len, params->data.array);
		break;
	default:
		fprintf(stderr, "Illegal SMBus size for write operation.\n");
		return -1;
	}

	if (result < 0) {
		fprintf(stderr, "can't write register 0x%02X, %s\n",
		        params->reg, strerror(errno));
		return -1;
	}

	return 0;
}

MAKE_PREREQ_PARAMS_FIXED_ARGS(smbus_read_params, 4,
	"<adapter> <address> <register>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(smbus_write_params, 5,
	"<adapter> <address> <register> <value>", 0);

#define MAKE_SMBUS_OP(name_, size_, fn_) \
	static const struct smbus_op name_ = { \
		.size = size_, \
		.perform_op = fn_, \
	}
#define MAKE_SMBUS_RW_OP(name_, size_, rd_fn_, wr_fn_) \
	MAKE_SMBUS_OP(name_## _r, SMBUS_SIZE_ ##size_, rd_fn_); \
	MAKE_SMBUS_OP(name_## _w, SMBUS_SIZE_ ##size_, wr_fn_)

MAKE_SMBUS_RW_OP(smbus_op_8, 8, smbus_read_op, smbus_write_op);
MAKE_SMBUS_RW_OP(smbus_op_16, 16, smbus_read_op, smbus_write_op);
MAKE_SMBUS_RW_OP(smbus_op_block, BLOCK, smbus_read_op, smbus_write_op);

#define MAKE_SMBUS_RW_CMDS(size_) \
	MAKE_CMD_WITH_PARAMS(smbus_read ##size_, smbus_read, \
	                     &smbus_op_ ##size_## _r, &smbus_read_params), \
	MAKE_CMD_WITH_PARAMS(smbus_write ##size_, smbus_write, \
	                     &smbus_op_ ##size_## _w, &smbus_write_params)

static const struct cmd_info smbus_cmds[] = {
	MAKE_SMBUS_RW_CMDS(8),
	MAKE_SMBUS_RW_CMDS(16),
	MAKE_SMBUS_RW_CMDS(block),
};

MAKE_CMD_GROUP(SMBus, "commands to access the system management bus",
               smbus_cmds);
REGISTER_CMD_GROUP(SMBus);
