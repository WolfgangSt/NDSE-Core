#ifndef _DISASSEMBLER_
#define _DISASSEMBLER_

#include <stdlib.h> // for _rotl
#include "Mem.h"
#include "osdep.h" // for rotr on *nix

struct INST
{
	typedef enum {
		UD,
		DEBUG,   // triggers debugger
		UNKNOWN, // used when SB0 SB1 doesnt match (for debugging)
		STR_I,
		LDR_I,
		STR_IW,
		LDR_IW,
		STRB_I,
		LDRB_I,
		STRB_IW,
		LDRB_IW,
		STR_IP,
		LDR_IP,
		STR_IPW,
		LDR_IPW,
		STRB_IP,
		LDRB_IP,
		STRB_IPW,
		LDRB_IPW,
		
		STR_R,
		LDR_R,
		STR_RW,
		LDR_RW,
		STRB_R,
		LDRB_R,
		STRB_RW,
		LDRB_RW,
		STR_RP,
		LDR_RP,
		STR_RPW,
		LDR_RPW,
		STRB_RP,
		LDRB_RP,
		STRB_RPW,
		LDRB_RPW,

		AND_I,
		EOR_I,
		SUB_I,
		RSB_I,
		ADD_I,
		ADC_I,
		SBC_I,
		RSC_I,
		TST_I,
		TEQ_I,
		CMP_I,
		CMN_I,
		ORR_I,
		MOV_I,
		BIC_I,
		MVN_I,

		MSR_CPSR_I,
		MSR_SPSR_I,

		AND_R,
		EOR_R,
		SUB_R,
		RSB_R,
		ADD_R,
		ADC_R,
		SBC_R,
		RSC_R,
		TST_R,
		TEQ_R,
		CMP_R,
		CMN_R,
		ORR_R,
		MOV_R,
		BIC_R,
		MVN_R,

		AND_RR,
		EOR_RR,
		SUB_RR,
		RSB_RR,
		ADD_RR,
		ADC_RR,
		SBC_RR,
		RSC_RR,
		TST_RR,
		TEQ_RR,
		CMP_RR,
		CMN_RR,
		ORR_RR,
		MOV_RR,
		BIC_RR,
		MVN_RR,

		MSR_CPSR_R,
		MSR_SPSR_R,
		MRS_CPSR,
		MRS_SPSR,

		CLZ,
		BKPT,
		SWI,

		BX,
		BLX,
		B,
		BL,

		MRC,
		MCR,

		STM,
		LDM,
		STM_W,
		LDM_W,


		STRX_I,
		LDRX_I,
		STRX_IW,
		LDRX_IW,
		STRX_IP,
		LDRX_IP,
		STRX_IPW,
		LDRX_IPW,
		STRX_R,
		LDRX_R,
		STRX_RW,
		LDRX_RW,
		STRX_RP,
		LDRX_RP,
		STRX_RPW,
		LDRX_RPW,

		BPRE, // branch prefix (for thumb bl[x])

		MUL_R,
		MLA_R,

		UMULL,
		UMLAL,
		SMULL,
		SMLAL,

		SWP,
		SWPB,

		/* missing instructions*/

		
		BLX_I
		

	} CODE;
	static const char* strings[];
};

struct SR_MASK
{
	typedef enum {
		____, ___C, __X_, __XC, _S__, _S_C, _SX_, _SXC,
		F___, F__C, F_X_, F_XC, FS__, FS_C, FSX_, FSXC
	} CODE;
	static const char* strings[16];
};


struct CONDITION
{
	typedef enum { 
		EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV
	} CODE;
	static const char* strings[16];
};


struct SHIFT
{
	typedef enum {
		LSL, LSR, ASR, ROR, RXX
	} CODE;
	static const char* strings[5];
};

struct EXTEND_MODE
{
	typedef enum {
		INVALID, H, SB, SH
	} CODE;
	static const char* strings[4];
};

struct ADDRESSING_MODE
{
	typedef enum {
		DA,IA,DB,IB
	} CODE;
	static const char* strings[4];
};

struct STRINGS
{
	static const char S_BIT;
	static const char R_PREFIX;
};

class disassembler
{
public:
	enum { 
		S_BIT = 1, // update flags
		U_BIT = 2, // signed
		S_CONSUMING = 8 // the instruction consumes an S bit (used for optimizer)
	};
	struct context
	{
		INST::CODE instruction;
		CONDITION::CODE cond;
		SHIFT::CODE shift;
		ADDRESSING_MODE::CODE addressing_mode;
		EXTEND_MODE::CODE extend_mode;
		unsigned int flags;
		unsigned long imm;
		unsigned int rd;
		unsigned int rn;
		unsigned int rm;
		unsigned int rs;
		unsigned int cp_num;
		unsigned int cp_op1;
		unsigned int cp_op2;

		unsigned long op;
		unsigned long addr;
		const char *fmt;
		char *buffer;
	};

	const context get_context() { return ctx; }

private:
	context ctx;
	


	void cat(const char *c)
	{
		while (*c)
			*ctx.buffer++ = *c++;
	}

	void cat(char c)
	{
		*ctx.buffer++ = c;
	}

	void cat_nibble_unchecked(unsigned long nibble)
	{
		static const char nibbles[] = {
			'0', '1', '2', '3', '4', '5', '6', '7', 
			'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
		};
		*ctx.buffer++ = nibbles[nibble];
	}

	void cat_nibble(unsigned long nibble)
	{
		cat_nibble_unchecked( nibble & 0xF );
	}

	void cat_hex8(unsigned long val)
	{
		cat_nibble( val >> 28 );
		cat_nibble( val >> 24 );
		cat_nibble( val >> 20 );
		cat_nibble( val >> 16 );
		cat_nibble( val >> 12 );
		cat_nibble( val >> 8 );
		cat_nibble( val >> 4 );
		cat_nibble( val );
	}

