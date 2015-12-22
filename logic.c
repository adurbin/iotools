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
#include <limits.h>
#include "commands.h"

typedef enum {
	OR_OP,
	AND_OP,
	XOR_OP,
	SHL_OP,
	SHR_OP,
	BTS_OP,
	BTR_OP,
} LOGIC_OP;

struct logic_info {
	int op;
};

static int
logic_op(int argc, const char *argv[], const struct cmd_info *info)
{
	int rc;
	unsigned long long result;
	unsigned long long val;
	struct logic_info *logic_info = (struct logic_info *)info->privdata;

	/* Strip off command name and obtain source value to be operated on. */
	argc--; argv++;
	result = strtoull(argv[0], NULL, 0);
	argc--; argv++;
	rc = 0;
	while (argc) {
		/* Next value to use in operation. */
		val = strtoull(argv[0], NULL, 0);
		switch (logic_info->op) {
		case OR_OP:
			result |= val;
			rc = (result == 0);
			break;
		case AND_OP:
			result &= val;
			rc = (result == 0);
			break;
		case XOR_OP:
			result ^= val;
			rc = (result == 0);
			break;
		case BTS_OP:
			val = (1 << val);
			rc = ((result & val) != 0);
			result |= val;
			break;
		case BTR_OP:
			val = (1 << val);
			rc = ((result & val) != 0);
			result &= ~val;
			break;
		default:
			fprintf(stderr, "Invalid logic op\n");
			return -1;
		}
		argc--; argv++;
	}

	printf("0x%llx\n", result);

	return rc;
}

static int
not(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long result;

	result = strtoull(argv[1], NULL, 0);
	result = ~result;

	printf("0x%llx\n", result);

	return (result == 0);
}

static int
shift(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long val;
	unsigned long shift;
	struct logic_info *logic_info = (struct logic_info *)info->privdata;

	val = strtoull(argv[1], NULL, 0);
	shift = strtoul(argv[2], NULL, 0);

	switch(logic_info->op) {
	case SHL_OP:
		val <<= shift;
		break;
	case SHR_OP:
		val >>= shift;
		break;
	default:
		fprintf(stderr, "Invalid shift operation\n");
		return -1;
	}
	printf("0x%llx\n", val);

	return 0;
}

MAKE_PREREQ_PARAMS_VAR_ARGS(logic_op_params, 2, INT_MAX,
                            "<value> <value> ...", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(not_params, 2, "<value>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(shift_params, 3, "<value> <shift>", 0);
MAKE_PREREQ_PARAMS_FIXED_ARGS(bit_params, 3, "<value> <bit>", 0);

#define MAKE_LOGIC_INFO(name_, type_) \
	static struct logic_info name_## _info = { .op = type_ }

MAKE_LOGIC_INFO(or, OR_OP);
MAKE_LOGIC_INFO(and, AND_OP);
MAKE_LOGIC_INFO(xor, XOR_OP);
MAKE_LOGIC_INFO(shl, SHL_OP);
MAKE_LOGIC_INFO(shr, SHR_OP);
MAKE_LOGIC_INFO(btr, BTR_OP);
MAKE_LOGIC_INFO(bts, BTS_OP);

#define MAKE_LOGIC_CMD(name_, entry_, params_) \
	MAKE_CMD_WITH_PARAMS(name_, entry_, &name_## _info, params_)

static const struct cmd_info logic_cmds[] = {
	MAKE_LOGIC_CMD(or, &logic_op, &logic_op_params),
	MAKE_LOGIC_CMD(and, &logic_op, &logic_op_params),
	MAKE_LOGIC_CMD(xor, &logic_op, &logic_op_params),
	MAKE_LOGIC_CMD(shl, &shift, &shift_params),
	MAKE_LOGIC_CMD(shr, &shift, &shift_params),
	MAKE_CMD_WITH_PARAMS(not, &not, NULL, &not_params),
	MAKE_LOGIC_CMD(btr, &logic_op, &bit_params),
	MAKE_LOGIC_CMD(bts, &logic_op, &bit_params),
};

MAKE_CMD_GROUP(LOGIC, "commands to perform boolean algebra operations",
               logic_cmds);
REGISTER_CMD_GROUP(LOGIC);
