#include "loader_raw.h"
#include "Util.h"
#include <stdio.h>
#include "MemMap.h"
#include <assert.h>

#include <sys/types.h>

#ifdef WIN32
#include <io.h>
#define read _read
#define lseek _lseek
#define open _open
#define close _close
// #define filelength _filelength
#else
#include <unistd.h>
#endif

// raw files are always valid
bool loader_raw::is_valid(int /*fd*/)
{
	return true;
}

struct stream_raw
{
	typedef int context;
	static void process(memory_block *b, char *mem, int sz, context ctx )
	{
		if ((b->flags & memory_block::PAGE_INVALID) == 0)
		{
			if (read( ctx, mem, sz ) != sz) // read
			{
				// problem
			}
			b->dirty();
		} else
		{
			// problem
		}
	}
};

// load hint not needed as .nds contains both arm7 and arm9
bool loader_raw::load(int fd, util::load_result &res, util::load_hint lh)
{
	long sz = filelength(fd);
	switch (lh)
	{
	case util::LH_ARM9:
	case util::LH_RAW9:
		memory_map<_ARM9>::process_memory<stream_raw>(0x02000000, sz, fd);
		res.arm9_entry = 0x02000000;
		res.flags = util::LOAD_ARM9;
		return true;
	case util::LH_ARM7:
	case util::LH_RAW7:
		memory_map<_ARM7>::process_memory<stream_raw>(0x03800000, sz, fd);
		res.arm7_entry = 0x03800000;
		res.flags = util::LOAD_ARM7;
		return true;
	}
	return false;
}
