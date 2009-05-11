#ifndef _MEMREGIONBASE_H_
#define _MEMREGIONBASE_H_

#include <cstring> // for size_t

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
};

#endif
