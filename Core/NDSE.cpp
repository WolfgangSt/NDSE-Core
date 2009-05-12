// NDSE.cpp : Defines the entry point for the console application.
//
#define EXPORT __declspec(dllexport)

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

#include "NDSE.h"
#include "osdep.h"



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
	io_observer::init();
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

const memory_region_base* STDCALL MEM_GetRegionInfo(memory_block *p)
{
	return memory::get_region_for( p );
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

bool STDCALL UTIL_LoadFile(const char *filename, util::load_result *result)
{
	return util::load_file(filename, *result);
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



template <typename T>
struct exception_context
{
	static host_context context;
};
template <typename T> host_context exception_context<T>::context;




#ifdef WIN32
bool check_mem_access(void *start, size_t sz)
{
	enum { 
		PROT = PAGE_READONLY | 
			PAGE_READWRITE |  
			PAGE_EXECUTE_READ | 
			PAGE_EXECUTE_READWRITE
	};
	char *s = (char*)start;
	MEMORY_BASIC_INFORMATION mi;

	for (;;)
	{
		if (VirtualQuery( s, &mi, sizeof(mi) ) < sizeof(mi))
			return false;
		size_t r = mi.RegionSize - (s - (char*)mi.BaseAddress);
		if (!(mi.Protect & PROT))
			return false;
		if (r >= sz)
			return true;
		sz -= r;
		s  += r;
	}
}

#else
#include <setjmp.h>

unsigned char g_ucCalledByTestRoutine = 0;
jmp_buf g_pBuf;
void check_mem_access_handler(int nSig)
{
     if ( g_ucCalledByTestRoutine ) longjmp ( g_pBuf, 1);
}
bool check_mem_access(void *start, size_t sz)
{
    char* pc;
    void(*pPrev)(int sig);
    g_ucCalledByTestRoutine = 1;
    if(setjmp(g_pBuf))
         return true;
    pPrev = signal(SIGSEGV, check_mem_access_handler);
    pc = (char*) malloc ( sz );
    memcpy ( pc, start, sz);
    free(pc);
    g_ucCalledByTestRoutine = 0;
    signal ( SIGSEGV, pPrev );
    return false;
}

#endif

// This definitly needs some more reliable way
// i.) currently having issues when R15 is out of date
//     (crash within R15 writing instruction after write but before jump)
// ii.) ebp unwinding will only work in debug mode when stack frames are 
//      enabled. Release build omits them for performance
//
// A possible (not really nice solution either though) would be using
// TF for single stepping out of the HLE till reaching either the JIT
// block R15 points at or till reaching the run/continuation call


template <typename T, typename U>
bool resolve_armpc_from_eip2(unsigned long R15, void *eip)
{
	memory_block *fault_page = memory_map<_ARM9>::addr2page(R15);
	compiled_block<U> *block = fault_page->get_jit<T, U>();

	// is eip within the block?
	if ((eip < block->code) || eip >= block->code + block->code_size)
		return false;

	// find the JIT instruction of the crash
	int i = 0;
	while ((eip >= block->remap[i+1]) && (i < block->REMAPS-1))
		i++;
	unsigned long arm_pc = (R15 & ~PAGING::ADDRESS_MASK) + (i << U::INSTRUCTION_SIZE_LG2);

	// discover the JIT subinstruction using distorm through the breakpoint interface
	breakpoint_defs::break_data bd;
	bd.addr = arm_pc;
	breakpoints<T, U>::disassemble_breakdata(&bd);
	
	unsigned long arm_jit = 0;
	while ((eip >= bd.jit_line[arm_jit+1].pos) && (arm_jit < bd.jit_instructions-1))
		arm_jit++;

	exception_context<T>::context.addr_resolved = true;
	exception_context<T>::context.addr = arm_pc;
	exception_context<T>::context.subaddr = arm_jit;
	return true;
}

template <typename T>
bool resolve_armpc_from_eip(void *eip)
{
	unsigned long R15 = processor<T>::context.regs[15];
	if (R15 & 1)
		return resolve_armpc_from_eip2<T, IS_THUMB>(R15, eip);
	else
		return resolve_armpc_from_eip2<T, IS_ARM>(R15, eip);
}

template <typename T>
char* resolve_eip()
{
#pragma warning(push)
#pragma warning(disable: 4311)
#pragma warning(disable: 4312)

	void *p = (void*)CONTEXT_EBP(exception_context<T>::context.ctx.uc_mcontext);
	void **lastp = 0;
	exception_context<T>::context.addr_resolved = false;

	// if ebp still is the context base we are either in emu JIT
	// or in a HLE that hasnt a stackframe (handle that case somehow)
	if (p == &processor<T>::context)
	{
		char *eip = (char*)CONTEXT_EIP(exception_context<T>::context.ctx.uc_mcontext);
		resolve_armpc_from_eip<T>(eip);
		return eip;
	}
	logging<T>::logf("A HLE or OS call crashed emulation");

	// scan till return is found
	while (p != &processor<T>::context)
	{
		lastp = (void**)p;
		if ( !check_mem_access( lastp, 2*sizeof(void*)) )
			return (char*)CONTEXT_EIP(exception_context<T>::context.ctx.uc_mcontext); // ebp has been screwed up
		p = *lastp;
	}
	void *eip = lastp[1];
	CONTEXT_EIP(exception_context<T>::context.ctx.uc_mcontext) = (unsigned long)eip;

	// finally try to retrieve the ARM address of the crash assuming it happened within
	// the block R15 points to
	resolve_armpc_from_eip<T>(eip);
	return (char*)eip;
#pragma warning(pop)
}


// rename this to do_init
template <typename T> struct runner
{
	enum { STACK_SIZE = 4096*64 }; // 64K reserved for JIT + HLE funcs
	static ucontext_t jit_ctx1, jit_ctx2, outer_ctx;
	static ucontext_t *jit_front;
	static ucontext_t *jit_back;
	static bool valid_state;
	typedef void (*jit_function)();

	static void jit_catch(int, siginfo_t *, void *pctx)
	{
		ucontext_t* ctx = (ucontext_t*)pctx;
		*jit_back = *ctx;
		exception_context<T>::context.ctx = *ctx;
		breakpoints_base<T>::trigger( resolve_eip<T>() );
		setcontext( &outer_ctx );
	}

	static bool jit_continue()
	{
		if (!valid_state)
			return false;
		swapcontext( &outer_ctx, jit_front );
		ucontext_t *tmp = jit_front;
		jit_front = jit_back;
		jit_back = tmp;
		return false;
	}

	template <typename U>
	static jit_function get_entry(unsigned long addr)
	{
		processor<T>::context.regs[15] = addr & (~PAGING::ADDRESS_MASK);
		memory_block *b = memory_map<T>::addr2page( addr );
		if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_EXECPROT))
			return 0;

		if (!b->get_jit<T, U>())
			b->recompile<T, U>();
		compiled_block<U> *code = b->get_jit<T, U>();

		unsigned long subaddr = addr & PAGING::ADDRESS_MASK;
		char *start = code->remap[subaddr >> U::INSTRUCTION_SIZE_LG2];
		//FlushInstructionCache( GetCurrentProcess(), start, code->code_size - (start - code->code) );
		cacheflush( start, (int)(code->code_size - (start - code->code)), ICACHE );
		return (jit_function)start;
	}

	static void jit_rebranch(unsigned long addr)
	{
		static char stack[STACK_SIZE];

		valid_state = false;
		jit_function jit_code;
		if (addr & 1)
			jit_code = get_entry<IS_THUMB>(addr);
		else
			jit_code = get_entry<IS_ARM>(addr);
		if (!jit_code)
			return;

		jit_front->uc_stack.ss_size  = STACK_SIZE; 
		jit_front->uc_stack.ss_sp    = stack;
		jit_front->uc_stack.ss_flags = 0;
		
		makecontext( jit_front,  jit_code, 0 );
		CONTEXT_EBP(jit_front->uc_mcontext) = PtrToUlong(&processor<T>::context);

		valid_state = true;
	}

	static void jit_init()
	{
		signal2( SIGINT, jit_catch );
		signal2( SIGILL, jit_catch );
		signal2( SIGSEGV, jit_catch );
		getcontext( &outer_ctx );
		getcontext( &jit_ctx1 );
		getcontext( &jit_ctx2 );
	}
};

