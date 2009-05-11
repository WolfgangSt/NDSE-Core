#include "winsig.h"

// windows compatibility mode
// windows only defines signal() but exposes a
// PEXCEPTION_POINTERS context via _pxcptinfoptrs
// this is enough to simulate a SA_SIGINFO signal processing

__declspec(thread) static sigaction_t handlers[NSIG] = {0};

void sigfunc(int sig)
{
	siginfo_t info;
	ucontext_t ctx;
	PEXCEPTION_POINTERS e = (PEXCEPTION_POINTERS)_pxcptinfoptrs;
	sigaction_t handler = handlers[sig];
	if (!handler)
		return;

	memset( &info, 0, sizeof(info) );
	memset( &ctx, 0, sizeof(ctx) );

	info.si_code = e->ExceptionRecord->ExceptionCode;
	info.si_addr = e->ExceptionRecord->ExceptionAddress;
	info.si_signo = sig;
	ctx.uc_mcontext = *e->ContextRecord;
	handler( sig, &info, &ctx );
}

void signal2(int sig, sigaction_t handler)
{
	handlers[sig] = handler;
	signal(sig, sigfunc);
}


typedef struct
{
	CONTEXT ctx;
	HANDLE thread;
	volatile long sync;
} thread_context_loader;

DWORD WINAPI context_switcher(void* pctx)
{
	thread_context_loader *ctx = (thread_context_loader*)(pctx);
	int success = FALSE;
	while (InterlockedCompareExchange( &ctx->sync, 1, 1 ) != 1)
			SwitchToThread();

	if (SuspendThread( ctx->thread ) != -1)
	{
		InterlockedExchange(&ctx->sync, 0);
		if (SetThreadContext( ctx->thread, &ctx->ctx ) == TRUE)
			success = TRUE;
		ResumeThread( ctx->thread );
	} else
		InterlockedExchange(&ctx->sync, 0);
	CloseHandle( ctx->thread );		
	return success;
}

DWORD WINAPI context_dumper(void* pctx)
{
	thread_context_loader *ctx = (thread_context_loader*)(pctx);
	int success = FALSE;
	while (InterlockedCompareExchange( &ctx->sync, 1, 1 ) != 1)
			SwitchToThread();
	if (SuspendThread( ctx->thread ) != -1)
	{
		if (GetThreadContext( ctx->thread, &ctx->ctx ) == TRUE)
			success = TRUE;
		ResumeThread( ctx->thread );
	}
	CloseHandle( ctx->thread );
	InterlockedExchange(&ctx->sync, 0);
	return success;
}

int setcontext(const ucontext_t *ucp)
{
	HANDLE thread;
	thread_context_loader ctx;
	DWORD exit_code;

	ctx.ctx = ucp->uc_mcontext;
	DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), 
		GetCurrentProcess(), &ctx.thread, 0, FALSE, DUPLICATE_SAME_ACCESS );
	InterlockedExchange( &ctx.sync, 0 );
	thread = CreateThread( 0, 0, context_switcher, &ctx, 0, 0 );
	InterlockedExchange( &ctx.sync, 1 );
	while (ctx.sync == 1) { /* thread is guaranted to suspend when in here */ }

	if (GetExitCodeThread(thread, &exit_code) != TRUE)
		return -1;
	return (exit_code == TRUE) ? 0: -1;
}

int getcontext(ucontext_t *ucp)
{
	HANDLE thread;
	thread_context_loader ctx;
	DWORD exit_code;

	ctx.ctx.ContextFlags = CONTEXT_FULL;
	DuplicateHandle( GetCurrentProcess(), GetCurrentThread(), 
		GetCurrentProcess(), &ctx.thread, 0, FALSE, DUPLICATE_SAME_ACCESS );
	InterlockedExchange( &ctx.sync, 0 );
	thread = CreateThread( 0, 0, context_dumper, &ctx, 0, 0 );
	InterlockedExchange( &ctx.sync, 1 );
	while (ctx.sync == 1) { /* thread is guaranted to suspend when in here */ }
	if (GetExitCodeThread(thread, &exit_code) != TRUE)
		return -1;

	ucp->uc_mcontext = ctx.ctx;
	return (exit_code == TRUE) ? 0: -1;
}

int makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
{
	int i;
    va_list ap;
	char *sp;

	/* Stack grows down */
	sp = (char *) (size_t) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;	

	/* Reserve stack space for the arguments (maximum possible: argc*(8 bytes per argument)) */
	sp -= argc*8;

	if ( sp < (char *)ucp->uc_stack.ss_sp) {
		/* errno = ENOMEM;*/
		return -1;
	}

	/* Set the instruction and the stack pointer */
	ucp->uc_mcontext.Eip = PtrToUlong( func );
	ucp->uc_mcontext.Esp = PtrToUlong( sp - 4 );

	/* Save/Restore the full machine context */
	ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;

	/* Copy the arguments */
	va_start (ap, argc);
	for (i=0; i<argc; i++) {
		memcpy(sp, ap, 8);
		ap +=8;
		sp += 8;
	}
	va_end(ap);

	return 0;
}


int swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	int ret;

	if ((oucp == NULL) || (ucp == NULL)) {
		/*errno = EINVAL;*/
		return -1;
	}

	ret = getcontext(oucp);
	if (ret == 0) {
		ret = setcontext(ucp);
	}
	return ret;
}

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