	void cat_i16(unsigned int i)
	{
		static const char* ints[16] = {
			"0", "1", "2", "3", "4", "5", "6", "7",
			"8", "9", "10", "11", "12", "13", "14", "15"
		};
		cat(ints[i]);
	}

	void cat_hex(unsigned long val)
	{
		unsigned long nval = val >> 4;
		if (nval)
			cat_hex( nval );
		cat_nibble(val);
	}

	void decode_condition()
	{
		ctx.cond = (CONDITION::CODE)(ctx.op >> 28);
	}

	void decode_shift()
	{
		ctx.shift = (SHIFT::CODE)((ctx.op >> 5) & 0x3);
		ctx.imm = (ctx.op >> 7) & 0x1F;
		if((ctx.imm == 0) && (ctx.shift != SHIFT::LSL))
		{
			if (ctx.shift == SHIFT::ROR)
				ctx.shift = SHIFT::RXX;
			else ctx.imm = 32;
		}
	}

	void decode_shift_reg()
	{
		ctx.shift = (SHIFT::CODE)((ctx.op >> 5) & 0x3);
		ctx.rs = decode_reg<8>();
	}

	void inst_ud()
	{
		ctx.instruction = INST::UD;
	}

	void inst_debug()
	{
		ctx.instruction = INST::DEBUG;
	}

	void inst_unknown()
	{
		ctx.instruction = INST::UNKNOWN;
	}

	void decode_simm24()
	{
		signed int addr = ctx.op & 0x00FFFFFF;
		ctx.imm = ((addr << 8) >> 6) + ctx.addr + 8;
	}

	void decode_imm_shifter()
	{
		unsigned long imm8 = ctx.op & 0xFF;
		unsigned long rot  = (ctx.op >> 7) & 0x1E;
		ctx.imm = _rotr( imm8, rot );
	}

	template <int n> unsigned int decode_reg() const
	{
		return (ctx.op >> n) & 0xF;
	}
	template <int n> unsigned int decode_regt() const
	{
		return (ctx.op >> n) & 0x7;
	}

public:

	void get_str(char *buffer) 
	{ 
		ctx.fmt = INST::strings[ctx.instruction];
		ctx.buffer = buffer;
		char *&d = ctx.buffer;

		const char *p = ctx.fmt;
		while (*p)
		{
			char c = *p++;
			if (c == '%')
			{
				char c = *p++;
				switch (c)
				{
				case '^':
					if (ctx.flags & S_BIT)
						cat('^');
					continue;
				case '-':
					if (!(ctx.flags & U_BIT))
						cat('-');
					continue;
				case '?':
					cat("???");
					//DebugBreak();
					continue;
				case 'c': // condition
					cat( CONDITION::strings[ctx.cond]);
					continue;
				case 's': // S-bit
					if (ctx.flags & S_BIT)
						cat(STRINGS::S_BIT);
					continue;
				case 'F': // Status register mask (in .rn)
					cat( SR_MASK::strings[(SR_MASK::CODE)ctx.rn] );
					continue;
				case 'S': // shift mode
					cat(SHIFT::strings[ctx.shift]);
					continue;
				case 'R': // Register
					{
						*d++ = STRINGS::R_PREFIX;
DecodeReg:
						char c = *p++;
						switch (c)
						{
						case 'd': cat_i16(ctx.rd); break;
						case 'n': cat_i16(ctx.rn); break;
						case 'm': cat_i16(ctx.rm); break;
						case 's': cat_i16(ctx.rs); break;
						case 'L':
							{
								d--;
								*d = 0;
								if (ctx.imm)
								{
									unsigned int tmp = ctx.imm;
									for (int i = 0; i < 16; i++)
									{
										if (tmp & 1)
										{
											*d++ = STRINGS::R_PREFIX;
											cat_i16(i);
											*d++ = ',';
										}
										tmp >>= 1;
									}
									d--;
									*d = 0;
								}
								break;
							}
						}
						continue;
					}
				case 'M': // addressing mode
					cat(ADDRESSING_MODE::strings[ctx.addressing_mode]);
					continue;
				case 'E': // addressing mode
					cat(EXTEND_MODE::strings[ctx.extend_mode]);
					continue;

				case 'I':
					cat_hex( ctx.imm );
					continue;
				case 'A':
					// cat address
					cat_hex8( ctx.imm );
					continue;
				case 'C':
					// coproc entity

					char c = *p++;
					switch (c)
					{
					case 'p': *d++ = 'p'; cat_i16( ctx.cp_num ); break;
					case '1': cat_i16(ctx.cp_op1); break;
					case '2': cat_i16(ctx.cp_op2); break;
					case 'R':
						*d++ = 'c';
						goto DecodeReg;
					}
					continue;

					
					continue;
				}
				*d++ = c;
				continue;
			}
			*d++ = c;
		}
		*d = 0;
	}

	// new decoder subs
private:
	void decode_instruction_NV()
	{
		inst_ud();
	}

