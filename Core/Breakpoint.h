#ifndef _BREAKPOINT_H_
#define _BREAKPOINT_H_

#include <boost/thread.hpp>
#include <map>
#include <distorm.h>

#include "forward.h"
#include "BreakpointBase.h"
#include "MemMap.h"
#include "SourceDebug.h"
#include "jitcode.h"

// when memory fragmentation ever gets a problem use pool allocators
// for break_data and all the maps! 

// TODO: handle single stepping breaks more safe (reusing the breakmap)
// as rebuilding on the fly using update_breakdata might lead to wrong 
// disassembly and thus jit counting!
// stepping over patched code currently doesnt work yet anyway

// need to guard the breakpoint table
// as JIT threads read on crash and emulator UI thread might change
// concurrently as they resolve

template <typename T>
class breakpoints_base: public breakpoint_defs
{
public:


protected:
	// shouldnt share this over processors
	// => this renders concurrent multi processor debugging unavailable!
	static boost::mutex dt_lock;
	static debugger_table dt;
	static break_info *last_error;
	static break_data last_block;
	static bool has_last_error;
public:	
	static break_info* resolve(char *ip)
	{
		breakpoint_defs::debugger_table::iterator it;
		boost::mutex::scoped_lock g(breakpoints_base::dt_lock);
		it = dt.find( ip );
		if (it == breakpoints_base::dt.end())
			return 0;
		return it->second;
	}

	static void trigger(char *ip);
	static break_info* get_last_error()
	{
		if (!has_last_error)
			return 0;
		return last_error;
	}

	static void clear_last_error()
	{
		has_last_error = false;
	}
};


template<typename T> breakpoint_defs::debugger_table breakpoints_base<T>::dt;
template<typename T> boost::mutex breakpoints_base<T>::dt_lock;
template<typename T> breakpoint_defs::break_info* breakpoints_base<T>::last_error;
template<typename T> bool breakpoints_base<T>::has_last_error;
template<typename T> breakpoint_defs::break_data breakpoints_base<T>::last_block;



template<typename T> struct OTHER_MODE {};
template<> struct OTHER_MODE<IS_ARM> { typedef IS_THUMB T; };
template<> struct OTHER_MODE<IS_THUMB> { typedef IS_ARM T; };

template<typename T, typename U> struct stepper
{
	static void step_in(unsigned int a1, unsigned int a2, unsigned int s2);
};


template <typename T, typename U> struct branch_pre
{
	static unsigned long get(unsigned long addr); 
};

template <typename T> struct branch_pre<T, IS_ARM>
{
	static unsigned long get(unsigned long /*addr*/) { return 0; } 
};

template <typename T> struct branch_pre<T, IS_THUMB>
{
	typedef IS_THUMB U;
	static unsigned long get(unsigned long addr)
	{ 
		// decode previous instruction and return imm if it was a bpre
		addr -= 2;
		memory_block *b = memory_map<T>::addr2page(addr);
		unsigned long op = *(U::T*)(b->mem + (addr & PAGING::ADDRESS_MASK));
		disassembler d;
		d.decode<IS_THUMB>(op, addr);
		const disassembler::context &ctx = d.get_context();
		if (ctx.instruction == INST::BPRE)
			return ctx.imm;
		return 0;
	}
};

