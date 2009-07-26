#ifndef _LOADER_RAW_
#define _LOADER_RAW_

#include "Util.h"

class loader_raw
{
public:
	static bool is_valid(int fd);
	static bool load(int fd, util::load_result &res, util::load_hint lh);
};

#endif
