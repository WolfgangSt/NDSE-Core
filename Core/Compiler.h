#ifndef _COMPILER_H_
#define _COMPILER_H_

// replaces the JIT with a nearly pure HL core
#undef HLE_CORE

// todo: pull all methods to template based versions rather than using 
// init_cpu / init_mode to gain some more performance

#include <map>
#include <sstream>
#include <list>
#include "forward.h"
#include "Disassembler.h"
#include "Mem.h"

#ifdef HLE_CORE
#include "HLCore.h"
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)
#define CONCAT(a,b) a ## b

#define D_DEBUG_BREAK 0xCF
#define D_DEBUG_BREAKU 0xCFU


const char DEBUG_BREAK = D_DEBUG_BREAKU; // '\xCF'; // iret (for debugging the debugger)
inline void DebugBreak_()
{
#ifdef WIN32
	__asm _emit D_DEBUG_BREAKU
#else
	asm(".byte " TOSTRING(D_DEBUG_BREAK) );
#endif
}

// static unsigned long FASTCALL(load32(unsigned long addr));

class compiler
{
private:
	std::ostringstream s;
	std::list<unsigned long> reloc_table;
	int flags_updated;
	//unsigned long preoff; // for thumb BPRE
	disassembler::context ctx;
	compiler();
	int INST_BITS;
	unsigned int inst;
	unsigned long lookahead_s;


	// helper funcs
	void update_dest(int size);
	static void count(unsigned long imm,  unsigned long &num, 
			unsigned long &lowest, unsigned long &highest, bool &region);
	void load_ecx_single();
	void push_multiple(int num);
	void load_ecx_reg_or_pc(int reg, unsigned long offset = 0);
	void load_eax_reg_or_pc(int reg, unsigned long offset = 0);

	void break_if_pc(int reg);
	void break_if_thumb();
	
	void load_edx_ecx_or_reg(int r1, int r2);
	void load_eax_ecx_or_reg(int r1, int r2);
	void store_rd_eax();
	void shiftop_eax_ecx();
	void add_ecx(unsigned long imm);
	void add_eax(unsigned long imm);
	void generic_store();
	void generic_store_p();
	void generic_store_r();
	void generic_load();
	void generic_load_r();
	void generic_load_rs(bool post, bool wb);
	void generic_loadstore_shift();
	void generic_postload_shift();
	void generic_load_x();
	void generic_store_x();
	void ldm_switchuser();
	void ldm_switchback();

	void generic_load_post();

	void generic_loadstore_postupdate_imm();
	void generic_loadstore_postupdate_ecx();

	void store_flags();
	void load_flags();
	void load_shifter_imm();

	void store_carry();
	void load_carry();

	void shift_eax_imm();

	void record_callstack();
	void update_callstack();
	void add_ecx_bpre();
	void load_r15_ecx();

	void compile_instruction();
	void epilogue(char *&mem, size_t &size);
	std::ostringstream::pos_type tellp();

	int pushs;
	void push(unsigned long imm);

#ifdef HLE_CORE
	void* hlc_funcs[INST::MAX_INSTRUCTIONS];
	void* hlc_checkflags;
#endif

	void* store32;
	void* store16;
	void* store8;
	void* store32_array;
	void* load32;
	void* load16u;
	void* load16s;
	void* load8u;
	void* load32_array;
	void* loadcpsr;
	void* storecpsr;
	
	void* pushcallstack;
	void* popcallstack;
	void* compile_and_link_branch_a;
	void* remap_tcm;
	void* is_priviledged;
	void* swi;
	void* debug_magic;

	template <typename T> void* FUNC2PTR(T p)
	{
		// probably not on x86 protected mode if this aint true
		assert(sizeof(p) == sizeof(void*));
		return (void*)p;
	}


public:
	template <typename U> void init_mode()
	{
		INST_BITS = U::INSTRUCTION_SIZE_LG2;
	}
	
