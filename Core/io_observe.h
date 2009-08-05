#ifndef _IO_OBSERVE_H_
#define _IO_OBSERVE_H_

#include "PhysMem.h"

class io_observer
{
private:
	// backup of all pages
	typedef unsigned long reactor_page[PAGING::SIZE / sizeof(unsigned long)];
	static reactor_page reactor_data9[REGISTERS9_1::PAGES];
	static reactor_page reactor_data7[REGISTERS7_1::PAGES];
	static void process9();
	static void process7();
	static void io_write9(memory_block *b);
	static void io_read9(memory_block *b, unsigned long addr);
	static void io_readipc9(memory_block *b, unsigned long addr);


	static void io_write7(memory_block *b);

public:
	static void init();
};


#endif
