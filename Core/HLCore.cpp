#include "HLCore.h"
#include "HLE.h"
#include "Namespaces.h"
#include "Compiler.h"

#include "MemMap.h"
#include "Processor.h"

enum
{
	FLAG_N = 0x80000000,
	FLAG_Z = 0x40000000,
	FLAG_C = 0x20000000,
	FLAG_V = 0x10000000
};

#define UPDATE_FLAGS(mask) u32 org = ctx.regs[Rd]; (org)=(org); u32 nzcv = (ctx.cpsr & (~(mask)))
#define UPDATE_Z() if (ctx.regs[Rd] == 0) nzcv |= FLAG_Z
#define UPDATE_N() nzcv |= (ctx.regs[Rd] & 0x80000000)
#define UPDATE_NC() if ((signed)ctx.regs[Rd] < 0) nzcv |= FLAG_N | FLAG_C
#define UPDATE_V() if (ctx.regs[Rd] < org) nzcv |= FLAG_V
#define UPDATE_C(x) if ((x)) nzcv |= FLAG_C

#define IBITS (2 << (~ctx.regs[15] & 1))
#define ADDR_IP(x) u32 x;\
	if (flags & disassembler::U_BIT)\
		x = ctx.regs[Rn];\
	else x = (u32)(-(signed long)ctx.regs[Rn]); \
	if (Rn == 15) \
		x += IBITS; \
	x += imm

#define BREAK_IF_PC(x) if (x == 15) undefined()

#define TICK() ctx.regs[15] += IBITS

template <typename T> void* HLCore<T>::funcs[INST::MAX_INSTRUCTIONS];

template <typename T> int HLCore<T>::check_flags()
{
	return 0;
}

template <typename T> void HLCore<T>::undefined()
{
	DebugBreak_();
	//TICK();
}


// returns carry after shifting imm by amount
u32 shifter(u32 &imm, u32 amount, SHIFT::CODE op, u32 cpsr)
{
	u32 carry;
	amount &= 0xFF;

	if (amount == 0)
		return cpsr & FLAG_C;

	switch (op)
	{
	case SHIFT::LSL: 
		if (amount < 32)
		{
			carry = imm & (1 << (32 - amount));
			imm = imm << amount;
			return carry;
		} 
		if (amount == 32)
		{
			u32 carry = imm & 0x1;
			imm = 0;
			return carry;
		}
		imm = 0;
		return 0;
	case SHIFT::LSR:
		if (amount < 32)
		{
			carry = imm & (1 << (amount - 1));
			imm = imm >> amount;
			return carry;
		} 
		if (amount == 32)
		{
			carry = imm & 0x80000000;
			imm = 0;
			return carry;
		}
		imm = 0;
		return 0;
	case SHIFT::ASR: 
		if (amount < 32)
		{
			carry = imm & (1 << (amount - 1));
			imm = (u32)(((signed)imm) >> amount);
			return carry;
		} 
		if (imm & 0x80000000)
		{
			imm = 0xFFFFFFFF;
			return 1;
		}
		imm = 0;
		return 0;
	case SHIFT::ROR:
		amount &= 0x1F;
		if (amount == 0)
			return imm & 0x80000000;
		carry = imm & (1 << (amount - 1));
		imm = _rotr(imm, amount);
		return carry;
	case SHIFT::RRX: // 33bit rotate right using carry of cpsr amount ignored
		u32 carry = (imm & 1);
		imm >>= 1;
		imm |= (cpsr << 2) & 0x80000000;
		return carry;
	}
	DebugBreak_();
	return 0;
}

template <typename T> void HLCore<T>::STR_I(   /* "str%c %Rd,[%-%Rn],#0x%I"   */) { undefined(); }
template <typename T> void HLCore<T>::LDR_I(   /* "ldr%c %Rd,[%-%Rn],#0x%I"   */) { undefined(); }
template <typename T> void HLCore<T>::STR_IW(  /* "str%ct %Rd,[%-%Rn],#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::LDR_IW(  /* "ldr%ct %Rd,[%-%Rn],#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_I(  /* "str%cb %Rd,[%-%Rn],#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_I(  /* "ldr%cb %Rd,[%-%Rn],#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_IW( /* "str%cbt [%Rd],%-%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_IW( /* "ldr%cbt [%Rd],%-%Rn,#0x%I" */) { undefined(); }

// "str%c %Rd,[%-%Rn,#0x%I]" 
template <typename T> void HLCore<T>::STR_IP(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	HLE<T>::store32( addr, ctx.regs[Rd] );
}

// "ldr%c %Rd,[%-%Rn,#0x%I]" 
template <typename T> void HLCore<T>::LDR_IP(u32 flags, u32 imm, u32 Rn, u32 Rd)
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	ctx.regs[Rd] = HLE<T>::load32( addr );
}

// "str%c %Rd,[%-%Rn,#0x%I]!" 
template <typename T> void HLCore<T>::STR_IPW(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	HLE<T>::store32( addr, ctx.regs[Rd] );
	ctx.regs[Rn] += imm;
}

// "ldr%c %Rd,[%-%Rn,#0x%I]!" 
template <typename T> void HLCore<T>::LDR_IPW(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	ctx.regs[Rd] = HLE<T>::load32( addr );
	ctx.regs[Rn] += imm;
}

// "str%cb %Rd,[%-%Rn,#0x%I]" 
template <typename T> void HLCore<T>::STRB_IP(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	HLE<T>::store8( addr, ctx.regs[Rd] );
}

// "ldr%cb %Rd,[%-%Rn,#0x%I]" 
template <typename T> void HLCore<T>::LDRB_IP(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	ctx.regs[Rd] = HLE<T>::load8u( addr );
}
template <typename T> void HLCore<T>::STRB_IPW(/* "str%cb %Rd,[%-%Rn,#0x%I]!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_IPW(/* "ldr%cb %Rd,[%-%Rn,#0x%I]!" */) { undefined(); }
		
