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
#include "commands.h"

int
main(int argc, const char *argv[])
{
	exit(run_command(argc, argv));
}

static void
usage(const char *argv[])
{
	fprintf(stderr, "usage: %s COMMAND\n", argv[0]);
	fprintf(stderr, "  COMMANDS:\n"
			"    --make-links\n"
			"    --clean-links\n"
			"    --list-cmds\n");
}

int
iotools_fallback(int argc, const char *argv[])
{
	if (argc != 2) {
		usage(argv);
		return EXIT_FAILURE;
	}

	if (strcmp(argv[1], "--make-links") == 0) {
		return make_command_links();
	}

	if (strcmp(argv[1], "--clean-links") == 0) {
		return clean_command_links();
	}

	if (strcmp(argv[1], "--list-cmds") == 0) {
		return list_commands();
	}

	fprintf(stderr, "'%s' sub-command not supported by iotools\n", argv[1]);
	usage(argv);
	return EXIT_FAILURE;
}
