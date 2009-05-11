#ifndef _SIGNAL2_H_
#define _SIGNAL2_H_

//

#ifdef WIN32
#include "signal2/winsig.h"
#else
#include <ucontext.h>
#include <sys/mman.h>
#include <asm/cachectl.h>
#endif

// signal2 sets a signal handler for sig but only on the calling thread
// posix needs to use pthread_sigmask here
// the handler has  SA_SIGINFO format, which however is not defined on 
// BSD, so it needs some special handling there


#endif