template <typename T> ucontext_t runner<T>::jit_ctx1;
template <typename T> ucontext_t runner<T>::jit_ctx2;
template <typename T> ucontext_t runner<T>::outer_ctx;
template <typename T> ucontext_t* runner<T>::jit_front = &runner<T>::jit_ctx1;
template <typename T> ucontext_t* runner<T>::jit_back = &runner<T>::jit_ctx2;
template <typename T> bool runner<T>::valid_state = false;




bool STDCALL ARM9_Run(unsigned long addr)
{
	runner<_ARM9>::jit_init();
	runner<_ARM9>::jit_rebranch(addr);
	return runner<_ARM9>::jit_continue();
	//return do_run<_ARM9>(addr);
}

bool STDCALL ARM7_Run(unsigned long addr)
{
	runner<_ARM7>::jit_init();
	runner<_ARM7>::jit_rebranch(addr);
	return runner<_ARM7>::jit_continue();
	//return do_run<_ARM7>(addr);
}

emulation_context* ARM9_GetContext()
{
	return &processor<_ARM9>::context;
}

emulation_context* ARM7_GetContext()
{
	return &processor<_ARM7>::context;
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
	if (processor<_ARM9>::context.regs[15] & 1)
		breakpoints<_ARM9, IS_THUMB>::step(mode);
	else breakpoints<_ARM9, IS_ARM>::step(mode);
}

void STDCALL ARM7_Step(breakpoint_defs::stepmode mode)
{
	if (processor<_ARM7>::context.regs[15] & 1)
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
	//ContinuationHandler<_ARM7>::rabranchAddr = addr;
	//ContinuationHandler<_ARM7>::rebranch = true;
}


void STDCALL ARM9_SetPC(unsigned long addr)
{
	return runner<_ARM9>::jit_rebranch(addr);
	//ContinuationHandler<_ARM9>::rabranchAddr = addr;
	//ContinuationHandler<_ARM9>::rebranch = true;
}


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

