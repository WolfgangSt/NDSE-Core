#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "Threading.h"

template <typename T>
struct interrupts
{
	enum { 
		VBLANK,
		HBLANK,
		VMATCH,
		TIMER0,
		TIMER1,
		TIMER2,
		TIMER3,
		SERIAL,
		DMA0,
		DMA1,
		DMA2,
		DMA3,
		KEYPAD,
		GAMEPAK,
		UNUSED0,
		UNUSED1
	}
	signal_event ints[16];

	static void wait_for(unsigned long mask)
	{
		unsigned int num = 0;
		signal_event w[16];
		for (int i = 0; i < 16; i++)
		{
			if (mask & 1)
				w[num++] = ints[i];
			mask >>= 1;
		}
		signal_event::wait_multiple(w, num);
	}

	static void signal(unsigned long mask)
	{
		for (int i = 0; i < 16; i++)
		{
			if (mask & 1)
				ints[i].signal();
			mask >>= 1;
		}
	}
};

#endif
