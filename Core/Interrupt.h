#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "HLE.h"


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

template <typename T>
struct interrupt 
{
	static volatile long signaled; // max long bits interrupts

	static void poll_process();
	
	static void fire(unsigned long interrupt)
	{
		assert(interrupt < 32);
		signaled |= 1 << interrupt;
	}
};


template <> inline void interrupt<_ARM9>::poll_process()
{
	static int ctr = 0;

	ctr++;
	if (ctr < 10000)
		return;
	ctr = 0;

	unsigned long intr = (unsigned long)_InterlockedExchange(&signaled, 0);
	//if (intr)
	{
		// block is 0x100 size and aligned by 0x100
		// as page size is 0x200 minimum this will always be within one block
		memory_block *b = &memory::data_tcm.blocks[0x3F00 >> PAGING::SIZE_BITS];
		wstack *istack = (wstack*)(b->mem + (0x3F00 & PAGING::ADDRESS_MASK));

		// select irq here
		logging<_ARM9>::logf("Interrupt to %08X (%x)", istack->irq_handler, sizeof(wstack));
		//unsigned long irq_addr = 0xbadc0de;
		//HLE<T>::invoke(irq_addr, &processor<T>::context);
	}
}

template <typename T> void interrupt<T>::poll_process()
{
}


template <typename T> volatile long interrupt<T>::signaled = 0;

#endif