	// fig. 3.3 specializations
	// could gain some perf here since some bits are already decoded
	// at this point, but this makes the code far more complex ...
	void decode_misc()
	{
		// this is already known:
		//ctx.rn = decode_reg<16>();
		//ctx.rd = decode_reg<12>();
		//ctx.rs = decode_reg<8>();
		//ctx.rm = decode_reg<0>();

		unsigned int sub_op = (ctx.op >> 21) & 0x3;

		if (ctx.op & (1 << 7))
			return inst_ud(); // Enhanced DSP extension
		switch ((ctx.op >> 4) & 0x7)
		{
		case 0x0: 
			if (ctx.rs != 0x0) return inst_unknown(); // SBZ check
			switch (sub_op)
			{
			case 0x0: 
				if (ctx.rn != 0xF) return inst_unknown(); // SBO check
				ctx.instruction = INST::MRS_CPSR;
				return;
			case 0x1: 
				if (ctx.rd != 0xF) return inst_unknown(); // SBO check
				ctx.instruction = INST::MSR_CPSR_R;
				return;
			case 0x2:
				if (ctx.rn != 0xF) return inst_unknown(); // SBO check
				ctx.instruction = INST::MRS_SPSR;
				return;
			case 0x3: 
				if (ctx.rd != 0xF) return inst_unknown(); // SBO check
				ctx.instruction = INST::MSR_SPSR_R;
				return;
			}
		
		case 0x1: 
			if ((ctx.rn != 0xF) || (ctx.rs != 0xF)) return inst_unknown(); // SBO check
			switch (sub_op)
			{
			case 0x1:
				if (ctx.rd != 0xF) return inst_unknown(); // SBO check
				ctx.instruction = INST::BX;
				return;
			case 0x3:
				ctx.instruction = INST::CLZ;
				return;
			}
			return inst_ud(); // undefined in fig 3.3

		case 0x2: return inst_ud(); // undefined in fig 3.3...
		
		case 0x3:
			if (sub_op != 0x1)
				return inst_ud(); // undefined in fig 3.3...
			if ((ctx.rn != 0xF) || (ctx.rd != 0xF) || (ctx.rs != 0xF))
				return inst_unknown(); // SBO check
			ctx.instruction = INST::BLX;
			return;
		case 0x4: return inst_ud(); // undefined in fig 3.3...
		case 0x5: return inst_ud(); // enhanced DSP
		case 0x6: return inst_ud(); // undefined in fig 3.3...
		case 0x7: 
			// software bp 
			if (sub_op != 0x1)
				return inst_ud();
			if (ctx.cond != CONDITION::AL)
				return inst_unknown();
			ctx.imm = (ctx.op & 0xF) | ((ctx.op & 0xFFF00) >> 4);
			ctx.instruction = INST::BKPT;
			return inst_debug();
		}
	}

	// loads nibble at bit 8 to rs
	// as decode_000_regs does per default and then invokes
	// the misc decoder
	void decode_misc2()
	{
		ctx.rs = decode_reg<8>();
		decode_misc();
	}

	void decode_00000_mula()
	{
		if (ctx.op & (1 << 22)) // bit 22 MBZ
			return inst_ud();
		// ctx.rm already read
		ctx.rs = decode_reg<8>();
		ctx.rn = decode_reg<12>();
		ctx.rd = decode_reg<16>();
		
		if (ctx.op & (1 << 20))
			ctx.flags |= S_BIT;


		switch ((ctx.op >> 20) & 0x3)
		{
		case 0x0: // page 166
			if (ctx.rn != 0) // SBZ
				return inst_unknown();
			ctx.instruction = INST::MUL_R;
			return;
		case 0x1:
			ctx.flags |= S_BIT;
			if (ctx.rn != 0) // SBZ
				return inst_unknown();
			ctx.instruction = INST::MUL_R;
			return;
		case 0x2: // page 154
			ctx.instruction = INST::MLA_R; 
			return;
		case 0x3:
			ctx.flags |= S_BIT;
			ctx.instruction = INST::MLA_R;
			return;
		}

		return inst_ud();
	}

	// cond_00001_xxxxxxxxxxxxxxx_1001_xxxx
	void decode_00001_mulal()
	{
		ctx.rs = decode_reg<8>();
		if (ctx.op & (1 << 20))
			ctx.flags |= S_BIT;
		switch ((ctx.op >> 21) & 0x3)
		{
		case 0: ctx.instruction = INST::UMULL; break;
		case 1: ctx.instruction = INST::UMLAL; break;
		case 2: ctx.instruction = INST::SMULL; break;
		case 3: ctx.instruction = INST::SMLAL; break;
		}
	}

	// cond_00010_xxxxxxxxxxxxxxx_1001_xxxx
	void decode_00010_swap()
	{
		switch ((ctx.op >> 20) & 0x7)
		{
		case 0: ctx.instruction = INST::SWP; break; // SWP
		case 4: ctx.instruction = INST::SWPB; break; // SWPB
		default:
			return inst_ud();
		}
	}

	// cond_000_xxxxxxxxxxxxxxxxx_1001_xxxx
	void decode_000_mulswap()
	{
		switch ((ctx.op >> 23) & 0x3)
		{
		case 0x0: return decode_00000_mula();
		case 0x1: return decode_00001_mulal();
		case 0x2: return decode_00010_swap();
		}
		return inst_ud();
	}

	void decode_000_mul_extraloadstore()
	{
		// cond_000_xxxxxxxxxxxxxxxxx_1_xx_1_xxxx
		// figure 3.2
		ctx.extend_mode = (EXTEND_MODE::CODE)((ctx.op >> 5) & 0x3);
		if (ctx.extend_mode == EXTEND_MODE::INVALID)
			return decode_000_mulswap();

		// Rm already decoded for register versions
		// decode imm, for imm versions
		int imm = ((ctx.op & 0xF00) >> 4) | (ctx.rm);
		if (ctx.op & (1 << 23))
		{
			ctx.imm = (unsigned long)imm;
			ctx.flags |= U_BIT;
		}
		else ctx.imm = (unsigned long)(-imm);


		switch ((ctx.op >> 20) & 0x17)
		{
		case 0x00: ctx.instruction = INST::STRX_R;  ctx.imm = 0; ctx.shift = SHIFT::LSL; return; 
		case 0x01: ctx.instruction = INST::LDRX_R;  ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x02: ctx.instruction = INST::STRX_RW; ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x03: ctx.instruction = INST::LDRX_RW; ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x04: ctx.instruction = INST::STRX_I; return;
		case 0x05: ctx.instruction = INST::LDRX_I; return;
		case 0x06: ctx.instruction = INST::STRX_IW; return;
		case 0x07: ctx.instruction = INST::LDRX_IW; return;
		case 0x10: ctx.instruction = INST::STRX_RP; ctx.imm = 0; ctx.shift = SHIFT::LSL; return; 
		case 0x11: ctx.instruction = INST::LDRX_RP; ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x12: ctx.instruction = INST::STRX_RPW; ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x13: ctx.instruction = INST::LDRX_RPW; ctx.imm = 0; ctx.shift = SHIFT::LSL; return;
		case 0x14: ctx.instruction = INST::STRX_IP; return;
		case 0x15: ctx.instruction = INST::LDRX_IP; return;
		case 0x16: ctx.instruction = INST::STRX_IPW; return;
		case 0x17: ctx.instruction = INST::LDRX_IPW; return;
		}
	}


