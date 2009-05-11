#ifndef _ARMCONTEXT_H_
#define _ARMCONTEXT_H_

struct syscontrol_context
{
	enum { CONTROL_DEFAULT = 0x0 };
	unsigned long control_register;
};

struct emulation_context
{
	enum { THUMB_BIT = (1 << 5) };
	unsigned long regs[16];
	unsigned long cpsr;
	unsigned long spsr;
	unsigned long x86_flags;
	syscontrol_context syscontrol;
};

#endif
