#ifndef _RUNNER_H_
#define _RUNNER_H_

#include "BreakpointBase.h"
#include "Breakpoint.h"

#ifndef WIN32
#define UlongToPtr(l) (void*)l
#endif

bool check_mem_access(void *start, size_t sz);

template <typename T> class breakpoints_base;
template <typename T, typename U> class breakpoints;

template <typename T>
struct exception_context
{
	static host_context context;
};
template <typename T> host_context exception_context<T>::context;

// This definitly needs some more reliable way
// i.) currently having issues when R15 is out of date
//     (crash within R15 writing instruction after write but before jump)
//
// use processor::last_page to trace back
// for current issues regarding this see processor.h
// also needs R15 to be up to date otherwise the crash 
// address wont be resolved for the right page!
// for catching failed branches R15 could be backed up, too
//
// the current interrupt system uses an imprecise interrupt
// schema in that it just switches context whenever triggered
// and runs into a safe state as recompilation of the paged
// stopped within occurs.
// could be made precides by doing the safe run before dispatch

// move to pseudo struct to make it gcc compatible
template <typename T, typename U>
void update_breakinfo(char *eip, const compiled_block<U> *block)
{
	// TODO: handle CPU mode
	unsigned long R15 = processor<T>::ctx().regs[15];
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
}

template <typename T>
char* resolve_eip_v2()
{
	memory_block *fault_page = processor<T>::last_page;
	const compiled_block<IS_ARM> *ba = fault_page->get_jit<T, IS_ARM>();
	const compiled_block<IS_THUMB> *bt = fault_page->get_jit<T, IS_THUMB>();
	const char* ba1;// = ba->code;
	const char* ba2;// = ba->code + ba->code_size;
	const char* bt1;// = bt->code;
	const char* bt2;// = bt->code + bt->code_size;

	if (ba)
	{
		ba1 = ba->code;
		ba2 = ba->code + ba->code_size;;
	} else
	{
		ba1 = 0;
		ba2 = 0;
	}

	if (bt)
	{
		bt1 = bt->code;;
		bt2 = bt->code + bt->code_size;
	} else
	{
		bt1 = 0;
		bt2 = 0;
	}

	exception_context<T>::context.addr_resolved = false;
	char *ip = (char*)UlongToPtr(
		CONTEXT_EIP(exception_context<T>::context.ctx.uc_mcontext));
	char **p = (char**)UlongToPtr(
		CONTEXT_ESP(exception_context<T>::context.ctx.uc_mcontext));

	for (;;)
	{
		if (ip >= ba1 && ip < ba2)
		{
			// resolved!
			update_breakinfo<T, IS_ARM>(ip, ba);
			return ip;
		}
		if (ip >= bt1 && ip < bt2)
		{
			// resolved!
			update_breakinfo<T, IS_THUMB>(ip, bt);
			return ip;
		}
		// ip was wrong try stackwalk
		if ( !check_mem_access( p, sizeof(char*)) )
			return 0;
		ip = *p++;
	}
}

