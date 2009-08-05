#ifndef _IOREGS_H_
#define _IOREGS_H_

#include "memregion.h"

struct REGISTERS9_1: public memory_region< PAGING::KB<8> >
{
	REGISTERS9_1( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};

struct REGISTERS7_1: public memory_region< PAGING::KB<8> >
{
	REGISTERS7_1( const char *name, unsigned long color, unsigned long priority );
	void store32(unsigned long addr, unsigned long value);
	void store16(unsigned long addr, unsigned long value);
	void store8(unsigned long addr, unsigned long value);
	void store32_array(unsigned long addr, int num, unsigned long *data);
	unsigned long load32(unsigned long addr);
	unsigned long load16u(unsigned long addr);
	unsigned long load16s(unsigned long addr);
	unsigned long load8u(unsigned long addr);
	void load32_array(unsigned long addr, int num, unsigned long *data);
};

#endif
