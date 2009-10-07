#ifndef _CPUMODE_H_
#define _CPUMODE_H_

//! Defines ARM CPU States
enum cpu_mode
{
	CPU_USER       = 0, /*!< user or system mode */
	CPU_SUPERVISOR = 1, /*!< svc mode */
	CPU_ABORT      = 2, /*!< abt mode */
	CPU_UNDEFINED  = 3, /*!< und mode */
	CPU_INTERRUPT  = 4, /*!< irq mode */
	CPU_FASTINT    = 5, /*!< fiq mode */
	CPU_MAX_MODES  = 6  /*!< number of modes */
};

#endif
