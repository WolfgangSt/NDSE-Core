#include "nixsig.h"
#include <signal.h>
#include <map>
#include <QThread>
#include <string.h>
#include <iostream>

struct siglist
{
	sigaction_t sig[NSIG];
	siglist()
	{
		memset(sig, 0, sizeof(sig));
	}
};

struct original_action
{
	bool override;
	struct sigaction original;
	original_action()
	{
		override = false;
	}
};

typedef std::map<Qt::HANDLE, siglist> SigMap;
static SigMap sigs;
static original_action original[NSIG];


static Qt::HANDLE current_thread()
{
	return QThread::currentThreadId(); 
}

static void global_handler(int sig, siginfo_t *info, void *ctx)
{
	SigMap::const_iterator it = sigs.find(current_thread());
	if (it != sigs.end())
	{
		// do the callback if specified
		sigaction_t cb = it->second.sig[sig];
		if (cb)
			return cb(sig, info, ctx);
	}
	// invoke the original handler
	original[sig].original
}

void signal2(int sig, sigaction_t handler)
{
	sigs[current_thread()].sig[sig] = handler;
	original_action &oa = original[sig];
	if (!oa.override)
	{
		oa.override = true;
		struct sigaction sa;
		sa.sa_sigaction = global_handler;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, sig);
		sigaction( sig, &sa, &oa.original );
	}
}

