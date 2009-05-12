#ifndef _COMPILER_H_
#define _COMPILER_H_

#include <map>
#include <sstream>
#include <list>
#include "Mem.h"
#include "Disassembler.h"
#include "HLE.h"
#include "CompiledBlock.h"

#define D_DEBUG_BREAK 0xCFU
#define QUOTE(x) #x

const char DEBUG_BREAK = D_DEBUG_BREAK; // '\xCF'; // iret (for debugging the debugger)
// const char DEBUG_BREAK = '\xCC'; // int 3

inline void DebugBreak_()
{
#ifdef WIN32
	__asm _emit D_DEBUG_BREAK
#else
	asm(".byte " QUOTE(D_DEBUG_BREAK));
#endif
}


class compiler
{
private:
	std::ostringstream s;
	std::list<unsigned long> reloc_table;
	int flags_updated;
	unsigned long preoff; // for thumb BPRE
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

	void break_if_pc(int reg);
	void break_if_thumb();
	
	void load_edx_ecx_or_reg(int r1, int r2);
	void load_eax_ecx_or_reg(int r1, int r2);
	void shiftop_eax_ecx();
	void add_ecx(unsigned long imm);
	void generic_store();
	void generic_store_p();
	void generic_store_r();
	void generic_load();
	void generic_load_r();
	void generic_loadstore_shift();
	void generic_load_x();
	void generic_store_x();

	void generic_load_post();

	void generic_loadstore_postupdate_imm();
	void generic_loadstore_postupdate_ecx();

	void store_flags();
	void load_flags();
	void load_shifter_imm();

	void record_callstack();
	void update_callstack();

	void compile_instruction();
	void epilogue(char *&mem, size_t &size);
	std::ostringstream::pos_type tellp();
public:
	template <typename T> void init_mode();
	template <typename T>
	static void compile(compiled_block<T> &cb)
	{
		compiler c;
		disassembler d;
		typename T::T* p = (typename T::T*)cb.block->mem;
		c.init_mode<T>();

		//logging<T>::logf("compiling block", cb.block);

		// decode first instruction
		d.decode<T>( *p++, 0 ); // ,0 => use relative addressing
		for (int i = 0; i < PAGING::INST<T>::NUM-1; i++ )
		{
			c.ctx = d.get_context();
			d.decode<T>( *p++, 0 ); // decode next
			cb.remap[i] = (char*)0 + c.tellp();
			c.inst = i;
			const disassembler::context &cnext = d.get_context();
			/*
			c.lookahead_s = (cnext.flags & 
				(disassembler::S_BIT | disassembler::S_CONSUMING))
				== disassembler::S_BIT;
			if (c.lookahead_s)
			{
				if ( (cnext.cond != CONDITION::AL) && (cnext.cond != CONDITION::NV) )
					c.lookahead_s = 0;
			}*/

			c.lookahead_s = (cnext.flags & disassembler::S_BIT);

			c.compile_instruction();
		}
		// final interation
		c.ctx = d.get_context();
		cb.remap[PAGING::INST<T>::NUM-1] = (char*)0 + c.tellp();
		c.inst = PAGING::INST<T>::NUM-1;
		c.compile_instruction();

		cb.remap[0] = 0;
		c.epilogue(cb.code, cb.code_size);

		// relocate remapping table
		for (int i = 0; i < PAGING::INST<T>::NUM; i++ )
			cb.remap[i] = cb.code + (size_t)cb.remap[i];
	}
};



template <typename T>
struct compiled_block: public compiled_block_base<T>
{
protected:
	compiled_block() { compiled_block_base<T>::block = 0; }
public:

	// X86 only! (doesnt matter though since we assemble X86 JIT ;P)
    template <typename U> void WRITE(char* &p, const T &t)
	{
                *reinterpret_cast<U*>(p) = t;
                p += sizeof(U);
	}

	void emulate(unsigned long subaddr, emulation_context &ctx);

	compiled_block(memory_block *blk)
	{
		compiled_block_base<T>::block = blk;
		compiler::compile( *this );
	}

	~compiled_block()
	{
		delete []compiled_block_base<T>::code;
	}
};



#endif