	void decode_000_regs()
	{
		if (ctx.op & (1 << 7))
			return decode_000_mul_extraloadstore(); // multiplies extra store load

		bool sbit = false;
		if (ctx.op & (1 << 20))
		{
			sbit = true;
			ctx.flags |= S_BIT;
		}

		decode_shift_reg();
		switch ((ctx.op >> 21) & 0xF)
		{
		case 0x0: ctx.instruction = INST::AND_RR; return;
		case 0x1: ctx.instruction = INST::EOR_RR; return;
		case 0x2: ctx.instruction = INST::SUB_RR; return;
		case 0x3: ctx.instruction = INST::RSB_RR; return;
		case 0x4: ctx.instruction = INST::ADD_RR; return;
		case 0x5: ctx.instruction = INST::ADC_RR; ctx.flags |= S_CONSUMING; return;
		case 0x6: ctx.instruction = INST::SBC_RR; ctx.flags |= S_CONSUMING; return;
		case 0x7: ctx.instruction = INST::RSC_RR; return;

		case 0x8: // TST
			if (sbit)
				ctx.instruction = INST::TST_RR;
			else
				decode_misc();
			return;
		case 0x9: // TEQ
			if (sbit)
				ctx.instruction = INST::TEQ_RR;
			else
				decode_misc();
			return;
		case 0xA: // CMP
			if (sbit)
				ctx.instruction = INST::CMP_RR;
			else
				decode_misc();
			return;
		case 0xB: // CMN
			if (sbit)
				ctx.instruction = INST::CMN_RR;
			else
				decode_misc();
			return;
		case 0xC: ctx.instruction = INST::ORR_RR; return;
		case 0xD: ctx.instruction = INST::MOV_RR; return;
		case 0xE: ctx.instruction = INST::BIC_RR; return;
		case 0xF: ctx.instruction = INST::MVN_RR; return;
		}
	}

	void decode_000_imms()
	{
		bool sbit = false;
		if (ctx.op & (1 << 20))
		{
			sbit = true;
			ctx.flags |= S_BIT;
		}

		decode_shift();		
		switch ((ctx.op >> 21) & 0xF)
		{
		case 0x0: ctx.instruction = INST::AND_R; return;
		case 0x1: ctx.instruction = INST::EOR_R; return;
		case 0x2: ctx.instruction = INST::SUB_R; return;
		case 0x3: ctx.instruction = INST::RSB_R; return;
		case 0x4: ctx.instruction = INST::ADD_R; return;
		case 0x5: ctx.instruction = INST::ADC_R; ctx.flags |= S_CONSUMING; return;
		case 0x6: ctx.instruction = INST::SBC_R; ctx.flags |= S_CONSUMING; return;
		case 0x7: ctx.instruction = INST::RSC_R; return;

		case 0x8: // TST
			if (sbit)
				ctx.instruction = INST::TST_R;
			else
				decode_misc2();
			return;
		case 0x9: // TEQ
			if (sbit)
				ctx.instruction = INST::TEQ_R;
			else
				decode_misc2();
			return;
		case 0xA: // CMP
			if (sbit)
				ctx.instruction = INST::CMP_R;
			else
				decode_misc2();
			return;
		case 0xB: // CMN
			if (sbit)
				ctx.instruction = INST::CMN_R;
			else
				decode_misc2();
			return;
		case 0xC: ctx.instruction = INST::ORR_R; return;
		case 0xD: ctx.instruction = INST::MOV_R; return;
		case 0xE: ctx.instruction = INST::BIC_R; return;
		case 0xF: ctx.instruction = INST::MVN_R; return;
		}
	}

	void decode_000_data_reg()
	{
		ctx.rn = decode_reg<16>();
		ctx.rd = decode_reg<12>();
		ctx.rm = decode_reg<0>();

		bool regshift = false;
		if (ctx.op & (1 << 4))
			regshift = true;

		if (regshift)
			return decode_000_regs();
		decode_000_imms();		
	}

	void decode_001_data_imm()
	{
		                           // meaning for mov -> status reg
		ctx.rn = decode_reg<16>(); // MASK
		ctx.rd = decode_reg<12>(); // SB0
		decode_imm_shifter();
		
		// decode s-bit
		bool sbit = false;
		if (ctx.op & (1 << 20))
		{
			sbit = true;
			ctx.flags |= S_BIT;
		}

		switch ((ctx.op >> 21) & 0xF)
		{
		case 0x0: ctx.instruction = INST::AND_I; return;
		case 0x1: ctx.instruction = INST::EOR_I; return;
		case 0x2: ctx.instruction = INST::SUB_I; return;
		case 0x3: ctx.instruction = INST::RSB_I; return;
		case 0x4: ctx.instruction = INST::ADD_I; return;
		case 0x5: ctx.instruction = INST::ADC_I; ctx.flags |= S_CONSUMING; return;
		case 0x6: ctx.instruction = INST::SBC_I; ctx.flags |= S_CONSUMING; return;
		case 0x7: ctx.instruction = INST::RSC_I; return;
		case 0x8: // TST
			if (sbit)
				ctx.instruction = INST::TST_I;
			else
				inst_ud(); // undefined [3]
			return;
		case 0x9: // TEQ
			if (sbit)
				ctx.instruction = INST::TEQ_I;
			else
			{
				if (ctx.rd != 0xF) // check SB0 for ones
					return inst_unknown();
				ctx.instruction = INST::MSR_CPSR_I;
			}
			return;
		case 0xA: // CMP
			if (sbit)
				ctx.instruction = INST::CMP_I;
			else
				inst_ud(); // undefined [3]
			return;
		case 0xB: // CMN
			if (sbit)
				ctx.instruction = INST::CMN_I;
			else
			{
				if (ctx.rd) // check SB0 for zero
					return inst_unknown();
				ctx.instruction = INST::MSR_SPSR_I;
			}
			return;
		case 0xC: ctx.instruction = INST::ORR_I; return;
		case 0xD: ctx.instruction = INST::MOV_I; return;
		case 0xE: ctx.instruction = INST::BIC_I; return;
		case 0xF: ctx.instruction = INST::MVN_I; return;
		}
		inst_ud();
	}



