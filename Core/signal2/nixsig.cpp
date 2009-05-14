#include "nixsig.h"
#include <signal.h>

void signal2(int sig, sigaction_t handler)
{
	struct sigaction sa;
	struct sigaction old;
	sigaction( sig, &sa, &old );
}

