#include "nixsig.h"
#include <signal.h>
#include <QThread>
#include <QSemaphore>
#include "../osdep.h"

class NixFiber: public Fiber, public QThread
{
private:
	fiber_cb *cb;
	static __thread NixFiber* instance;
	QSemaphore cont;

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
		cont.acquire();
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
		handle(SIGSEGV);
		handle(SIGINT);
		handle(SIGILL);	
		handle(SIGTSTP);	
		raise(SIGSEGV);
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
		cont.release();
	}
};

__thread NixFiber* NixFiber::instance;


Fiber* Fiber::create(size_t stacksize, fiber_cb cb)
{
	return new NixFiber(stacksize, cb);
}