template <typename T> void HLCore<T>::STR_R(   /* "str%c %Rd,[%-%Rn],%Rm %S#0x%I"   */) { undefined(); }
template <typename T> void HLCore<T>::LDR_R(   /* "ldr%c %Rd,[%-%Rn],%Rm %S#0x%I"   */) { undefined(); }
template <typename T> void HLCore<T>::STR_RW(  /* "str%ct %Rd,[%-%Rn],%Rm %S#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::LDR_RW(  /* "ldr%ct %Rd,[%-%Rn],%Rm %S#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_R(  /* "str%cb %Rd,[%-%Rn],%Rm %S#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_R(  /* "ldr%cb %Rd,[%-%Rn],%Rm %S#0x%I"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_RW( /* "str%cbt %Rd,[%-%Rn],%Rm %S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_RW( /* "ldr%cbt %Rd,[%-%Rn],%Rm %S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::STR_RP(  /* "str%c %Rd,[%-%Rn,%Rm %S#0x%I]"   */) { undefined(); }
template <typename T> void HLCore<T>::LDR_RP(  /* "ldr%c %Rd,[%-%Rn,%Rm %S#0x%I]"   */) { undefined(); }
template <typename T> void HLCore<T>::STR_RPW( /* "str%c %Rd,[%-%Rn,%Rm %S#0x%I]!"  */) { undefined(); }
template <typename T> void HLCore<T>::LDR_RPW( /* "ldr%c %Rd,[%-%Rn,%Rm %S#0x%I]!"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_RP( /* "str%cb %Rd,[%-%Rn,%Rm %S#0x%I]"  */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_RP( /* "ldr%cb %Rd,[%-%Rn,%Rm %S#0x%I]"  */) { undefined(); }
template <typename T> void HLCore<T>::STRB_RPW(/* "str%cb %Rd,[%-%Rn,%Rm %S#0x%I]!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRB_RPW(/* "ldr%cb %Rd,[%-%Rn,%Rm %S#0x%I]!" */) { undefined(); }

template <typename T> void HLCore<T>::AND_I(/* "and%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::EOR_I(/* "eor%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::SUB_I(/* "sub%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::RSB_I(/* "rsb%c%s %Rd,%Rn,#0x%I" */) { undefined(); }

/* "add%c%s %Rd,%Rn,#0x%I" */
template <typename T> void HLCore<T>::ADD_I(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	BREAK_IF_PC(Rn);
	emulation_context &ctx = processor<T>::ctx();
	if (flags & disassembler::S_BIT)
	{
		UPDATE_FLAGS(FLAG_N | FLAG_Z | FLAG_C | FLAG_V);
		ctx.regs[Rd] = ctx.regs[Rn] + imm;
		UPDATE_Z();
		UPDATE_NC(); // C = N
		UPDATE_V();
		ctx.cpsr = nzcv;
	} else
	{
		ctx.regs[Rd] = ctx.regs[Rn] + imm;
	}
	TICK();
}

template <typename T> void HLCore<T>::ADC_I(/* "adc%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::SBC_I(/* "sbc%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::RSC_I(/* "rsc%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::TST_I(/* "tst%c %Rn,#0x%I"       */) { undefined(); }
template <typename T> void HLCore<T>::TEQ_I(/* "teq%c %Rn,#0x%I"       */) { undefined(); }
template <typename T> void HLCore<T>::CMP_I(/* "cmp%c %Rn,#0x%I"       */) { undefined(); }
template <typename T> void HLCore<T>::CMN_I(/* "cmn%c %Rn,#0x%I"       */) { undefined(); }

// "orr%c%s %Rd,%Rn,#0x%I" 
template <typename T> void HLCore<T>::ORR_I(u32 flags, u32 imm, u32 Rn, u32 Rd) 
{ 
	BREAK_IF_PC(Rd);
	BREAK_IF_PC(Rn);
	emulation_context &ctx = processor<T>::ctx();
	if (flags & disassembler::S_BIT)
	{
		UPDATE_FLAGS(FLAG_N | FLAG_Z);
		ctx.regs[Rd] = ctx.regs[Rn] | imm;
		UPDATE_Z();
		UPDATE_N();
		ctx.cpsr = nzcv;
	} else
	{
		ctx.regs[Rd] = ctx.regs[Rn] | imm;
	} 
	TICK();
}

// "mov%c%s %Rd,#0x%I"
template <typename T> void HLCore<T>::MOV_I(u32 flags, u32 imm, u32 Rd)
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	if (flags & disassembler::S_BIT)
	{
		UPDATE_FLAGS(FLAG_N | FLAG_Z);
		ctx.regs[Rd] = imm;
		UPDATE_N();
		UPDATE_Z();
	} else
	{
		ctx.regs[Rd] = imm;
	}
	TICK();
}

template <typename T> void HLCore<T>::BIC_I(/* "bic%c%s %Rd,%Rn,#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::MVN_I(/* "mvn%c%s %Rd,#0x%I"     */) { undefined(); }

// "msr cpsr_%F,#0x%I"
template <typename T> void HLCore<T>::MSR_CPSR_I(u32 /*flags*/, u32 mask, u32 imm) 
{ 
	undefined();
	emulation_context &ctx = processor<T>::ctx();
	HLE<T>::loadcpsr( imm, mask );
	TICK();
}

template <typename T> void HLCore<T>::MSR_SPSR_I(/* "msr spsr_%F,#0x%I" */) { undefined(); }

