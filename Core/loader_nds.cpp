#include "loader_nds.h"
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
#else
#include <unistd.h>
#endif

struct card_rom_region
{
	unsigned long offset;
	unsigned long length;
};

#include <pshpack1.h>
struct ndsheader_main
{
	char game_name[12];
	unsigned long game_code;
	unsigned short maker_code;
	unsigned char product_id;
	unsigned char device_type;
	unsigned char device_size;
	unsigned char reserved_a[9];
	unsigned char game_version;
	unsigned char property_;
	unsigned long main_rom_offset;
	unsigned long main_entry_address;
	unsigned long main_ram_address;
	unsigned long main_size;
	unsigned long sub_rom_offset;
	unsigned long sub_entry_address;
	unsigned long sub_ram_address;
	unsigned long sub_size;

	card_rom_region fnt;
	card_rom_region fat;
	card_rom_region main_ovt;
	card_rom_region sub_ovt;

	unsigned char rom_param_a[8];
	unsigned long banner_offset;
	unsigned short secure_crc;
	unsigned char rom_param_b[2];
	unsigned long main_autoload_done;
	unsigned long sub_autoload_done;
	unsigned char rom_param_c[8];

	unsigned long rom_size;
    unsigned long header_size;
	unsigned char reserved_a2[56];

	unsigned char nintendo_logo[156];
	unsigned short nintendo_logo_crc16;
};

struct ndsheader_sub
{
	unsigned short header_crc16;

	unsigned long debug_rom_source;
	unsigned long debug_rom_size;
	unsigned long debug_rom_destination;
	unsigned long offset_0x16C;
	char zero[0x90];

};

struct ndsheader
{
	ndsheader_main main;
	ndsheader_sub sub;
};
#include <poppack.h>


struct max_size
{
	unsigned long sz;
	void _max(unsigned long sz)
	{
		if (sz > this->sz)
			this->sz = sz;
	}
	void _max(const card_rom_region &reg)
	{
		_max(reg.offset + reg.length);
	}
};

bool loader_nds::is_valid(int fd)
{
	ndsheader hdr;
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		return false;
	lseek( fd, 0, SEEK_SET );

	max_size sz;
	sz.sz = 0;
	sz._max( hdr.main.main_rom_offset + hdr.main.main_size );
    sz._max( hdr.main.sub_rom_offset  + hdr.main.sub_size );
    sz._max( hdr.main.fnt );
    sz._max( hdr.main.fat );
    sz._max( hdr.main.main_ovt );
    sz._max( hdr.main.sub_ovt );
    sz._max( hdr.main.header_size );
    sz._max( hdr.main.rom_size );

	if (sz.sz > hdr.main.rom_size)
		return false;

	return (util::get_crc16( 0xFFFF, (char*)&hdr.main, sizeof(hdr.main) ) 
		    == hdr.sub.header_crc16) &&
		   (util::get_crc16( 0xFFFF, (char*)&hdr.main.nintendo_logo, 
		     sizeof(hdr.main.nintendo_logo))
		    == hdr.main.nintendo_logo_crc16);
}

struct stream_nds
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
bool loader_nds::load(int fd, util::load_result &res, util::load_hint /*lh*/)
{
	ndsheader hdr;

	// ERR SHIT sizeof(hdr aint 512 ....)
	assert(sizeof(hdr) == 512);
	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		return false;

	lseek( fd, hdr.main.main_rom_offset, SEEK_SET );
	memory_map<_ARM9>::process_memory<stream_nds>( 
		hdr.main.main_ram_address,
		hdr.main.main_size, 
		fd );
	res.arm9_entry = hdr.main.main_entry_address;

	lseek( fd, hdr.main.sub_rom_offset, SEEK_SET );
	memory_map<_ARM7>::process_memory<stream_nds>( 
		hdr.main.sub_ram_address,
		hdr.main.sub_size, 
		fd );
	res.arm7_entry = hdr.main.sub_entry_address;

	res.flags = util::LOAD_ARM7 | util::LOAD_ARM9;
	return true;
}
