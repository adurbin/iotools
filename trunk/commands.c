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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/io.h>
#include "commands.h"

/* Provide shared parameters the describe the size of the operation for 
 * subcommand implementations. */

#define MAKE_SIZE_PARAM(size_) \
	const struct size_param size ##size_ = { \
		.size = SIZE ##size_, \
	}

MAKE_SIZE_PARAM(8);
MAKE_SIZE_PARAM(16);
MAKE_SIZE_PARAM(32);
MAKE_SIZE_PARAM(64);

static struct cmd_group *group_head;

static const struct cmd_info *
locate_command(const char *cmd)
{
	int i;
	const struct cmd_group *group;

	for (group = group_head; group; group = group->next) {
		for (i=0; i < group->num_commands; i++) {
			if (!strcmp(cmd, group->commands[i].name))
				return &group->commands[i];
		}
	}

	/* Command not found. */
	return NULL;
}

static int
check_prereqs(int argc, const char *argv[], const struct prereq_params *params)
{
	/* If there are no prerequisites the check has succeeded. */
	if (params == NULL) {
		return 0;
	}

	if (argc < params->min_args || argc > params->max_args) {
		fprintf(stderr, "usage: %s %s\n", argv[0], params->usage);
		return -1;
	}

	/* If iopl_needed is non-zero attempt to change to iopl. */
	if (params->iopl_needed != 0 && iopl(params->iopl_needed) < 0) {
		fprintf(stderr, "can't set io privilege level\n");
		return -1;
	}

	return 0;
}

static int
_run_command(int argc, const char *argv[], const struct cmd_info *cmd_info)
{
	if (check_prereqs(argc, argv, cmd_info->params) < 0) {
		return -1;
	}

	return cmd_info->entry(argc, argv, cmd_info);
}

int
run_command(int argc, const char *argv[])
{
	const struct cmd_info *cmd_info;
	const char *cmd_name;

	/* First check if the 1st parameter is a command that exists.
	 * i.e. iotools io_read8 0x70 */
	if (argc > 1) {
		cmd_info = locate_command(argv[1]);
		/* If command is found, execute it directly. */
		if (cmd_info != NULL) {
			return _run_command(--argc, ++argv, cmd_info);
		}
	}

	/* Assume iotools was executed through a symlink, but strip off the any
	 * leading path  to obtain the desired subcommand. */
	cmd_name = strrchr(argv[0], '/');
	cmd_name = (cmd_name == NULL) ? argv[0] : cmd_name+1;
	cmd_info = locate_command(cmd_name);

	if (cmd_info != NULL) {
		return _run_command(argc, argv, cmd_info);
	}

	/* Subcommand was not found. Perform the fallback operation. */
	return iotools_fallback(argc, argv);
}

static int
locate_path_of_binary(char **path_to_bin, char **bin_name)
{
	char *lbin_name, *lpath_to_bin;
	static char bin_fullpath[FILENAME_MAX];

	/* Find the location of the currently executing binary. */
	if (readlink("/proc/self/exe", bin_fullpath, FILENAME_MAX) == -1) {
		fprintf(stderr, "Unable to locate currently running binary.\n");
		return -1;
	}

	lbin_name = strrchr(bin_fullpath, '/');
	*lbin_name++ = '\0';
	lpath_to_bin = &bin_fullpath[0];

	*bin_name = lbin_name;
	*path_to_bin = lpath_to_bin;

	return 0;
}

static const char *
build_symlink_name(const char *path_to_bin, const struct cmd_info *cmd)
{
	static char link_name[FILENAME_MAX];

	snprintf(link_name, FILENAME_MAX, "%s/%s", path_to_bin, cmd->name);

	return link_name;
}

int
make_command_links(void)
{
	char *bin_name, *path_to_bin;
	int i;
	const struct cmd_group *group;

	if (locate_path_of_binary(&path_to_bin, &bin_name) < 0) {
		return -1;
	}

	for (group = group_head; group; group = group->next) {
		for (i=0; i < group->num_commands; i++) {
			const char *link_name;

			link_name = build_symlink_name(path_to_bin,
			                               &group->commands[i]);
			fprintf(stdout, "Creating link: %s -> %s\n",
				link_name, bin_name);
			if (symlink(bin_name, link_name) != 0) {
				fprintf(stderr,
				        "Unable to create link: %s -> %s\n",
				        link_name, bin_name);
			}
		}
	}

	return 0;
}

int
clean_command_links(void)
{
	char *bin_name, *path_to_bin;
	int i;
	const struct cmd_group *group;

	if (locate_path_of_binary(&path_to_bin, &bin_name) < 0) {
		return -1;
	}

	for (group = group_head; group; group = group->next) {
		for (i=0; i < group->num_commands; i++) {
			int r;
			const char *link_name;

			link_name = build_symlink_name(path_to_bin,
			                               &group->commands[i]);

			r = unlink(link_name);
			if (r < 0 && errno != ENOENT) {
				fprintf(stderr,
				        "Unable to remove link: %s: %s\n",
				        link_name, strerror(errno));
			}
		}
	}

	return 0;
}

int
list_commands(void)
{
	int i;
	const struct cmd_group *group;

	for (group = group_head; group; group = group->next) {
		fprintf(stdout, "%s%s%s\n", group->name,
		        group->description ? ": " : "",
		        group->description ? : "");
		for (i=0; i < group->num_commands; i++) {
			fprintf(stdout, "  %s\n", group->commands[i].name);
		}
	}

	return 0;
}

int
register_command_group(struct cmd_group *group)
{
	/* insert group at the beginning of the list. */
	group->next = group_head;
	group_head = group;

	return 0;
}