template <typename T> void HLCore<T>::AND_R(/* "and%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::EOR_R(/* "eor%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::SUB_R(/* "sub%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::RSB_R(/* "rsb%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::ADD_R(/* "add%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::ADC_R(/* "adc%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::SBC_R(/* "sbc%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::RSC_R(/* "rsc%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::TST_R(/* "tst%c %Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::TEQ_R(/* "teq%c %Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::CMP_R(/* "cmp%c %Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::CMN_R(/* "cmn%c %Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::ORR_R(/* "orr%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }

/* "mov%c%s %Rd,%Rm,%S#0x%I" */
template <typename T> void HLCore<T>::MOV_R(u32 flags, u32 imm, SHIFT::CODE shift, u32 Rm, u32 Rd) 
{ 
	emulation_context &ctx = processor<T>::ctx();
	if (flags & disassembler::S_BIT)
	{
		UPDATE_FLAGS(FLAG_N | FLAG_Z | FLAG_C);
		UPDATE_C( shifter( ctx.regs[Rm], imm, shift, ctx.cpsr ) );
		ctx.regs[Rd] = imm;
		UPDATE_N();
		UPDATE_Z();
	} else
	{
		shifter( ctx.regs[Rm], imm, shift, ctx.cpsr );
		ctx.regs[Rd] = imm;
	}
	TICK();
}

template <typename T> void HLCore<T>::BIC_R(/* "bic%c%s %Rd,%Rn,%Rm,%S#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::MVN_R(/* "mvn%c%s %Rd,%Rm,%S#0x%I" */) { undefined(); }

template <typename T> void HLCore<T>::AND_RR(/* "and%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::EOR_RR(/* "eor%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::SUB_RR(/* "sub%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::RSB_RR(/* "rsb%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::ADD_RR(/* "add%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::ADC_RR(/* "adc%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::SBC_RR(/* "sbc%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::RSC_RR(/* "rsc%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::TST_RR(/* "tst%c %Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::TEQ_RR(/* "teq%c %Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::CMP_RR(/* "cmp%c %Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::CMN_RR(/* "cmn%c %Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::ORR_RR(/* "orr%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::MOV_RR(/* "mov%c%s %Rd,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::BIC_RR(/* "%?bic%c%s %Rd,%Rn,%Rm,%S %Rs" */) { undefined(); }
template <typename T> void HLCore<T>::MVN_RR(/* "mvn%c%s %Rd,%Rm,%S %Rs" */) { undefined(); }

/* "msr%c cpsr_%F,%Rm" */
template <typename T> void HLCore<T>::MSR_CPSR_R(u32 /*flags*/, u32 Rm, u32 mask) 
{ 
	BREAK_IF_PC(Rm);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	HLE<T>::loadcpsr(ctx.regs[Rm], mask); 
}

template <typename T> void HLCore<T>::MSR_SPSR_R(/* "msr%c spsr_%F,%Rm" */) { undefined(); }
template <typename T> void HLCore<T>::MRS_CPSR(/* "msr%c %Rd,cpsr" */) { undefined(); }
template <typename T> void HLCore<T>::MRS_SPSR(/* "mrs%c %Rd,spsr" */) { undefined(); }

template <typename T> void HLCore<T>::CLZ(/* "clz%c %Rd,%Rm" */) { undefined(); }
template <typename T> void HLCore<T>::BKPT(/* "bkpt #0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::SWI(/* "swi #0x%I" */) { undefined(); }

template <typename T> void HLCore<T>::BX(/* "bx%c %Rm" */) { undefined(); }
template <typename T> void HLCore<T>::BLX(/* "blx%c %Rm" */) { undefined(); }
template <typename T> void HLCore<T>::B(/* "b%c #0x%A" */) { undefined(); }
template <typename T> void HLCore<T>::BL(/* "bl%c #0x%A" */) { undefined(); }

template <typename T> void HLCore<T>::MRC(/* "mrc%c %Cp,%C1,%Rd,%CRn,%CRm,%C2" */) { undefined(); }

/* "mcr%c %Cp,%C1,%Rd,%CRn,%CRm,%C2" */
template <typename T> void HLCore<T>::MCR(u32 /*flags*/, u32 C2, u32 CRm, u32 CRn, u32 Rd, u32 C1, u32 Cp)
{ 
	switch (Cp) 
	{
	case 0xF: // System control coprocessor
		if (C1 != 0) // SBZ
			goto default_;   // unpredictable
		if (Rd == 0xF)   // Rd must not be R15
			goto default_;   // unpredictable
		switch (CRn) // select register
		{
			
		case 1: // control bits (r/w)
			// seemed bogus
			break;
			
		// MMU/PU not supported yet
		case 2:
		case 3:
		case 5:
		case 6: // CRm and opcode 2 select the protected region/bank
			break; // skip (not supported yet)

		case 7: // cache and write buffers (write only)
			break; // not yet emulated/unimportant xD
		case 9: // cache lockdown or TCM remapping
			switch (CRm)
			{
			case 0x1: // TCM remapping
				HLE<T>::remap_tcm( Rd, C2 );
				break;
			default:
				break; // lockdown not emulated
			}
			break;

		case 0: // may not write to this register
		default:
			goto default_; // not supported yet
		}
		break;
	default:
	default_:
		undefined();
	}
}

template <typename T> void HLCore<T>::STM(/* "stm%c%M %Rn,{%RL}%^" */) { undefined(); }
template <typename T> void HLCore<T>::LDM(/* "ldm%c%M %Rn,{%RL}%^" */) { undefined(); }
template <typename T> void HLCore<T>::STM_W(/* "stm%c%M %Rn!,{%RL}%^" */) { undefined(); }
template <typename T> void HLCore<T>::LDM_W(/* "ldm%c%M %Rn!,{%RL}%^" */) { undefined(); }

template <typename T> void HLCore<T>::STRX_I(/* "str%c%E %Rd,[%-%Rn],#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_I(/* "ldr%c%E %Rd,[%-%Rn],#0x%I" */) { undefined(); }
template <typename T> void HLCore<T>::STRX_IW(/* "str%c%E %Rd,[%-%Rn],#0x%I!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_IW(/* "ldr%c%E %Rd,[%-%Rn],#0x%I!" */) { undefined(); }

// "str%c%E %Rd,[%-%Rn,#0x%I]" 
template <typename T> void HLCore<T>::STRX_IP(u32 flags, u32 imm, u32 Rn, u32 Rd, EXTEND_MODE::CODE mode) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	switch (mode)
	{
	case EXTEND_MODE::H: return HLE<T>::store16(addr, ctx.regs[Rd]);
	}
	undefined(); 
}

// "ldr%c%E %Rd,[%-%Rn,#0x%I]"
template <typename T> void HLCore<T>::LDRX_IP(u32 flags, u32 imm, u32 Rn, u32 Rd, EXTEND_MODE::CODE mode) 
{ 
	BREAK_IF_PC(Rd);
	emulation_context &ctx = processor<T>::ctx();
	TICK();
	ADDR_IP(addr);
	switch (mode)
	{
	case EXTEND_MODE::H: ctx.regs[Rd] = HLE<T>::load16u(addr); return;
	case EXTEND_MODE::SB: ctx.regs[Rd] = (u32)((signed long)((signed char)HLE<T>::load8u(addr))); return;
	case EXTEND_MODE::SH: ctx.regs[Rd] = HLE<T>::load16s(addr); return;
	}
	undefined(); 
}


template <typename T> void HLCore<T>::STRX_IPW(/* "str%c%E %Rd,[%-%Rn,#0x%I]!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_IPW(/* "ldr%c%E %Rd,[%-%Rn,#0x%I]!" */) { undefined(); }
template <typename T> void HLCore<T>::STRX_R(/* "str%c%E %Rd,[%-%Rn],%Rm" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_R(/* "ldr%c%E %Rd,[%-%Rn],%Rm" */) { undefined(); }
template <typename T> void HLCore<T>::STRX_RW(/* "str%c%E %Rd,[%-%Rn],%Rm!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_RW(/* "ldr%c%E %Rd,[%-%Rn],%Rm!" */) { undefined(); }
template <typename T> void HLCore<T>::STRX_RP(/* "str%c%E %Rd,[%-%Rn,%Rm]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_RP(/* "ldr%c%E %Rd,[%-%Rn,%Rm]" */) { undefined(); }
template <typename T> void HLCore<T>::STRX_RPW(/* "str%c%E %Rd,[%-%Rn,%Rm]!" */) { undefined(); }
template <typename T> void HLCore<T>::LDRX_RPW(/* "ldr%c%E %Rd,[%-%Rn,%Rm]!" */) { undefined(); }

template <typename T> void HLCore<T>::BPRE(/* "bpre #0x%A" */) { undefined(); }

