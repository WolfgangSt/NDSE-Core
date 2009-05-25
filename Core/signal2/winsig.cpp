#include "../osdep.h"
#include <iostream>
#include <stdlib.h>
#include <assert.h>

int mprotect(const void *addr, size_t len, int prot)
{
	DWORD winprot;
	DWORD unused;

	switch (prot)
	{
	case PROT_NONE: 
		winprot = PAGE_NOACCESS; break;
	case PROT_READ: 
		winprot = PAGE_READONLY; break;
	case PROT_WRITE: 
		winprot = PAGE_WRITECOPY; break;
	case PROT_READ | PROT_WRITE:
		winprot = PAGE_READWRITE; break;
	case PROT_EXEC:
		winprot = PAGE_EXECUTE; break;
	case PROT_EXEC | PROT_READ:
		winprot = PAGE_EXECUTE_READ; break;
	case PROT_EXEC | PROT_WRITE:
		winprot = PAGE_EXECUTE_WRITECOPY; break;
	case PROT_EXEC | PROT_READ | PROT_WRITE:
		winprot = PAGE_EXECUTE_READWRITE; break;
	default:
		winprot = PAGE_NOACCESS;
	};
  
	return VirtualProtect( (void*)addr, len, winprot, &unused ) == TRUE ? 0 : -1;
}

int cacheflush(char *addr, int nbytes, int /* cache */)
{
	// only handles ICACHE
	HANDLE p = GetCurrentProcess();
	return FlushInstructionCache( p, addr, nbytes ) == TRUE ? 0 : -1;
}

////////////////////////////////////////////////////////////////////////////////
// New OS Dependant API

class WinFiber: public Fiber
{
private:
	HANDLE cont;
	bool running;
	fiber_cb *cb;
	ucontext_t ctx;

	static DWORD WINAPI entry_( LPVOID lpThreadParameter)
	{
		((WinFiber*)lpThreadParameter)->entry();
		return 0;
	}

	void do_callback()
	{
		cb(this);
		WaitForSingleObject(cont, INFINITE);
	}

	void handle_failure(PEXCEPTION_POINTERS e)
	{
		running = false;
		context.uc_mcontext.ctx = *e->ContextRecord;
		do_callback();
		*e->ContextRecord = context.uc_mcontext.ctx;
		running = true;
	}

	void entry()
	{
		__try
		{
			RaiseException(0, 0, 0, 0);
		} __except(handle_failure(GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION)
		{
		}
		for (;;)
			do_callback();
	}
public:
	WinFiber(size_t stacksize, fiber_cb cb) : cb(cb)
	{
		// create a thread for the fiber
		running = true;
		cont = CreateEvent(0, FALSE, FALSE, 0);
		HANDLE h = CreateThread(0, stacksize, entry_, this, 0, 0);
		SetThreadPriority( h, THREAD_PRIORITY_BELOW_NORMAL );
	}

	void do_continue()
	{
		SetEvent(cont);
	}
};


Fiber* Fiber::create(size_t stacksize, fiber_cb cb)
{
	return new WinFiber(stacksize, cb);
}
