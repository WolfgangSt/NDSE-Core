#ifndef _HLE_H_
#define _HLE_H_

#include <map>
#include "Mem.h"
//#include "runner.h"
//#include "Compiler.h"

#ifdef WIN32
#define FASTCALL(x) __fastcall x
#define FASTCALL_IMPL(x) FASTCALL(x)
#define NAKEDCALL_IMPL(x) __declspec(naked) __fastcall x

#else
#define FASTCALL(x) x; __attribute__((fastcall))
#define FASTCALL_IMPL(x) __attribute__((fastcall)) x
#define NAKEDCALL_IMPL(x) __attribute__((fastcall)) __attribute__((naked)) x
#endif

enum {
	NOCASH_DEBUGOUT = 0x6464,
	NOCASH_EXT_HALT = 0x9090,
	NOCASH_EXT_VERS = 0x5344,
	NOCASH_EXT_SCRI = 0x3841
};

enum { NDSE_VERSION = 1 };

struct stream_debugstring
{
	enum { MAX_LEN = 4096 };
	struct context
	{
		char string[MAX_LEN];
		bool running;
		unsigned long pos;
	};

	static void process(memory_block *b, char *mem, int sz, context &ctx)
	{
		if (!ctx.running)
			return;
		if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		{
		end:
			ctx.string[ctx.pos++] = 0;
			ctx.running = 0;			
		} else
		{
			for (int i = 0; i < sz; i++)
			{
				ctx.string[ctx.pos++] = mem[i];
				if (mem[i] == 0)
					goto end;
			}
		}
	}
};

struct emulation_context;
template <typename T>
struct HLE
{
private:
	static char* FASTCALL(compile_and_link_branch_a_real(unsigned long addr));
	
	static void IntrWait();       // SWI 4h
	static void sqrt();           // SWI 8h
	static void CpuSet();         // SWI Bh
	static void crc16();          // SWI Eh
	static void LZ77UnCompVram(); // SWI 12h
	static unsigned long last_halt; // used by NOCASH_EXT_HALT
public:
	static void init();
	static void invoke(unsigned long addr, emulation_context *ctx);

	static void invalid_read(unsigned long addr);
	static void invalid_write(unsigned long addr);
	static void invalid_branch(unsigned long addr);

	static void FASTCALL(store32(unsigned long addr, unsigned long value));
	static void FASTCALL(store16(unsigned long addr, unsigned long value));
	static void FASTCALL(store8(unsigned long addr, unsigned long value));
	static void store32_array(unsigned long addr, int num, unsigned long *data);

	static unsigned long FASTCALL(load32(unsigned long addr));
	static unsigned long FASTCALL(load16u(unsigned long addr));
	static unsigned long FASTCALL(load16s(unsigned long addr));
	static unsigned long FASTCALL(load8u(unsigned long addr));
	static void load32_array(unsigned long addr, int num, unsigned long *data);

	static char compile_and_link_branch_a[7];
	static char invoke_arm[19];

	static void FASTCALL(is_priviledged());
	static void FASTCALL(remap_tcm(unsigned long value, unsigned long mode));
	static void FASTCALL(pushcallstack(unsigned long addr));
	static void FASTCALL(popcallstack(unsigned long addr));
	static void FASTCALL(swi(unsigned long idx));
	static void FASTCALL(debug_magic(unsigned long addr))
	{
		unsigned short magic, flags;
		memory_block *b;
		if (addr & 1)
		{
			// thumb mode
			addr++; // skip branch
			// try reading a word
			b = memory_map<T>::addr2page(addr);
			if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
				return;
			magic = *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
			addr += 2;
			b = memory_map<T>::addr2page(addr);
			if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
				return;
			flags = *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
		} else
		{
			// arm mode
			addr += 4; // skip branch

			// must be 4b aligned so read magic + flags
			b = memory_map<T>::addr2page(addr);
			if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
				return;
			magic = *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
			addr += 2;
			flags = *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
		}
		addr += 2;
		switch (magic)
		{
		case NOCASH_DEBUGOUT:
			// stream till terminating 0 or max 4kb
			{
				stream_debugstring::context ctx;
				ctx.pos = 0;
				ctx.running = true;
				// pull the string
				memory_map<T>::process_memory<stream_debugstring>( 
					addr, stream_debugstring::MAX_LEN, ctx );
				logging<T>::log(ctx.string);
			}
			break;
		case NOCASH_EXT_HALT:
			// break
			if (last_halt != addr)
			{
				last_halt = addr;
				logging<T>::logf("Emulator Software Breakpoint at %08X", addr);
				
				runner<T>::skip_instructions = 1;
				DebugBreak_();
				/*
				__try {
					DebugTrap_();
				}  __except(1)
				{
					std::cout << "Break!\n";
				}*/
			}
			break;
		case NOCASH_EXT_VERS:
			processor<T>::context.regs[0] = NDSE_VERSION;
			break;
		case NOCASH_EXT_SCRI:
			{
				//unsigned long addr = processor<T>::context.regs[0];
				//unsigned long size = processor<T>::context.regs[1];
				unsigned long script = processor<T>::context.regs[2];
				
				stream_debugstring::context ctx;
				ctx.pos = 0;
				ctx.running = true;
				// pull the string
				memory_map<T>::process_memory<stream_debugstring>( 
					script, stream_debugstring::MAX_LEN, ctx );
				// try to evaluate the string
				// this feature is not available yet!

				logging<T>::log("Hostcalls are not supported yet");

				processor<T>::context.regs[0] = 0;
				break;
			}
		default:
			logging<T>::logf("Unknown Debug Magic at %08X (%04X %04X)", addr, magic, flags);
		}
	}

	static void dump_btab();
};
template <typename T> unsigned long HLE<T>::last_halt = 0;

struct symbols
{
	typedef std::map<void*, const char*> symmap;
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