template <typename T, typename U> class breakpoints: public breakpoints_base<T>
{
public:
	typedef typename breakpoint_defs::break_info break_info;
	typedef typename breakpoint_defs::break_data break_data;
	typedef typename breakpoint_defs::stepmode stepmode;

	static bool is_singlestep( break_info *bi )
	{
		return (bi->block == &single_step[0]) || (bi->block == &single_step[1]);
	}

private:
	typedef std::map<char*, break_data*> breakmap;
	static breakmap breaks;
	static break_data single_step[2];

	// backup and patch in a breakpoint
	static void patch_bi( break_info &bi )
	{
		if (bi.patched)
			return;

		// need to patch
		{
			boost::mutex::scoped_lock g(breakpoints_base<T>::dt_lock);
			breakpoints_base<T>::dt[bi.pos] = &bi;
		}
		*bi.pos = DEBUG_BREAK;
		bi.patched = true;
	}

	// reset original code
	static void unpatch_bi( break_info &bi )
	{
		if (!bi.patched)
			return;
		
		// need to unpatch
		*bi.pos = bi.original_byte;
		bi.patched = false;
		{
			boost::mutex::scoped_lock g(breakpoints_base<T>::dt_lock);
			breakpoints_base<T>::dt.erase( bi.pos );
		}
	}

	// removes a breakpoint from lookup list without resetting
	// this is used when an old block has been deleted or is to be deleted
	static void remove_bi( break_info &bi )
	{
		if (bi.patched)
		{
			boost::mutex::scoped_lock g(breakpoints_base<T>::dt_lock);
			breakpoints_base<T>::dt.erase(bi.pos);
		}
		bi.pos = 0;
		bi.patched = false;
	}

	// toggles a breakpoint, (un)patches if possible
	static void toggle_sub( break_info &bi)
	{
		// if onetime is set we basically deal with a different breakpoint
		bi.used = !bi.used;
		if (bi.pos) // is the breakpoint currently patchable?
		{
			if (bi.used)
				patch_bi( bi );
			else
				unpatch_bi( bi );
		}
	}

	static void add_bp(unsigned long addr, unsigned long subcode)
	{
		if (subcode >= breakpoint_defs::MAX_SUBINSTRUCTIONS)
			return;

		char *physical = &memory_map<T>::addr2page( addr )->mem[addr & PAGING::ADDRESS_MASK];
		break_data* &bd = breaks[physical];
		if (!bd)
		{
			bd = break_data::allocate();
			bd->addr = addr;
			update_breakdata(bd);
		}
		toggle_sub( bd->jit_line[subcode] );
	}

	static bool check_numsub(unsigned long subcode)
	{
		if (subcode >= breakpoint_defs::MAX_SUBINSTRUCTIONS)
		{
			std::cerr << "The JIT for this instruction contains more instructions than supported by the Debugger\n"
				"Please report this error.\n";
			return false;
		}
		return true;
	}

	static bool get_branch_dest(unsigned long addr, unsigned long &dest)
	{
		memory_block *b = memory_map<T>::addr2page(addr);

		//logging<T>::logf("GBA Mode %i", U::INSTRUCTION_SIZE);

		unsigned long op = *(typename U::T*)(b->mem + (addr & PAGING::ADDRESS_MASK));
		disassembler d;
		d.decode<U>(op, addr);
		const disassembler::context &ctx = d.get_context();
		switch (ctx.instruction)
		{
		case INST::B:
		case INST::BL:
			dest = ctx.imm + branch_pre<T,U>::get(addr);
			return true;
		case INST::BLX:
		case INST::BX:
			// TODO: handle CPU mode
			dest = processor<T>::ctx().regs[ctx.rm];
			return true;
		}
		return false;
	}

public:
	static void step(stepmode mode)
	{
		break_info* last = breakpoints_base<T>::get_last_error();
		if (last)
		{
			unsigned long addr = last->block->addr;
			unsigned long next_addr = addr + U::INSTRUCTION_SIZE;
			unsigned long next_sub = last->subaddr + 1;
			unsigned long dest;
			switch (mode)
			{
			case breakpoint_defs::STEP_INTO_EMU:
				if (next_sub < last->block->jit_instructions)
				{
					if (get_branch_dest(addr, dest))
						stepper<T, U>::step_in(  dest, addr, next_sub );
					else set_onetime(addr, next_sub, next_addr, 0, 2 );
					return;
				}
				break;

			case breakpoint_defs::STEP_INTO_ARM:
				if (get_branch_dest(addr, dest))
				{
					stepper<T, U>::step_in( dest, next_addr, 0 );
					// possible mode switch here
					return;
				}
				break;

			case breakpoint_defs::STEP_INTO_SRC:
				{
					const source_set *src = source_debug::line_for(last->block->addr);
					if (!src)
						return; // there was no line info available for the break
					runner<T>::skipline( src, breakpoint_defs::STEP_INTO_ARM );
					break;
				}
			case breakpoint_defs::STEP_OVER_SRC:
				{
					const source_set *src = source_debug::line_for(last->block->addr);
					if (!src)
						return; // there was no line info available for the break
					runner<T>::skipline( src, breakpoint_defs::STEP_OVER_ARM );
					break;
				}


			case breakpoint_defs::STEP_OVER_EMU:
				if (next_sub < last->block->jit_instructions)
				{
					set_onetime( addr, next_sub, next_addr, 0, 2 ); // used only first bp before?
					return;
				}
				break;
			}
			// if reaching here, process a simple stop over
			set_onetime( next_addr, 0, 0, 0, 1 );
		} else 
		{
			// use R15
			// TODO: handle CPU mode
			unsigned long addr = processor<T>::ctx().regs[15];
			unsigned long next_addr = addr + U::INSTRUCTION_SIZE;
			set_onetime( addr, 0, next_addr, 0, 2 );
		}
	}

	// enables a single step bp
	static void enable_step(break_info &bi)
	{
		// additional handling in case theres already a regular bp
		// at the destination could be handled here
		toggle_sub(bi);
	}

	static void set_onetime(unsigned long addr1, unsigned long subcode1,
		unsigned long addr2, unsigned long subcode2,
		int num_addr)
	{		
		logging<T>::logf("BP [%s]: %08X:%i, %08X:%i", U::name, addr1, subcode1, addr2, subcode2);
		

		// validity check
		if (!check_numsub(subcode1))
			return;
		if ((num_addr > 1) && (!check_numsub(subcode2)))
			return;

		// disable any old outdated break
		for (int i = 0; i < breakpoint_defs::MAX_SUBINSTRUCTIONS; i++)
		{
			if(single_step[0].jit_line[i].used)
				toggle_sub( single_step[0].jit_line[i] );
			if(single_step[1].jit_line[i].used)
				toggle_sub( single_step[1].jit_line[i] );
		}

		// decode instruction if not single stepping emu
		if (single_step[0].addr != addr1)
		{
			single_step[0].addr = addr1;
			update_breakdata( &single_step[0] );
		}
		enable_step( single_step[0].jit_line[subcode1] );

		if (num_addr <= 1)
			return;

		// add second breakpoint
		if (addr2 == addr1)
		{
			// can reuse single_step[0]
			if (subcode1 == subcode2)
				return; // same breakpoint
			enable_step( single_step[0].jit_line[subcode2] );
		} else
		{
			// prepare a second break
			if (single_step[1].addr != addr2)
			{
				single_step[1].addr = addr2;
				update_breakdata( &single_step[1] );
			}
			enable_step( single_step[1].jit_line[subcode2] );
		}
	}

	static void unset_onetime()
	{
		for (int i = 0; i < breakpoint_defs::MAX_SUBINSTRUCTIONS; i++)
		{
			if (single_step[0].jit_line[i].used)
				toggle_sub( single_step[0].jit_line[i] );
			if (single_step[1].jit_line[i].used)
				toggle_sub( single_step[1].jit_line[i] );
		}
	}

	static void disassemble_breakdata(break_data *bd)
	{
		size_t sz;
		memory_block *block = memory_map<T>::addr2page(bd->addr);
		compiled_block<U> *code = block->get_jit<T, U>();
		if (!code)
			return;

		unsigned long inst = (bd->addr & PAGING::ADDRESS_MASK) >> U::INSTRUCTION_SIZE_LG2;
		char *start = code->remap[inst];
		inst++;
		if (inst >= compiled_block<U>::REMAPS)
			sz = code->code_size - (start - code->code);
		else sz = code->remap[inst] - start;
				
		// should lower those if possible ...
		_DecodedInst instructions[breakpoint_defs::MAX_SUBINSTRUCTIONS_DISTORM];
		unsigned int decoded = 0;
		if (distorm_decode( 0, (unsigned char*)start, (int)sz, Decode32Bits, 
			instructions, breakpoint_defs::MAX_SUBINSTRUCTIONS_DISTORM, &decoded ) == DECRES_SUCCESS)
		{
			if (decoded  > breakpoint_defs::MAX_SUBINSTRUCTIONS)
				decoded = breakpoint_defs::MAX_SUBINSTRUCTIONS;

			bd->jit_instructions = decoded;
			for (unsigned int i = 0; i < decoded; i++)
			{
				break_info &bi = bd->jit_line[i];
				bi.pos = start;
				bi.original_byte = *start;
				if (bi.used)
					patch_bi(bi);
				start += instructions[i].size;
			}
			for (unsigned int i = decoded; i < breakpoint_defs::MAX_SUBINSTRUCTIONS; i++)
				bd->jit_line[i].pos = 0;
		}
	}

	static void update_breakdata(break_data *bd)
	{
		// disassemble instruction if available and populate code pointers
		// this allows easier toggling within a jit block
		for (unsigned int i = 0; i < breakpoint_defs::MAX_SUBINSTRUCTIONS; i++)
			remove_bi( bd->jit_line[i] );
		disassemble_breakdata(bd);
	}

	template <typename Functor> struct for_region
	{
		static void f(char* start, char* end)
		{
			// TODO lower_bound seems to return the first greater
			// bp and not equal one ...
			breakmap::iterator it = breaks.lower_bound(start);
			while ((it != breaks.end()) && (it->first < end))
			{
				Functor::callback(it->first, it->second);
				++it;
			}

			// translate single_step address to physical

			unsigned long addr = single_step[0].addr;
			char *physical = &memory_map<T>::addr2page( addr )->mem[addr & PAGING::ADDRESS_MASK];
			if ((physical >= start) && (physical < end))
				Functor::callback( physical, &single_step[0] );

			addr = single_step[1].addr;
			physical = &memory_map<T>::addr2page( addr )->mem[addr & PAGING::ADDRESS_MASK];
			if ((physical >= start) && (physical < end))
				Functor::callback( physical, &single_step[1] );
		}
	};


	static const break_data* get(unsigned long addr)
	{
		char *physical = &memory_map<T>::addr2page( addr )->mem[addr & PAGING::ADDRESS_MASK];
		breakmap::const_iterator it = breaks.find( physical );
		if (it == breaks.end())
			return 0;
		return it->second;
	}

	static void get_jit(unsigned long addr, jit_code &jit)
	{
		memory_block *b = memory_map<T>::addr2page(addr);
		compiled_block<U> *j = b->get_jit<T,U>();
		if (!j)
		{
			jit.data = 0;
			jit.original = 0;
			jit.len = 0;
			return;
		} else
		{
			unsigned long saddr = addr & PAGING::ADDRESS_MASK;
			unsigned long entry = saddr >> U::INSTRUCTION_SIZE_LG2;
			char *code = j->remap[entry];
			jit.original = code;

			size_t code_size;
			if (entry < compiled_block<U>::REMAPS - 1)
				code_size = j->remap[entry+1] - code;
			else code_size = (j->code_size - 0) - (code - j->code);
			jit.len  = (unsigned long)code_size;

			// final step: check if theres a break on the address
			breakmap::const_iterator it = breaks.find( b->mem + saddr );
			if (it == breaks.end())
			{
				// no patched bytes, just return the original data
				jit.data = code;
				return;
			}

			// theres patched data here make a copy and unpatch
			static char buffer[1024];
			assert(code_size <= sizeof(buffer));
			memcpy( buffer, code, code_size );
			const break_data *bd = it->second;
			for (unsigned int i = 0; i < bd->jit_instructions; i++)
			{
				const break_info &bi = bd->jit_line[i];
				if (bi.patched)
				{
					size_t offset = bi.pos - code;
					assert((offset >= 0) && (offset < code_size));
					buffer[offset] = bi.original_byte;
				}
			}
			jit.data = buffer;
		}
	}

	static void toggle(unsigned long addr, unsigned long subcode )
	{
		if (!check_numsub(subcode))
			return;

		char *physical = &memory_map<T>::addr2page( addr )->mem[addr & PAGING::ADDRESS_MASK];
		breakmap::iterator it = breaks.find( physical );
		if (it == breaks.end())
			add_bp(addr, subcode ); // enable
		else
		{
			toggle_sub( it->second->jit_line[subcode] );
			for (int i = 0; i < breakpoint_defs::MAX_SUBINSTRUCTIONS; i++)
				if (it->second->jit_line[i].used)
					return;
			// erase the whole entry
			break_data::free( it->second );
			breaks.erase(it);
		}
	}
};


template <typename T> struct stepper<T, IS_ARM>
{
	static void step_in(unsigned int a1, unsigned int a2, unsigned int s2)
	{
		if (a1 & 1) // modeswitch
		{
			// add singlestep bp at destination in thumb mode
			breakpoints<T,IS_THUMB>::set_onetime(a1 & (~1), 0, 0, 0, 1 );
			// add singelstep bp behind branch in arm mode 
			// in case the instruction is not taken
			breakpoints<T,IS_ARM>::set_onetime(a2, s2, 0, 0, 1 );
		} else // no modeswitch just install both bp's in ARM mode
			breakpoints<T,IS_ARM>::set_onetime(a1, 0, a2, s2, 2 );
	}
};

template <typename T>
struct stepper<T, IS_THUMB>
{
	static void step_in(unsigned int a1, unsigned int a2, unsigned int s2)
	{
		breakpoints<T,IS_THUMB>::set_onetime(a1, 0, a2, s2, 2 );
	}
};



template <typename T, typename U> 
	typename breakpoints<T,U>::breakmap breakpoints<T,U>::breaks;
template <typename T, typename U> 
	breakpoint_defs::break_data breakpoints<T,U>::single_step[2];




#endif
