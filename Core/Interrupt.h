#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "HLE.h"


// current approach:
//
// HLE branch invokes a poll
// when there is an interrupt available then a HLE::invoke is done
// on the irq.
// This is just a suboptimal handling when it comes to recursive
// interrupts that (potentially) never actually return
// as they would create a recursive callchain inside the poll/invoke
// exhausting the hosts stack.

// optimal approach:
//
// stop the JIT (how?! we dont know about threads within the emu)
// trying to patch breaks by looking forward would cause races
// the only really feasable way atm is polling
// this however is definitly a potential candidate for opts
//
// backup the current context
//
// rebranch to interrupt handler
// the handler will restore the backup after done
//
// continue execution


/*
BIOS interrupt dispatcher:

   stmdb r13!,{r0-r3,r12,r14}
   mov r0,#0x4000000
   adr lr,IntRet
   ldr pc,[r0,#-4] @ pc = [0x3007ffc]
IntRet:
   ldmia r13!,{r0-r3,r12,r14}
   subs pc,r14,#4

*/


struct wstack
{
	char irq_stack[160];
	char svc_stack[64];
	char area1[16];
	unsigned long sound_buffer;
	unsigned long area2;
	unsigned long icf;
	unsigned long irq_handler;
};

struct irq_entry
{
	unsigned long irq;
	unsigned long mask;
};

template <typename T>
struct interrupt 
{
	enum { MAX_INTERRUPTS = 25 };
	static volatile long signaled; // max long bits interrupts

	static void poll_process();
	
	static void fire(unsigned long interrupt)
	{
		assert(interrupt < 32);
		signaled |= 1 << interrupt;
	}
};


// _ARM9 hardcoded here
template <> inline void interrupt<_ARM9>::poll_process()
{
	// early out of IE is false
	static bool inside = false; // dont allow recursive interrupts for now
	static volatile unsigned long &IME = *(volatile unsigned long*)
		(memory::registers9_1.blocks[0x208 >> PAGING::SIZE_BITS].mem + 
		 (0x208 & PAGING::ADDRESS_MASK));
	static volatile unsigned long &IE = *(volatile unsigned long*)
		(memory::registers9_1.blocks[0x210 >> PAGING::SIZE_BITS].mem + 
		 (0x210 & PAGING::ADDRESS_MASK));
	static volatile unsigned long &IF = *(volatile unsigned long*)
		(memory::registers9_1.blocks[0x214 >> PAGING::SIZE_BITS].mem + 
		 (0x214 & PAGING::ADDRESS_MASK));

	
	if (inside)
		return;
	if (!(IME & 1))
		return;
	
	unsigned long intr = (unsigned long)_InterlockedExchange(&signaled, 0);

	emulation_context &ctx = processor<_ARM9>::context;
	emulation_context backup = ctx;
	for (unsigned long mask = 1; mask; mask <<= 1)
	{
		unsigned long itr = intr & mask & IE;
		if (itr)
		{
			IF = itr;
			// wstack is 0x100 size and aligned by 0x100
			// as page size is 0x200 minimum this will always be within one memory_block
			memory_block *b = &memory::data_tcm.blocks[0x3F00 >> PAGING::SIZE_BITS];
			wstack *istack = (wstack*)(b->mem + (0x3F00 & PAGING::ADDRESS_MASK));

			unsigned long addr = istack->irq_handler;
			//logging<_ARM9>::logf("Interrupt to %08X", addr); 
						
			ctx.regs[15] = addr;
			inside = true;
			HLE<_ARM9>::invoke(addr, &ctx);
			inside = false;
		}
	}
	ctx = backup;
}

// _ARM7 hardcoded here
template <> inline void interrupt<_ARM7>::poll_process()
{
	// early out of IE is false
	static bool inside = false; // dont allow recursive interrupts for now
	static volatile unsigned long &IME = *(volatile unsigned long*)
		(memory::registers7_1.blocks[0x208 >> PAGING::SIZE_BITS].mem + 
		 (0x208 & PAGING::ADDRESS_MASK));
	static volatile unsigned long &IE = *(volatile unsigned long*)
		(memory::registers7_1.blocks[0x210 >> PAGING::SIZE_BITS].mem + 
		 (0x210 & PAGING::ADDRESS_MASK));
	static volatile unsigned long &IF = *(volatile unsigned long*)
		(memory::registers7_1.blocks[0x214 >> PAGING::SIZE_BITS].mem + 
		 (0x214 & PAGING::ADDRESS_MASK));

	
	if (inside)
		return;
	if (!(IME & 1))
		return;
	

	unsigned long intr = (unsigned long)_InterlockedExchange(&signaled, 0);
	
	intr = intr & IE;
	if (intr)
	{
		IF = intr;
		// wstack is 0x100 size and aligned by 0x100
		// as page size is 0x200 minimum this will always be within one memory_block
		memory_block *b = &memory::arm7_wram.blocks[0xFF00 >> PAGING::SIZE_BITS];
		wstack *istack = (wstack*)(b->mem + (0xFF00 & PAGING::ADDRESS_MASK));

		unsigned long addr = istack->irq_handler;
		//logging<_ARM7>::logf("Interrupt to %08X", addr); 
		emulation_context &ctx = processor<_ARM7>::context;
		emulation_context backup = ctx;
		
		
		ctx.regs[15] = addr;
		inside = true;
		HLE<_ARM7>::invoke(addr, &ctx);
		inside = false;
		
		ctx = backup;
	}
}


template <typename T> volatile long interrupt<T>::signaled = 0;

#endif
