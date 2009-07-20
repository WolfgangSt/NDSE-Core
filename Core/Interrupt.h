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
	static int ctr = 0;

	ctr++;
	if (ctr < 1000000)
		return;
	ctr = 0;

	unsigned long intr = (unsigned long)_InterlockedExchange(&signaled, 0);
	//if (intr)
	{
		/*
		// block is 0x100 size and aligned by 0x100
		// as page size is 0x200 minimum this will always be within one block
		memory_block *b = &memory::data_tcm.blocks[0x3F00 >> PAGING::SIZE_BITS];
		wstack *istack = (wstack*)(b->mem + (0x3F00 & PAGING::ADDRESS_MASK));

		// assume correct alignment!
		// that is all irq handlers must be on the same page
		unsigned long addr = istack->irq_handler;
		unsigned long addr_low = (addr - MAX_INTERRUPTS * 8);
		b = memory_map<_ARM9>::addr2page(addr_low);
		assert(b == memory_map<_ARM9>::addr2page(addr));
		irq_entry *irqs = (irq_entry*)(b->mem + (addr_low & PAGING::ADDRESS_MASK));

		unsigned long irq = irqs[0].irq;


		// select irq here
		logging<_ARM9>::logf("Interrupt to %08X (%x)", istack->irq_handler, irq); 
		emulation_context ctx = processor<_ARM9>::context;
		//HLE<_ARM9>::invoke(irq, &ctx);
		*/

		emulation_context &ctx = processor<_ARM9>::context;
		unsigned long backup[6] = { 
			ctx.regs[0], 
			ctx.regs[1],
			ctx.regs[2],
			ctx.regs[3],
			ctx.regs[12],
			ctx.regs[14]
		};
		

		// stmdb r13!,{r0-r3,r12,r14}
		ctx.regs[13] -= 6*4;
		HLE<_ARM9>::store32_array( ctx.regs[13], 6, backup );
		
		// rebranching wont work here as this is called from within the JIT
		// so for now we just do a HLE invoke

		// mov r0,#0x4000000
		ctx.regs[0]  = 0x4000000;
		// adr lr,IntRet <-- implicitly done within HLE::invoke
		// ldr pc,[r0,#-4] @ pc = [0x3007ffc]
		ctx.regs[15] = 0x3007FFC;
		HLE<_ARM9>::invoke( 0x3007FFC, &ctx );
		// on interrupt return we will reach this point, undo the stores

		//ldmia r13!,{r0-r3,r12,r14}
		HLE<_ARM9>::load32_array( ctx.regs[13], 6, backup );
		ctx.regs[13] += 6*4;

		ctx.regs[0] = backup[0];
		ctx.regs[1] = backup[1];
		ctx.regs[2] = backup[2];
		ctx.regs[3] = backup[3];
		ctx.regs[12] = backup[4];
		ctx.regs[14] = backup[5];
	}
}

template <typename T> void interrupt<T>::poll_process()
{
}


template <typename T> volatile long interrupt<T>::signaled = 0;

#endif
