#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include "NDSE.h"

template <typename T>
class logging
{
public:
	
	static log_callback cb;

	static void log(const char *msg)
	{
		if (cb)
			cb(msg);
		else std::cout << msg << "\n";
	}

	static void logf(const char *fmt, ...)
	{
		char buffer[256];
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf( buffer, fmt, argptr ); 
		va_end( argptr );
		log( buffer );
	}

	static void logf_assert(bool b, const char *fmt, ...)
	{
		if (!b)
			return;
		char buffer[256];
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf( buffer, fmt, argptr ); 
		va_end( argptr );
		log( buffer );
	}

};

template <typename T> log_callback logging<T>::cb;

#endif
