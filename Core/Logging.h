#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include "NDSE.h"

typedef void (STDCALL *log_callback)(char* log);

template <typename T>
class logging
{
public:
	
	static log_callback cb;

	static void log(char *msg)
	{
		if (cb)
			cb(msg);
		else std::cout << msg << "\n";
	}

	static void logf(char *fmt, ...)
	{
		char buffer[256];
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf_s( buffer, sizeof(buffer), fmt, argptr ); 
		va_end( argptr );
		log( buffer );
	}

	static void logf_assert(bool b, char *fmt, ...)
	{
		if (!b)
			return;
		char buffer[256];
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf_s( buffer, sizeof(buffer), fmt, argptr ); 
		va_end( argptr );
		log( buffer );
	}

};

template <typename T> log_callback logging<T>::cb;

#endif
