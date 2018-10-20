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
	
	debug_stacks_print();

	if(g_debugstreams[1])
		fclose(g_debugstreams[1]);

	//sleep_ms(3000);

	abort();
}

/*
	DebugStack
*/

DebugStack::DebugStack(threadid_t id)
{
	threadid = id;
	stack_i = 0;
	stack_max_i = 0;
}

void DebugStack::print(FILE *file, bool everything)
{
	fprintf(file, "DEBUG STACK FOR THREAD %x:\n",
			(unsigned int)threadid);

	for(int i=0; i<stack_max_i; i++)
	{
		if(i == stack_i && everything == false)
			continue;

		if(i < stack_i)
			fprintf(file, "#%d  %s\n", i, stack[i]);
		else
			fprintf(file, "(Leftover data: #%d  %s)\n", i, stack[i]);
	}

	if(stack_i == DEBUG_STACK_SIZE)
		fprintf(file, "Probably overflown.\n");
}


core::map<threadid_t, DebugStack*> g_debug_stacks;
JMutex g_debug_stacks_mutex;

void debug_stacks_init()
{
	g_debug_stacks_mutex.Init();
}

void debug_stacks_print()
{
	JMutexAutoLock lock(g_debug_stacks_mutex);

	DEBUGPRINT("Debug stacks:\n");

	for(core::map<threadid_t, DebugStack*>::Iterator
			i = g_debug_stacks.getIterator();
			i.atEnd() == false; i++)
	{
		DebugStack *stack = i.getNode()->getValue();

		for(int i=0; i<DEBUGSTREAM_COUNT; i++)
		{
			if(g_debugstreams[i] != NULL)
				stack->print(g_debugstreams[i], true);
		}
	}
}

DebugStacker::DebugStacker(const char *text)
{
	threadid_t threadid = get_current_thread_id();

	JMutexAutoLock lock(g_debug_stacks_mutex);

	core::map<threadid_t, DebugStack*>::Node *n;
	n = g_debug_stacks.find(threadid);
	if(n != NULL)
	{
		m_stack = n->getValue();
	}
	else
	{
		/*DEBUGPRINT("Creating new debug stack for thread %x\n",
				(unsigned int)threadid);*/
		m_stack = new DebugStack(threadid);
		g_debug_stacks.insert(threadid, m_stack);
	}

	if(m_stack->stack_i >= DEBUG_STACK_SIZE)
	{
		m_overflowed = true;
	}
	else
	{
		m_overflowed = false;

		snprintf(m_stack->stack[m_stack->stack_i],
				DEBUG_STACK_TEXT_SIZE, "%s", text);
		m_stack->stack_i++;
		if(m_stack->stack_i > m_stack->stack_max_i)
			m_stack->stack_max_i = m_stack->stack_i;
	}
}

DebugStacker::~DebugStacker()
{
	JMutexAutoLock lock(g_debug_stacks_mutex);
	
	if(m_overflowed == true)
		return;
	
	m_stack->stack_i--;

	if(m_stack->stack_i == 0)
	{
		threadid_t threadid = m_stack->threadid;
		/*DEBUGPRINT("Deleting debug stack for thread %x\n",
				(unsigned int)threadid);*/
		delete m_stack;
		g_debug_stacks.remove(threadid);
	}
}

