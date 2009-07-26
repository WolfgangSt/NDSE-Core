#ifndef _MEMREGION_H_
#define _MEMREGION_H_

#include "MemRegionBase.h"

typedef std::pair<unsigned long, unsigned long> _region;

// a physical region containing several physical pages
template <typename size_type > struct memory_region: public memory_region_base
{
	enum { SIZE = size_type::SIZE };
	enum { PAGES = PAGING::PAGES< SIZE >::SIZE };
	memory_block blocks[PAGES];
	memory_region( const char *name, unsigned long color, unsigned long priority )
	{
		// init default loadstores
		ls[_ARM9::VALUE] = &HLE<_ARM9>::loadstore;
		ls[_ARM7::VALUE] = &HLE<_ARM7>::loadstore;

		this->name = name;
		this->color = color;
		this->pages = PAGES;
		this->priority = priority;
		start = &blocks[0];
		end = &blocks[PAGES];
		for (int i = 0; i < PAGES; i++)
			blocks[i].base = this;
	};
};

#endif

