#include <sstream>
#include <list>
#include <assert.h>

#include "Disassembler.h"
#include "ArmContext.h"
#include "Compiler.h"
#include "MemMap.h"
#include "Logging.h"
#include "HLE.h"

#include "osdep.h"

// cycle counter requires SSE
#define CLOCK_CYCLES


////////////////////////////////////////////////////////////////////////////////
// IMPORTANT BUGS
// flag prediction:
// the lookahead optimizer will remove flag updates if the follow up instruction
// would overwrite the flag.
// in a similiar fashion it removes restores when a consuming instruction
// follows straight after a producing instruction.
//
// A problem that can arise with that is when some branch goes to a consuming 
// instruction (this should be handled within the branch HLE for performance!)
//
// A second problem is 2 instruction branches in thumb mode beeing on different
// pages .

// Critical bugs: many ops screw when processing R15
// read accesses are out of sync, write accesses need branch

// generic_loadstore_postupdate_imm can be optimized, see STR_IPW



/*
[23:49] <sgstair> arm7 docs show STM writes out the base at the end of the second cycle of the instruction, and a LDM always overwrites the updated base if the base is in the list.
[23:50] <sgstair> so if the base is the lowest register in the list, STM writes the unchanged base, otherwise it writes the final base.
*/

////////////////////////////////////////////////////////////////////////////////
// TODO: sometime in the future
//
// some more performant way would be using immediate calls
// and jumps between codeblocks.
// however this would require that each block holds information
// about all all jump/call instructions to it, so they can be
// relocated.
// recompiling a page would relocate all those jumps
//
// Advantages => no more page / destination lookups
//            => no more indirect jumps
// Disadv     => could break emulation in such a scenario:
//               code gets executed at an address and a mirrored address
//               however the relative jump out could go to different
//               locations
//               => this needs extra work
//
// Memory accesses could be optimized the same way

char NOT_SUPPORTED_YET_BUT_SKIP = '\x90';

#define WRITE_P(p) { void *x = p; s.write((char*)&x, sizeof(x)); }
#define OFFSET(z) ((char*)&__context_helper.z - (char*)&__context_helper)
// need to find a better way to do calls ...
#define CALL(f) { s << '\xE8'; reloc_table.push_back(s.tellp());\
	char *x = (char*)&f; x -= 4; s.write((char*)&x, sizeof(x)); }
#define CALLP(f) { s << '\xE8'; reloc_table.push_back(s.tellp());\
	char *x = (char*)f; x -= 4; s.write((char*)&x, sizeof(x)); }
#define JMP(f) { s << '\xE9'; reloc_table.push_back(s.tellp());\
	char *x = (char*)&f; x -= 4; s.write((char*)&x, sizeof(x)); }
#define JMPP(f) { s << '\xE9'; reloc_table.push_back(s.tellp());\
	char *x = (char*)f; x -= 4; s.write((char*)&x, sizeof(x)); }
static emulation_context __context_helper; // temporary for OFFSET calculation
#define RECORD_CALLSTACK CALL

template <typename T> void write(std::ostream &s, const T &t)
{
	s.write((char*)&t, sizeof(t));
}

void compiler::store_carry()
{
	s << "\xD1\xD6"; // rcl esi, 1
}

void compiler::load_carry()
{
	s << "\xD1\xDE"; // rcr esi, 1
}


void compiler::load_shifter_imm()
{
	if (ctx.shift == SHIFT::RXX)
		load_flags();
	load_eax_reg_or_pc(ctx.rm);
	switch (ctx.shift)
	{
	case SHIFT::LSL:
		if (ctx.imm == 0)
		{
			// carry out = carry in
			//s << "\x8B\x75" << (char)OFFSET(x86_flags); // mov esi, [ebp+x86_flags]
			//s << "\xC1\xEE\x09";                        // shr esi, 9 
			break;
		}
		if (ctx.imm >= 32) // optimize => doesnt need the initial load
			//s << "\x33\xC0";                  // xor eax,eax
		{
			s << "\xC1\xE0" << (char)31; // shl eax, 31
			s << "\xC1\xE0" << (char)1;  // shl eax, 1
		} else
			s << "\xC1\xE0" << (char)ctx.imm; // shl eax, imm
		break;
	case SHIFT::LSR:
		if (ctx.imm >= 32) // optimize => doesnt need the initial load
			//s << "\x33\xC0";                  // xor eax,eax
		{
			s << "\xC1\xE8" << (char)31; // shr eax, 31
			s << "\xC1\xE8" << (char)1;  // shr eax, 1
		} else
			s << "\xC1\xE8" << (char)ctx.imm; // shr eax, imm
		break;
	case SHIFT::ASR:
		if (ctx.imm != 0)
		{
			if (ctx.imm == 32)
			{
				s << "\xC1\xF8" << (char)31; // sar eax, 31
				s << "\xC1\xF8" << (char)1;  // sar eax, 1
			} else
			{
				s << "\xC1\xF8" << (char)ctx.imm; // sar eax, imm
			}
		}
		break;
	case SHIFT::ROR:
		if (ctx.imm != 0)
		{
			s << "\xC1\xC8" << (char)ctx.imm; // ror eax, imm
		}
		break;
	case SHIFT::RXX:
		s << "\xD1\xD8"; // rcr eax,1 
		break;
	}
	store_carry();
}


void compiler::store_flags()
{
	flags_updated = 1;
	if (lookahead_s) // next instruction would overwrite
		return;

	s << "\x0F\x90\xC0";                        // seto al
	s << '\x9F';                                // lahf
	s << "\x89\x45" << (char)OFFSET(x86_flags); // mov [ebp+x86_flags], eax

	//s << '\x9C';                                // pushfd
	//s << "\x8F\x45" << (char)OFFSET(x86_flags); // pop [ebp+x86_flags]
}

void compiler::load_flags()
{	
	s << "\x8B\x45" << (char)OFFSET(x86_flags); // mov eax, [ebp+x86_flags]
	s << "\xD0\xC8";                            // ror al, 1
	s << '\x9E';                                // sahf

	//s << "\xFF\x75" << (char)OFFSET(x86_flags); // push [ebp+x86_flags]
	//s << '\x9D';                                // popfd
}



void compiler::update_dest(int size)
{
	switch (ctx.addressing_mode)
	{
	case ADDRESSING_MODE::DA:
	case ADDRESSING_MODE::DB:
		s << "\x83\x6D" << (char)OFFSET(regs[ctx.rn]); // sub dword ptr[ebp+Rn],
		s << (char)(size << 2);                        // size*4
		break;
	case ADDRESSING_MODE::IA:
	case ADDRESSING_MODE::IB:
		s << "\x83\x45" << (char)OFFSET(regs[ctx.rn]); // add dword ptr[ebp+Rn],
		s << (char)(size << 2);                        // size*4
		break;
	}
}

void compiler::count(unsigned long imm, 
	unsigned long &num, 
	unsigned long &lowest, 
	unsigned long &highest,
	bool &region)
{
	int i;
	num = 0;
	for (i = 0; i < 16; i++)
	{
		unsigned long b = imm & 1;
		imm >>= 1;
		if (b)
		{
			num++;
			lowest = i;
			highest = i;
			break;
		}
	}
	i++;
	for (;i < 16; i++)
	{
		unsigned long b = imm & 1;
		imm >>= 1;
		if (b)
		{
			num++;
			highest = i;
		}
	}
	region = (highest - lowest + 1) == num;
}

// loads ecx with the destination address if there is only
// one register to be stored/loaded
void compiler::load_ecx_single()
{
	s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]);  // mov ecx, dword ptr [ebp+Rn] 
	switch (ctx.addressing_mode)
	{
	case ADDRESSING_MODE::DA: break; // start at Rn - 1*4 + 4 = Rn
	case ADDRESSING_MODE::IA: break; // start at Rn
	case ADDRESSING_MODE::DB:        // start at Rn - 1*4
		s << "\x83\xE9\x04"; // sub ecx, 4
		break;
	case ADDRESSING_MODE::IB:        // start at Rn+4
		s << "\x83\xC1\x04"; // add ecx, 4
		break;
	}
}

