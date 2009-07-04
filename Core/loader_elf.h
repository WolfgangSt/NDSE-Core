#ifndef _LOADER_ELF_
#define _LOADER_ELF_

#include "Util.h"

// loader is _not_ threadsafe!
class loader_elf
{
public:
	static bool is_valid(int fd);
	static bool load(int fd, util::load_result &res);

	// allows just loading debug infos from an .elf
	// this is useful for metrowerks concept where they generate a debug .elf
	// and a rom image
	static bool load_debug(int fd);
	static void init();
};

#endif
