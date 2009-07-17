#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

#include "ArmContext.h"

template<typename T> class processor
{
private:
	typedef memory_map<T> mem;
public:
	static emulation_context context;

	// holds the current page ARM code gets executed on
	// updated in HLE branch command
	// this has a race condition when used during branches
	// when updating but not having jumped yet ... (fix this!)
	static memory_block* last_page;

	static void reset_mapping()
	{
		memory::initializer<T>::initialize_mapping();
	}

	static void reset()
	{
		reset_mapping();
	}
};

template<typename T> emulation_context processor<T>::context;
template<typename T> memory_block* processor<T>::last_page;

#endif