template <typename T> void HLCore<T>::MUL_R(/* "mul%c%s %Rd,%Rm,%Rs" */) { undefined(); }
template <typename T> void HLCore<T>::MLA_R(/* "mla%c%s %Rd,%Rm,%Rs,%Rn" */) { undefined(); }

template <typename T> void HLCore<T>::UMULL(/* "umull%c%s %Rd,%Rn,%Rm,%Rs" */) { undefined(); }
template <typename T> void HLCore<T>::UMLAL(/* "umlal%c%s %Rd,%Rn,%Rm,%R" */) { undefined(); }
template <typename T> void HLCore<T>::SMULL(/* "smull%c%s %Rd,%Rn,%Rm,%Rs" */) { undefined(); }
template <typename T> void HLCore<T>::SMLAL(/* "smlal%c%s %Rd,%Rn,%Rm,%Rs" */) { undefined(); }

template <typename T> void HLCore<T>::SWP(/* "swp%c %Rd,%Rm,[%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::SWPB(/* "swpb%c %Rd,%Rm,[%Rn]" */) { undefined(); }

template <typename T> void HLCore<T>::BLX_I(/* "blx%c #0x%A" */) { undefined(); }

template <typename T> void HLCore<T>::LDRD_R(/* "? ldr%cd_R %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RW(/* "? ldr%cd_RW %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RI(/* "ldr%cd %Rd,[%Rn],%I" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RIW(/* "? ldr%cd_RIW %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RP(/* "? ldr%cd_RP %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RPW(/* "? ldr%cd_RPW %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RIP(/* "? ldr%cd_RIP %Rd,[%-%Rn]" */) { undefined(); }
template <typename T> void HLCore<T>::LDRD_RIPW(/* "? ldr%cd_RIPW %Rd,[%-%Rn]" */) { undefined(); }

template <typename T> void HLCore<T>::PLD_I(/* "pld [%Rn,#%-%I]" */) { undefined(); }
template <typename T> void HLCore<T>::PLD_R(/* "? pld [%Rn]" */) { undefined(); }


template <typename T> 
void HLCore<T>::init()
{
	for (int i = 0; i < INST::MAX_INSTRUCTIONS; i++)
		funcs[i] = undefined;

	funcs[INST::STR_I] = STR_I;
	funcs[INST::LDR_I] = LDR_I;
	funcs[INST::STR_IW] = STR_IW;
	funcs[INST::LDR_IW] = LDR_IW;
	funcs[INST::STRB_I] = STRB_I;
	funcs[INST::LDRB_I] = LDRB_I;
	funcs[INST::STRB_IW] = STRB_IW;
	funcs[INST::LDRB_IW] = LDRB_IW;
	funcs[INST::STR_IP] = STR_IP;
	funcs[INST::LDR_IP] = LDR_IP;
	funcs[INST::STR_IPW] = STR_IPW;
	funcs[INST::LDR_IPW] = LDR_IPW;
	funcs[INST::STRB_IP] = STRB_IP;
	funcs[INST::LDRB_IP] = LDRB_IP;
	funcs[INST::STRB_IPW] = STRB_IPW;
	funcs[INST::LDRB_IPW] = LDRB_IPW;
		
	funcs[INST::STR_R] = STR_R;
	funcs[INST::LDR_R] = LDR_R;
	funcs[INST::STR_RW] = STR_RW;
	funcs[INST::LDR_RW] = LDR_RW;
	funcs[INST::STRB_R] = STRB_R;
	funcs[INST::LDRB_R] = LDRB_R;
	funcs[INST::STRB_RW] = STRB_RW;
	funcs[INST::LDRB_RW] = LDRB_RW;
	funcs[INST::STR_RP] = STR_RP;
	funcs[INST::LDR_RP] = LDR_RP;
	funcs[INST::STR_RPW] = STR_RPW;
	funcs[INST::LDR_RPW] = LDR_RPW;
	funcs[INST::STRB_RP] = STRB_RP;
	funcs[INST::LDRB_RP] = LDRB_RP;
	funcs[INST::STRB_RPW] = STRB_RPW;
	funcs[INST::LDRB_RPW] = LDRB_RPW;

	funcs[INST::AND_I] = AND_I;
	funcs[INST::EOR_I] = EOR_I;
	funcs[INST::SUB_I] = SUB_I;
	funcs[INST::RSB_I] = RSB_I;
	funcs[INST::ADD_I] = ADD_I;
	funcs[INST::ADC_I] = ADC_I;
	funcs[INST::SBC_I] = SBC_I;
	funcs[INST::RSC_I] = RSC_I;
	funcs[INST::TST_I] = TST_I;
	funcs[INST::TEQ_I] = TEQ_I;
	funcs[INST::CMP_I] = CMP_I;
	funcs[INST::CMN_I] = CMN_I;
	funcs[INST::ORR_I] = ORR_I;
	funcs[INST::MOV_I] = MOV_I;
	funcs[INST::BIC_I] = BIC_I;
	funcs[INST::MVN_I] = MVN_I;

	funcs[INST::MSR_CPSR_I] = MSR_CPSR_I;
	funcs[INST::MSR_SPSR_I] = MSR_SPSR_I;

	funcs[INST::AND_R] = AND_R;
	funcs[INST::EOR_R] = EOR_R;
	funcs[INST::SUB_R] = SUB_R;
	funcs[INST::RSB_R] = RSB_R;
	funcs[INST::ADD_R] = ADD_R;
	funcs[INST::ADC_R] = ADC_R;
	funcs[INST::SBC_R] = SBC_R;
	funcs[INST::RSC_R] = RSC_R;
	funcs[INST::TST_R] = TST_R;
	funcs[INST::TEQ_R] = TEQ_R;
	funcs[INST::CMP_R] = CMP_R;
	funcs[INST::CMN_R] = CMN_R;
	funcs[INST::ORR_R] = ORR_R;
	funcs[INST::MOV_R] = MOV_R;
	funcs[INST::BIC_R] = BIC_R;
	funcs[INST::MVN_R] = MVN_R;

	funcs[INST::AND_RR] = AND_RR;
	funcs[INST::EOR_RR] = EOR_RR;
	funcs[INST::SUB_RR] = SUB_RR;
	funcs[INST::RSB_RR] = RSB_RR;
	funcs[INST::ADD_RR] = ADD_RR;
	funcs[INST::ADC_RR] = ADC_RR;
	funcs[INST::SBC_RR] = SBC_RR;
	funcs[INST::RSC_RR] = RSC_RR;
	funcs[INST::TST_RR] = TST_RR;
	funcs[INST::TEQ_RR] = TEQ_RR;
	funcs[INST::CMP_RR] = CMP_RR;
	funcs[INST::CMN_RR] = CMN_RR;
	funcs[INST::ORR_RR] = ORR_RR;
	funcs[INST::MOV_RR] = MOV_RR;
	funcs[INST::BIC_RR] = BIC_RR;
	funcs[INST::MVN_RR] = MVN_RR;

	funcs[INST::MSR_CPSR_R] = MSR_CPSR_R;
	funcs[INST::MSR_SPSR_R] = MSR_SPSR_R;
	funcs[INST::MRS_CPSR] = MRS_CPSR;
	funcs[INST::MRS_SPSR] = MRS_SPSR;

	funcs[INST::CLZ] = CLZ;
	funcs[INST::BKPT] = BKPT;
	funcs[INST::SWI] = SWI;

	funcs[INST::BX] = BX;
	funcs[INST::BLX] = BLX;
	funcs[INST::B] = B;
	funcs[INST::BL] = BL;

	funcs[INST::MRC] = MRC;
	funcs[INST::MCR] = MCR;

	funcs[INST::STM] = STM;
	funcs[INST::LDM] = LDM;
	funcs[INST::STM_W] = STM_W;
	funcs[INST::LDM_W] = LDM_W;


	funcs[INST::STRX_I] = STRX_I;
	funcs[INST::LDRX_I] = LDRX_I;
	funcs[INST::STRX_IW] = STRX_IW;
	funcs[INST::LDRX_IW] = LDRX_IW;
	funcs[INST::STRX_IP] = STRX_IP;
	funcs[INST::LDRX_IP] = LDRX_IP;
	funcs[INST::STRX_IPW] = STRX_IPW;
	funcs[INST::LDRX_IPW] = LDRX_IPW;
	funcs[INST::STRX_R] = STRX_R;
	funcs[INST::LDRX_R] = LDRX_R;
	funcs[INST::STRX_RW] = STRX_RW;
	funcs[INST::LDRX_RW] = LDRX_RW;
	funcs[INST::STRX_RP] = STRX_RP;
	funcs[INST::LDRX_RP] = LDRX_RP;
	funcs[INST::STRX_RPW] = STRX_RPW;
	funcs[INST::LDRX_RPW] = LDRX_RPW;

	funcs[INST::BPRE] = BPRE;

	funcs[INST::MUL_R] = MUL_R;
	funcs[INST::MLA_R] = MLA_R;

	funcs[INST::UMULL] = UMULL;
	funcs[INST::UMLAL] = UMLAL;
	funcs[INST::SMULL] = SMULL;
	funcs[INST::SMLAL] = SMLAL;

	funcs[INST::SWP] = SWP;
	funcs[INST::SWPB] = SWPB;

	funcs[INST::BLX_I] = BLX_I;

	funcs[INST::LDRD_R] = LDRD_R;
	funcs[INST::LDRD_RW] = LDRD_RW;
	funcs[INST::LDRD_RI] = LDRD_RI;
	funcs[INST::LDRD_RIW] = LDRD_RIW;
	funcs[INST::LDRD_RP] = LDRD_RP;
	funcs[INST::LDRD_RPW] = LDRD_RPW;
	funcs[INST::LDRD_RIP] = LDRD_RIP;
	funcs[INST::LDRD_RIPW] = LDRD_RIPW;

	funcs[INST::PLD_I] = PLD_I;
	funcs[INST::PLD_R] = PLD_R;
};

