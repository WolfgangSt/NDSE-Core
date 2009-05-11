#ifndef _LOADER_ELF_
#define _LOADER_ELF_

#include "Util.h"

class loader_elf
{
public:
	static bool is_valid(int fd);
	static bool load(int fd, util::load_result &res);
	static void init();
};

#endif
