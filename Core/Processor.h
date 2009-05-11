#ifndef _PROCESSOR_H_
#define _PROCESSOR_H_

#include "ArmContext.h"

template<typename T> class processor
{
private:
	typedef memory_map<T> mem;
public:
	static emulation_context context;
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
#endif
