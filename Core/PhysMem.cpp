#include "PhysMem.h"
#include "MemMap.h"

#define _RGB(r,g,b) (r | (g << 8) | (b << 16))

memory_region< PAGING >          memory::null_region("(NULL)",                0x0000FF00, 0);
memory_region< PAGING >          memory::hle_bios("HLE BIOS", _RGB(199,199,23), 1);        // 0xEFEF0000
memory_region< PAGING >          memory::arm9_ipc("ARM9 IPC Transfer Region", 0x00000000, 1);

memory_region< PAGING::KB<64> >  memory::accessory_ram("DS Accessory RAM",    0x00000000, 1); 
memory_region< PAGING::KB<32> >  memory::accessory_rom("DS Accessorry ROM",   0x00000000, 1); // 32MB?
memory_region< PAGING::KB<2> >   memory::oam_ab("2D Graphics Engine A/B OAM", 0x00000000, 1);
memory_region< PAGING::KB<128> > memory::obj_vram_b("2D Graphics Engine B OBJ-VRAM", 0x00000000, 1);
memory_region< PAGING::KB<256> > memory::obj_vram_a("2D Graphics Engine A OBJ-VRAM", 0x00000000, 1);
memory_region< PAGING::KB<128> > memory::bg_vram_b("2D Graphics Engine B BG-VRAM",   0x00000000, 1);
memory_region< PAGING::KB<512> > memory::bg_vram_a("2D Graphics Engine A BG-VRAM",   0x00000000, 1);
memory_region< PAGING::B<2048> > memory::palettes("Palette Memory",                  0x00000000, 1);

// io registers
memory_region< PAGING::KB<32> >  memory::arm7_shared("Shared Internal Work RAM", 0x00000000, 1);
memory_region< PAGING::KB<16> >  memory::data_tcm("Data TCM",                    0x00C000C0, 1000);
memory_region< PAGING::MB<4> >   memory::ram("Main Memory",                  0x00FF0000, 1);
memory_region< PAGING::KB<32> >  memory::inst_tcm("Instruction TCM",             0x00C0C000, 1001);
memory_region< PAGING::KB<256> > memory::exp_wram("Internal Expanded Work RAM",  0x00000000, 1);
// wireless communication wait state 1
// wireless communication wait state 0
memory_region< PAGING::KB<64> >  memory::arm7_wram("ARM7 Exclusive Internal Work RAM", 0x00000000, 1);
memory_region< PAGING::B<0x10400> >  memory::system_rom("System ROM", 0x00000000, 1);

REGISTERS9_1                     memory::registers9_1("IO registers 9.0", 0x00000000, 1); // 0x04000000
memory_region< PAGING::B<512> >  memory::registers9_3("IO registers 9.2", 0x00000000, 1); // 0x04100000
memory::REGISTERS7_1             memory::registers7_1("IO registers 7.0", 0x00000000, 1); // 0x04000000

// VRAM banks
memory_region< PAGING::KB<128> > memory::vram_a("VRAM-A", 0x00000000, 2);
memory_region< PAGING::KB<128> > memory::vram_b("VRAM-B", 0x00000000, 3);
memory_region< PAGING::KB<128> > memory::vram_c("VRAM-C", 0x00000000, 4);
memory_region< PAGING::KB<128> > memory::vram_d("VRAM-D", 0x00000000, 5);
memory_region< PAGING::KB< 64> > memory::vram_e("VRAM-E", 0x00000000, 6);
memory_region< PAGING::KB< 16> > memory::vram_f("VRAM-F", 0x00000000, 7);
memory_region< PAGING::KB< 16> > memory::vram_g("VRAM-G", 0x00000000, 8);
memory_region< PAGING::KB< 32> > memory::vram_h("VRAM-H", 0x00000000, 9);
memory_region< PAGING::KB< 16> > memory::vram_i("VRAM-I", 0x00000000, 10);

const memory_region_base* memory::regions[memory::NUM_REGIONS] = { 
	&accessory_ram,
	&accessory_rom,

	&vram_a,
	&vram_b,
	&vram_c,
	&vram_d,
	&vram_e,
	&vram_f,
	&vram_g,
	&vram_h,
	&vram_i,

	&oam_ab,
	&obj_vram_b,
	&obj_vram_a,
	&bg_vram_b,
	&bg_vram_a,
	&palettes,
	&arm7_shared,
	&data_tcm,
	&ram,
	&inst_tcm,
	&exp_wram,
	&arm7_wram,
	&system_rom,
	&registers9_1,
	&registers7_1
};

// gotta figure the correct mappings somehow yet ...
template <> void memory::initializer<_ARM7>::initialize_mapping()
{
	memory_map<_ARM7>::init_null();
	memory_map<_ARM7>::map_region( &hle_bios,      PAGING::PAGES<0xEFEF0000>::PAGE );

	memory_map<_ARM7>::map_region( &accessory_ram, PAGING::REGION(0x0A000000, 0x0A100000) );
	memory_map<_ARM7>::map_region( &accessory_rom, PAGING::REGION(0x08000000, 0x0A000000) );
	memory_map<_ARM7>::map_region( &exp_wram,      PAGING::REGION(0x06000000, 0x07000000) );
	//memory_map<_ARM7>::map_region( &wcom_wait1,    PAGING::PAGES<0x04808000>::PAGE );
	//memory_map<_ARM7>::map_region( &wcom_wait0,    PAGING::PAGES<0x04800000>::PAGE );
	memory_map<_ARM7>::map_region( &registers7_1,  PAGING::PAGES<0x04000000>::PAGE );

	memory_map<_ARM7>::map_region( &arm7_wram,     PAGING::REGION(0x03800000, 0x04000000) );
	memory_map<_ARM7>::map_region( &arm7_shared,   PAGING::REGION(0x03000000, 0x03800000) );

			
			
			
	// bogus: this is not how spec says but how emulator does it
	// the emulated RAM is 0x3E0000
	// figure somehow how extended RAM behaves.
	// is it just larger or does it leave gap between 0x023E0000 and 0x2400000
	// needs testing on actual hardware ...
	memory_map<_ARM7>::map_region( &ram,           PAGING::REGION(0x02000000, 0x03000000) );

	memory_map<_ARM7>::map_region( &system_rom,    PAGING::REGION(0x00000000, 0x00010400) );
}


template <> void memory::initializer<_ARM9>::initialize_mapping()
{
	memory_map<_ARM9>::init_null();
	memory_map<_ARM9>::map_region( &hle_bios,      PAGING::PAGES<0xEFEF0000>::PAGE );

			
	memory_map<_ARM9>::map_region( &oam_ab,        PAGING::PAGES<0x07000000>::PAGE );
	memory_map<_ARM9>::map_region( &palettes,      PAGING::PAGES<0x05000000>::PAGE );
	memory_map<_ARM9>::map_region( &registers9_1,  PAGING::PAGES<0x04000000>::PAGE );
	memory_map<_ARM9>::map_region( &arm9_ipc,      PAGING::PAGES<0x04100000>::PAGE );

	memory_map<_ARM9>::map_region( &ram,           PAGING::REGION(0x02000000, 0x03000000) );
};



