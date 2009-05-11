#ifndef _JITCODE_H_
#define _JITCODE_H_

struct jit_code
{
	char *data;
	char *original;
	unsigned long len;
};

#endif
