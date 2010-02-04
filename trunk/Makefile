# Copyright 2008 Google Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

VER_MAJOR = 1
VER_MINOR = 3

CC=$(CROSS_COMPILE)gcc

# build options
DEBUG ?= 1
STATIC ?= 1

ifneq ($(strip $(STATIC)),0)
IOTOOLS_STATIC = -static
endif

ifneq ($(strip $(DEBUG)),0)
IOTOOLS_DEBUG = -O0 -ggdb
else
IOTOOLS_DEBUG = -O2 -DNDEBUG
endif

CFLAGS = -Wall -Werror $(DEFS) $(ARCHFLAGS) $(IOTOOLS_STATIC) $(IOTOOLS_DEBUG)
DEFS = -D_GNU_SOURCE -DVER_MAJOR=$(VER_MAJOR) -DVER_MINOR=$(VER_MINOR)
SBINDIR ?= /usr/local/sbin

BINARY=iotools
OBJS_TO_BUILD=$(filter-out iotools.o, $(patsubst %.c,%.o,$(wildcard *.c)))
OBJS=$(OBJS_TO_BUILD)

all: $(BINARY)

$(BINARY): $(OBJS) iotools.o Makefile
	$(CC) $(CFLAGS) -o $@ iotools.o $(OBJS)

RUSER ?= root
RHOST ?=
rinstall: $(BINARY) $(SCRIPTS)
	@if [ -n "$(RHOST)" ]; then \
		scp $^ $(RUSER)@$(RHOST):$(SBINDIR); \
	else \
		echo $@: no RHOST defined; \
	fi

clean:
	$(RM) *.o $(BINARY)
