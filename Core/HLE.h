#ifndef _HLE_H_
#define _HLE_H_

#include <map>
//#include "Compiler.h"

struct emulation_context;
template <typename T>
struct HLE
{
private:
	static char* __fastcall compile_and_link_branch_a_real(unsigned long addr);
	static void LZ77UnCompVram();
public:
	static void invoke(unsigned long addr, emulation_context *ctx);

	static void invalid_read(unsigned long addr);
	static void invalid_write(unsigned long addr);
	static void invalid_branch(unsigned long addr);

	static void __fastcall store32(unsigned long addr, unsigned long value);
	static void __fastcall store16(unsigned long addr, unsigned long value);
	static void __fastcall store8(unsigned long addr, unsigned long value);
	static void __cdecl store32_array(unsigned long addr, int num, unsigned long *data);

	static unsigned long __fastcall load32(unsigned long addr);
	static unsigned long __fastcall load16u(unsigned long addr);
	static unsigned long __fastcall load16s(unsigned long addr);
	static unsigned long __fastcall load8u(unsigned long addr);
	static void __cdecl load32_array(unsigned long addr, int num, unsigned long *data);

	static void __fastcall compile_and_link_branch_a(unsigned long addr);

	static void __fastcall is_priviledged();
	static void __fastcall remap_tcm(unsigned long value, unsigned long mode);
	static void __fastcall pushcallstack(unsigned long addr);
	static void __fastcall popcallstack(unsigned long addr);
	static void __fastcall swi(unsigned long idx);

	static void dump_btab();
};

struct symbols
{
	typedef std::map<void*, char*> symmap;
	static symmap syms;
	static void init();
};


struct callstack_context
{
	enum { MAX_DEPTH_BITS = 10 };
	enum { MAX_DEPTH = 1 << MAX_DEPTH_BITS };
	enum { MAX_DEPTH_MASK = MAX_DEPTH - 1 };

	unsigned long max_depth;
	unsigned long ctr;
	unsigned long stack[MAX_DEPTH];
	callstack_context()
	{
		max_depth = MAX_DEPTH;
	}
};

template <typename T>
struct callstack_tracer
{
	static callstack_context ctx;

	static void push(unsigned long addr)
	{
		// start at entry 1, so popping is easier
		ctx.ctr++;
		ctx.ctr &= callstack_context::MAX_DEPTH_MASK;
		ctx.stack[ctx.ctr] = addr;
	}

	static void pop(unsigned long addr)
	{
		if (ctx.stack[ctx.ctr] == addr)
		{
			ctx.ctr--;
			ctx.ctr &= callstack_context::MAX_DEPTH_MASK;
		}
	}
};

template <typename T> callstack_context callstack_tracer<T>::ctx;


#endif
