#ifndef _HOSTCONTEXT_H_
#define _HOSTCONTEXT_H_

#include "osdep.h"

struct host_context
{
	ucontext_t ctx;
	bool addr_resolved;
	unsigned long addr;
	unsigned long subaddr;
};

#endif
