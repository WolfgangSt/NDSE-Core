#ifndef _MEM_H_
#define _MEM_H_

#include <utility>
#include "Namespaces.h"
#include "forward.h"

// used to synchronize memory updates
#ifdef __GNUC__
#define _InterlockedOr(p,v) __sync_fetch_and_or(p, v)
//#define _InterlockedOr(p,v) (*p |= v)
#else
#include <intrin.h>
#pragma intrinsic (_InterlockedOr)
#pragma intrinsic (_InterlockedExchange)
#endif

template <int n> struct verify_zero { verify_zero<n> error;  };
template <>      struct verify_zero<0> {};

struct IS_ARM
{
	typedef unsigned long T;
	enum { INSTRUCTION_SIZE_LG2 = 2 };
	enum { INSTRUCTION_SIZE = 1 << INSTRUCTION_SIZE_LG2 };
	static const char *name;
};

struct IS_THUMB
{
	typedef unsigned short T;
	enum { INSTRUCTION_SIZE_LG2 = 1 };
	enum { INSTRUCTION_SIZE = 1 << INSTRUCTION_SIZE_LG2 };
	static const char *name;
};

struct PAGING
{
	enum { SIZE_BITS = 9 };
	enum { TOTAL_PAGES = 1 << (32-SIZE_BITS) };
// DO NOT MODIFY BELOW HERE
	enum { SIZE = 1 << SIZE_BITS }; // 512 is the highest granularity due to palette mapping
	enum { ADDRESS_MASK = SIZE-1 };

	template <typename T> struct INST { enum { NUM = SIZE >> T::INSTRUCTION_SIZE_LG2 }; };

	template <unsigned int n> struct B { enum { SIZE = n }; };
	template <unsigned int n> struct KB { enum { SIZE = 1024 * n }; };
	template <unsigned int n> struct MB { enum { SIZE = 1048576 * n}; };
	template <unsigned int n> struct PAGES : public verify_zero<n % SIZE>
	{ 
		enum { SIZE = n / PAGING::SIZE }; // when used to calculate #pages
		enum { PAGE = SIZE };             // when used to index pages
		enum { BYTES = n };
	};
	
	static std::pair<unsigned long, unsigned long> REGION(unsigned long low, unsigned long high)
	{
		// verify granularity and return mapped region
		return std::pair<unsigned long, unsigned long>( 
			low / PAGES<SIZE>::BYTES, high / PAGES<SIZE>::BYTES );
	};
};

// a raw memory block (physical page) of the emulated system

struct compile_info
{
	compiled_block<IS_ARM> *arm;
	compiled_block<IS_THUMB> *thumb;
	template <typename T> compiled_block<T>* &get();
};
/*
template <> compiled_block<IS_THUMB>* &compile_info::get<IS_THUMB>() { return thumb; };
template <> compiled_block<IS_ARM>* &compile_info::get<IS_ARM>()   { return arm; };
*/

struct memory_block /* final, do not inherit! */
{
	enum {
		PAGE_READPROT      = 0x01, // page cannot be read from
		PAGE_WRITEPROT     = 0x02, // page cannot be written to
		PAGE_EXECPROT      = 0x04, // page cannot be executed
		PAGE_INVALID       = 0x08, // page does not exist for client
		PAGE_NULL          = PAGE_INVALID | PAGE_EXECPROT | PAGE_WRITEPROT | PAGE_READPROT,

		// custom plugins might add dirty flags here
		PAGE_DIRTY_J7      = 0x10, // JIT uses this for optimizing recompiling
		PAGE_DIRTY_J9      = 0x20, // code pages on ARM7 and ARM9
		PAGE_DIRTY_REACTOR = 0x40,
		PAGE_DIRTY_VRAM    = 0x80,
		PAGE_DIRTY_SRAM    = 0x100,
		PAGE_DIRTY         = PAGE_DIRTY_J7 | PAGE_DIRTY_J9 | PAGE_DIRTY_REACTOR | PAGE_DIRTY_VRAM |
							 PAGE_DIRTY_SRAM,

		PAGE_ACCESSHANDLER = 0x1000, // page needs special mem access handling
		PAGE_WRITEPROT8    = 0x2000  // fast write protection handler for VRAM
	};

	typedef void (*mem_callback)(memory_block *block);  
	typedef void (*read_callback)(memory_block *block, unsigned long addr);

	unsigned long flags;
	// todo: blocks for thumb modes

	compile_info arm7;
	compile_info arm9;
	memory_region_base *base;
	unsigned long recompiles;

	template <typename T, typename U> compiled_block<U>* &get_jit();

	char mem[PAGING::SIZE];

	template <typename T, typename U> void recompile();
	bool react();

	inline void dirty()
	{
		_InterlockedOr( (long*)&flags, PAGE_DIRTY );
	}

	memory_block();
	void flush();
};
/*
template <> compiled_block<IS_ARM>* &memory_block::get_jit<_ARM9, IS_ARM>() { return arm9.get<IS_ARM>(); }
template <> compiled_block<IS_ARM>* &memory_block::get_jit<_ARM7, IS_ARM>() { return arm7.get<IS_ARM>(); }
template <> compiled_block<IS_THUMB>* &memory_block::get_jit<_ARM9, IS_THUMB>() { return arm9.get<IS_THUMB>(); }
template <> compiled_block<IS_THUMB>* &memory_block::get_jit<_ARM7, IS_THUMB>() { return arm7.get<IS_THUMB>(); }
*/

#endif
