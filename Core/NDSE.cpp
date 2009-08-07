// NDSE.cpp : Defines the entry point for the console application.
//

#ifdef WIN32
#define EXPORT __declspec(dllexport)
#endif

#include <iostream>
#include <fstream>
#include <map>
#include <assert.h>
#include <distorm.h>

#include <sstream> // debug

#include "Namespaces.h"
#include "Mem.h"
#include "MemMap.h"
#include "PhysMem.h"

#include "Disassembler.h"
#include "Util.h"
#include "Compiler.h"
#include "Breakpoint.h"
#include "Logging.h"
#include "BIOS.h"
#include "HLE.h"
#include "loader_elf.h"
#include "Processor.h"
#include "io_observe.h"
#include "SourceDebug.h"

#include "NDSE.h"
#include "osdep.h"
#include "runner.h"
#include "interrupt.h"
#include "vram.h"


template <typename T>
class hle_block: public compiled_block<T>
{
public:
	hle_block()
	{
		static char retcode = '\xC3';
		mprotect(&retcode, 1, PROT_READ | PROT_WRITE | PROT_EXEC);
		compiled_block<T>::code = &retcode;
		compiled_block<T>::code_size = 1;
		for (int i = 0; i < compiled_block<T>::REMAPS; i++)
			compiled_block<T>::remap[i] = compiled_block<T>::code;
	}
	~hle_block()
	{
		compiled_block<T>::code = 0;
		compiled_block<T>::code_size = 0;
	}
};
hle_block<IS_ARM>   arm_hle;
hle_block<IS_THUMB> thumb_hle;

void STDCALL Init()
{
	symbols::init();
	memory::null_region.blocks[0].flags = memory_block::PAGE_NULL;
	memory_block &b = memory::hle_bios.blocks[0];
	b.flags = memory_block::PAGE_NULL &~ memory_block::PAGE_EXECPROT;
	b.arm9.arm   = &arm_hle;
	b.arm9.thumb = &thumb_hle;
	b.arm7 = b.arm9;

	processor<_ARM7>::reset();
	processor<_ARM9>::reset();
	loader_elf::init();
	vram::init();
	//io_observer::init();

	InitHLE();
}

unsigned long STDCALL PageSize()
{
	return PAGING::SIZE_BITS;
}

unsigned long STDCALL DebugMax()
{
	return breakpoint_defs::MAX_SUBINSTRUCTIONS;
}

memory_block* STDCALL ARM7_GetPage(unsigned long addr)
{
	return memory_map<_ARM7>::addr2page(addr);
}

memory_block* STDCALL ARM9_GetPage(unsigned long addr)
{
	return memory_map<_ARM9>::addr2page(addr);
}

void STDCALL MEM_FlushPage(memory_block *p)
{
	p->flush();
}

memory_region_base* STDCALL MEM_GetVRAM(int bank)
{
	switch (bank)
	{
	case 0: return &memory::vram_a;
	case 1: return &memory::vram_b;
	case 2: return &memory::vram_c;
	case 3: return &memory::vram_d;
	case 4: return &memory::vram_e;
	case 5: return &memory::vram_f;
	case 6: return &memory::vram_g;
	case 7: return &memory::vram_h;
	case 8: return &memory::vram_i;
	}
	return 0;
}

unsigned short STDCALL UTIL_GetCRC16(unsigned short crc, char* addr, int len)
{
	return util::get_crc16( crc, addr, len );
}

bool STDCALL UTIL_LoadFile(const char *filename, util::load_result *result, util::load_hint lh)
{
	return util::load_file(filename, *result, lh);
}

unsigned short STDCALL BIOS_ARM9_GetCRC16(unsigned short crc, unsigned long addr, int len)
{
	return bios<_ARM9>::get_crc16( crc, addr, len );
}

unsigned short STDCALL BIOS_ARM7_GetCRC16(unsigned short crc, unsigned long addr, int len)
{
	return bios<_ARM7>::get_crc16( crc, addr, len );
}


// Wrapper for streaming export
struct stream_cb
{
	typedef void (STDCALL *callback)(memory_block*, char*, int, void*);
	struct context
	{
		callback cb;
		void *user;

		context(callback cb, void *user) : cb(cb), user(user) {}
	};

	static void process(memory_block *b, char *addr, int len, context &ctx)
	{
		ctx.cb( b, addr, len, ctx.user );
	}
};

void STDCALL ARM7_Stream(unsigned long addr, int len, stream_cb::callback cb, void *user)
{
	stream_cb::context ctx(cb, user);
	memory_map<_ARM7>::process_memory<stream_cb>( addr, len, ctx );
}

