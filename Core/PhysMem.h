#ifndef _PHYSMEM_H_
#define _PHYSMEM_H_

#include "MemMap.h"

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
	

	static memory_region< PAGING::KB<8> >   registers1; // 0x04000000 - 0x04002000
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

	template <typename T> struct initializer {};
	template <> struct initializer<_ARM7>
	{
		// gotta figure the correct mappings somehow yet ...
		static void initialize_mapping()
		{
			memory_map<_ARM7>::init_null();

			memory_map<_ARM7>::map_region( &accessory_ram, PAGING::REGION(0x0A000000, 0x0A100000) );
			memory_map<_ARM7>::map_region( &accessory_rom, PAGING::REGION(0x08000000, 0x0A000000) );
			memory_map<_ARM7>::map_region( &exp_wram,      PAGING::REGION(0x06000000, 0x07000000) );
			//memory_map<_ARM7>::map_region( &wcom_wait1,    PAGING::PAGES<0x04808000>::PAGE );
			//memory_map<_ARM7>::map_region( &wcom_wait0,    PAGING::PAGES<0x04800000>::PAGE );
			//memory_map<_ARM7>::map_region( &ioregs,        PAGING::PAGES<0x04000000>::PAGE );

			memory_map<_ARM7>::map_region( &arm7_wram,     PAGING::REGION(0x03800000, 0x04000000) );
			memory_map<_ARM7>::map_region( &arm7_shared,   PAGING::REGION(0x03000000, 0x03800000) );
			
			
			
			// bogus: this is not how spec says but how emulator does it
			// the emulated RAM is 0x3E0000
			// figure somehow how extended RAM behaves.
			// is it just larger or does it leave gap between 0x023E0000 and 0x2400000
			// needs testing on actual hardware ...
			memory_map<_ARM7>::map_region( &ram,           PAGING::REGION(0x02000000, 0x02000000 + ram.SIZE) );

			memory_map<_ARM7>::map_region( &system_rom,    PAGING::REGION(0x00000000, 0x00010400) );
		}
	};
	template <> struct initializer<_ARM9>
	{
		static void initialize_mapping()
		{
			memory_map<_ARM9>::init_null();
			memory_map<_ARM9>::map_region( &hle_bios,      PAGING::PAGES<0xEFEF0000>::PAGE );

			
			memory_map<_ARM9>::map_region( &oam_ab,        PAGING::PAGES<0x07000000>::PAGE );
			memory_map<_ARM9>::map_region( &palettes,      PAGING::PAGES<0x05000000>::PAGE );
			memory_map<_ARM9>::map_region( &registers1,    PAGING::PAGES<0x04000000>::PAGE );
			
			
			//memory_map<_ARM9>::map_region( &data_tcm,      PAGING::REGION(0x027E0000, 0x02800000) );
			memory_map<_ARM9>::map_region( &ram,           PAGING::REGION(0x02000000, 0x02000000 + ram.SIZE) );
			//memory_map<_ARM9>::map_region( &ram,           PAGING::REGION(0x02400000, 0x02400000 + ram.SIZE) );
			memory_map<_ARM9>::map_region( &cart_header,   PAGING::PAGES<0x27FF000>::PAGE );
		}
	};
};


#endif