// pushs the start address if multiple regs are to be stored/loaded
void compiler::push_multiple(int num)
{
	switch (ctx.addressing_mode)
	{
	case ADDRESSING_MODE::IA: // start at Rn
		s << "\xFF\x75" << (char)OFFSET(regs[ctx.rn]); // push dword ptr[ebp+Rn]
		break;
	case ADDRESSING_MODE::IB: // start at Rn+4
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, dword ptr[ebp+Rn]
		s << "\x83\xC1\x04";                           // add ecx, 4
		s << "\x51";                                   // push ecx
		break;
	case ADDRESSING_MODE::DA: // start at Rn - num*4 + 4, num >= 1
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, dword ptr[ebp+Rn]
		s << "\x83\xE9" << (char)((num-1) << 2);       // sub ecx, (num-1)*4
		s << "\x51";                                   // push ecx
		break;
	case ADDRESSING_MODE::DB: // start at Rn - num * 4, mum >= 1
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, dword ptr[ebp+Rn]
		s << "\x83\xE9" << (char)(num << 2);           // sub ecx, num*4
		s << "\x51";                                   // push ecx
		break;
	}
}

// loads a register content or PC relative address to ecx
// (+2 instructions)
// fix the +2 at callees in future!
// this is intended to do only +1 ...
void compiler::load_ecx_reg_or_pc(int reg, unsigned long offset)
{
	if (reg != 0xF)
	{
		s << "\x8B\x4D" << (char)OFFSET(regs[reg]);      // mov ecx, [ebp+rn]
	} else
	{
		// ip relative
		s << "\x8B\x4D" << (char)OFFSET(regs[15]);    // mov ecx, [ebp+rn]
		s << "\x81\xE1"; write( s, (unsigned long)(~PAGING::ADDRESS_MASK) ); // and ecx, ~PAGING::ADDR_MASK
		offset += ((inst+2) << INST_BITS) & ~3;
	}
	add_ecx(offset);
}


// loads a register content or PC relative address to eax
// (+1 instruction)
void compiler::load_eax_reg_or_pc(int reg, unsigned long offset)
{
	if (reg != 0xF)
	{
		s << "\x8B\x45" << (char)OFFSET(regs[reg]);      // mov eax, [ebp+rn]
	} else
	{
		// ip relative
		s << "\x8B\x45" << (char)OFFSET(regs[15]);    // mov eax, [ebp+rn]
		s << "\x25"; write( s, (unsigned long)(~PAGING::ADDRESS_MASK) ); // and eax, ~PAGING::ADDR_MASK
		offset += ((inst+2) << INST_BITS) & ~3;
	}
	add_eax(offset);
}


void compiler::break_if_pc(int reg)
{
	if (reg == 0xF)
		s << DEBUG_BREAK;
}

void compiler::break_if_thumb()
{
	if (INST_BITS == 1)
		s << DEBUG_BREAK;
}

void compiler::load_edx_ecx_or_reg(int r1, int r2)
{
	if (r1 == r2)                                   // reuse same reg?
		s << "\x8B\xD1";                            // mov edx, ecx
	else s << "\x8B\x55" << (char)OFFSET(regs[r1]); // mov edx, [ebp+rn]
}

void compiler::load_eax_ecx_or_reg(int r1, int r2)
{
	if (r1 == r2)                                   // reuse same reg?
		s << "\x8B\xC1";                            // mov eax, ecx
	else // handle R15 here
	{
		if (r1 == 0xF)
			load_eax_reg_or_pc(r1, 1 << INST_BITS);
		else load_eax_reg_or_pc(r1, 0);
	}
}

// switch register banks temporarily to usermode
void compiler::ldm_switchuser()
{
	if (ctx.flags & disassembler::S_BIT)
	{
		s << "\xFF\x75" << (char)OFFSET(cpsr);      // push cpsr
		s << '\xB9'; write(s, (unsigned long)0x10); // mov ecx, 0x10 (user mode)
		s << '\xBA'; write(s, (unsigned long)0x1F); // mov edx, 0x1F (mode mask)
		CALLP(loadcpsr)
		s << '\x59';                                // pop ecx (old mode)
	}
}

void compiler::ldm_switchback()
{
	if (ctx.flags & disassembler::S_BIT)
	{
		s << '\xBA'; write(s, (unsigned long)0x1F);  // mov edx, 0x1F (mode mask)
		CALLP(loadcpsr)
	}
}

unsigned long _31 = 31;

void compiler::shiftop_eax_ecx()
{
	// clamp ecx at 31
	std::ostringstream::pos_type jmpbyte;

	// shift amount 0  => bypass shifter and keep carry
	// shift amount 32 => LSL: load 0, carry = lowest in bit
	//                    LSR: load 0, carry = highest in bit
	//                    ASR: same as shift 31, carry = highest bit
	//                    ROR: not clamped but masked!

	// load carry from x86flags to esi in case we do a 0 shift to keep it       
	s << "\x8B\x75" << (char)OFFSET(x86_flags); // mov esi, [ebp+x86_flags]
	s << "\xC1\xEE\x08";                        // shr esi, 8 (CF in bit 0)
	s << "\x0B\xC9";       // or ecx, ecx
	s << '\x74';           // jz
	jmpbyte = s.tellp();
	s << '\x00';           //  + $0


	switch (ctx.shift)
	{
	case SHIFT::ROR:
		s << "\x83\xE1\x1F";   // and ecx, 0x1F wrap around
		break;
	case SHIFT::LSR:
	case SHIFT::ASR:
		s << '\x49';           // dec ecx use additional shift,1 step
	default:
		// sature shift
		s << "\x6A\x1F"        // push 0x1F
		  << '\x5E'            // pop esi
		  << "\x3B\xCE"        // cmp ecx, esi
		  << "\x0F\x47\xCE";   // cmova ecx, esi 
	}
	

	// add carry out handling for shifts > 31!
	switch (ctx.shift)
	{
	case SHIFT::LSL:
		s << "\xD3\xE0"; // shl eax, cl
		break;
	case SHIFT::LSR:
		s << "\xD3\xE8"; // shr eax, cl
		s << "\xC1\xE8" << (char)1; // shr eax, imm
		break;
	case SHIFT::ASR:
		s << "\xD3\xF8"; // sar eax, cl
		s << "\xC1\xF8" << (char)1;  // sar eax, 1
		break;
	case SHIFT::ROR:
		s << "\xD3\xC8"; // ror eax, cl
		break;
	}
	store_carry(); // "\xD1\xD6"; // rcl esi, 1
	//s << "\xEB\x06"; // jmp $+6


	std::ostringstream::pos_type cur = s.tellp();
	size_t off = cur - jmpbyte - 1;
	if(off >= 128)
	{
		std::cerr << "Generated code too large: " << off << "\n";			
		assert(0);
	}
	s.seekp( jmpbyte );
	s << (char)off;
	s.seekp( cur );


	//s << "\x8B\x75" << (char)OFFSET(x86_flags); // mov esi, [ebp+x86_flags]
	//s << "\xC1\xEE\x08";                        // shr esi, 9 

	// 0 amount shift (do nothing)
}


void compiler::add_ecx(unsigned long imm)
{
	if (imm)
	{
		s << "\x81\xC1"; write( s, imm ); // add ecx, imm
	}
}

void compiler::add_eax(unsigned long imm)
{
	if (imm)
	{
		s << "\x05"; write( s, imm ); // add eax, imm
	}
}

void compiler::generic_store()
{
	break_if_pc(ctx.rd);                 // todo handle rd = PC
	load_ecx_reg_or_pc(ctx.rn);          // ecx = reg[rn]
	load_edx_ecx_or_reg(ctx.rd, ctx.rn); // edx = reg[rd]
}

void compiler::generic_loadstore_postupdate_imm()
{
	if (ctx.imm)
	{
		s << "\x81\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+Rn],
		write( s, ctx.imm );                           //  imm
	}
}

void compiler::generic_loadstore_postupdate_ecx()
{
	s << "\x89\x4D" << (char)OFFSET(regs[ctx.rn]); // mov [ebp+rn], ecx
}


void compiler::generic_store_p()
{
	generic_store();
	add_ecx(ctx.imm);                    // ecx += imm
}


void compiler::generic_store_r()
{
	generic_store();
	generic_loadstore_shift();
}

void compiler::generic_loadstore_shift()
{
	if ((ctx.shift == SHIFT::LSL) && (ctx.imm == 0))
	{
		// simply add Rm to ecx
		s << "\x03\x4D" << (char)OFFSET(regs[ctx.rm]); // add ecx, [ebp+rm]
		return;
	} 
	
	s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax,[ebp+rm]
	switch (ctx.shift)
	{
	case SHIFT::LSL:
		s << "\xC1\xE0" << (char)ctx.imm;          // shl eax, imm
		//break; // couldnt verify yet
	default:
		s << DEBUG_BREAK;
	}		
	s << "\x03\xC8";                               // add ecx, eax
}

