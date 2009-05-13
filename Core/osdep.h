#ifndef _SIGNAL2_H_
#define _SIGNAL2_H_

//

#ifdef WIN32
#include "signal2/winsig.h"
#define CONTEXT_EBP(x) x.Ebp
#define CONTEXT_ESP(x) x.Esp
#define CONTEXT_EIP(x) x.Eip
#define CONTEXT_EAX(x) x.Eax
#define CONTEXT_EBX(x) x.Ebx
#define CONTEXT_ECX(x) x.Ecx
#define CONTEXT_EDX(x) x.Edx
#define CONTEXT_EDI(x) x.Edi
#define CONTEXT_ESI(x) x.Esi
#define CONTEXT_EFLAGS(x) x.EFlags


#else
#include "signal2/nixsig.h"
#include <ucontext.h>
#include <sys/mman.h>
// #include <asm/cachectl.h>
#define cacheflush(x,y,z) (0)
#define _rotr(value, shift)(((unsigned long)value >> shift)|((unsigned long)value << (32-shift)))

// signal2 sets a signal handler for sig but only on the calling thread
// posix needs to use pthread_sigmask here
// the handler has  SA_SIGINFO format, which however is not defined on 
// BSD, so it needs some special handling there

#define PtrToUlong(x) ((unsigned long)x)

#define CONTEXT_EBP(x) x.gregs[REG_EBP]
#define CONTEXT_ESP(x) x.gregs[REG_ESP]
#define CONTEXT_EIP(x) x.gregs[REG_EIP]
#define CONTEXT_EAX(x) x.gregs[REG_EAX]
#define CONTEXT_EBX(x) x.gregs[REG_EBX]
#define CONTEXT_ECX(x) x.gregs[REG_ECX]
#define CONTEXT_EDX(x) x.gregs[REG_EDX]
#define CONTEXT_EDI(x) x.gregs[REG_EDI]
#define CONTEXT_ESI(x) x.gregs[REG_ESI]
#define CONTEXT_EFLAGS(x) x.gregs[REG_EFL]


#endif

#endif
