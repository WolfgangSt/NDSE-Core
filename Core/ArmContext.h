#ifndef _ARMCONTEXT_H_
#define _ARMCONTEXT_H_

// maximum of 31 u32 registers addressable through ebp+imm8
// expandable to 63 when using negative offsets

struct syscontrol_context
{
	enum { CONTROL_DEFAULT = 0x0 };
	unsigned long control_register;
};

// currently 21 registers used
struct emulation_context
{
	enum { THUMB_BIT = (1 << 5) };
	unsigned long regs[16];
	unsigned long cpsr;
	unsigned long spsr;
	unsigned long x86_flags;
	unsigned long bpre; // branch prefix for thumb mode
	syscontrol_context syscontrol;
};

#endif