	void decode_010_loadstore_imm()
	{
		// page 235

		// Load/store immediate offset
		ctx.rn = decode_reg<16>();
		ctx.rd = decode_reg<12>();
		if (ctx.op & (1 << 23)) // u bit
		{
			ctx.imm =  ctx.op & 0xFFF;
			ctx.flags |= U_BIT; // not necessary since imm contains -imm already
		}
		else 
			ctx.imm = (unsigned)-(signed)(ctx.op & 0xFFF);

		// decode PBWL
		switch ((ctx.op >> 20) & 0x17)
		{
		case 0x00: ctx.instruction = INST::STR_I; return; 
		case 0x01: ctx.instruction = INST::LDR_I; return;
		case 0x02: ctx.instruction = INST::STR_IW; return;
		case 0x03: ctx.instruction = INST::LDR_IW; return;
		case 0x04: ctx.instruction = INST::STRB_I; return;
		case 0x05: ctx.instruction = INST::LDRB_I; return;
		case 0x06: ctx.instruction = INST::STRB_IW; return;
		case 0x07: ctx.instruction = INST::LDRB_IW; return;
		case 0x10: ctx.instruction = INST::STR_IP; return;
		case 0x11: ctx.instruction = INST::LDR_IP; return;
		case 0x12: ctx.instruction = INST::STR_IPW; return;
		case 0x13: ctx.instruction = INST::LDR_IPW; return;
		case 0x14: ctx.instruction = INST::STRB_IP; return;
		case 0x15: ctx.instruction = INST::LDRB_IP; return;
		case 0x16: ctx.instruction = INST::STRB_IPW; return;
		case 0x17: ctx.instruction = INST::LDRB_IPW; return;
		}
		inst_ud();
	}

	void decode_011_loadstore_reg()
	{
		if (ctx.op & (1 << 4))
			return inst_ud();

		if (ctx.op & (1 << 23)) // u bit
			ctx.flags |= U_BIT;

		ctx.rn = decode_reg<16>();
		ctx.rd = decode_reg<12>();
		ctx.rm = decode_reg<0>();

		decode_shift();
		// AL   grp PUBWL Rn   Rd   Shift SC 0 Rm
		// 0001 011 11000 0001 0000 00010 00 0 0010

		switch ((ctx.op >> 20) & 0x17)
		{
		case 0x00: ctx.instruction = INST::STR_R; return; 
		case 0x01: ctx.instruction = INST::LDR_R; return;
		case 0x02: ctx.instruction = INST::STR_RW; return;
		case 0x03: ctx.instruction = INST::LDR_RW; return;
		case 0x04: ctx.instruction = INST::STRB_R; return;
		case 0x05: ctx.instruction = INST::LDRB_R; return;
		case 0x06: ctx.instruction = INST::STRB_RW; return;
		case 0x07: ctx.instruction = INST::LDRB_RW; return;
		case 0x10: ctx.instruction = INST::STR_RP; return;
		case 0x11: ctx.instruction = INST::LDR_RP; return;
		case 0x12: ctx.instruction = INST::STR_RPW; return;
		case 0x13: ctx.instruction = INST::LDR_RPW; return;
		case 0x14: ctx.instruction = INST::STRB_RP; return;
		case 0x15: ctx.instruction = INST::LDRB_RP; return;
		case 0x16: ctx.instruction = INST::STRB_RPW; return;
		case 0x17: ctx.instruction = INST::LDRB_RPW; return;
		}

		inst_ud();
	}


	void decode_011()
	{
		inst_ud();
	}

	// load store multiple
	void decode_100_loadstoremultiple()
	{
		ctx.rn  = decode_reg<16>();
		ctx.imm = ctx.op & 0xFFFF;  // imm holds register mask

		// todo: should add new flag instead of using S_BIT here
		// since this probably needs quite some special handling
		if  (ctx.op & (1 << 22))
			ctx.flags |= S_BIT;

		// PU bits
		ctx.addressing_mode = (ADDRESSING_MODE::CODE)((ctx.op >> 23) & 0x3);

		// WL bits
		switch ((ctx.op >> 20 & 0x3))
		{
		case 0: ctx.instruction = INST::STM; break;
		case 1: ctx.instruction = INST::LDM; break;
		case 2: ctx.instruction = INST::STM_W; break;
		case 3: ctx.instruction = INST::LDM_W; break;
		}
	}

	void decode_101_branch()
	{
		decode_simm24(); // decode destination
		if (ctx.op & (1 << 24))
			ctx.instruction = INST::BL;
		else
			ctx.instruction = INST::B;
	}

	void decode_110()
	{
		inst_ud();
	}

	void decode_swi()
	{
		// decode_simm24 ??
		ctx.imm = ctx.op & 0xFFFFFF;
		ctx.instruction = INST::SWI;
	}

