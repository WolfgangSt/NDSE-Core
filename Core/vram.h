#ifndef _VRAM_H_
#define _VRAM_H_

#include "MemMap.h"

struct vram
{
	static void remap();
	static void init();
};

template <typename T> struct vram_region: public memory_region<T>
{
	vram_region( const char *name, unsigned long color, unsigned long priority )
		: memory_region<T>(name, color, priority)
	{
		// fast version can use PAGE_WRITEPROT8
		// all specializing accesses gains would be a more detailed log info
		// but at severe speed costs
		
		//for (int i = 0; i < PAGES; i++)
		//	blocks[i].flags |= memory_block::PAGE_ACCESSHANDLER;

		for (int i = 0; i < memory_region<T>::PAGES; i++)
			memory_region<T>::blocks[i].flags |= memory_block::PAGE_WRITEPROT8;
	}

	virtual void store32(unsigned long addr, unsigned long value)
	{
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		*(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]) = value; // needs swizzle?
		b->dirty();
	}

	virtual void store16(unsigned long addr, unsigned long value)
	{ 
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		*(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]) = (unsigned short)value;
		b->dirty();
	}
	virtual void store8(unsigned long /*addr*/, unsigned long /*value*/) 
	{ 		
		logging<_DEFAULT>::log("Warning DS doesnt allow 8bit stores to VRAM!");
		DebugBreak_();		
	}

	virtual void store32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/) 
	{ 
		logging<_DEFAULT>::log("Store multiple not supported to VRAM");
		DebugBreak_(); 
	}
	virtual unsigned long load32(unsigned long addr)
	{ 
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		return _rotr(*(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]), (addr & 3) << 3);
	}

	virtual unsigned long load16u(unsigned long addr)
	{ 
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		return *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
	}

	virtual unsigned long load16s(unsigned long addr)
	{
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		return *(signed short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
	}

	virtual unsigned long load8u(unsigned long addr)
	{ 
		memory_block *b = memory_map<_ARM9>::addr2page(addr);
		return *(unsigned char*)(&b->mem[addr & (PAGING::ADDRESS_MASK)]);
	}

	virtual void load32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/)
	{ 
		logging<_DEFAULT>::log("Store multiple not supported to VRAM");
		DebugBreak_(); 
	}
};

#endif
