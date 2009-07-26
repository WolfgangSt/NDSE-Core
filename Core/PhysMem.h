#ifndef _PHYSMEM_H_
#define _PHYSMEM_H_

#include "MemRegion.h"
#include "IORegs.h"

// physical memory
struct memory 
{
	static memory_region< PAGING >          null_region;
	static memory_region< PAGING >          hle_bios;    // 0xEFEF0000
	static memory_region< PAGING >          arm9_ipc;    // 0x04100000 ipc transfer region

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
	static memory_region< PAGING::MB<4> > ram;


	static memory_region< PAGING::KB<32> >  inst_tcm;
	static memory_region< PAGING::KB<256> > exp_wram; // probably same as ram_ext???
	// wireless communication wait state 1
	// wireless communication wait state 0
	static memory_region< PAGING::KB<64> >  arm7_wram;
	static memory_region< PAGING::B<0x10400> >  system_rom;

	static memory_region< PAGING::KB<16> >  arm7_sys_rom;
	static memory_region< PAGING::KB<8>  >  arm9_sys_rom;
	
	
	// arm9 ioregs
	typedef memory_region< PAGING::KB<8> > REGISTERS9_1; // arm9 io regs
	static REGISTERS9_1                    registers9_1; // 0x04000000 - 0x04002000
	static memory_region< PAGING::B<512> > registers9_3; // 0x04100000

	// arm7 ioregs
	typedef memory_region< PAGING::KB<8> > REGISTERS7_1; // arm7 io regs
	static REGISTERS7_1                    registers7_1; // 0x04000000 - 0x04002000

	enum { NUM_REGIONS = 26 };
	static const memory_region_base* regions[NUM_REGIONS];

	static memory_block* get_nullblock()
	{
		return &null_region.blocks[0];
	}

	template <typename T> struct initializer { static void initialize_mapping(); };
};


#endif
