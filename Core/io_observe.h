#ifndef _IO_OBSERVE_H_
#define _IO_OBSERVE_H_

#include "PhysMem.h"

class io_observer
{
private:
	// backup of all pages
	typedef unsigned long reactor_page[PAGING::SIZE / sizeof(unsigned long)];
	static reactor_page reactor_data[memory::REGISTERS1::PAGES];
	static void process();
	static void io_write(memory_block *b);

public:
	static void init();
};


#endif
