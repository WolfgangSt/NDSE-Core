#ifndef _SIGNAL2_H_
#define _SIGNAL2_H_

// TODO: cleanup context store for less copy overheads

#ifdef WIN32
#include "signal2/winsig.h"
#define CONTEXT_EBP(x) x.ctx.Ebp
#define CONTEXT_ESP(x) x.ctx.Esp
#define CONTEXT_EIP(x) x.ctx.Eip
#define CONTEXT_EAX(x) x.ctx.Eax
#define CONTEXT_EBX(x) x.ctx.Ebx
#define CONTEXT_ECX(x) x.ctx.Ecx
#define CONTEXT_EDX(x) x.ctx.Edx
#define CONTEXT_EDI(x) x.ctx.Edi
#define CONTEXT_ESI(x) x.ctx.Esi
#define CONTEXT_EFLAGS(x) x.ctx.EFlags


#else
#include "signal2/nixsig.h"
#include <ucontext.h>
#include <sys/mman.h>
// #include <asm/cachectl.h>
#define cacheflush(x,y,z) (0)
//#define _rotr(value, shift)(((unsigned long)value >> shift)|((unsigned long)value << (32-shift)))
#define _rotr(value, shift) _rotr_inline(value, shift)

inline unsigned long _rotr_inline (unsigned long value, unsigned long shift) __attribute__((always_inline));
inline unsigned long _rotr_inline (unsigned long value, unsigned long shift)
{
	return (((unsigned long)value >> shift)|((unsigned long)value << (32-shift)));
}

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

class Fiber
{
public:
	typedef void (fiber_cb)(Fiber *f);
	
	
	virtual void do_continue() = 0;
	// access to the context is only valid while not in running state
	ucontext_t context; 
	static Fiber* create(size_t stacksize, fiber_cb cb);
};

#endif
