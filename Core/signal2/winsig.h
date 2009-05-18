#include <windows.h>
#include <signal.h>

struct mcontext_t
{
	CONTEXT ctx;
	unsigned long fs0;
};

typedef unsigned long __sigset_t;

typedef struct __stack {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} stack_t;

typedef struct __ucontext {
	unsigned long int	uc_flags;
	struct __ucontext	*uc_link;
	stack_t				uc_stack;
	mcontext_t			uc_mcontext; 
	__sigset_t			uc_sigmask;
} ucontext_t;

// #include <sys/mman.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
int mprotect(const void *addr, size_t len, int prot);

// #include <asm/cachectl.h>

#define ICACHE 1
int cacheflush(char *addr, int nbytes, int cache);


////////////////////////////////////////////////////////////////////////////////
// New Platform dependant API