void compiler::generic_load()
{
	load_ecx_reg_or_pc(ctx.rn, ctx.imm);
}

void compiler::generic_load_post()
{
	break_if_pc(ctx.rd);            // todo handle rd = PC
	load_ecx_reg_or_pc(ctx.rn, 0);  // ecx = reg[rn]
}

void compiler::generic_load_r()
{
	
	break_if_pc(ctx.rd);           // todo handle rd = PC
	//generic_load();
	load_ecx_reg_or_pc(ctx.rn, 0); // imm is used for shifter here!
	//generic_loadstore_shift();   // r versions should use imm for register id
	
	/*
	unsigned long imm4 = ctx.imm & 0xF;
	break_if_pc(imm4);
	s << "\x03\x4D" << (char)OFFSET(regs[imm4]); // add ecx, [ebp+Rimm4]
	*/

	if (ctx.flags & disassembler::U_BIT)
		s << "\x03\x4D" << (char)OFFSET(regs[ctx.rm]); // add ecx, [ebp+rm]
	else s << "\x2B\x4D" << (char)OFFSET(regs[ctx.rm]); // sub ecx, [ebp+rm]
	
}

void compiler::generic_load_rs(bool post, bool wb)
{
	break_if_pc(ctx.rd);
	break_if_pc(ctx.rn);
	load_shifter_imm();                            // eax = rm SHIFT imm
	
	if (!(ctx.flags & disassembler::U_BIT))
		s << "\xF7\xD8";                           // neg eax

	if (post)
	{
		s << "\x03\x45" << (char)OFFSET(regs[ctx.rn]); // add eax, [ebp+rn]	
		s << "\x8B\xC8";                               // mov ecx, eax
		if (wb)
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rn]); // mov [ebp+rn], eax
	} else
	{
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, [ebp+rn]
		if (wb)
			s << "\x01\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+rn], eax
	}
}

void compiler::generic_postload_shift()
{
	/*
	if (ctx.rn != ctx.rd) // unpredictable load wins according to armwrestler
	{
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		load_shifter_imm();                            // eax = rm SHIFT imm
		if (!(ctx.flags & disassembler::U_BIT))
			s << "\xF7\xD8";                           // neg eax
		s << "\x01\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+rn], eax
	}
	*/

	s << '\x50';                                   // push eax (load result)
	load_shifter_imm();
	if (!(ctx.flags & disassembler::U_BIT))
			s << "\xF7\xD8";                       // neg eax
	s << "\x01\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+rn], eax
	s << "\x8F\x45" << (char)OFFSET(regs[ctx.rd]); // pop [ebp+rd] 
}

void compiler::generic_load_x()
{
	switch (ctx.extend_mode)
	{
	case EXTEND_MODE::H:  CALLP(load16u); break;
	case EXTEND_MODE::SB: CALLP(load8u) 
		s << "\x0F\xBE\xC0"; // movsx eax,al .
		break;
	case EXTEND_MODE::SH: CALLP(load16s); break;
	default:
		s << DEBUG_BREAK;
	}
}

void compiler::generic_store_x()
{
	switch (ctx.extend_mode)
	{
	case EXTEND_MODE::H:  CALLP(store16); break;
	//case EXTEND_MODE::SB: CALLP(store8s); break; // special instruction ...
	//case EXTEND_MODE::SH: CALLP(store16s); break; // special instruction ...
	default:
		s << DEBUG_BREAK;
	}
}


void compiler::load_r15_ecx()
{
	s << "\x8B\x4D" << (char)OFFSET(regs[15]);                               // mov ecx, [ebp+R15]
	s << "\x81\xE1"; write( s, (unsigned long)(~PAGING::ADDRESS_MASK | 1) ); // and ecx, ~PAGING::ADDR_MASK 
}


const unsigned long cpsr_masks[16] =
{
	0x00000000,
	0x000000FF,
	0x0000FF00,
	0x0000FFFF,
	0x00FF0000,
	0x00FF00FF,
	0x00FFFF00,
	0x00FFFFFF,
	0xFF000000,
	0xFF0000FF,
	0xFF00FF00,
	0xFF00FFFF,
	0xFFFF0000,
	0xFFFF00FF,
	0xFFFFFF00,
	0xFFFFFFFF
};

// EVERYTHING HERE IS ONLY VALID FOR THE ARM9 SO FAR!!!

compiler::compiler() : s(std::ostringstream::binary | std::ostringstream::out)
{
	flags_updated = false;
	//preoff = 0;
}

void compiler::record_callstack()
{
	s << '\x51'; // push ecx
	CALLP(pushcallstack);
	s << '\x59'; // pop ecx
}

void compiler::update_callstack()
{
	s << '\x51'; // push ecx
	CALLP(popcallstack);
	s << '\x59'; // pop ecx
}


void compiler::add_ecx_bpre()
{
	// originally was
	// s << "\x81\xC1"; write( s, bpre + ctx.imm - 4 );   // add ecx, imm
	// but this causes trouble with bpre beeing in the previous page
	// could do trickier optimization here though

	s << "\x81\xC1"; write( s, ctx.imm - 4 );             // add ecx, imm
	s << "\x03\x4D" << (char)OFFSET(bpre);				  // add ecx, [ebp+bpre]
	s << "\xC7\x45" << (char)OFFSET(bpre); write( s, 0 ); // mov [ebp+bpre], 0
}

// mov [ebp+rd], eax
// branch when rd = R15
void compiler::store_rd_eax()
{
	s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
	if (ctx.rd == 15)
	{
		s << "\x8B\xC8"; // mov ecx, eax
		JMPP(compile_and_link_branch_a)
	}
}

