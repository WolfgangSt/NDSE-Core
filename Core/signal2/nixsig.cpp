#include "nixsig.h"
#include <signal.h>
#include <pthread.h>
#include <boost/thread.hpp>
#include "../osdep.h"

// boost thread is not flexible enough for our needs
// thus for now this is based on pthreads



class PThread
{
private:
	pthread_t thread;
	pthread_attr_t attr;

	size_t threadsize;

	static void *run_(void *thisptr)
	{ 
		((PThread*)thisptr)->run();
	}

public:
	enum Priority { 
		IdlePriority, 
		LowestPriority, 
		LowPriority,
		NormalPriority,
		HighPriority,
		HighestPriority,
		TimeCriticalPriority,
		InheritPriority
	};

	PThread()
	{
		pthread_attr_init( &attr );
	}

	void start(Priority priority = InheritPriority)
	{
		pthread_create(&thread, &attr, run_, this);
	}

	~PThread()
	{
		pthread_cancel(thread);
	}

	void setStackSize (unsigned int stackSize)
	{
		pthread_attr_setstacksize(&attr, stackSize);
	}

	virtual void run() = 0;
};

class NixFiber: public Fiber, PThread
{
private:
	fiber_cb *cb;
	static __thread NixFiber* instance;
	boost::mutex cont;

	static void handler(int sig, siginfo_t *info, void *ctx)
	{
		ucontext_t* uctx = (ucontext_t*)ctx;
		instance->handler2(sig, info, uctx);
		setcontext(uctx); // continue execution at (modified) ctx

	}

	void handler2(int sig, siginfo_t *info, ucontext_t *ctx)
	{
		context = *ctx;
		do_callback();
		*ctx = context;
	}

	void do_callback()
	{
		cb(this);
		cont.lock();
	}

	void handle(int sig)
	{
		struct sigaction sa;
		struct sigaction oa;
		sa.sa_sigaction = handler;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, sig);
		sigaction( sig, &sa, &oa );
	}

	void run()
	{
		// entry point for the thread
		instance = this;
		cont.lock();
		handle(SIGSEGV);
		handle(SIGINT);
		handle(SIGILL);	
		handle(SIGTSTP);	
		raise(SIGSEGV);
		//raise(SIGINT); // for debugging with GDB!		
		for (;;)
			do_callback();
	}

public:
	NixFiber(size_t stacksize, fiber_cb cb) : cb(cb)
	{
		setStackSize(stacksize);
		start(LowPriority);
	}

	void do_continue()
	{
		cont.unlock();
	}
};

__thread NixFiber* NixFiber::instance;


Fiber* Fiber::create(size_t stacksize, fiber_cb cb)
{
	return new NixFiber(stacksize, cb);
}

int mprotect2(const void *addr, size_t len, int prot)
{
	// align addresses
	char *c = (char*)addr;
	static long int pagesize = sysconf(_SC_PAGESIZE);
	long int addend = (c - (char*)0) % pagesize;
	c -= addend;
	len += addend + pagesize - 1;
	len %= pagesize;
	return mprotect(c, len, prot);
}