	void decode_coproc()
	{
		ctx.rn = decode_reg<16>();
		ctx.rd = decode_reg<12>();
		ctx.cp_num = decode_reg<8>();
		ctx.rm = decode_reg<0>();
		ctx.cp_op2 = (ctx.op >> 5) & 0x7;

		if (ctx.op & (1 << 4))
		{
			// coproc register transfer
			ctx.cp_op1 = (ctx.op >> 21) & 0x7;
			if (ctx.op & (1 << 20)) // load or store?
				ctx.instruction = INST::MRC; // load (R -> C)
			else ctx.instruction = INST::MCR;
		} else
		{
			// coprocessor data processing
			ctx.cp_op1 = (ctx.op >> 20) & 0xF;
			ctx.instruction = INST::UD;
		}
	}

	void decode_111()
	{
		if (ctx.op & (1 << 24))
			decode_swi();
		else
			decode_coproc();
	}

public:

	// new decoder
	void decode_instruction()
	{
		if (ctx.cond == CONDITION::NV)
			return decode_instruction_NV();

		switch ((ctx.op >> 25) & 0x7)
		{
		case 0x0: return decode_000_data_reg();
		case 0x1: return decode_001_data_imm();
		case 0x2: return decode_010_loadstore_imm();
		case 0x3: return decode_011_loadstore_reg();
		case 0x4: return decode_100_loadstoremultiple();
		case 0x5: return decode_101_branch();
		case 0x6: return decode_110();
		case 0x7: return decode_111();
		}
		inst_ud();
	}

	template <typename T>
	void decode(unsigned int op, unsigned int addr);


////////////////////////////////////////////////////////////////////////////////
// THUMB DECODER

	void decode_t000()
	{
		ctx.flags |= S_BIT;
		ctx.rd = decode_regt<0>();
		ctx.rn = decode_regt<3>();
		ctx.imm = 0;
		ctx.shift = SHIFT::LSL;
		switch ((ctx.op >> 11) & 0x3)
		{
		// lsl lsr asr
		case 0x0:  // lsl page 363
			ctx.rm = ctx.rn;
			ctx.imm = (ctx.op >> 6) & 0x1F;
			ctx.instruction = INST::MOV_R; // OK
			return;
		case 0x1: // lsr page 366
			ctx.rm = ctx.rn;
			ctx.imm = (ctx.op >> 6) & 0x1F;
			if (ctx.imm == 0)
				ctx.imm = 32;
			ctx.shift = SHIFT::LSR;
			ctx.instruction = INST::MOV_R;
			return;
		case 0x2: // asr page 318
			ctx.rm = ctx.rn;
			ctx.imm = (ctx.op >> 6) & 0x1F;
			if (ctx.imm == 0)
				ctx.imm = 32;
			ctx.shift = SHIFT::ASR;
			ctx.instruction = INST::MOV_R;
			return; 
		case 0x3:
			ctx.rm  = decode_regt<6>();
			switch ((ctx.op >> 9) & 0x3)
			{
			case 0x0: // page 311
				ctx.instruction = INST::ADD_R; 
				return; 
			case 0x1: // page 404
				ctx.instruction = INST::SUB_R; 
				return; 
			case 0x2: // page 309
				ctx.imm = ctx.rm;
				ctx.instruction = INST::ADD_I; 
				return; 
			case 0x3: // page 402
				ctx.imm = ctx.rm;
				ctx.instruction = INST::SUB_I; 
				return; 
			}
		}
		return inst_ud();
	}
	void decode_t001()
	{
		unsigned int reg = decode_regt<8>();
		ctx.imm = ctx.op & 0xFF;
		ctx.rd = reg;
		ctx.rn = reg;
		ctx.flags |= S_BIT;

		// add sub mov imm
		switch ((ctx.op >> 11) & 0x3)
		{
		case 0x0: ctx.instruction = INST::MOV_I; return; // page 370
		case 0x1: ctx.instruction = INST::CMP_I; return; // page 339
		case 0x2: ctx.instruction = INST::ADD_I; return; // page 310
		case 0x3: ctx.instruction = INST::SUB_I; return; // page 403
		}
		return inst_ud();
	}


	void decode_t0101_loadstore_reg()
	{
		ctx.rd = decode_regt<0>();
		ctx.rn = decode_regt<3>();
		ctx.rm = decode_regt<6>();
		ctx.shift = SHIFT::LSL;
		ctx.imm = 0;

		ctx.flags |= U_BIT;
		switch ((ctx.op >> 9) & 0x7)
		{
		case 0x0: // STR   page 392
			ctx.instruction = INST::STR_RP;
			return;
		case 0x1: // STRH  page 400
			ctx.instruction = INST::STRX_RP;
			ctx.extend_mode = EXTEND_MODE::H;
			return;
		case 0x2: // STRB  page 397
			ctx.instruction = INST::STRB_RP;
			return;
		case 0x3: // LDRSB page 360
			break;
		case 0x4: // LDR   page 348
			ctx.instruction = INST::LDR_RP;
			return;
		case 0x5: // LDRH  page 358 
			ctx.instruction = INST::LDRX_RP;
			ctx.extend_mode = EXTEND_MODE::H;
			return;
		case 0x6: // LDRB  page 355
			ctx.instruction = INST::LDRB_RP;
			return;
		case 0x7: // LDRSH page 361
			ctx.instruction = INST::LDRX_RP;
			ctx.extend_mode = EXTEND_MODE::SH; // 	"LDR%c%E %Rd,[%Rn,%Rm]",
			return;
		}

		return inst_ud();
	}

	void decode_t01001_loadliteral()
	{
		// page 350
		ctx.rd = decode_regt<8>();
		ctx.imm = (ctx.op & 0xFF) << 2; // unsigned?!
		ctx.instruction = INST::LDR_IP; // "LDR%c %Rd,[%Rn,#0x%I]",   // OK
		ctx.flags |= U_BIT;
		ctx.rn = 15;
	}