template <typename T> struct runner
{
	enum { STACK_SIZE = 4096*64 }; // 64K reserved for JIT + HLE funcs
	typedef void (*jit_function)();

	static Fiber *fiber;
	static Fiber::fiber_cb *cb;
	static breakpoint_defs::break_info *repatch;
	static const source_set* skipsrc;
	static breakpoint_defs::stepmode skipmode;
	static unsigned long skip_instructions;



	template <typename U>
	static jit_function get_entry(unsigned long addr)
	{
		// TODO: handle CPU mode
		processor<T>::ctx().regs[15] = addr & (~PAGING::ADDRESS_MASK);
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

	static void jit_break()
	{
		// set a breakpoint at each ARM line
		// warning this is not thredsafe in regards to normal breakpoints!
		// the executing fiber is also required to be in a sleeping state
		// before execution
		// an unavoidable race condition would occur when the fiber
		// broke within a branching instruction to a different page
		//
		// better approach: 
		// 1. break fiber
		// 2. resolve arm instruction through stackwalk + processor::last_page
		// 3. use single step into arm breakpoint
	}

	static bool jit_continue()
	{
		// TODO: ARM memory might have been invalidated meantime
		// check if break was on a ARM line and recompile if needed
		// this has quite some problems though

		// if there still is a breakpoint at the current location
		// unpatch it single step in host using TF
		// and then repatch the BP finally continue execution
		// the repatch is done in the fiber_cb
		breakpoint_defs::break_info *bi =
			breakpoints_base<T>::resolve( (char*)UlongToPtr(CONTEXT_EIP(fiber->context.uc_mcontext)) );
		if (bi && bi->patched)
		{
			repatch = bi;
			*bi->pos = bi->original_byte;
			CONTEXT_EFLAGS(fiber->context.uc_mcontext) |= (1 << 8); // set trap flag
		}

		CONTEXT_EIP(fiber->context.uc_mcontext) += skip_instructions;
		skip_instructions = 0;


		fiber->do_continue();
		return true;
	}

	static void jit_rebranch(unsigned long addr)
	{
		// resolve new entrypoint
		jit_function jit_code;
		if (addr & 1)
			jit_code = get_entry<IS_THUMB>(addr);
		else
			jit_code = get_entry<IS_ARM>(addr);
		if (!jit_code)
			return;

		// set EIP and R15 according to addr
		// TODO: handle CPU mode
		processor<T>::ctx().regs[15] = addr;
		// could safe the next call as evaluated within get_entry() above
		processor<T>::last_page = memory_map<T>::addr2page( addr );
		CONTEXT_EIP(fiber->context.uc_mcontext) = PtrToUlong(jit_code);
	}

	static void internal_cb(Fiber *f)
	{
		static bool initialized = false;
		if (repatch)
		{
			// single step caused this
			*repatch->pos = DEBUG_BREAK;
			repatch = 0;
			f->do_continue();
			return;
		}

		exception_context<T>::context.ctx = f->context;
		if (initialized)
			breakpoints_base<T>::trigger( resolve_eip_v2<T>() );
		else
		{
			initialized = true;
			CONTEXT_EBX(f->context.uc_mcontext) = 0;
			CONTEXT_EBP(f->context.uc_mcontext) = PtrToUlong(&processor<T>::ctx());
		}
		if (skipsrc)
			if (skipcb())
				return;
		cb(f);
	}

	static void jit_init(Fiber::fiber_cb c)
	{
		cb = c;
		fiber = Fiber::create( STACK_SIZE, internal_cb);
	}

	static bool skipcb()
	{
		// check if current trigger was in the source set
		// if so continue stepping
		breakpoint_defs::break_info *bi = breakpoints_base<T>::get_last_error();
		if (bi)
		{
			unsigned long addr = bi->block->addr;
			// if the address is within the source set continue stepping
			for (source_set::info_set::const_iterator it =
				skipsrc->set.begin(); it != skipsrc->set.end(); ++it)
			{
				source_info *si = *it;
				if ((addr >= si->lopc) && (addr < si->hipc)) // inside block?
				{
					// continue
					// TODO: handle CPU mode
					if (processor<T>::ctx().regs[15] & 1)
						breakpoints<T, IS_THUMB>::step(skipmode);
					else breakpoints<T, IS_ARM>::step(skipmode);
					jit_continue();
					return true;
				}
			}
		}
		return false;
	}

	static void skipline(const source_set *line, breakpoint_defs::stepmode mode)
	{
		skipsrc = line;
		skipmode = mode;
		skipcb();
	}

};

template <typename T> Fiber* runner<T>::fiber;
template <typename T> Fiber::fiber_cb* runner<T>::cb;
template <typename T> breakpoint_defs::break_info* runner<T>::repatch = 0;
template <typename T> const source_set* runner<T>::skipsrc = 0;
template <typename T> breakpoint_defs::stepmode runner<T>::skipmode;
template <typename T> unsigned long runner<T>::skip_instructions = 0;

#endif