bool InitHLCore()
{
	HLCore<_ARM7>::init();
	HLCore<_ARM9>::init();

	symbols::syms[(void*)HLCore<_ARM7>::undefined]  = "arm7::ud";
	symbols::syms[(void*)HLCore<_ARM9>::undefined]  = "arm9::ud";
	symbols::syms[(void*)HLCore<_ARM7>::check_flags]  = "arm7::checkflags";
	symbols::syms[(void*)HLCore<_ARM9>::check_flags]  = "arm9::checkflags";



	symbols::syms[(void*)HLCore<_ARM7>::STR_I] = "arm7::STR_I";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_I] = "arm7::LDR_I";
	symbols::syms[(void*)HLCore<_ARM7>::STR_IW] = "arm7::STR_IW";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_IW] = "arm7::LDR_IW";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_I] = "arm7::STRB_I";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_I] = "arm7::LDRB_I";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_IW] = "arm7::STRB_IW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_IW] = "arm7::LDRB_IW";
	symbols::syms[(void*)HLCore<_ARM7>::STR_IP] = "arm7::STR_IP";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_IP] = "arm7::LDR_IP";
	symbols::syms[(void*)HLCore<_ARM7>::STR_IPW] = "arm7::STR_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_IPW] = "arm7::LDR_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_IP] = "arm7::STRB_IP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_IP] = "arm7::LDRB_IP";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_IPW] = "arm7::STRB_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_IPW] = "arm7::LDRB_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::STR_R] = "arm7::STR_R";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_R] = "arm7::LDR_R";
	symbols::syms[(void*)HLCore<_ARM7>::STR_RW] = "arm7::STR_RW";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_RW] = "arm7::LDR_RW";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_R] = "arm7::STRB_R";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_R] = "arm7::LDRB_R";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_RW] = "arm7::STRB_RW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_RW] = "arm7::LDRB_RW";
	symbols::syms[(void*)HLCore<_ARM7>::STR_RP] = "arm7::STR_RP";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_RP] = "arm7::LDR_RP";
	symbols::syms[(void*)HLCore<_ARM7>::STR_RPW] = "arm7::STR_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDR_RPW] = "arm7::LDR_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_RP] = "arm7::STRB_RP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_RP] = "arm7::LDRB_RP";
	symbols::syms[(void*)HLCore<_ARM7>::STRB_RPW] = "arm7::STRB_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRB_RPW] = "arm7::LDRB_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::AND_I] = "arm7::AND_I";
	symbols::syms[(void*)HLCore<_ARM7>::EOR_I] = "arm7::EOR_I";
	symbols::syms[(void*)HLCore<_ARM7>::SUB_I] = "arm7::SUB_I";
	symbols::syms[(void*)HLCore<_ARM7>::RSB_I] = "arm7::RSB_I";
	symbols::syms[(void*)HLCore<_ARM7>::ADD_I] = "arm7::ADD_I";
	symbols::syms[(void*)HLCore<_ARM7>::ADC_I] = "arm7::ADC_I";
	symbols::syms[(void*)HLCore<_ARM7>::SBC_I] = "arm7::SBC_I";
	symbols::syms[(void*)HLCore<_ARM7>::RSC_I] = "arm7::RSC_I";
	symbols::syms[(void*)HLCore<_ARM7>::TST_I] = "arm7::TST_I";
	symbols::syms[(void*)HLCore<_ARM7>::TEQ_I] = "arm7::TEQ_I";
	symbols::syms[(void*)HLCore<_ARM7>::CMP_I] = "arm7::CMP_I";
	symbols::syms[(void*)HLCore<_ARM7>::CMN_I] = "arm7::CMN_I";
	symbols::syms[(void*)HLCore<_ARM7>::ORR_I] = "arm7::ORR_I";
	symbols::syms[(void*)HLCore<_ARM7>::MOV_I] = "arm7::MOV_I";
	symbols::syms[(void*)HLCore<_ARM7>::BIC_I] = "arm7::BIC_I";
	symbols::syms[(void*)HLCore<_ARM7>::MVN_I] = "arm7::MVN_I";
	symbols::syms[(void*)HLCore<_ARM7>::MSR_CPSR_I] = "arm7::MSR_CPSR_I";
	symbols::syms[(void*)HLCore<_ARM7>::MSR_SPSR_I] = "arm7::MSR_SPSR_I";
	symbols::syms[(void*)HLCore<_ARM7>::AND_R] = "arm7::AND_R";
	symbols::syms[(void*)HLCore<_ARM7>::EOR_R] = "arm7::EOR_R";
	symbols::syms[(void*)HLCore<_ARM7>::SUB_R] = "arm7::SUB_R";
	symbols::syms[(void*)HLCore<_ARM7>::RSB_R] = "arm7::RSB_R";
	symbols::syms[(void*)HLCore<_ARM7>::ADD_R] = "arm7::ADD_R";
	symbols::syms[(void*)HLCore<_ARM7>::ADC_R] = "arm7::ADC_R";
	symbols::syms[(void*)HLCore<_ARM7>::SBC_R] = "arm7::SBC_R";
	symbols::syms[(void*)HLCore<_ARM7>::RSC_R] = "arm7::RSC_R";
	symbols::syms[(void*)HLCore<_ARM7>::TST_R] = "arm7::TST_R";
	symbols::syms[(void*)HLCore<_ARM7>::TEQ_R] = "arm7::TEQ_R";
	symbols::syms[(void*)HLCore<_ARM7>::CMP_R] = "arm7::CMP_R";
	symbols::syms[(void*)HLCore<_ARM7>::CMN_R] = "arm7::CMN_R";
	symbols::syms[(void*)HLCore<_ARM7>::ORR_R] = "arm7::ORR_R";
	symbols::syms[(void*)HLCore<_ARM7>::MOV_R] = "arm7::MOV_R";
	symbols::syms[(void*)HLCore<_ARM7>::BIC_R] = "arm7::BIC_R";
	symbols::syms[(void*)HLCore<_ARM7>::MVN_R] = "arm7::MVN_R";
	symbols::syms[(void*)HLCore<_ARM7>::AND_RR] = "arm7::AND_RR";
	symbols::syms[(void*)HLCore<_ARM7>::EOR_RR] = "arm7::EOR_RR";
	symbols::syms[(void*)HLCore<_ARM7>::SUB_RR] = "arm7::SUB_RR";
	symbols::syms[(void*)HLCore<_ARM7>::RSB_RR] = "arm7::RSB_RR";
	symbols::syms[(void*)HLCore<_ARM7>::ADD_RR] = "arm7::ADD_RR";
	symbols::syms[(void*)HLCore<_ARM7>::ADC_RR] = "arm7::ADC_RR";
	symbols::syms[(void*)HLCore<_ARM7>::SBC_RR] = "arm7::SBC_RR";
	symbols::syms[(void*)HLCore<_ARM7>::RSC_RR] = "arm7::RSC_RR";
	symbols::syms[(void*)HLCore<_ARM7>::TST_RR] = "arm7::TST_RR";
	symbols::syms[(void*)HLCore<_ARM7>::TEQ_RR] = "arm7::TEQ_RR";
	symbols::syms[(void*)HLCore<_ARM7>::CMP_RR] = "arm7::CMP_RR";
	symbols::syms[(void*)HLCore<_ARM7>::CMN_RR] = "arm7::CMN_RR";
	symbols::syms[(void*)HLCore<_ARM7>::ORR_RR] = "arm7::ORR_RR";
	symbols::syms[(void*)HLCore<_ARM7>::MOV_RR] = "arm7::MOV_RR";
	symbols::syms[(void*)HLCore<_ARM7>::BIC_RR] = "arm7::BIC_RR";
	symbols::syms[(void*)HLCore<_ARM7>::MVN_RR] = "arm7::MVN_RR";
	symbols::syms[(void*)HLCore<_ARM7>::MSR_CPSR_R] = "arm7::MSR_CPSR_R";
	symbols::syms[(void*)HLCore<_ARM7>::MSR_SPSR_R] = "arm7::MSR_SPSR_R";
	symbols::syms[(void*)HLCore<_ARM7>::MRS_CPSR] = "arm7::MRS_CPSR";
	symbols::syms[(void*)HLCore<_ARM7>::MRS_SPSR] = "arm7::MRS_SPSR";
	symbols::syms[(void*)HLCore<_ARM7>::CLZ] = "arm7::CLZ";
	symbols::syms[(void*)HLCore<_ARM7>::BKPT] = "arm7::BKPT";
	symbols::syms[(void*)HLCore<_ARM7>::SWI] = "arm7::SWI";
	symbols::syms[(void*)HLCore<_ARM7>::BX] = "arm7::BX";
	symbols::syms[(void*)HLCore<_ARM7>::BLX] = "arm7::BLX";
	symbols::syms[(void*)HLCore<_ARM7>::B] = "arm7::B";
	symbols::syms[(void*)HLCore<_ARM7>::BL] = "arm7::BL";
	symbols::syms[(void*)HLCore<_ARM7>::MRC] = "arm7::MRC";
	symbols::syms[(void*)HLCore<_ARM7>::MCR] = "arm7::MCR";
	symbols::syms[(void*)HLCore<_ARM7>::STM] = "arm7::STM";
	symbols::syms[(void*)HLCore<_ARM7>::LDM] = "arm7::LDM";
	symbols::syms[(void*)HLCore<_ARM7>::STM_W] = "arm7::STM_W";
	symbols::syms[(void*)HLCore<_ARM7>::LDM_W] = "arm7::LDM_W";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_I] = "arm7::STRX_I";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_I] = "arm7::LDRX_I";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_IW] = "arm7::STRX_IW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_IW] = "arm7::LDRX_IW";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_IP] = "arm7::STRX_IP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_IP] = "arm7::LDRX_IP";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_IPW] = "arm7::STRX_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_IPW] = "arm7::LDRX_IPW";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_R] = "arm7::STRX_R";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_R] = "arm7::LDRX_R";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_RW] = "arm7::STRX_RW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_RW] = "arm7::LDRX_RW";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_RP] = "arm7::STRX_RP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_RP] = "arm7::LDRX_RP";
	symbols::syms[(void*)HLCore<_ARM7>::STRX_RPW] = "arm7::STRX_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRX_RPW] = "arm7::LDRX_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::BPRE] = "arm7::BPRE";
	symbols::syms[(void*)HLCore<_ARM7>::MUL_R] = "arm7::MUL_R";
	symbols::syms[(void*)HLCore<_ARM7>::MLA_R] = "arm7::MLA_R";
	symbols::syms[(void*)HLCore<_ARM7>::UMULL] = "arm7::UMULL";
	symbols::syms[(void*)HLCore<_ARM7>::UMLAL] = "arm7::UMLAL";
	symbols::syms[(void*)HLCore<_ARM7>::SMULL] = "arm7::SMULL";
	symbols::syms[(void*)HLCore<_ARM7>::SMLAL] = "arm7::SMLAL";
	symbols::syms[(void*)HLCore<_ARM7>::SWP] = "arm7::SWP";
	symbols::syms[(void*)HLCore<_ARM7>::SWPB] = "arm7::SWPB";
	symbols::syms[(void*)HLCore<_ARM7>::BLX_I] = "arm7::BLX_I";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_R] = "arm7::LDRD_R";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RW] = "arm7::LDRD_RW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RI] = "arm7::LDRD_RI";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RIW] = "arm7::LDRD_RIW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RP] = "arm7::LDRD_RP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RPW] = "arm7::LDRD_RPW";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RIP] = "arm7::LDRD_RIP";
	symbols::syms[(void*)HLCore<_ARM7>::LDRD_RIPW] = "arm7::LDRD_RIPW";
	symbols::syms[(void*)HLCore<_ARM7>::PLD_I] = "arm7::PLD_I";
	symbols::syms[(void*)HLCore<_ARM7>::PLD_R] = "arm7::PLD_R";

	symbols::syms[(void*)HLCore<_ARM9>::STR_I] = "arm9::STR_I";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_I] = "arm9::LDR_I";
	symbols::syms[(void*)HLCore<_ARM9>::STR_IW] = "arm9::STR_IW";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_IW] = "arm9::LDR_IW";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_I] = "arm9::STRB_I";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_I] = "arm9::LDRB_I";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_IW] = "arm9::STRB_IW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_IW] = "arm9::LDRB_IW";
	symbols::syms[(void*)HLCore<_ARM9>::STR_IP] = "arm9::STR_IP";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_IP] = "arm9::LDR_IP";
	symbols::syms[(void*)HLCore<_ARM9>::STR_IPW] = "arm9::STR_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_IPW] = "arm9::LDR_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_IP] = "arm9::STRB_IP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_IP] = "arm9::LDRB_IP";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_IPW] = "arm9::STRB_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_IPW] = "arm9::LDRB_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::STR_R] = "arm9::STR_R";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_R] = "arm9::LDR_R";
	symbols::syms[(void*)HLCore<_ARM9>::STR_RW] = "arm9::STR_RW";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_RW] = "arm9::LDR_RW";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_R] = "arm9::STRB_R";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_R] = "arm9::LDRB_R";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_RW] = "arm9::STRB_RW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_RW] = "arm9::LDRB_RW";
	symbols::syms[(void*)HLCore<_ARM9>::STR_RP] = "arm9::STR_RP";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_RP] = "arm9::LDR_RP";
	symbols::syms[(void*)HLCore<_ARM9>::STR_RPW] = "arm9::STR_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDR_RPW] = "arm9::LDR_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_RP] = "arm9::STRB_RP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_RP] = "arm9::LDRB_RP";
	symbols::syms[(void*)HLCore<_ARM9>::STRB_RPW] = "arm9::STRB_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRB_RPW] = "arm9::LDRB_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::AND_I] = "arm9::AND_I";
	symbols::syms[(void*)HLCore<_ARM9>::EOR_I] = "arm9::EOR_I";
	symbols::syms[(void*)HLCore<_ARM9>::SUB_I] = "arm9::SUB_I";
	symbols::syms[(void*)HLCore<_ARM9>::RSB_I] = "arm9::RSB_I";
	symbols::syms[(void*)HLCore<_ARM9>::ADD_I] = "arm9::ADD_I";
	symbols::syms[(void*)HLCore<_ARM9>::ADC_I] = "arm9::ADC_I";
	symbols::syms[(void*)HLCore<_ARM9>::SBC_I] = "arm9::SBC_I";
	symbols::syms[(void*)HLCore<_ARM9>::RSC_I] = "arm9::RSC_I";
	symbols::syms[(void*)HLCore<_ARM9>::TST_I] = "arm9::TST_I";
	symbols::syms[(void*)HLCore<_ARM9>::TEQ_I] = "arm9::TEQ_I";
	symbols::syms[(void*)HLCore<_ARM9>::CMP_I] = "arm9::CMP_I";
	symbols::syms[(void*)HLCore<_ARM9>::CMN_I] = "arm9::CMN_I";
	symbols::syms[(void*)HLCore<_ARM9>::ORR_I] = "arm9::ORR_I";
	symbols::syms[(void*)HLCore<_ARM9>::MOV_I] = "arm9::MOV_I";
	symbols::syms[(void*)HLCore<_ARM9>::BIC_I] = "arm9::BIC_I";
	symbols::syms[(void*)HLCore<_ARM9>::MVN_I] = "arm9::MVN_I";
	symbols::syms[(void*)HLCore<_ARM9>::MSR_CPSR_I] = "arm9::MSR_CPSR_I";
	symbols::syms[(void*)HLCore<_ARM9>::MSR_SPSR_I] = "arm9::MSR_SPSR_I";
	symbols::syms[(void*)HLCore<_ARM9>::AND_R] = "arm9::AND_R";
	symbols::syms[(void*)HLCore<_ARM9>::EOR_R] = "arm9::EOR_R";
	symbols::syms[(void*)HLCore<_ARM9>::SUB_R] = "arm9::SUB_R";
	symbols::syms[(void*)HLCore<_ARM9>::RSB_R] = "arm9::RSB_R";
	symbols::syms[(void*)HLCore<_ARM9>::ADD_R] = "arm9::ADD_R";
	symbols::syms[(void*)HLCore<_ARM9>::ADC_R] = "arm9::ADC_R";
	symbols::syms[(void*)HLCore<_ARM9>::SBC_R] = "arm9::SBC_R";
	symbols::syms[(void*)HLCore<_ARM9>::RSC_R] = "arm9::RSC_R";
	symbols::syms[(void*)HLCore<_ARM9>::TST_R] = "arm9::TST_R";
	symbols::syms[(void*)HLCore<_ARM9>::TEQ_R] = "arm9::TEQ_R";
	symbols::syms[(void*)HLCore<_ARM9>::CMP_R] = "arm9::CMP_R";
	symbols::syms[(void*)HLCore<_ARM9>::CMN_R] = "arm9::CMN_R";
	symbols::syms[(void*)HLCore<_ARM9>::ORR_R] = "arm9::ORR_R";
	symbols::syms[(void*)HLCore<_ARM9>::MOV_R] = "arm9::MOV_R";
	symbols::syms[(void*)HLCore<_ARM9>::BIC_R] = "arm9::BIC_R";
	symbols::syms[(void*)HLCore<_ARM9>::MVN_R] = "arm9::MVN_R";
	symbols::syms[(void*)HLCore<_ARM9>::AND_RR] = "arm9::AND_RR";
	symbols::syms[(void*)HLCore<_ARM9>::EOR_RR] = "arm9::EOR_RR";
	symbols::syms[(void*)HLCore<_ARM9>::SUB_RR] = "arm9::SUB_RR";
	symbols::syms[(void*)HLCore<_ARM9>::RSB_RR] = "arm9::RSB_RR";
	symbols::syms[(void*)HLCore<_ARM9>::ADD_RR] = "arm9::ADD_RR";
	symbols::syms[(void*)HLCore<_ARM9>::ADC_RR] = "arm9::ADC_RR";
	symbols::syms[(void*)HLCore<_ARM9>::SBC_RR] = "arm9::SBC_RR";
	symbols::syms[(void*)HLCore<_ARM9>::RSC_RR] = "arm9::RSC_RR";
	symbols::syms[(void*)HLCore<_ARM9>::TST_RR] = "arm9::TST_RR";
	symbols::syms[(void*)HLCore<_ARM9>::TEQ_RR] = "arm9::TEQ_RR";
	symbols::syms[(void*)HLCore<_ARM9>::CMP_RR] = "arm9::CMP_RR";
	symbols::syms[(void*)HLCore<_ARM9>::CMN_RR] = "arm9::CMN_RR";
	symbols::syms[(void*)HLCore<_ARM9>::ORR_RR] = "arm9::ORR_RR";
	symbols::syms[(void*)HLCore<_ARM9>::MOV_RR] = "arm9::MOV_RR";
	symbols::syms[(void*)HLCore<_ARM9>::BIC_RR] = "arm9::BIC_RR";
	symbols::syms[(void*)HLCore<_ARM9>::MVN_RR] = "arm9::MVN_RR";
	symbols::syms[(void*)HLCore<_ARM9>::MSR_CPSR_R] = "arm9::MSR_CPSR_R";
	symbols::syms[(void*)HLCore<_ARM9>::MSR_SPSR_R] = "arm9::MSR_SPSR_R";
	symbols::syms[(void*)HLCore<_ARM9>::MRS_CPSR] = "arm9::MRS_CPSR";
	symbols::syms[(void*)HLCore<_ARM9>::MRS_SPSR] = "arm9::MRS_SPSR";
	symbols::syms[(void*)HLCore<_ARM9>::CLZ] = "arm9::CLZ";
	symbols::syms[(void*)HLCore<_ARM9>::BKPT] = "arm9::BKPT";
	symbols::syms[(void*)HLCore<_ARM9>::SWI] = "arm9::SWI";
	symbols::syms[(void*)HLCore<_ARM9>::BX] = "arm9::BX";
	symbols::syms[(void*)HLCore<_ARM9>::BLX] = "arm9::BLX";
	symbols::syms[(void*)HLCore<_ARM9>::B] = "arm9::B";
	symbols::syms[(void*)HLCore<_ARM9>::BL] = "arm9::BL";
	symbols::syms[(void*)HLCore<_ARM9>::MRC] = "arm9::MRC";
	symbols::syms[(void*)HLCore<_ARM9>::MCR] = "arm9::MCR";
	symbols::syms[(void*)HLCore<_ARM9>::STM] = "arm9::STM";
	symbols::syms[(void*)HLCore<_ARM9>::LDM] = "arm9::LDM";
	symbols::syms[(void*)HLCore<_ARM9>::STM_W] = "arm9::STM_W";
	symbols::syms[(void*)HLCore<_ARM9>::LDM_W] = "arm9::LDM_W";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_I] = "arm9::STRX_I";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_I] = "arm9::LDRX_I";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_IW] = "arm9::STRX_IW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_IW] = "arm9::LDRX_IW";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_IP] = "arm9::STRX_IP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_IP] = "arm9::LDRX_IP";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_IPW] = "arm9::STRX_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_IPW] = "arm9::LDRX_IPW";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_R] = "arm9::STRX_R";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_R] = "arm9::LDRX_R";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_RW] = "arm9::STRX_RW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_RW] = "arm9::LDRX_RW";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_RP] = "arm9::STRX_RP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_RP] = "arm9::LDRX_RP";
	symbols::syms[(void*)HLCore<_ARM9>::STRX_RPW] = "arm9::STRX_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRX_RPW] = "arm9::LDRX_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::BPRE] = "arm9::BPRE";
	symbols::syms[(void*)HLCore<_ARM9>::MUL_R] = "arm9::MUL_R";
	symbols::syms[(void*)HLCore<_ARM9>::MLA_R] = "arm9::MLA_R";
	symbols::syms[(void*)HLCore<_ARM9>::UMULL] = "arm9::UMULL";
	symbols::syms[(void*)HLCore<_ARM9>::UMLAL] = "arm9::UMLAL";
	symbols::syms[(void*)HLCore<_ARM9>::SMULL] = "arm9::SMULL";
	symbols::syms[(void*)HLCore<_ARM9>::SMLAL] = "arm9::SMLAL";
	symbols::syms[(void*)HLCore<_ARM9>::SWP] = "arm9::SWP";
	symbols::syms[(void*)HLCore<_ARM9>::SWPB] = "arm9::SWPB";
	symbols::syms[(void*)HLCore<_ARM9>::BLX_I] = "arm9::BLX_I";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_R] = "arm9::LDRD_R";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RW] = "arm9::LDRD_RW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RI] = "arm9::LDRD_RI";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RIW] = "arm9::LDRD_RIW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RP] = "arm9::LDRD_RP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RPW] = "arm9::LDRD_RPW";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RIP] = "arm9::LDRD_RIP";
	symbols::syms[(void*)HLCore<_ARM9>::LDRD_RIPW] = "arm9::LDRD_RIPW";
	symbols::syms[(void*)HLCore<_ARM9>::PLD_I] = "arm9::PLD_I";
	symbols::syms[(void*)HLCore<_ARM9>::PLD_R] = "arm9::PLD_R";

	return true;
}