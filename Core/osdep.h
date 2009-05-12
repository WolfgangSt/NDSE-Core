#ifndef _SIGNAL2_H_
#define _SIGNAL2_H_

//

#ifdef WIN32
#include "signal2/winsig.h"
#define CONTEXT_EBP(x) x.Ebp
#define CONTEXT_EIP(x) x.Eip
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
#define CONTEXT_EIP(x) x.gregs[REG_EIP]

#endif

#endif
