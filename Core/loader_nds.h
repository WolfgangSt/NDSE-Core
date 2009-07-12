#ifndef _LOADER_NDS_
#define _LOADER_NDS_

#include "Util.h"

class loader_nds
{
public:
	static bool is_valid(int fd);
	static bool load(int fd, util::load_result &res, util::load_hint lh);
};

#endif
