#include "runner.h"

#ifdef WIN32
bool check_mem_access(void *start, size_t sz)
{
	enum { 
		PROT = PAGE_READONLY | 
			PAGE_READWRITE |  
			PAGE_EXECUTE_READ | 
			PAGE_EXECUTE_READWRITE
	};
	char *s = (char*)start;
	MEMORY_BASIC_INFORMATION mi;

	for (;;)
	{
		if (VirtualQuery( s, &mi, sizeof(mi) ) < sizeof(mi))
			return false;
		size_t r = mi.RegionSize - (s - (char*)mi.BaseAddress);
		if (!(mi.Protect & PROT))
			return false;
		if (r >= sz)
			return true;
		sz -= r;
		s  += r;
	}
}

#else
#include <setjmp.h>

unsigned char g_ucCalledByTestRoutine = 0;
jmp_buf g_pBuf;
void check_mem_access_handler(int nSig)
{
     if ( g_ucCalledByTestRoutine ) longjmp ( g_pBuf, 1);
}
bool check_mem_access(void *start, size_t sz)
{
    char* pc;
    void(*pPrev)(int sig);
    g_ucCalledByTestRoutine = 1;
    if(setjmp(g_pBuf))
         return true;
    pPrev = signal(SIGSEGV, check_mem_access_handler);
    pc = (char*) malloc ( sz );
    memcpy ( pc, start, sz);
    free(pc);
    g_ucCalledByTestRoutine = 0;
    signal ( SIGSEGV, pPrev );
    return false;
}

#endif