	void decode_t010001_sdata_b()
	{
		ctx.rm = decode_reg<3>(); // read H bit and thumb reg
		ctx.rd = decode_regt<0>() | ((ctx.op >> 4) & 0x8);
		ctx.imm = 0;
		ctx.shift = SHIFT::LSL;

		switch ((ctx.op >> 8) & 0x3)
		{
		case 0x0: // add page 312
			ctx.rn = ctx.rd;
			ctx.instruction = INST::ADD_R;
			return;
		case 0x1: // cmp page 341
			ctx.rn = ctx.rd;
			ctx.instruction = INST::CMP_R;
			return;
		case 0x2: // mov page 372
			ctx.instruction = INST::MOV_R;
			return;
		case 0x3: // branch 
			if (ctx.op & (1 << 7))
				ctx.instruction = INST::BLX;
			else ctx.instruction = INST::BX;
			if (decode_regt<0>() != 0) // SBZ
				return inst_unknown();
			return;
		}
		return inst_ud();
	}

	void decode_t010000_dataproc()
	{
		ctx.rd = decode_regt<0>();
		ctx.rn = decode_regt<3>(); // named rm on thumb but rn on arm mode!
		ctx.shift = SHIFT::LSL;
		ctx.imm   = 0;
		ctx.flags |= S_BIT;
		switch ((ctx.op >> 6) & (0xF))
		{
		case 0x0: // and page 317 (OK)
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::AND_R; 
			return;
		case 0x1: // eor page 343
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::EOR_R;
			return;
		case 0x2: // lsl page 364 
			ctx.rd = decode_regt<0>();
			ctx.rm = ctx.rd;
			ctx.rs = decode_regt<3>();
			ctx.instruction = INST::MOV_RR;
			return;
		case 0x3: // lsr page 368
			ctx.rd = decode_regt<0>();
			ctx.rm = ctx.rd;
			ctx.rs = decode_regt<3>();
			ctx.shift = SHIFT::LSR;
			ctx.instruction = INST::MOV_RR;
			return;
		case 0x4: // asr page 320
			ctx.rd = decode_regt<0>();
			ctx.rm = ctx.rd;
			ctx.rs = decode_regt<3>();
			ctx.shift = SHIFT::ASR;
			ctx.instruction = INST::MOV_RR;
			return;
		case 0x5: // adc page 308 
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.flags |= S_CONSUMING;
			ctx.instruction = INST::ADC_R; // "ADC%c%s %Rd,%Rn,%Rm,%S#0x%I",
			return;
		case 0x6: // sbc page 386
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::SBC_R; // "SBC%c%s %Rd,%Rn,%Rm,%S#0x%I",
			return;
		case 0x7: // ror page 384
			ctx.rd = decode_regt<0>();
			ctx.rm = ctx.rd;
			ctx.rs = decode_regt<3>();
			ctx.shift = SHIFT::ROR;
			ctx.instruction = INST::MOV_RR;
			return;
		case 0x8: // tst page 407 (OK)
			ctx.rn = decode_regt<0>();
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::TST_R;
			return; 
		case 0x9: // neg page 377 (Needs check)
			ctx.rd = decode_regt<0>();
			ctx.rn = decode_regt<3>();
			ctx.instruction = INST::RSB_I;
			return;
		case 0xA: // cmp page 340 (OK)
			ctx.rn = decode_regt<0>();
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::CMP_R;
			return;
		case 0xB: // cmn page 338
			break;
		case 0xC: // orr page 378 (OK)
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::ORR_R;
			return;
		case 0xD: // mul page 374
			ctx.rd = decode_regt<0>(); 
			ctx.rm = decode_regt<3>();
			ctx.rs = ctx.rd;
			ctx.instruction = INST::MUL_R; // "MUL%c%s %Rd,%Rm,%Rs",
			return;
		case 0xE: // bic page 326 (OK)
			ctx.rd = decode_regt<0>();
			ctx.rn = ctx.rd;
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::BIC_R;
			return; 
		case 0xF: // mvn page 376
			ctx.rd = decode_regt<0>();
			ctx.rm = decode_regt<3>();
			ctx.instruction = INST::MVN_R; // "MVN%c%s %Rd,%Rm,%S#0x%I",
			return;
		}

		inst_ud();
	}

	void decode_t010()
	{
		if (ctx.op & (1 << 12))
			return decode_t0101_loadstore_reg();
		if (ctx.op & (1 << 11))
			return decode_t01001_loadliteral();
		if (ctx.op & (1 << 10))
			return decode_t010001_sdata_b();
		return decode_t010000_dataproc();
	}
	void decode_t011()
	{
		// load store (b) imm
		ctx.rd = decode_regt<0>();
		ctx.rn = decode_regt<3>();
		ctx.flags |= U_BIT;
		switch ((ctx.op >> 11) & 0x3)
		{
		case 0x0: // str page 390 
			ctx.imm = (ctx.op >> 4) & 0x7C;
			ctx.instruction = INST::STR_IP; 
			break;
		case 0x1: // ldr page 346
			ctx.imm = (ctx.op >> 4) & 0x7C;
			ctx.instruction = INST::LDR_IP; 
			break; 
		case 0x2: // strb page 396
			ctx.imm = (ctx.op >> 6) & 0x1F;
			ctx.instruction = INST::STRB_IP; 
			break;
		case 0x3: // ldrb page 354 
			ctx.imm = (ctx.op >> 6) & 0x1F;
			ctx.instruction = INST::LDRB_IP; 
			break; 
		}
	}

	void decode_t1001_loadstore_stack()
	{
		ctx.imm = (ctx.op & 0xFF) << 2;
		ctx.rd = decode_regt<8>();
		ctx.rn = 13;
		ctx.flags |= U_BIT;
		if (ctx.op & (1 << 11))
			ctx.instruction = INST::LDR_IP;  // page 352 => LDR (10011)
		else ctx.instruction = INST::STR_IP; // page 394 => STR (10010)
	}

