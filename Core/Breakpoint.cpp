#include "Breakpoint.h"
#include "Compiler.h"

template <typename T>
void breakpoints_base<T>::trigger(char *ip)
{
	// breakpoint has been triggered check if it is onetime and flip
	break_info *bi = resolve(ip);
	if (bi)
	{	
		// copy block and line info to temp buffers
		// since the toggle might delete the information
		last_block = *bi->block;
		last_error = &last_block.jit_line[bi->subaddr];
		has_last_error = true;

		if (breakpoints<_ARM9, IS_ARM>::is_singlestep( bi ))
			breakpoints<_ARM9, IS_ARM>::unset_onetime();

		if (breakpoints<_ARM9, IS_THUMB>::is_singlestep( bi ))
			breakpoints<_ARM9, IS_THUMB>::unset_onetime();
	} else has_last_error = false;
}

void instanciate_trigger()
{
	breakpoints_base<_ARM9>::trigger(0);
	breakpoints_base<_ARM7>::trigger(0);
}
