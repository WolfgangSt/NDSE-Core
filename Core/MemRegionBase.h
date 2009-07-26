#ifndef _MEMREGIONBASE_H_
#define _MEMREGIONBASE_H_

#include <cstring> // for size_t
#include "Mem.h"

// this holds debug informations for a physical memory region
// dont never ever use those inside emulation for sake of performance
// this struct is just meant for error recovery / statistic reporting


struct memory_region_base
{
	const char *name;
	unsigned int pages;
	unsigned long color;
	unsigned int priority;
	memory_block *start, *end;

	size_t address( memory_block *page ) const
	{
		return page - start;
	}

	bool page_in_region( memory_block *page ) const
	{
		if (page < start)
			return false;
		if (page >= end)
			return false;
		return true;
	}

	// this need to be specialized for PAGE_ACCESSHANDLER pages
	// CPU information is not propagated through those calls.
	// if this is needed at some point we gotta store function pointers here
	// for each CPU like:
	// void *store32[MAX_CPU]
	// for now this should be sufficient however

	virtual void store32(unsigned long /*addr*/, unsigned long /*value*/) { assert(0); }
	virtual void store16(unsigned long /*addr*/, unsigned long /*value*/) { assert(0); }
	virtual void store8(unsigned long /*addr*/, unsigned long /*value*/) { assert(0); }
	virtual void store32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/) { assert(0); }
	virtual unsigned long load32(unsigned long /*addr*/) { assert(0); return 0; }
	virtual unsigned long load16u(unsigned long /*addr*/) { assert(0); return 0; }
	virtual unsigned long load16s(unsigned long /*addr*/) { assert(0); return 0; }
	virtual unsigned long load8u(unsigned long /*addr*/) { assert(0); return 0; }
	virtual void load32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/) { assert(0); }
};

#endif