void STDCALL ARM9_Stream(unsigned long addr, int len, stream_cb::callback cb, void *user)
{
	stream_cb::context ctx(cb, user);
	memory_map<_ARM9>::process_memory<stream_cb>( addr, len, ctx );
}


#ifdef WIN32
#define THREADVAR __declspec(thread)
#else
#define THREADVAR __thread
#endif

const char* STDCALL ARM9_DisassembleA(unsigned long op, unsigned long addr)
{
	disassembler d;
	d.decode<IS_ARM>(op, addr);
	static THREADVAR char buffer[256];
	d.get_str(buffer);
	return buffer;
}

const char* STDCALL ARM9_DisassembleT(unsigned long op, unsigned long addr)
{
	disassembler d;
	d.decode<IS_THUMB>(op, addr);
	static THREADVAR char buffer[256];
	d.get_str(buffer);
	return buffer;
}

// currently no distinguishing between ARM9 and ARM7
const char* STDCALL ARM7_DisassembleA(unsigned long op, unsigned long addr)
{
	return ARM9_DisassembleA(op, addr);
}
const char* STDCALL ARM7_DisassembleT(unsigned long op, unsigned long addr)
{
	return ARM9_DisassembleT(op, addr);
}

void STDCALL ARM9_Init(Fiber::fiber_cb cb)
{
	runner<_ARM9>::jit_init(cb);
}

void STDCALL ARM7_Init(Fiber::fiber_cb cb)
{
	runner<_ARM7>::jit_init(cb);
}

emulation_context* STDCALL ARM9_GetContext()
{
	return &processor<_ARM9>::context[0];
}

emulation_context* STDCALL ARM7_GetContext()
{
	return &processor<_ARM7>::context[0];
}

cpu_mode STDCALL ARM9_GetMode()
{
	return processor<_ARM9>::mode;
}

cpu_mode STDCALL ARM7_GetMode()
{
	return processor<_ARM7>::mode;
}


bool STDCALL ARM9_Continue()
{
	return runner<_ARM9>::jit_continue();
	//return do_continue<_ARM9>();
}

bool STDCALL ARM7_Continue()
{
	return runner<_ARM7>::jit_continue();
	//return do_continue<_ARM7>();
}

void STDCALL ARM9_ToggleBreakpointA(unsigned long addr, unsigned long subcode )
{
	breakpoints<_ARM9, IS_ARM>::toggle( addr, subcode );
}

void STDCALL ARM9_ToggleBreakpointT(unsigned long addr, unsigned long subcode )
{
	breakpoints<_ARM9, IS_THUMB>::toggle( addr, subcode );
}

void STDCALL ARM7_ToggleBreakpointA(unsigned long addr, unsigned long subcode )
{
	breakpoints<_ARM7, IS_ARM>::toggle( addr, subcode );
}

void STDCALL ARM7_ToggleBreakpointT(unsigned long addr, unsigned long subcode )
{
	breakpoints<_ARM7, IS_THUMB>::toggle( addr, subcode );
}

// installs breakpoints for single stepping
void STDCALL ARM9_Step(breakpoint_defs::stepmode mode)
{
	// TODO: handle CPU mode
	if (processor<_ARM9>::ctx().regs[15] & 1)
		breakpoints<_ARM9, IS_THUMB>::step(mode);
	else breakpoints<_ARM9, IS_ARM>::step(mode);
}

void STDCALL ARM7_Step(breakpoint_defs::stepmode mode)
{
	// TODO: handle CPU mode
	if (processor<_ARM7>::ctx().regs[15] & 1)
		breakpoints<_ARM7, IS_THUMB>::step(mode);
	else breakpoints<_ARM7, IS_ARM>::step(mode);
}

const breakpoint_defs::break_data* STDCALL ARM9_GetBreakpointA(unsigned long addr)
{
	return breakpoints<_ARM9, IS_ARM>::get( addr );
}

const breakpoint_defs::break_data* STDCALL ARM9_GetBreakpointT(unsigned long addr)
{
	return breakpoints<_ARM9, IS_THUMB>::get( addr );
}

const breakpoint_defs::break_data* STDCALL ARM7_GetBreakpointA(unsigned long addr)
{
	return breakpoints<_ARM7, IS_ARM>::get( addr );
}

const breakpoint_defs::break_data* STDCALL ARM7_GetBreakpointT(unsigned long addr)
{
	return breakpoints<_ARM7, IS_THUMB>::get( addr );
}