	template <typename T> void init_cpu()
	{
#ifdef HLE_CORE
		for (int i = 0; i < INST::MAX_INSTRUCTIONS; i++)
			hlc_funcs[i] = HLCore<T>::funcs[i];
		hlc_checkflags = HLCore<T>::check_flags;
#endif
		store32 = FUNC2PTR(HLE<T>::store32);
		store16 = FUNC2PTR(HLE<T>::store16);
		store8 = FUNC2PTR(HLE<T>::store8);
		store32_array = FUNC2PTR(HLE<T>::store32_array);

		load32 = FUNC2PTR(HLE<T>::load32);
		load16u = FUNC2PTR(HLE<T>::load16u);
		load16s = FUNC2PTR(HLE<T>::load16s);
		load8u = FUNC2PTR(HLE<T>::load8u);
		load32_array = FUNC2PTR(HLE<T>::load32_array);

		loadcpsr = FUNC2PTR(HLE<T>::loadcpsr);
		storecpsr = FUNC2PTR(HLE<T>::storecpsr);

		pushcallstack = FUNC2PTR(HLE<T>::pushcallstack);
		popcallstack = FUNC2PTR(HLE<T>::popcallstack);

		compile_and_link_branch_a = FUNC2PTR(HLE<T>::compile_and_link_branch_a);
		remap_tcm = FUNC2PTR(HLE<T>::remap_tcm);
		is_priviledged = FUNC2PTR(HLE<T>::is_priviledged);
		swi = FUNC2PTR(HLE<T>::swi);
		debug_magic = FUNC2PTR(HLE<T>::debug_magic);
	}


	template <typename T, typename U>
	static void compile(compiled_block<U> &cb)
	{
		compiler c;
		disassembler d;
		typename U::T* p = (typename U::T*)cb.block->mem;
		c.init_mode<U>();
		c.init_cpu<T>();

		// decode first instruction
		d.decode<U>( *p++, 0 ); // ,0 => use relative addressing
		for (int i = 0; i < PAGING::INST<U>::NUM-1; i++ )
		{
			c.ctx = d.get_context();
			d.decode<U>( *p++, 0 ); // decode next
			cb.remap[i] = (char*)0 + c.tellp();
			c.inst = i;
			const disassembler::context &cnext = d.get_context();
			c.lookahead_s = (cnext.flags & disassembler::S_BIT);
			if (c.lookahead_s)
			{
				// check if lookahead instruction is flag consuming
				switch (cnext.instruction)
				{
				case INST::ADC_I:
				case INST::ADC_R:
				case INST::ADC_RR:
				case INST::SBC_I:
				case INST::SBC_R:
				case INST::SBC_RR:
					c.lookahead_s = 0;
				}

				if (cnext.shift == SHIFT::RRX)
					c.lookahead_s = 0;
			}
			c.compile_instruction();
		}
		// final interation
		c.ctx = d.get_context();
		c.lookahead_s = false;
		cb.remap[PAGING::INST<U>::NUM-1] = (char*)0 + c.tellp();
		c.inst = PAGING::INST<U>::NUM-1;
		c.compile_instruction();

		cb.remap[0] = 0;
		c.epilogue(cb.code, cb.code_size);

		// relocate remapping table
		for (int i = 0; i < PAGING::INST<U>::NUM; i++ )
			cb.remap[i] = cb.code + (size_t)cb.remap[i];
	}
};



template <typename U>
struct compiled_block: public compiled_block_base<U>
{
protected:
	compiled_block() { compiled_block_base<U>::block = 0; }
public:

	// X86 only! (doesnt matter though since we assemble X86 JIT ;P)
    template <typename T> void WRITE(char* &p, const T &t)
	{
                *reinterpret_cast<T*>(p) = t;
                p += sizeof(T);
	}

	void emulate(unsigned long subaddr, emulation_context &ctx);

	compiled_block(memory_block *blk)
	{
		compiled_block_base<U>::block = blk;
		//compiler::compile( *this ); // callee needs to do this now!
	}

	~compiled_block()
	{
		delete []compiled_block_base<U>::code;
	}
};



#endif
