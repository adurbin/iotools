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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>
#include "commands.h"
#include "platform.h"

/*
 * There is a chance that we don't have cpu_set_t available to us, like
 * in the case where we are building against klibc.  In these cases,
 * dummy up the glibc interfaces.
 */
#ifdef __KLIBC__
#define __CPU_SETSIZE	1024
#define __NCPUBITS	(8 * sizeof (__cpu_mask))
typedef unsigned long int __cpu_mask;

/* Data structure to describe CPU mask. */
typedef struct {
	__cpu_mask __bits[__CPU_SETSIZE / __NCPUBITS];
} cpu_set_t;

static void CPU_ZERO(cpu_set_t *set)
{
	memset(set, 0, sizeof(set));
}
static void CPU_SET(int cpu, cpu_set_t *set)
{
	set->__bits[cpu / __NCPUBITS] |= 1 << (cpu % __NCPUBITS);
}

/*
 * sched_setaffinity actually takes an "unsigned long *" for the mask,
 * while glibc takes a "cpu_set_t *".  Wrap the call to deal with the
 * inconsistency.
 */
static int local_setaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
	return sched_setaffinity(pid, cpusetsize, (unsigned long *)mask);
}
#else /* ifdef __KLIBC__ */
#define local_setaffinity sched_setaffinity
#endif /* ifdef __KLIBC__ */

/* Helper function to set the affinity of the process to a given cpu. */
static int
set_cpu_affinity(int cpu)
{
	cpu_set_t cpuset;

	/* run on the specified CPU */
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	if (local_setaffinity(getpid(), sizeof(cpuset), &cpuset) < 0) {
		perror("sched_setaffinity()");
		return -1;
	}

	return 0;
}

static int
busy_loop(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long count = 0;
	struct timeval t0;
	struct timeval t1;
	int reps;
	int printed = 0;

	if (argc == 2) {
		reps = atoi(argv[1]);
	} else {
		reps = -1;
	}

	gettimeofday(&t0, NULL);
	while (printed != reps) {
		count++;
		if ((count % 16384) == 0) {
			unsigned int elapsed;
			gettimeofday(&t1, NULL);
			elapsed = (t1.tv_sec - t0.tv_sec)*1000000;
			elapsed += t1.tv_usec - t0.tv_usec;
			if (elapsed >= 1000000) {
				printf("%llu\n", count);
				gettimeofday(&t0, NULL);
				count = 0;
				if (reps != -1) {
					printed++;
				}
			}
		}
	}

	return 0;
}

#ifdef ARCH_X86

#define rdtscll(val) do { \
	uint32_t __a, __d; \
	__asm__ __volatile__("rdtsc" : "=a" (__a), "=d" (__d)); \
	(val) = ((uint64_t)__a) | (((uint64_t)__d)<<32); \
} while(0)

static int
rdtsc(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long long tsc;

	rdtscll(tsc);
	printf("0x%016llx\n", tsc);

	return 0;
}

static int
cpuid_inline(int cpu, int function, int index, uint32_t *data)
{
	/* run on the specified CPU */
	if (set_cpu_affinity(cpu) < 0) {
		return -1;
	}

	asm volatile (
#if defined(__i386__) && defined(__PIC__)
	      /* We can't use %ebx on 32 bit builds with PIC.
	       *
	       * The need for this is, IMO, a bug in GCC.  We should not
	       * need to know what registers it is using internally.  It
	       * should be saving and restoring them itself.
	       */
	      "xchg %%ebx, %%esi;" /* save ebx in esi */
	      "cpuid;"
	      "xchg %%esi, %%ebx;" /* restore ebx, data moves to esi */
	      : "=a" (data[0]), "=S" (data[1]), "=c" (data[2]), "=d" (data[3])
	      : "0" (function), "2" (index)
	      : "memory"
#else
	      "cpuid;"
	      : "=a" (data[0]), "=b" (data[1]), "=c" (data[2]), "=d" (data[3])
	      : "0" (function), "2" (index)
	      : "memory"
#endif
	);

	return 0;
}

static int
cpuid(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long function;
	unsigned long index;
	int cpu;
	uint32_t data[4];

	cpu = strtol(argv[1], NULL, 0);
	function = strtoul(argv[2], NULL, 0);
	index = 0;
	if (argc == 4) {
		index = strtoul(argv[3], NULL, 0);
	}

	if (cpuid_inline(cpu, function, index, data) < 0) {
		fprintf(stderr, "Could not obtain cpuid result.\n");
		return -1;
	}

	printf("0x%08x 0x%08x 0x%08x 0x%08x\n",
	       data[0], data[1], data[2], data[3]);

	return 0;
}

#endif /* #ifdef ARCH_X86 */

static int
cpu_list(int argc, const char *argv[], const struct cmd_info *info)
{
#ifdef _SC_NPROCESSORS_ONLN
	int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	int i;

	for (i = 0; i < ncpus; i++) {
		printf("%d\n", i);
	}
	return 0;
#else /* ifdef _SC_NPROCESSORS_ONLN */
	/* Some libraries do not define sysconf() properly. */
	fprintf(stderr, "Operation not supported by library.\n");
	return -1;
#endif /* ifdef _SC_NPROCESSORS_ONLN */
}

/*
 * runon()
 *
 * At least two parameters :
 *
 * argv[1]: cpu to run command on
 * argv[2]: command to run
 * argv[3]  and on are arguments to pass to exec()
 *
 */
static int
runon(int argc, const char *argv[], const struct cmd_info *info)
{
	unsigned long cpu;

	/* strip program command line */
	argc--; argv++;

	/* if we wanted to be clever we could parse it as cpu,cpu,cpu */
	cpu = strtoul(argv[0], NULL, 0);
	argc--; argv++;

	/* run on the specified CPU */
	if (set_cpu_affinity(cpu) < 0) {
		return -1;
	}

	/* load the target */
	execvp(argv[0], (void *)argv);

	/* if we get here, we didn't exec... */
	perror("exec");

	return -1;
}

MAKE_PREREQ_PARAMS_VAR_ARGS(cpuid_params, 3, 4, "<cpu> <function> [index]", 0);
MAKE_PREREQ_PARAMS_VAR_ARGS(runon_params, 3, INT_MAX, "<cpu> <cmd> [args]", 0);

static const struct cmd_info misc_cmds[] = {
#ifdef ARCH_X86
	MAKE_CMD(rdtsc, rdtsc, NULL),
	MAKE_CMD_WITH_PARAMS(cpuid, cpuid, NULL, &cpuid_params),
#endif /* #ifdef ARCH_X86 */
	MAKE_CMD(busy_loop, &busy_loop, NULL),
	MAKE_CMD(cpu_list, cpu_list, NULL),
	MAKE_CMD_WITH_PARAMS(runon, &runon, NULL, &runon_params),
};

MAKE_CMD_GROUP(MISC, NULL, misc_cmds);
REGISTER_CMD_GROUP(MISC);