void compiler::compile_instruction()
{
	bool patch_jump = false;
	std::ostringstream::pos_type jmpbyte;
	//unsigned long bpre = preoff;
	//preoff = 0;
	int flags_actual = flags_updated;

	
#ifdef CLOCK_CYCLES
	// how to do this without updating flags?!
	// when postpending this to a flag producing op it'd invalidate the flag
	// when prepending it to a consuming op it invalidates too
	s << '\x43'; // inc ebx
	flags_actual = 0;

	// might use SSE at some time ... the only way i see so far for not
	// requiring to invalidate flags
	//s << "\x0F\xD4\xC1"; // paddq mm0,mm1
#endif

	if (ctx.cond != CONDITION::AL)
	{
		if (ctx.cond != CONDITION::NV)
		{
			if (!flags_actual)
			{
				load_flags();
				flags_actual = 1;
			}
			switch (ctx.cond)
			{
			// conditions are _reversed_ in the jumps because a jump taken
			// means the instruction will _not_ be executed.
			case CONDITION::EQ: s << '\x75'; break; // jnz  (ZF = 0)
			case CONDITION::NE: s << '\x74'; break; // jz   (ZF = 1)
			case CONDITION::CS: s << '\x73'; break; // jnc  (CF = 0)
			case CONDITION::CC: s << '\x72'; break; // jc   (CF = 1)
			case CONDITION::MI: s << '\x79'; break; // jns  (SF = 0)
			case CONDITION::PL: s << '\x78'; break; // js   (SF = 1)
			case CONDITION::VS: s << '\x71'; break; // jno  (OF = 0)
			case CONDITION::VC: s << '\x70'; break; // jo   (OF = 1)
			case CONDITION::HI: s << '\x76'; break; // jbe  (CF = 1 or  ZF = 1)
			case CONDITION::LS: s << '\x77'; break; // jnbe (CF = 0 and ZF = 0)
			case CONDITION::GE: s << '\x7C'; break; // jl   (SF != OF)
			case CONDITION::LT: s << '\x7D'; break; // jge  (SF == OF) (jnl)
			case CONDITION::GT: s << '\x7E'; break; // jle  (ZF = 0 or  SF != OF)
			case CONDITION::LE: s << '\x7F'; break; // jg   (ZF = 0 and SF == OF) (jnle)
			}
			jmpbyte = s.tellp();
			patch_jump = true;
			s << '\x00';
		}
	}
	flags_updated = 0;

////////////////////////////////////////////////////////////////////////////////
	switch (ctx.instruction)
	{
	case INST::MOV_I:
		break_if_pc(ctx.rd);
		s << "\xC7\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], imm32
		write( s, ctx.imm );
		if (ctx.flags & disassembler::S_BIT)
		{
			s << "\x83\x4D" << (char)OFFSET(regs[ctx.rd]) << '\0'; // or [ebp+rd], 0
			store_flags();
		}
		break;

	case INST::MOV_R:

		// check for mov r12, r12 (debug magic)
		if ((ctx.rd == ctx.rm) && (ctx.imm == 0))
		{
			if (ctx.rd == 12)
			{
				load_r15_ecx();
				s << "\x81\xC1"; write( s, (unsigned long)(inst+1) << INST_BITS);  
				CALLP(debug_magic)
			} else
				s << "\x90"; // << DEBUG_BREAK;
			break;
		}
		load_shifter_imm();
		if (ctx.rd == 0xF)
		{
			if (ctx.flags & disassembler::S_BIT)
				s << DEBUG_BREAK;
			s << "\x8B\x4D" << (char)OFFSET(regs[15]); // mov ecx, [r15]
			s << "\x83\xE1\x01";                       // and ecx, 1
			s << "\x0B\xC8";                           // or ecx, eax
			s << "\x89\x4D" << (char)OFFSET(regs[15]); // mov [ebp+rd], ecx
			JMPP(compile_and_link_branch_a)
		} else
		{
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}

		load_carry();
		if (ctx.flags & disassembler::S_BIT)
		{
			// only when no shifter op
			if (ctx.imm == 0)
				s << "\x83\xC8" << '\x0'; // or eax, 0
			store_flags();
		}
		break;

	case INST::MOV_RR: 	// "MOV%c%s %Rd,%Rm,%S %Rs",
		break_if_pc(ctx.rd);
		load_ecx_reg_or_pc(ctx.rs);          // ecx = reg[rn]
		load_eax_ecx_or_reg(ctx.rm, ctx.rs); // eax = reg[rd]
		shiftop_eax_ecx();
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		if (ctx.flags & disassembler::S_BIT)
		{
			// todo optimize the flagcheck (doesnt need to do the cmp)
			//store_carry();
			s << "\x83\xC8" << '\x0'; // or eax, 0
			load_carry();
			store_flags();
		}
		break;

	case INST::MVN_I:
		break_if_pc(ctx.rd);
		s << "\xC7\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ~imm32
		write( s, ~ctx.imm );
		break;
	case INST::MVN_R:
		break_if_pc(ctx.rd);
		if ((ctx.rd == ctx.rm) && (ctx.shift == SHIFT::LSL) && (ctx.imm == 0))
		{
			s << "\xF7\x55" << (char)OFFSET(regs[ctx.rd]);  // not [ebp+rd]
		} else
		{
			load_shifter_imm();
			s << "\xF7\xD0";                                // not eax
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		}
		load_carry();
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::STR_I:
		generic_store();
		CALLP(store32)
		generic_loadstore_postupdate_imm();
		break;
	case INST::STRB_I:
		generic_store();
		CALLP(store8);
		generic_loadstore_postupdate_imm();
		break;

	/*
	case INST::STR_IW:
		s << "\x90";
		s << DEBUG_BREAK;
		break;
	*/
	case INST::STRX_I:
		generic_store();
		generic_store_x();
		generic_loadstore_postupdate_imm();
		break;

	case INST::STRX_IP:
		generic_store_p();
		generic_store_x();
		break;
	case INST::STR_IPW:
		generic_store_p();
		//s << "\x89\x4D" << (char)OFFSET(regs[ctx.rn]); // mov [ebp+rn], ecx
		CALLP(store32)
		generic_loadstore_postupdate_imm();	
		break;
	case INST::STRX_RP:
		generic_store_r();
		generic_store_x();
		break;
	case INST::STRB_IP:
		generic_store_p();
		CALLP(store8) 
		break;
	case INST::STR_IP:
		generic_store_p();
		CALLP(store32) 
		break;
	case INST::STR_RP:
		generic_store_r();
		CALLP(store32) 
		break;
	case INST::STRB_RP:
		generic_store_r();
		CALLP(store8) 
		break;


	// Post index loads
	case INST::LDR_I:
		generic_load_post();
		generic_loadstore_postupdate_imm();
		CALLP(load32)
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;

	// Pre index loads
	case INST::LDR_IP:
		generic_load();
		CALLP(load32)
		store_rd_eax();
		break;
	case INST::LDRX_IP:
		generic_load();
		generic_load_x();
		store_rd_eax();
		break;

	case INST::LDR_IPW:
		generic_load();
		CALLP(load32)
		generic_loadstore_postupdate_imm();
		store_rd_eax();
		break;
	case INST::LDRB_IPW:
		generic_load();
		CALLP(load8u)
		generic_loadstore_postupdate_imm();
		store_rd_eax();
		break;
	case INST::LDRX_IPW:
		generic_load();
		generic_load_x();
		generic_loadstore_postupdate_imm();
		store_rd_eax();
		break;


	case INST::LDR_R:
		//generic_load_rs(false, false);
		load_ecx_reg_or_pc(ctx.rn);
		CALLP(load32)
		
		// post increment: load shifter and update rn
		generic_postload_shift();
		
		break;

	case INST::LDR_RP:
		generic_load_rs(true, false);
		CALLP(load32) 
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;

	case INST::LDR_RPW:
		generic_load_rs(true, true);
		s << "\x89\x4D" << (char)OFFSET(regs[ctx.rn]);  // mov [ebp+rn], ecx
		CALLP(load32) 
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;

	case INST::LDRB_I:
		generic_load_post();
		generic_loadstore_postupdate_imm();
		CALLP(load8u)
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;
	case INST::LDRB_IP:
		break_if_pc(ctx.rd);                  // todo handle rd = PC
		generic_load();
		CALLP(load8u)
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;
	case INST::LDRB_RP: 
		generic_load_r();
		CALLP(load8u)
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;

	case INST::LDRX_RP: // bogus!
		generic_load_r();
		generic_load_x();
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);  // mov [ebp+rd], eax
		break;


	case INST::BLX_I:
		// Branch and link
		load_r15_ecx();
		s << "\x81\xC1"; write( s, (unsigned long)(inst+1) << INST_BITS);        // add ecx, imm
		s << "\x89\x4D" << (char)OFFSET(regs[14]);             // mov [ebp+r14], ecx
		record_callstack();
		s << "\x81\xE1"; write( s, (unsigned long)(~1) );  // and ecx, ~1
		s << "\x83\xE0\xFE";                               // and eax, 0FFFFFFFEh 
		add_ecx_bpre();
		s << "\x89\x4D" << (char)OFFSET(regs[15]);         // mov [ebp+r15], ecx
		JMPP(compile_and_link_branch_a)
		break;
	case INST::BL:
		// Branch and link
		load_r15_ecx();
		s << "\x81\xC1"; write( s, (unsigned long)(inst+1) << INST_BITS);        // add ecx, imm
		s << "\x89\x4D" << (char)OFFSET(regs[14]);             // mov [ebp+r14], ecx
		record_callstack();
		add_ecx_bpre();
		s << "\x89\x4D" << (char)OFFSET(regs[15]);             // mov [ebp+r15], ecx
		JMPP(compile_and_link_branch_a)
		break;
	case INST::BX:
		// Branch to register (generally R14)
		load_ecx_reg_or_pc(ctx.rm, 0);              // mov ecx, [ebp+rm]
		s << "\x89\x4D" << (char)OFFSET(regs[15]);  // mov [ebp+r15], ecx
		update_callstack();
		JMPP(compile_and_link_branch_a)
		//s << "\xFF\xE0";                          // jmp eax
		break;

	case INST::BLX: // Branch and link register
		// link
		load_r15_ecx();
		s << "\x81\xC1"; write( s, (unsigned long)(inst+1) << INST_BITS); // add ecx, imm
		s << "\x89\x4D" << (char)OFFSET(regs[14]);             // mov [ebp+r14], ecx
		record_callstack();
		// branch
		load_ecx_reg_or_pc(ctx.rm, 0);                         // mov ecx, [ebp+rm]
		s << "\x89\x4D" << (char)OFFSET(regs[15]);             // mov [ebp+r15], ecx
		JMPP(compile_and_link_branch_a)
		//s << "\xFF\xE0";                                       // jmp eax
		break;
	// case BLX_I => use +bpre

	case INST::B:
		// This branch jumps correct now
		load_r15_ecx();
		s << "\x81\xC1"; write( s, ctx.imm + (unsigned long)((inst) << INST_BITS)); // add ecx, imm
		s << "\x89\x4D" << (char)OFFSET(regs[15]);    // mov [ebp+r15], ecx
		update_callstack();
		JMPP(compile_and_link_branch_a)
		//s << "\xFF\xE0";                              // jmp eax
		break;
	case INST::BPRE:
		//preoff = ctx.imm;
		// VC bug: mov [ebp+0x10], 0xbadc0de => mov byte ptr ...
		s << "\xC7\x45" << (char)OFFSET(bpre); write( s, (unsigned long)ctx.imm); // mov [ebp+bpre], offset
		break;



	// MCR => ARM -> Coprocessor (write to Coproc)
	// MRC => ARM <- Coprocessor (read from Coproc)

	// this is pretty much like machine specific registers on x86
	case INST::MRC: // transfer to registers
		switch (ctx.cp_num) 
		{
		case 0xF: // System control coprocessor
			if (ctx.cp_op1 != 0) // SBZ
				goto default_;   // unpredictable
			if (ctx.rd == 0xF)   // Rd must not be R15
				goto default_;   // unpredictable
			switch (ctx.rn) // select register
			{
			case 0: // ID codes (read only)
				switch (ctx.cp_op2)
				{
				//case 1:  // Cache Type register (ensata doesnt emulate this, no$gba crashs lol) 
				//	break; // spec says only 0 is mandatory so this is ok
				case 0: // Main ID register
				default: // if unimplemented register is queried return Main ID register
					// no$gba returns 0x41059461
					s << "\xC7\x45" << (char)OFFSET(regs[ctx.rd]); write( s, (unsigned long)0x41059460 );
					break;
				}
				break;
			case 1: // control bits (r/w)
				// SBZ but ignored
				//if ((ctx.rm != 0) || (ctx.cp_op2 != 0))
				//	goto default_;
				s << "\x8B\x45" << (char)OFFSET(syscontrol.control_register); // mov eax,[ebp+creg]
				s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);                // mov [ebp+rd],eax
				break;
			default:
				goto default_; // not supported yet
			}
			break;
		default:
			goto default_;
		}
		break;


	case INST::MCR: // load coproc from arm register
		switch (ctx.cp_num) 
		{
		case 0xF: // System control coprocessor
			if (ctx.cp_op1 != 0) // SBZ
				goto default_;   // unpredictable
			if (ctx.rd == 0xF)   // Rd must not be R15
				goto default_;   // unpredictable
			switch (ctx.rn) // select register
			{
			
			case 1: // control bits (r/w)
				// SBZ but ignored
				//if ((ctx.rm != 0) || (ctx.cp_op2 != 0))
				//	goto default_;
				s << "\x8B\x45" << (char)OFFSET(regs[ctx.rd]);                // mov eax, [ebp+rd]
				s << "\x89\x45" << (char)OFFSET(syscontrol.control_register); // mov [ebp+creg], eax
				s << "\x0B\xC0"; // or eax, eax
				s << "\x75\x05"; // jmp $+5
				s << '\xB8'; write(s, (unsigned long)syscontrol_context::CONTROL_DEFAULT );
				// mov eax, syscontrol_context::CONTROL_DEFAULT
				break;
			
			// MMU/PU not supported yet
			case 2:
			case 3:
			case 5:
			case 6: // CRm and opcode 2 select the protected region/bank
			 
				s << NOT_SUPPORTED_YET_BUT_SKIP;

			case 7: // cache and write buffers (write only)
				s << '\x90'; // not yet emulated or unimportant xD
				break;
			case 9: // cache lockdown or TCM remapping
				switch (ctx.rm)
				{
				case 0x1: // TCM remapping
					load_ecx_reg_or_pc(ctx.rd);           // mov ecx, [ebp+rd]
					s << '\xBA'; s.write( (char*)&ctx.cp_op2, sizeof(ctx.cp_op2) ); // mov edx, ctx.cp_op2
					CALLP(remap_tcm);
					break;
				default:
					s << '\x90'; // lockdown not emulated
					break;
				}
				break;

			case 0: // may not write to this register
			default:
				goto default_; // not supported yet
			}
			break;
		default:
			goto default_;
		}
		break;

	case INST::BIC_R: // Rd = Rn & ~shifter_imm
		load_shifter_imm();
		s << "\xF7\xD0";                               // not eax;
		if (ctx.rn == ctx.rd)
		{
			s << "\x21\x45" << (char)OFFSET(regs[ctx.rn]); // and [ebp+rd], eax
		} else
		{
			s << "\x23\x45" << (char)OFFSET(regs[ctx.rn]); // and eax, [ebp+rn]
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		load_carry();
		if (ctx.flags & disassembler::S_BIT)
			store_flags(); // todo: set carry = shifter carry
		break;
	case INST::AND_R: // Rd = Rn & shifter_imm
		load_shifter_imm();
		if (ctx.rn == ctx.rd)
		{
			s << "\x21\x45" << (char)OFFSET(regs[ctx.rn]); // and [ebp+rd], eax
		} else
		{
			s << "\x23\x45" << (char)OFFSET(regs[ctx.rn]); // and eax, [ebp+rn]
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		load_carry();
		if (ctx.flags & disassembler::S_BIT)
			store_flags(); // todo: set carry = shifter carry
		break;
	case INST::EOR_R: // Rd = Rn ^ shifter_imm
		load_shifter_imm();
		if (ctx.rn == ctx.rd)
		{
			s << "\x31\x45" << (char)OFFSET(regs[ctx.rn]); // xor [ebp+rd], eax
		} else
		{
			s << "\x33\x45" << (char)OFFSET(regs[ctx.rn]); // xor eax, [ebp+rn]
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		load_carry();
		if (ctx.flags & disassembler::S_BIT)
			store_flags(); // todo: set carry = shifter carry
		break;


	case INST::BIC_I:
		break_if_pc(ctx.rn);
		break_if_pc(ctx.rd);
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax,[ebp+Rn]
		s << '\x25'; write(s, ~ctx.imm);               // and eax, ~imm
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::ORR_I: // Rd = Rn | imm
		break_if_pc(ctx.rn);
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			s << "\x81\x4D" << (char)OFFSET(regs[ctx.rn]); // or [ebp+rn],
			write( s, ctx.imm );                           //  imm
		} else
		{
			s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax, [ebp+rn]
			s << "\x0D"; write(s, ctx.imm );               // or eax, imm
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::AND_I: // Rd = Rn & imm
		break_if_pc(ctx.rn);
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			s << "\x81\x65" << (char)OFFSET(regs[ctx.rn]); // and [ebp+rn],
			write( s, ctx.imm );                           //  imm
		} else
		{
			s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax, [ebp+rn]
			s << "\x25"; write(s, ctx.imm );               // and eax, imm
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::EOR_I: // Rd = Rn ^ imm
		break_if_pc(ctx.rn);
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			s << "\x81\x75" << (char)OFFSET(regs[ctx.rn]); // xor [ebp+rn],
			write( s, ctx.imm );                           //  imm
		} else
		{
			s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax, [ebp+rn]
			s << "\x35"; write(s, ctx.imm );               // xor eax, imm
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		}
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::ADD_I: // Rd = Rn + imm
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			break_if_pc(ctx.rn);
			s << "\x81\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+rn],
			write( s, ctx.imm );                           // imm
		} else
		{
			load_ecx_reg_or_pc(ctx.rn, ctx.imm);
			s << "\x89\x4D" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ecx
		}
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	case INST::ADD_R: // Rd = Rn + shifter_imm
		{
			load_shifter_imm();
			if (ctx.rn == ctx.rd)
			{
				s << "\x01\x45" << (char)OFFSET(regs[ctx.rn]); // add [ebp+Rn], eax
			} else
			{
				s << "\x03\x45" << (char)OFFSET(regs[ctx.rn]); // add eax, [ebp+Rn]
				s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
			}
			if (ctx.flags & disassembler::S_BIT)
				store_flags();
			break;
		}
	case INST::ADC_R: // Rd = Rn + shifter_imm + carry
		{
			// optimize this
			load_shifter_imm();
			s << '\x50'; // push eax
			load_flags();
			s << '\x58'; // pop eax
			if (ctx.rn == ctx.rd)
			{
				s << "\x11\x45" << (char)OFFSET(regs[ctx.rn]); // adc [ebp+Rn], eax
			} else
			{
				s << "\x13\x45" << (char)OFFSET(regs[ctx.rn]); // adc eax, [ebp+Rn]
				s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
			}
			if (ctx.flags & disassembler::S_BIT)
				store_flags();
			break;
		}

	case INST::SUB_I: // Rd = Rn - imm
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			break_if_pc(ctx.rn);
			s << "\x81\x6D" << (char)OFFSET(regs[ctx.rn]); // sub [ebp+rn],
			write( s, ctx.imm );                           // imm
		} else
		{
			load_ecx_reg_or_pc(ctx.rn, (unsigned long)(-(signed long)ctx.imm));
			s << "\x89\x4D" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ecx
		}
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	case INST::SUB_R: // Rd = Rn - shifter_imm
		{
			load_shifter_imm();
			if (ctx.rn == ctx.rd)
			{
				s << "\x29\x45" << (char)OFFSET(regs[ctx.rn]); // sub [ebp+Rn], eax
			} else
			{
				s << "\x8B\x55" << (char)OFFSET(regs[ctx.rn]); // mov edx, [ebp+Rn]
				s << "\x2B\xD0";                               // sub edx, eax
				s << "\x89\x55" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], edx
			}
			if (ctx.flags & disassembler::S_BIT)
				store_flags(); // todo: set carry = shifter carry
			break;
		}

	case INST::SBC_I: // Rd = Rn - imm - !carry
		{
			if (!flags_actual)
				load_flags();
			s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax, [ebp+rn]
			s << "\xF5";                                   // cmc
			s << "\x1D" << (unsigned long)(ctx.imm);       // sbb eax, imm
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
			if (ctx.flags & disassembler::S_BIT)
			{
				s << "\xF5";   // cmc
				store_flags(); // todo: set carry = shifter carry
			}

			break;
		}

	case INST::SBC_R: // Rd = Rn - shifter_imm - !carry
		{
			// optimize (see RSC)
			load_shifter_imm();
			s << "\x8B\xC8";                               // mov ecx, eax (flagload destroy eax)
			if (!flags_actual)
				load_flags();
			s << "\xF5";                                   // cmc
			if (ctx.rn == ctx.rd)
			{
				s << "\x19\x4D" << (char)OFFSET(regs[ctx.rn]); // sbb [ebp+Rn], ecx
			} else
			{
				s << "\x8B\x55" << (char)OFFSET(regs[ctx.rn]); // mov edx, [ebp+Rn]
				s << "\x1B\xD1";                               // sbb edx, ecx
				s << "\x89\x55" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], edx
			}
			if (ctx.flags & disassembler::S_BIT)
			{
				s << "\xF5";   // cmc
				store_flags(); // todo: set carry = shifter carry
			}
			break;
		}

	case INST::RSB_I:
		break_if_pc(ctx.rn);
		break_if_pc(ctx.rd);
		if (ctx.rn == ctx.rd)
		{
			if (ctx.imm == 0)
			{
				s << "\xF7\x5D" << (char)OFFSET(regs[ctx.rn]); // neg [ebp+Rn]
				if (ctx.flags & disassembler::S_BIT)
					store_flags();
				break;
			}
		} else
		{
			if (ctx.imm == 0)
			{
				// dst = neg src
				s << "\x8B\x45" << (char)OFFSET(regs[ctx.rn]); // mov eax, [ebp+rn]
				s << "\xF7\xD8";                               // neg eax
				s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
				if (ctx.flags & disassembler::S_BIT)
					store_flags();				
				break;
			}
			break;
		}

		// generic form
		// dst = shifter_imm - rn
		s << '\xB8'; write(s, ctx.imm);                // mov eax, imm
		s << "\x2B\x45" << (char)OFFSET(regs[ctx.rn]); // sub eax, [ebp+rn]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		break;
	case INST::RSB_R:
		load_shifter_imm();
		s << "\x2B\x45" << (char)OFFSET(regs[ctx.rn]); // sub eax, [ebp+rn]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;


	case INST::ORR_R: // Rd = Rn | shifter_imm
		load_shifter_imm();
		s << "\x0B\x45" << (char)OFFSET(regs[ctx.rn]); // or eax, [ebp+Rn]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		load_carry();
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	case INST::RSC_R: // Rd = shifter - Rn - !carry
		if (!flags_actual)
			load_flags();
		s << "\xF5";                                   // cmc
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, [ebp+Rn]
		s << "\x83\xD1" << '\x00';                     // adc ecx, 0
		load_shifter_imm();
		s << "\x2B\xC1";                               // sub eax, ecx
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		if (ctx.flags & disassembler::S_BIT)
		{
			s << "\xF5";   // cmc
			store_flags();
		}
		break;
		/*
	case INST::SBC_R: // Rd = Rn - shifter - !carry
		s << "\xF5";                                   // cmc
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, [ebp+Rn]
		__asm sbb ecx, 0
		s << "\x1B\x4D" << (char)OFFSET(regs[ctx.rn]); // sbb ecx, 0
		load_shifter_imm();
		s << "\x2B\xC1";                               // sub eax, ecx
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	*/


	case INST::CLZ:
		break_if_pc(ctx.rm);
		break_if_pc(ctx.rd);
		
		//s << DEBUG_BREAK;
		/*
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+Rm]
		s << "\x33\xC9"; // xor ecx, ecx
		s << "\xD1\xE8"; // shr eax, 1
		s << "\x74\x03"; // jz $+3
		s << "\x41";     // inc ecx
		s << "\xEB\xF9"; // jmp $-7

		s << "\xB8"; write(s, (unsigned long)0x20); // mov eax, 20
		s << "\x2B\xC1"; // sub eax, ecx
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		//#s << "\x89\x4D" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ecx
		*/

		//s << "\x0F\xBD\x45" << (char)OFFSET(regs[ctx.rm]); // bsr eax, [ebp+Rm]
		
		/*
		__asm not eax
		__asm bsf ecx, eax
		__asm bsr ecx, eax
		*/


		/*
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+Rm]
		s << "\xF7\xD0";                               // not eax
		//s << "\x0F\xBC\xC8";                           // bsf ecx, eax
		s << "\x0F\xBD\xC8";                           // bsr ecx, eax
		s << "\x89\x4D" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ecx
		*/


		//s << "\x0F\xBC\x45" << (char)OFFSET(regs[ctx.rm]); // bsf eax, [ebp+Rm]
		//s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);     // mov [ebp+rd], eax


		// 00001010
		// 11110101

		
		//__asm rcl eax, 1
			// 01838247 D1 D0            rcl         eax,1 
		//s << '\xB9'; write(s, (unsigned long)0x20);    // mov ecx, 0x20
		
		/*
		s << "\x33\xC9";             // xor ecx, ecx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+Rm]
		s << "\xC1\xE8" << (char)1;  // shr eax, 1
		s << "\x76\x03";             // jbe $+3 (if zero or carry was set exit)
		s << "\x41";                 // inc ecx
		s << "\xEB\xF9";             // jmp $-7
		s << "\x72\x03";             // jc $+1
		s << "\x41";                 // inc ecx (add 1 more when carry was not set)		
		s << "\x89\x4D" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], ecx
		*/

		
		s << "\xB9"; write(s, (unsigned long)0xFFFFFFFF); // mov ecx, 0xFFFFFFFF
		s << "\xC7\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], 
		write(s, (unsigned long)32);                   //   32
		s << "\x8B\xC1";                               // mov eax, ecx
		s << "\x23\x45" << (char)OFFSET(regs[ctx.rm]); // and eax, [ebp+rm]
		s << "\x74\x07";                               // jz $+7
		//s << "\xD1\xE9";                               // shr ecx, 1
		s << "\xD1\xE1";                               // shr ecx, 1
		s << "\xFF\x4D" << (char)OFFSET(regs[ctx.rd]); // dec [ebp+rd]
		s << "\xEB\xF2";                               // jmp $-14
		break;

	case INST::MLA_R:
		break_if_pc(ctx.rm);
		break_if_pc(ctx.rs);
		break_if_pc(ctx.rd);
		break_if_pc(ctx.rn);
		
		// Rd = Rm * Rs + Rn
		s << "\x33\xD2"; // xor edx, edx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << "\xF7\x65" << (char)OFFSET(regs[ctx.rs]); // mul [ebp+rs]
		s << "\x03\x45" << (char)OFFSET(regs[ctx.rn]); // add eax, [ebp+Rn]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		// flag need to be updated to not be affected by high 32bit...
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	
	case INST::UMULL:
		s << "\x33\xD2"; // xor edx, edx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << "\xF7\x65" << (char)OFFSET(regs[ctx.rs]); // mul [ebp+rs]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		s << "\x89\x55" << (char)OFFSET(regs[ctx.rn]); // mov [ebp+rd], edx 
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	case INST::UMLAL:
		s << "\x33\xD2"; // xor edx, edx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << "\xF7\x65" << (char)OFFSET(regs[ctx.rs]); // mul [ebp+rs]
		s << "\x01\x45" << (char)OFFSET(regs[ctx.rd]); // add [ebp+rd], eax
		s << "\x11\x55" << (char)OFFSET(regs[ctx.rn]); // adc [ebp+rd], edx 
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::SMULL:
		s << "\x33\xD2"; // xor edx, edx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << "\xF7\x6D" << (char)OFFSET(regs[ctx.rs]); // imul [ebp+rs]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		s << "\x89\x55" << (char)OFFSET(regs[ctx.rn]); // mov [ebp+rd], edx 
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;
	case INST::SMLAL:
		s << "\x33\xD2"; // xor edx, edx
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << "\xF7\x6D" << (char)OFFSET(regs[ctx.rs]); // imul [ebp+rs]
		s << "\x01\x45" << (char)OFFSET(regs[ctx.rd]); // add [ebp+rd], eax
		s << "\x11\x55" << (char)OFFSET(regs[ctx.rn]); // adc [ebp+rd], edx 
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::MUL_R: // Rd = Rm * Rs (Rd must not be Rm)
		break_if_pc(ctx.rm);
		break_if_pc(ctx.rs);
		break_if_pc(ctx.rd);

		
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << '\x99';                                   // cdq (sign extend)
		//s << "\x33\xD2";                               // xor edx, edx (zero extend)
		//s << "\xF7\x65" << (char)OFFSET(regs[ctx.rs]); // mul [ebp+rs]
		
		s << "\xF7\x6D" << (char)OFFSET(regs[ctx.rs]); // imul [ebp+rs]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		s << "\x0B\xC0";                               // or eax, eax // flag update

		
		
		if (ctx.flags & disassembler::S_BIT)
			store_flags();
		break;

	case INST::SWP:
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, [ebp+rn]
		s << '\x51'; // push ecx
		CALLP(load32)
		s << '\x59'; // pop ecx
		s << "\x8B\x55" << (char)OFFSET(regs[ctx.rm]); // mov edx, [ebp+rm]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		CALLP(store32)
		break;

	case INST::SWPB:
		s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rn]); // mov ecx, [ebp+rn]
		s << '\x51'; // push ecx
		CALLP(load8u)
		s << '\x59'; // pop ecx
		s << "\x8B\x55" << (char)OFFSET(regs[ctx.rm]); // mov edx, [ebp+rm]
		s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]); // mov [ebp+rd], eax
		CALLP(store8)
		break;

	case INST::MRS_CPSR:
	case INST::MRS_SPSR:
		{
			break_if_pc(ctx.rd);
			if (ctx.instruction == INST::MRS_SPSR)
				s << "\x8B\x45" << (char)OFFSET(spsr); // mov eax, SPSR
			else
			{
				CALLP(storecpsr)
			}
			s << "\x89\x45" << (char)OFFSET(regs[ctx.rd]);     // mov [ebp+rd], eax
			break;
		}

	case INST::MSR_CPSR_I:
	case INST::MSR_SPSR_I:
		{
			break_if_pc(ctx.rm);
			/*
			char sr = (char)OFFSET(cpsr);
			if (ctx.instruction == INST::MSR_SPSR_R)
				sr = (char)OFFSET(spsr);

			// mask = Rn
			// todo: priviledge mode check depending on mask
			if (ctx.rn == 0)
				s << "\x90"; // nothing masked = nothing to write...
			else
			{
				if (ctx.rn & 0x7)
					CALLP(is_priviledged)
				
				s << '\xB8'; write(s, ctx.imm); // mov eax, imm
				if (ctx.rn != 0xF)
				{
					// mask out
					s << '\x25'; write(s, cpsr_masks[ctx.rn]); // and eax, mask imm
				}
				

				// store to cpsr
				s << "\x89\x45" << sr; // mov [ebp+*psr], eax

				if (ctx.rn & 0x8)
				{
					// update x86 flags
					// eax-BIT x86-FLAG  PSR-BIT
					//   0       OF        28
					//   8       CF        29
					//   14      ZF        30
					//   15      SF        31
					s << '\x25'; write(s, (unsigned long)0x0F8000000); // and eax, 0x0F8000000
					s << "\x8B\xD0";                                   // mov edx, eax
					s << "\xC1\xE8\x10";                               // shr eax, 16 (ZF and SF placed)
					s << "\xC1\xEA\x15";                               // shr edx, 21 (CF placed)
					s << "\x0B\xC2";                                   // or eax, edx
					s << "\xC1\xEA\x07";                               // shr edx, 7 (OF placed)
					s << "\x0B\xC2";                                   // or eax, edx
					s << '\x25'; write(s, (unsigned long)0xC101);      // and eax, 0C101h 
					s << "\x89\x45" << (char)OFFSET(x86_flags);        // mov [ebp+x86_flags], eax
				}
			}
			*/

			if (ctx.rn == 0)
			{
				s << "\x90"; // nothing masked = nothing to change ...nop...
				break;
			}

			if (ctx.instruction == INST::MSR_SPSR_I)
			{
				s << '\xB8'; write(s, ctx.imm);                // mov eax, imm
				if (ctx.rn != 0xF)
				{
					// mask out
					s << '\x25'; write(s, cpsr_masks[ctx.rn]); // and eax, mask imm
				}
				s << "\x89\x45" << (char)OFFSET(spsr);         // mov [ebp+spsr], eax
			}
			else
			{
				// ecx edx
				s << '\xB9'; write(s, ctx.imm);                // mov ecx, imm
				s << '\xBA'; write(s, cpsr_masks[ctx.rn]);     // mov edx, mask
				CALLP(loadcpsr)
				s << "\x8B\xE8";                               // mov ebp, eax handle register set swap
			}
			break;
		}

	case INST::MSR_CPSR_R:
	case INST::MSR_SPSR_R:
		{
			break_if_pc(ctx.rm);

			/*
			char sr = (char)OFFSET(cpsr);
			if (ctx.instruction == INST::MSR_SPSR_R)
				sr = (char)OFFSET(spsr);

			// mask = Rn
			

			// todo: priviledge mode check depending on mask
			if (ctx.rn == 0)
				s << "\x90"; // nothing masked = nothing to write...
			else
			{
				if (ctx.rn & 0x7)
					CALLP(is_priviledged)
				
				s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+Rm]
				if (ctx.rn != 0xF)
				{
					// mask out
					s << '\x25'; write(s, cpsr_masks[ctx.rn]); // and eax, mask
				}
				// store to cpsr
				s << "\x89\x45" << sr; // mov [ebp+*psr], eax
			}
			*/

			if (ctx.rn == 0)
			{
				s << "\x90"; // nothing masked = nothing to change ...nop...
				break;
			}

			
			if (ctx.instruction == INST::MSR_SPSR_R)
			{
				s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]);     // mov eax, [ebp+Rm]
				if (ctx.rn != 0xF)
				{
					// mask out
					s << '\x25'; write(s, cpsr_masks[ctx.rn]); // and eax, mask imm
				}
				s << "\x89\x45" << (char)OFFSET(spsr);         // mov [ebp+spsr], eax
			} else
			{
				s << "\x8B\x4D" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+Rm]
				s << '\xBA'; write(s, cpsr_masks[ctx.rn]);     // mov edx, mask
				CALLP(loadcpsr)
				s << "\x8B\xE8";                               // mov ebp, eax handle register set swap
			}
			break;
		}
	case INST::TST_I: // TST Rn, imm
		break_if_pc(ctx.rn);
		s << "\xF7\x45" << (char)OFFSET(regs[ctx.rn]); // test [ebp+Rm],
		write( s, ctx.imm );                           // imm
		store_flags();
		break;
	case INST::TST_R:
		break_if_pc(ctx.rn);
		load_shifter_imm(); // eax = [ebp+Rm] x shifter
		s << "\x85\x45" << (char)OFFSET(regs[ctx.rn]); // test [ebp+Rd], eax
		store_flags();		
		break;

	case INST::TEQ_I: // TEQ Rn, imm
		break_if_pc(ctx.rn);
		s << "\x8B\x45" << (char)OFFSET(regs[ctx.rm]); // mov eax, [ebp+rm]
		s << '\x35'; write( s, ctx.imm );              // xor eax, imm
		store_flags();
		break;
	case INST::TEQ_R:
		break_if_pc(ctx.rn);
		load_shifter_imm(); // eax = [ebp+Rm] x shifter
		s << "\x33\x45" << (char)OFFSET(regs[ctx.rn]); // xor eax, [ebp+Rd]
		store_flags();		
		break;

	case INST::CMP_I:
		break_if_pc(ctx.rn);
		s << "\x81\x7D" << (char)OFFSET(regs[ctx.rn]); // cmp [ebp+Rn],
		write(s, ctx.imm);                             // imm
		//s << '\xF5';                                   // cmc
		store_flags();
		break;
	case INST::CMN_I:
		break_if_pc(ctx.rn);		
		s << "\xB8"; write( s, ctx.imm );              // mov eax, imm
		s << "\x03\x45" << (char)OFFSET(regs[ctx.rn]); // add eax, [ebp+Rn]
		store_flags();
		break;


	case INST::CMP_R:
		break_if_pc(ctx.rn);
		load_shifter_imm();
		s << "\x39\x45" << (char)OFFSET(regs[ctx.rn]); // cmp [ebp+Rn], eax
		//s << '\xF5';                                   // cmc
		store_flags();
		break;
	case INST::CMN_R:
		break_if_pc(ctx.rn);
		load_shifter_imm();
		s << "\x03\x45" << (char)OFFSET(regs[ctx.rn]); // add eax, [ebp+Rn]
		store_flags();
		break;

	case INST::STM:
	case INST::STM_W:
		{
			if (ctx.flags & disassembler::S_BIT)
				s << DEBUG_BREAK;

			unsigned long num, highest, lowest;
			bool region;
			count( ctx.imm, num, lowest, highest, region );
			switch (num)
			{
			case 0: // spec says this is undefined
				s << DEBUG_BREAK;
				break;
			case 1: // simple 32bit store
				load_ecx_single();      // load ecx, start_address
				s << "\x8B\x55" << (char)OFFSET(regs[highest]); // mov edx, dword ptr [ebp+R_highest]
				CALLP(store32) // dont use array store but simple store here!
				break;
			default:
				unsigned int pop = 0;
				if (region) // optimized version when a region Ra-Rb is stored
				{
					char ofs = (char)OFFSET(regs[lowest]);
					if (ofs == 0)
						s << '\x55';            // push ebp
					else
					{
						s << "\x8D\x45" << ofs; // lea eax, [ebp+Rlow]
						s << '\x50';                // push eax
					}
				} else
				{
					pop = num;

					// check PC
					if (ctx.imm & (1 << 15))
					{
						// not tested!
						load_r15_ecx();
						s << "\x81\xC1"; write( s, (unsigned long)(inst+1) << INST_BITS); // add ecx, imm
						s << '\x51'; // push ecx
					}

					for (int i = 14; i >= 0; i--)
					{
						if (ctx.imm & (1 << i))
						{
							s << "\xFF\x75" << (char)OFFSET(regs[i]); // push dword ptr[ebp+Ri]
						}
					}
					s << '\x54';              // push esp
				}
				s << '\x6A' << (char)num; // push num
				// determine start address
				push_multiple(num);
				CALLP(store32_array) 
				s << "\x83\xC4" << (char)((pop+3) << 2);       // add esp, num*4
			}

			// w-bit is set, update destination register
			// but only when it was not in the loaded list!
			if (ctx.instruction == INST::STM_W)
			{
			//if (!(ctx.imm & (1 << ctx.rn)))
				update_dest(num);
			}
		}
		break;
	case INST::LDM:
	case INST::LDM_W:
		{
			unsigned long num, highest, lowest;
			bool region;
			bool special = false; // rn needs backup
			count( ctx.imm, num, lowest, highest, region );

			if (ctx.instruction == INST::LDM_W)
			{
				if ((1 << ctx.rn) & ctx.imm)
				{
					// rn inside {regs}
					if (!(((1 << ctx.rn) - 1) & ctx.imm))
					{
						// special case keep original
						s << "\xFF\x75" << (char)OFFSET(regs[ctx.rn]); // push [ebp+rn]
						special = true;
					} else
						ctx.instruction = INST::LDM; // keep loaded
				}
			}

			if (ctx.flags & disassembler::S_BIT)
			{
				/* use the "simple" version for simple register bank switchs */

				s << "\x83\xEC" << (char)(num << 2); // sub esp, num*4
				s << '\x54';                         // push esp
				s << '\x6A' << (char)num;            // push num
				push_multiple(num);
				CALLP(load32_array) 
				s << "\x83\xC4\x0C";                 // add esp, 3*4
				ldm_switchuser();
				unsigned long mask = ctx.imm;
				for (int i = 0; i < 16; i++)
				{
					if (mask & 1)
					{
						s << "\x8F\x40" << (char)OFFSET(regs[i]); // pop dword ptr[eax+Ri]
					}
					mask >>= 1;
				}
				ldm_switchback();

			} else
			switch (num)
			{
			case 0: // spec says this is undefined
				s << DEBUG_BREAK;
				break;
			case 1: // simple 32bit store
				load_ecx_single(); // load ecx, start_address
				CALLP(load32)      // dont use array load but simple load here!
				s << "\x89\x45" << (char)OFFSET(regs[highest]); // mov [ebp+R_highest], eax
				break;
			default:
				if (region) // optimized version when a region Ra-Rb is stored
				{
					char ofs = (char)OFFSET(regs[lowest]);
					if (ofs == 0)
						s << '\x55';            // push ebp
					else
					{
						s << "\x8D\x45" << ofs; // lea eax, [ebp+Rlow]
						s << '\x50';            // push eax
					}
					s << '\x6A' << (char)num;   // push num
					push_multiple(num);
					CALLP(load32_array) // (addr, num, data)
					s << "\x83\xC4\x0C";        // add esp, 3*4
				} else
				{
					//s << '\x90' << DEBUG_BREAK; // not supported yet
					s << "\x83\xEC" << (char)(num << 2); // sub esp, num*4
					s << '\x54';                         // push esp
					s << '\x6A' << (char)num;            // push num
					push_multiple(num);
					CALLP(load32_array) 
					s << "\x83\xC4\x0C";                 // add esp, 3*4

					unsigned long mask = ctx.imm;
					for (int i = 0; i < 16; i++)
					{
						if (mask & 1)
						{
							s << "\x8F\x45" << (char)OFFSET(regs[i]); // pop dword ptr[ebp+Ri]
						}
						mask >>= 1;
					}
				}
				break;
			}
			//ldm_switchback();

			// w-bit is set, update destination register
			if (ctx.instruction == INST::LDM_W)
			{
				// seems to depend on prefetches
				// when rd is the first reg specified then dont update

				// according to armwrestler
		
				if (special)
					s << "\x8F\x45" << (char)OFFSET(regs[ctx.rn]); // pop dword ptr[ebp+rn]
				update_dest(num);
			}


			// if PC was specified handle the jump!
			if (ctx.imm & (1 << 15))
			{
				s << "\x8B\x4D" << (char)OFFSET(regs[15]); // mov ecx, [ebp+R15]
				update_callstack();
				JMPP(compile_and_link_branch_a)
			}
		}
		break;

	case INST::SWI:
		s << "\xB9"; write(s, ctx.imm); // mov ecx, imm
		CALLP(swi);
		break;

	case INST::UD:
	default:
	default_:
		s << DEBUG_BREAK; // no idea how to handle this!
	}

	if (patch_jump)
	{
		std::ostringstream::pos_type cur = s.tellp();
		size_t off = cur - jmpbyte - 1;
		if(off >= 128)
		{
			//char buffer[100];
			//sprintf_s(buffer, 100, "Generated code too large: %i", off );
			//MessageBoxA(0,buffer,0,0);
			std::cerr << "Generated code too large: " << off << "\n";			
			assert(0);
		}
		s.seekp( jmpbyte );
		s << (char)off;
		s.seekp( cur );
		patch_jump = false;
	}
}

void compiler::epilogue(char *&mem, size_t &size)
{
	//s << DEBUG_BREAK << '\xC3'; // terminate with int 3 and return

	// branch to next page
	load_r15_ecx();
	s << "\x81\xC1"; write( s, (unsigned long)PAGING::SIZE);  // add ecx, imm
	s << "\x89\x4D" << (char)OFFSET(regs[15]); // mov [ebp+r15], ecx
	JMPP(compile_and_link_branch_a)
	//s << "\xFF\xE0";                           // jmp eax
	
	// some instruction reaches end of block
	// this has to be replaced with a jump to the next block

	size = s.tellp();
	mem = new char[size];
	mprotect( mem, size, PROT_READ | PROT_WRITE | PROT_EXEC );

#pragma warning(push)
#pragma warning(disable: 4996)
	s.str().copy( mem, size );
#pragma warning(push)


	// relocate all relative addresses
	for (std::list<unsigned long>::iterator it = reloc_table.begin(); 
		it != reloc_table.end(); ++it)
	{
		char** c = (char**)&mem[*it];
		*c -= (size_t)c;
	}
}
#undef OFFSET

std::ostringstream::pos_type compiler::tellp()
{
	return s.tellp();
}
