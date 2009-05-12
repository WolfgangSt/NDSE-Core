#ifndef _PHYSMEM_H_
#define _PHYSMEM_H_

#include "MemRegion.h"

// physical memory
struct memory 
{
	static memory_region< PAGING >          null_region;
	static memory_region< PAGING >          hle_bios;    // 0xEFEF0000

	static memory_region< PAGING::KB<64> >  accessory_ram; 
	static memory_region< PAGING::KB<32> >  accessory_rom; // 32MB?
	static memory_region< PAGING::KB<2> >   oam_ab;
	static memory_region< PAGING::KB<128> > obj_vram_b;
	static memory_region< PAGING::KB<256> > obj_vram_a;
	static memory_region< PAGING::KB<128> > bg_vram_b;
	static memory_region< PAGING::KB<512> > bg_vram_a;
	static memory_region< PAGING::B<2048> > palettes;

	static memory_region< PAGING::KB<128> > vram_a;
	static memory_region< PAGING::KB<128> > vram_b;
	static memory_region< PAGING::KB<128> > vram_c;
	static memory_region< PAGING::KB<128> > vram_d;
	static memory_region< PAGING::KB< 64> > vram_e;
	static memory_region< PAGING::KB< 16> > vram_f;
	static memory_region< PAGING::KB< 16> > vram_g;
	static memory_region< PAGING::KB< 32> > vram_h;
	static memory_region< PAGING::KB< 16> > vram_i;

	// io registers
	static memory_region< PAGING::KB<32> >  arm7_shared;
	static memory_region< PAGING::KB<16> >  data_tcm;
	static memory_region< PAGING::B<0x3E0000> > ram;
	static memory_region< PAGING::KB<32> >  inst_tcm;
	static memory_region< PAGING::KB<256> > exp_wram; // probably same as ram_ext???
	// wireless communication wait state 1
	// wireless communication wait state 0
	static memory_region< PAGING::KB<64> >  arm7_wram;
	static memory_region< PAGING::B<0x10400> >  system_rom;

	static memory_region< PAGING::KB<16> >  arm7_sys_rom;
	static memory_region< PAGING::KB<8>  >  arm9_sys_rom;
	
	typedef memory_region< PAGING::KB<8> > REGISTERS1;
	static REGISTERS1                       registers1; // 0x04000000 - 0x04002000
	static memory_region< PAGING::B<512> >  registers2; // 0x04001000
	static memory_region< PAGING::B<512> >  registers3; // 0x04100000

	static memory_region< PAGING::B<4096> > cart_header; // 0x027FF000

	enum { NUM_REGIONS = 26 };
	static const memory_region_base* regions[NUM_REGIONS];

	static memory_block* get_nullblock()
	{
		return &null_region.blocks[0];
	}

	static const memory_region_base* get_region_for(memory_block *page)
	{
		for (int i = 0; i < NUM_REGIONS; i++)
			if (regions[i]->page_in_region( page ))
				return regions[i];
		return &null_region;
	}

	template <typename T> struct initializer { static void initialize_mapping(); };
};



#endif
