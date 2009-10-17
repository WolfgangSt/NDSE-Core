/*

*/
#include "Mem.h"
#include "Breakpoint.h"
#include "Logging.h"
#include "Compiler.h"


const char* IS_ARM::name = "Arm";
const char* IS_THUMB::name = "Thumb";

template <> compiled_block<IS_THUMB>* &compile_info::get<IS_THUMB>() { return thumb; };
template <> compiled_block<IS_ARM>* &compile_info::get<IS_ARM>()   { return arm; };
template <> compiled_block<IS_ARM>* &memory_block::get_jit<_ARM9, IS_ARM>() { return arm9.get<IS_ARM>(); }
template <> compiled_block<IS_ARM>* &memory_block::get_jit<_ARM7, IS_ARM>() { return arm7.get<IS_ARM>(); }
template <> compiled_block<IS_THUMB>* &memory_block::get_jit<_ARM9, IS_THUMB>() { return arm9.get<IS_THUMB>(); }
template <> compiled_block<IS_THUMB>* &memory_block::get_jit<_ARM7, IS_THUMB>() { return arm7.get<IS_THUMB>(); }


memory_block::memory_block()
{
	flags = 0;
	arm9.arm = 0;
	arm9.thumb = 0;
	arm7.arm = 0;
	arm7.thumb = 0;
	recompiles = 0;
}

/*
template <>
void memory_block::recompile<_ARM7>()
{
	if (arm7.arm)
		delete arm7.arm;
	arm7.arm = new compiled_block<IS_ARM>(this);
}
*/

template <typename T, typename U>
struct adjust_breakpoints
{
	static void callback(char* /*addr*/, breakpoint_defs::break_data* bi)
	{
		// this somehow destroys remap tables!
		breakpoints<T,U>::update_breakdata(bi);
	}
};

template <typename T, typename U> void memory_block::recompile()
{
	recompiles++;
	if (recompiles == 100)
		logging<T>::logf("Performance warning: Page %p recompiled 100 times.", this);
	compiled_block<U>* &b = get_jit<T, U>();
	if (b)
		delete b;
	b = new compiled_block<U>(this);
	compiler::compile<T,U>(*b);
	breakpoints<T,U>::template for_region< adjust_breakpoints<T,U> >::f( mem, mem + PAGING::SIZE );
}


void memory_block::flush()
{
	if (flags & PAGE_DIRTY_J7)
	{
		flags &= ~PAGE_DIRTY_J7;
		if ( get_jit<_ARM7, IS_ARM>() )
			recompile<_ARM7, IS_ARM>();
		if ( get_jit<_ARM7, IS_THUMB>() )
			recompile<_ARM7, IS_THUMB>();
	}

	if (flags & PAGE_DIRTY_J9)
	{
		flags &= ~PAGE_DIRTY_J9;
		if ( get_jit<_ARM9, IS_ARM>() )
			recompile<_ARM9, IS_ARM>();
		if ( get_jit<_ARM9, IS_THUMB>() )
			recompile<_ARM9, IS_THUMB>();
	}
}

bool memory_block::react()
{
	if (flags & PAGE_DIRTY_REACTOR)
	{
		flags &= ~PAGE_DIRTY_REACTOR;
		return true;
	}
	return false;
}
