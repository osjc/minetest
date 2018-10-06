/*
Minetest-c55
Copyright (C) 2010 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
(c) 2018 Jozef Behran
*/

#ifndef TYPES_HEADER
#define TYPES_HEADER

/*
  These types are shared with Irrlicht so if you are in C++ code, import
  them from Irrlicht. Otherwise the C++ code will break as for the compiler
  the definitions below are different from the Irrlicht types even when the
  underlying definitions are exactly the same (stupid C++).
*/
#ifdef __cplusplus
#include <irrTypes.h>
using namespace irr;
#else
typedef unsigned char		u8;
typedef signed char		s8;
typedef unsigned short int	u16;
typedef signed short int	s16;
typedef unsigned int		u32;
typedef signed int		s32;
typedef float			f32;
typedef double			f64;
typedef u8			bool;
#define true (1==1)
#define false (1==0)
#endif

/*
  These types are not in Irrlicht so it is safe to define them
  unconditionally.
*/
typedef unsigned long long	u64;
typedef signed long long	s64;

#endif