void STDCALL DEFAULT_Log(log_callback cb) { logging<_DEFAULT>::cb = cb; }
void STDCALL ARM7_Log(log_callback cb) { logging<_ARM7>::cb = cb; }
void STDCALL ARM9_Log(log_callback cb) { logging<_ARM9>::cb = cb; }


callstack_context* STDCALL ARM7_Callstack()
{
	return &callstack_tracer<_ARM7>::ctx;
}


callstack_context* STDCALL ARM9_Callstack()
{
	return &callstack_tracer<_ARM9>::ctx;
}


breakpoint_defs::break_info* STDCALL ARM9_GetDebuggerInfo()
{
	return breakpoints_base<_ARM9>::get_last_error();
}

breakpoint_defs::break_info* STDCALL ARM7_GetDebuggerInfo()
{
	return breakpoints_base<_ARM7>::get_last_error();
}

host_context* STDCALL ARM9_GetException()
{
	return &exception_context<_ARM9>::context;
}

host_context* STDCALL ARM7_GetException()
{
	return &exception_context<_ARM7>::context;
}


const char* STDCALL DEBUGGER_GetSymbol(void *addr)
{
	symbols::symmap::const_iterator it = symbols::syms.find( addr );
	if (it == symbols::syms.end())
		return 0;
	return it->second;
};

const wchar_t* STDCALL DEBUGGER_GetFilename(int fileno)
{
	if ((fileno < 0) || (fileno >= (int)source_debug::files.size()))
		return 0;
	return source_debug::files[fileno].filename.c_str();
}

const source_info* STDCALL DEBUGGER_SourceLine(int fileno, int lineno, int idx)
{
	if ((fileno < 0) || (fileno > (int)source_debug::files.size()) || (lineno < 0) || (idx < 0))
		return 0;

	std::vector<source_infos*> &sil = source_debug::files[fileno].lineinfo;
	if (lineno >= (int)sil.size())
		return 0;
	source_infos *sis = sil[lineno];
	if (!sis)
		return 0;
	if (idx >= (int)sis->size())
		return 0;
	std::list<source_info*>::iterator it = sis->begin();
	while (idx--)
		++it;
	return *it;
}


void STDCALL ARM9_GetJITA(unsigned long addr, jit_code *code)
{
	return breakpoints<_ARM9, IS_ARM>::get_jit( addr, *code );
}

void STDCALL ARM7_GetJITA(unsigned long addr, jit_code *code)
{
	return breakpoints<_ARM7, IS_ARM>::get_jit( addr, *code );
}

void STDCALL ARM9_GetJITT(unsigned long addr, jit_code *code)
{
	return breakpoints<_ARM9, IS_THUMB>::get_jit( addr, *code );
}

void STDCALL ARM7_GetJITT(unsigned long addr, jit_code *code)
{
	return breakpoints<_ARM7, IS_THUMB>::get_jit( addr, *code );
}

void STDCALL ARM7_SetPC(unsigned long addr)
{
	return runner<_ARM7>::jit_rebranch(addr);
}


void STDCALL ARM9_SetPC(unsigned long addr)
{
	return runner<_ARM9>::jit_rebranch(addr);
}

source_info* STDCALL ARM7_SourceLine(unsigned long addr, int idx)
{
	// not differentiated yet for ARM7/9
	// source debugging infos are SHARED for the moment ...
	return ARM9_SourceLine(addr, idx);
	//return 0;
}


void STDCALL ARM9_Interrupt(unsigned long intr)
{
	interrupt<_ARM9>::fire(intr);
}

void STDCALL ARM7_Interrupt(unsigned long intr)
{
	interrupt<_ARM7>::fire(intr);
}

source_info* STDCALL ARM9_SourceLine(unsigned long addr, int idx)
{
	const source_set *set = source_debug::line_for(addr);
	if (!set)
		return 0;

	if (idx >= 0)
	{
		source_set::info_set::const_iterator it = set->set.begin();
		source_set::info_set::const_iterator end = set->set.end();
		while (idx--)
		{
			++it;
			if (it == end)
				return 0;
		}
		return *it;
	} else
	{
		source_set::info_set::const_reverse_iterator it = set->set.rbegin();
		source_set::info_set::const_reverse_iterator end = set->set.rend();
		idx++;
		while (idx < 0)
		{
			++it;
			if (it == end)
				return 0;
		}
		return *it;
	}
}

#if 0
int main(int /*argc*/, char** /*argv*/)
{
	for (int i = -10; i < 10; i++)
	{
		std::cout << i << ": " << (1 << (1 << i)) << "\n";
	}

	Init();
	util::load_result result;
	util::load_file("Test\\main.srl", result);
	
	std::cout << "Press enter";
	std::cin.get();
	return 0;
}
#endif

