/*
 Copyright 2012 Google Inc.

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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

/*
 * Platform-specific support.
 */

#include <byteswap.h>

#if defined(__i386__) || defined(__x86_64__)
# define ARCH_X86 1
#endif

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define IS_BIG_ENDIAN 1
# define le_to_host_8(x) (x)
# define le_to_host_16(x) bswap_16(x)
# define le_to_host_32(x) bswap_32(x)
# define host_to_le_8(x) (x)
# define host_to_le_16(x) bswap_16(x)
# define host_to_le_32(x) bswap_32(x)
#else
# define IS_LITTLE_ENDIAN 1
# define le_to_host_8(x) (x)
# define le_to_host_16(x) (x)
# define le_to_host_32(x) (x)
# define host_to_le_8(x) (x)
# define host_to_le_16(x) (x)
# define host_to_le_32(x) (x)
#endif

#endif /* _PLATFORM_H_ */
