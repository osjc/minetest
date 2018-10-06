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


#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)

/*
	Debug output
*/

FILE *g_debugstreams[DEBUGSTREAM_COUNT] = {stderr, NULL};

void debugstreams_init(const char *filename)
{
	if(filename)
		g_debugstreams[1] = fopen(filename, "a");
		
	if(g_debugstreams[1])
	{
		fprintf(g_debugstreams[1], "\n\n-------------\n");
		fprintf(g_debugstreams[1],     "  Separator  \n");
		fprintf(g_debugstreams[1],     "-------------\n\n");
	}
}

void debugstreams_deinit()
{
	if(g_debugstreams[1] != NULL)
		fclose(g_debugstreams[1]);
}

Debugbuf debugbuf;
std::ostream dstream(&debugbuf);
Nullstream dummyout;

/*
	Assert
*/

void assert_fail(const char *assertion, const char *file,
		unsigned int line, const char *function)
{
	DEBUGPRINT("\nIn thread %x:\n"
			"%s:%d: %s: Assertion '%s' failed.\n",
			(unsigned int)get_current_thread_id(),
			file, line, function, assertion);

	if(g_debugstreams[1])
		fclose(g_debugstreams[1]);

	//sleep_ms(3000);

	abort();
}
