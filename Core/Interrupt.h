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
	static bool inside;

	static void poll_process();
	
	static void fire(unsigned long interrupt)
	{
		static volatile long &DISPSTAT = *(volatile long*)
		(memory::registers9_1.blocks[0x4 >> PAGING::SIZE_BITS].mem + 
		 (0x4 & PAGING::ADDRESS_MASK));


		assert(interrupt < 32);
		_InterlockedOr( &signaled, 1 << interrupt);
		switch (interrupt)
		{
		case 0: // vblank
			// set VBLK bit
			_InterlockedOr( &DISPSTAT, 1 );
			break;
		case 1: // vcount
			break;
		}
	}

	static void switch_and_invoke(unsigned long addr)
	{
		emulation_context &ctx_old = processor<T>::ctx();
		unsigned long cpsr = ctx_old.cpsr;

		// change to irq and backup cpsr
		HLE<T>::loadcpsr(0x12, 0x1F); 
		emulation_context &ctx_new = processor<T>::ctx();
		ctx_new.spsr = cpsr;
		

		// invoke interrupt
		// dispatching currently only works when it actually returns to the
		// internal BIOS (lr)
		// the only way to handle this more exact would be popping the 
		// callstack down to the invoking HLE
		// this might be archieved through esp backup at jit time
		// and restoring here, making this func a non returning one
		// bypassing the internal BIOS
		unsigned long pc = ctx_new.regs[15]; // will be modified by internal BIOS
		ctx_new.regs[15] = addr;
		inside = true;
		HLE<T>::invoke(addr, &ctx_new);
		inside = false;
		HLE<T>::loadcpsr(cpsr, 0x1F); // have to load old mode in order to not desync ebp (sorry for now!)
		processor<T>::ctx().regs[15] = pc;   // restore old lr

		
		// switch back (client has to do this)
		//emulation_context &ctx_exit = processor<T>::ctx();
		//HLE<T>::loadcpsr(ctx_exit.spsr, 0xFFFFFFFF);
	}

	// uses a full temporary register bank
	static void switch_and_invoke_old(unsigned long addr)
	{	
		emulation_context backup = processor<T>::ctx();

		// switch to irq and use it to run the interrupt
		HLE<T>::loadcpsr(0x12, 0x1F); 
		emulation_context &ctx = processor<T>::ctx();
		ctx.regs[15] = addr;
		inside = true;
		HLE<T>::invoke(addr, &ctx);
		inside = false;

		// restore backup
		HLE<T>::loadcpsr(backup.cpsr, 0x1F);
		processor<T>::ctx() = backup;
	}
};


// _ARM9 hardcoded here
template <> inline void interrupt<_ARM9>::poll_process()
{
	// early out of IE is false
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
			switch_and_invoke_old(addr);
		}
	}
}

// _ARM7 hardcoded here
template <> inline void interrupt<_ARM7>::poll_process()
{
	// early out of IE is false
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


	for (unsigned long mask = 1; mask; mask <<= 1)
	{
		unsigned long itr = intr & mask & IE;
		if (itr)
		{
			IF = itr;
			// wstack is 0x100 size and aligned by 0x100
			// as page size is 0x200 minimum this will always be within one memory_block
			memory_block *b = &memory::arm7_wram.blocks[0xFF00 >> PAGING::SIZE_BITS];
			wstack *istack = (wstack*)(b->mem + (0xFF00 & PAGING::ADDRESS_MASK));

			unsigned long addr = istack->irq_handler;
			//logging<_ARM7>::logf("Interrupt to %08X", addr); 
			// TODO: handle CPU mode switch!
			switch_and_invoke_old(addr);
		}
	}
}


template <typename T> volatile long interrupt<T>::signaled = 0;
template <typename T> bool interrupt<T>::inside = false;

#endif
