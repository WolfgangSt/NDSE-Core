#ifndef _CPUMODE_H_
#define _CPUMODE_H_

enum cpu_mode
{
	CPU_USER       = 0, // user or system
	CPU_SUPERVISOR = 1, // svc
	CPU_ABORT      = 2, // abt
	CPU_UNDEFINED  = 3, // und
	CPU_INTERRUPT  = 4, // irq
	CPU_FASTINT    = 5, // fiq
	CPU_MAX_MODES  = 6
};

#endif
