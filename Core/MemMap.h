#ifndef _MEMMAP_H_
#define _MEMMAP_H_


#include <assert.h>
#include <list>
#include <algorithm>
#include <utility>
#include <itl/itl_value.hpp>
#include <itl/split_interval_map.hpp>

#include "forward.h"
#include "MemRegionBase.h"
#include "Logging.h"
#include "Compiler.h" // for DebugBreak()_

// information for a mapped interval
// accumulates all overlapping memory_regions for its interval
struct region_entry
{
	typedef std::list<memory_region_base*> sectionlist;
	sectionlist sections;
	memory_region_base *prio;

	void recalc_prio()
	{
		prio = 0;
		if (sections.size() > 0)
		{
			sectionlist::iterator it = sections.begin();
			prio = *it;
			it++;
			for (; it != sections.end(); ++it)
			{
				if((*it)->priority == prio->priority)
				{
					logging<_DEFAULT>::logf("Memory mapping clash between: %s and %s", (*it)->name, prio->name);
					DebugBreak_();
				}
				if ((*it)->priority > prio->priority)
					prio = *it;
			}
		}
	}

	bool operator == (const region_entry &other) const { return sections == other.sections; }
	void operator += (const region_entry &other) 
	{ 
		/*
		sectionlist::iterator mid = sections.end();
		sections.insert(mid, other.sections.begin(), other.sections.end() );
		std::inplace_merge( sections.begin(), mid, sections.end() ); 
		*/
		sectionlist tmp = other.sections;
		sections.merge( tmp );
		
		// could optimize this as only need to update against other.sections
		recalc_prio();
	}
	void operator -= (const region_entry &other)
	{
		// other will always be a subset of this
		sectionlist::iterator end = std::set_difference( sections.begin(), sections.end(), 
			other.sections.begin(), other.sections.end(), sections.begin() );
		sections.erase( end, sections.end() );
		recalc_prio();
	}
};

#include "PhysMem.h"


// a memory mapping table used by the processors to access memory

// error: memory_map<T> needs memory::get_nullblock()
// memory needs memory_map<T>

template <typename T> class memory_map
{
private:
	enum { PAGES = PAGING::TOTAL_PAGES };
	typedef itl::split_interval_map<unsigned long, region_entry> ovl_set;
	static memory_block* map[PAGES];
	static ovl_set ovl;


	static void map_block(memory_block *block, unsigned int page)
	{
		assert((page >= 0) && (page < PAGES));
	
		memory_block *null = memory::get_nullblock();
		if ((map[page] != null) && (block != null))
		{
			memory_block *p = map[page];

			const memory_region_base *region_p = p->base;
			size_t addr_p = region_p->address( p );
			const memory_region_base *region_q = block->base;
			size_t addr_q = region_q->address( block );
			
			logging<T>::logf(  "Remapping at 0x%08X from <%s>:%X to <%s>:%X", 
				page * PAGING::SIZE, region_p->name, addr_p, region_q->name, addr_q );
		}

		map[page] = block;
	}
	static void apply_mapping( memory_region_base *region, const _region &reg )
	{
		unsigned int i = 0;
		assert(reg.second > reg.first);

		for (unsigned int p = reg.first; p != reg.second; p++)
		{
			map_block( &region->start[i % region->pages], p );
			i++;
		}
	}

	static void apply_mapping_internal(ovl_set &set)
	{
		for (ovl_set::iterator it = set.begin(); it != set.end(); ++it)
		{
			// update the mapping for this region!
			const ovl_set::interval_type &x = it->first;
			
			assert(x.is_rightopen());
			if (it->second.prio)
			{
				apply_mapping( it->second.prio , _region(x.lower(), x.upper()) );
			} else
			{
				// this should never have happened ...
				assert(0);
			}		
		}
	}

public:

	static void init_null()
	{
		memory_block *null_block = memory::get_nullblock();
		for (unsigned int i = 0; i < PAGES; i++)
			map[i] = null_block;

		ovl_set::interval_type i(0, PAGES, ovl_set::interval_type::RIGHT_OPEN);
		region_entry re;
		re.sections.push_back(&memory::null_region);
		re.recalc_prio();
		std::pair< typename ovl_set::interval_type, region_entry > p(i, re);
		ovl += p;
	}

	static void map_region(memory_region_base *region, unsigned long page)
	{
		map_region( region, _region(page, page + region->pages) );
	}

	static void unmap(memory_region_base *region)
	{
		// create interval for whole memory region
		ovl_set::interval_type i(0, PAGES, ovl_set::interval_type::RIGHT_OPEN);

		// create region entry
		region_entry re;
		re.sections.push_back(region);
		re.recalc_prio();
		std::pair< typename ovl_set::interval_type, region_entry > p(i, re);

		// iterate and collect all regions where region is mapped
		// those regions will need to be remapped afterwards
		itl::interval_set<unsigned long> s;
		for (ovl_set::iterator it = ovl.begin(); it != ovl.end(); ++it)
		{
			if (it->second.prio == region)
				s += it->first;
		}

		// remove the mapping
		ovl -= p;

		// remap with new highest priority region
		ovl_set intersect;
		ovl.intersect(intersect, s);
		apply_mapping_internal(intersect);
	}

	static void map_region(memory_region_base *region, const _region &reg)
	{
		// create an interval out of the address pair
		ovl_set::interval_type i(reg.first, reg.second, ovl_set::interval_type::RIGHT_OPEN);

		// create a new region entry for the new interval
		region_entry re;
		re.sections.push_back(region);
		re.recalc_prio();
		std::pair< typename ovl_set::interval_type, region_entry > p(i, re);

		// add the new interval
		ovl += p;

		// intersect the interval with all added regions
		ovl_set intersect;
		ovl.intersect(intersect, i);

		// iterate over the segments in the region
		apply_mapping_internal(intersect);
	}

	static memory_block* addr2page(unsigned long addr)
	{
		return memory_map<T>::get_page( addr >> PAGING::SIZE_BITS );
	}

	static memory_block* get_page(unsigned int page)
	{
		return map[page];
	}


	template <typename functor>
	static void process_memory(unsigned long addr, int len, typename functor::context &ctx)
	{
		memory_block *b = addr2page( addr );
		int todo = len;
		const int subaddr = addr & PAGING::ADDRESS_MASK;
		int left = PAGING::SIZE - subaddr;
		if (todo > left)
			todo = left;
		functor::process( b, &b->mem[subaddr], todo, ctx );
		len -= todo;
		addr += todo;

		// process followup pages from start
		while (len > 0)
		{
			todo = len;
			if (todo > PAGING::SIZE)
				todo = PAGING::SIZE;
			b = memory_map<T>::addr2page( addr );
			functor::process( b, b->mem, todo, ctx );
			len  -= todo;
			addr += todo;
		}
	}
};
template <typename T> memory_block* memory_map<T>::map[memory_map<T>::PAGES];
template <typename T> typename memory_map<T>::ovl_set memory_map<T>::ovl;

#endif
