#ifdef WIN32
#define STDCALL __stdcall 
#else
#define STDCALL
#endif

#ifndef EXPORT
	#ifdef WIN32
	#define IMPORT __declspec(dllimport)
	#else
	#define IMPORT
	#endif
#else
	#ifdef WIN32
	#define IMPORT EXPORT
	#else
	#define IMPORT __attribute__((visibility("default")))
	#endif
#endif

#include "forward.h"

typedef void (STDCALL *stream_callback)(memory_block*, char*, int, void*);
typedef void (STDCALL *log_callback)(const char* log);
typedef void (STDCALL *io_callback)(unsigned long addr, unsigned long *value);


