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

#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <stdint.h>

/* This is a shared data type to handle different type sizes for subcommands. */
typedef union {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
} data_store;


/* Provide shared parameters the describe the size of the operation for
 * subcommand implementations. */
enum IO_SIZE
{
	SIZE8 = 8,
	SIZE16 = 16,
	SIZE32 = 32,
	SIZE64 = 64,
};

struct size_param {
	int size;
};

#define EXTERN_SIZE_PARAM(_size) \
	extern const struct size_param size ##_size

EXTERN_SIZE_PARAM(8);
EXTERN_SIZE_PARAM(16);
EXTERN_SIZE_PARAM(32);
EXTERN_SIZE_PARAM(64);

/* The min and max args need to include the argv[0] parameter. For example,
 * 'shl 1 3' would list the min and max arguments as 3. */
struct prereq_params {
	int min_args;      /* Miniumum number of arguments required. */
	int max_args;      /* Maxiumum number of arguments required. */
	const char *usage; /* Usage string - requires 1st param %s for arv[0] */
	int iopl_needed;   /* non-zero if iopl needed. */
};

#define _PREREQ_PARAMS_VAR_ARGS(min_args_, max_args_, usage_, iopl_) \
	{ \
		.min_args = min_args_, \
		.max_args = max_args_, \
		.usage = usage_, \
		.iopl_needed = iopl_, \
	}

#define MAKE_PREREQ_PARAMS_FIXED_ARGS(name_, nargs_, usage_, iopl_) \
	static const struct prereq_params name_ = \
		_PREREQ_PARAMS_VAR_ARGS(nargs_, nargs_, usage_, iopl_)

#define MAKE_PREREQ_PARAMS_VAR_ARGS(name_, min_args_, max_args_, usage_, iopl_)\
	static const struct prereq_params name_ = \
		_PREREQ_PARAMS_VAR_ARGS(min_args_, max_args_, usage_, iopl_)

struct cmd_group
{
	const char *name;
	const char *description;
	const struct cmd_info *commands;
	const int num_commands;
	/* Pointer to next group of commands. It is used internally to keep
	 * track of command groups. Command group implemenations should not set
	 * this field. */
	struct cmd_group *next;
};

#define MAKE_CMD_GROUP(grp_name_, description_, cmd_array_) \
	static struct cmd_group grp_name_## _group = { \
		.name = #grp_name_, \
		.description = description_, \
		.commands = cmd_array_, \
		.num_commands = arraysize(cmd_array_), \
		.next = NULL, \
	}

#define REGISTER_CMD_GROUP(grp_name_) \
	static void register_ ##grp_name_(void) __attribute__ ((constructor)); \
	static void register_ ##grp_name_(void) \
	{ \
		register_command_group(&grp_name_## _group); \
	}

struct cmd_info
{
	const char *name;
	int (*entry)(int argc, const char *argv[], const struct cmd_info *info);
	const void *privdata;
	const struct prereq_params *params;
};

#define _MAKE_CMD(name_, entry_point_, priv_data_, params_) \
	{ \
		.name = #name_, \
		.entry = entry_point_, \
		.privdata = priv_data_, \
		.params = params_, \
	}

#define MAKE_CMD(name_, entry_point_, priv_data_) \
	_MAKE_CMD(name_, entry_point_, priv_data_, NULL)

#define MAKE_CMD_WITH_PARAMS(name_, entry_point_, priv_data_, params_) \
	_MAKE_CMD(name_, entry_point_, priv_data_, params_)

int run_command(int argc, const char *argv[]);
int make_command_links(void);
int clean_command_links(void);
int list_commands(void);
int iotools_fallback(int argc, const char *argv[]);
int register_command_group(struct cmd_group *group);

#define arraysize(array_) \
	(sizeof((array_))/sizeof((array_)[0]))

/* Helper function to obtain size information embedded in private data of
 * cmd_info structure. */
static inline int
get_command_size(const struct cmd_info *info)
{
	const struct size_param *size_info =
		(const struct size_param *)info->privdata;

	return size_info->size;
}

#endif /* _COMMANDS_H_ */