	void decode_t100()
	{
		if (ctx.op & (1 << 12))
			return decode_t1001_loadstore_stack();

		// load store half
		ctx.rd = decode_regt<0>();
		ctx.rn = decode_regt<3>();
		ctx.imm = (ctx.op >> 5) & 0x3E;
		ctx.extend_mode = EXTEND_MODE::H;
		ctx.flags |= U_BIT;
		if (ctx.op & (1  << 11))
			ctx.instruction = INST::LDRX_IP;
		else ctx.instruction = INST::STRX_IP;		
	}

	void decode_t1011_misc()
	{
		// figure 6.2
		ctx.imm = ctx.op & 0xFF;
		switch ((ctx.op >> 8) & 0xF)
		{
		case 0x0: // adjust stack pointer
			if (ctx.op & (1 << 7)) 
			{ // page 405: SUB SP, #imm7*4
				ctx.imm = (ctx.imm & 0x7F) << 2;
				ctx.instruction = INST::SUB_I;
				ctx.rd = 13;
				ctx.rn = 13;
			} else 
			{ // page 316: ADD SP, #imm7*4
				ctx.imm = (ctx.imm & 0x7F) << 2;
				ctx.instruction = INST::ADD_I;
				ctx.rd = 13;
				ctx.rn = 13;
			}
			return;

		case 0x5: // push SR
			ctx.imm |= (1 << 14); // mask link register
		case 0x4: // push (page 383)
			ctx.instruction = INST::STM_W;
			ctx.addressing_mode = ADDRESSING_MODE::DB;
			ctx.rn = 13;
			return;
		
		case 0xD: // pop PC
			ctx.imm |= (1 << 15); // mask PC register
		case 0xC: // pop 
			ctx.instruction = INST::LDM_W;
			ctx.addressing_mode = ADDRESSING_MODE::IA;
			ctx.rn = 13;	
			return;

		case 0xE: // software bp
			ctx.instruction = INST::BKPT;
			return;
			//return inst_debug();
		}
		inst_ud();
	}

	void decode_t1010_add_sp_pc()
	{
		ctx.rd = decode_regt<8>();
		ctx.imm = (ctx.op & 0xFF) << 2;
		if (ctx.op & (1 << 11))
			ctx.rn = 13;  // add ,sp page 315
		else ctx.rn = 15; // add ,pc page 314
		ctx.instruction = INST::ADD_I; // 	"ADD%c%s %Rd,%Rn,#0x%I",
	}

	void decode_t101()
	{
		if (ctx.op & (1 << 12))
			return decode_t1011_misc();
		return decode_t1010_add_sp_pc();
	}

	void decode_t1101_bcond_swi()
	{
		// decode condition
		ctx.cond = (CONDITION::CODE)((ctx.op >> 8) & 0xF);

		// sign extend shl 1
		signed long imm = ctx.op & 0xFF;
	
		switch (ctx.cond)
		{
		case CONDITION::AL: return inst_ud();
		case CONDITION::NV: 
			ctx.imm  = imm; 
			ctx.instruction = INST::SWI;
			break;
		default:
			imm <<= 24;
			imm >>= 23;
			ctx.imm  = ctx.addr + imm + 4; 
			ctx.instruction = INST::B;
		}		
	}

	void decode_t1100_loadstore_multiple()
	{
		ctx.rn = decode_regt<8>();
		ctx.imm = ctx.op & 0xFF;
		ctx.addressing_mode = ADDRESSING_MODE::IA;
		if (ctx.op & (1 << 11))
			ctx.instruction = INST::LDM_W; // ldmia page 344
		else ctx.instruction = INST::STM_W; // stmia page 389
	}

	void decode_t110()
	{
		if (ctx.op & (1 << 12))
			return decode_t1101_bcond_swi();
		return decode_t1100_loadstore_multiple();
	}
	void decode_t111()
	{
		ctx.imm = ctx.op & 0x7FF;
		switch ((ctx.op >> 11) & 0x3)
		{
		case 0x0: // simple B
			{
				// << 1 sign extend
				signed long imm = ctx.imm;
				imm <<= 21;
				imm >>= 20;
				ctx.imm = ctx.addr + 4 + (unsigned long)imm;
				ctx.instruction = INST::B;
				break; 
			}
		case 0x1: // BLX suffix or UD if imm odd
			if (ctx.op & 1)
				return inst_ud();
			// BLX_I not supported yet
			ctx.imm <<= 1;
			ctx.instruction = INST::BLX_I;
			break; 
		case 0x2: // BL/BLX prefix
			{
				// << 12 sign extend
				signed long imm = ctx.imm;
				imm <<= 21;
				imm >>= 9;
				ctx.imm = ctx.addr + 4 + (unsigned long)imm;
				ctx.instruction = INST::BPRE;
				break; 
			}
		case 0x3: // BL suffix page 330
			// doesnt seem to need +addr+4? spec says it needs though
			ctx.imm <<= 1;
			ctx.instruction = INST::BL;
			break; 
		}
	}

	void decode_instruction_thumb()
	{
		ctx.instruction = INST::DEBUG;
		ctx.cond = CONDITION::AL;
		ctx.imm = 0;

		switch (ctx.op >> 13)
		{
		case 0x0: return decode_t000(); // shift add sub
		case 0x1: return decode_t001();// add sub cmp mov
		case 0x2: return decode_t010();// data processing, special, branch, loadstore
		case 0x3: return decode_t011();// load store word
		case 0x4: return decode_t100(); // load store half/stack
		case 0x5: return decode_t101();// add sp/pc, special
		case 0x6: return decode_t110();// load store multiple, conditional brach, 
		case 0x7: return decode_t111();
		}
		inst_ud();
	}



	
};

#endif
