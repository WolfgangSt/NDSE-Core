#ifndef _HOSTCONTEXT_H_
#define _HOSTCONTEXT_H_

#include "osdep.h"

/*! Defines a host context for controlling the JIT */
struct host_context
{
	ucontext_t ctx;         /*! CPU context */
	bool addr_resolved;     /*! flags validity of addr:subaddr */
	unsigned long addr;     /*! Last ARM address executed */
	unsigned long subaddr;  /*! Last JIT line of addr that got executed */
};

#endif
