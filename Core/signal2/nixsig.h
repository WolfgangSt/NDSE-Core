#ifndef _NIXSIG2_H_
#define _NIXSIG2_H_

#include <signal.h>

typedef void (*sigaction_t)(int, siginfo_t *, void *);
void signal2(int sig, sigaction_t handler);

#endif


