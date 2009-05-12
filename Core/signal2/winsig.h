#include <windows.h>
#include <signal.h>

union sigval {
	int sival_int;
	void *sival_ptr;
};

typedef int pid_t;
typedef int uid_t;
typedef unsigned long sigset_t;

typedef struct {
	int si_signo;
	int si_code;
	union sigval si_value;
	int si_errno;
	pid_t si_pid;
	uid_t si_uid;
	void *si_addr;
	int si_status;
	int si_band;
} siginfo_t;

typedef CONTEXT mcontext_t;
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


typedef void (*sigaction_t)(int, siginfo_t *, void *);
void signal2(int sig, sigaction_t handler);

// #include <ucontext.h>
int getcontext(ucontext_t *ucp);
int setcontext(const ucontext_t *ucp);
int makecontext(ucontext_t *, void (*)(), int, ...);
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);



// #include <sys/mman.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
int mprotect(const void *addr, size_t len, int prot);

// #include <asm/cachectl.h>

#define ICACHE 1
int cacheflush(char *addr, int nbytes, int cache);
