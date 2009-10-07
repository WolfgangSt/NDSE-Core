#ifndef _ARMCONTEXT_H_
#define _ARMCONTEXT_H_

// important: a maximum of 31 u32 registers is addressable through 
// JIT by using ebp+imm8.
// This is expandable to 63 when using negative offsets if needed.

// currently 21 registers are used


/*! Defines ARM System control context */
struct syscontrol_context
{
	enum { CONTROL_DEFAULT = 0x0 };
	unsigned long control_register;
};

/*! Defines an ARM CPU context */
struct emulation_context
{
	enum { THUMB_BIT = (1 << 5) };
	unsigned long regs[16]; /*! GPRs */
	unsigned long cpsr;     /*! cpsr */
	unsigned long spsr;     /*! spsr */

	/*! backup of the x86 flags register the last S operation produced */
	unsigned long x86_flags; 
	unsigned long bpre; /* branch prefix addend for thumb mode */
	syscontrol_context syscontrol; /* system control context */
};

#endif
