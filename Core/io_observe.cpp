#include "io_observe.h"
#include "Logging.h"
#include "MemMap.h"

// move this to DMA threads (= parallelize)
// alternativly use memcpy's if instant DMA is wanted

io_observer::reactor_page io_observer::reactor_data9[memory::REGISTERS9_1::PAGES];
io_observer::reactor_page io_observer::reactor_data7[memory::REGISTERS7_1::PAGES];

unsigned long *reg_at(unsigned long offset)
{
	return (unsigned long*)&memory::registers9_1.blocks[
		offset / PAGING::SIZE].mem[offset & PAGING::ADDRESS_MASK
		];
}

void start_dma3()
{
	unsigned long *_src  = reg_at(0xD4);
	unsigned long *_dst  = reg_at(0xD8);
	unsigned long *_ctrl = reg_at(0xDC);
	unsigned long subaddr_src;
	unsigned long subaddr_dst;

	// bruteforce method
	logging<_ARM9>::logf("DMA started [%08X => %08X]", *_src, *_dst);
	while ((*_ctrl & 0x801FFFFF) > 0x80000000)
	{
		unsigned long src = *_src;
		unsigned long dst = *_dst;
		unsigned long ctrl = *_ctrl;
		unsigned long sz = 4;

		memory_block *bs = memory_map<_ARM9>::addr2page(src);
		memory_block *bd = memory_map<_ARM9>::addr2page(dst);
		//logging<_ARM9>::logf("DMA iteration: %08X => %08X", src, dst);
		if (bs->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		{
			// problem here
			logging<_DEFAULT>::logf("Invalid source for DMA: %08X", src);
			if (!(ctrl & (1 << 26)))
				sz = 2;
			goto next;
		}
		if (bd->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		{
			// problem here
			logging<_DEFAULT>::logf("Invalid destination for DMA: %08X", dst);
			if (!(ctrl & (1 << 26)))
				sz = 2;
			goto next;
		}

		subaddr_src = src & PAGING::ADDRESS_MASK;
		subaddr_dst = dst & PAGING::ADDRESS_MASK;
		

		if (ctrl & (1 << 26))
		{
			// transfer 32bits
			if ((subaddr_src | subaddr_dst) & 3) // check alignment
			{
align_error:
				logging<_DEFAULT>::logf("Invalid alignment during DMA: %08X => %08X", src, dst);
				goto next;
			}
			*((unsigned long*)&bd->mem[subaddr_dst]) = *((unsigned long*)&bs->mem[subaddr_src]);
		} else
		{
			sz = 2;
			// transfer 16 bits
			if ((subaddr_src | subaddr_dst) & 1) // check alignment
				goto align_error;
			*((unsigned short*)&bd->mem[subaddr_dst]) = *((unsigned short*)&bs->mem[subaddr_src]);
		}
		bd->flags |= memory_block::PAGE_DIRTY;
		// dont invoke writecb here since this would retrigger DMA
next:
		switch ((ctrl >> 21) & 0x3)
		{

		case 0x3:
		case 0x0: // d++
			*_dst += sz; 
			break; 
		case 0x1: // d--
			*_dst -= sz;
			break; 
		//case 0x2: break; // fixed
		}

		switch ((ctrl >> 23) & 0x3)
		{
		case 0x0: // s++
			*_src += sz; 
			break; 
		case 0x1: // s--
			*_src -= sz;
			break;
		case 0x3:
			// problem
			logging<_DEFAULT>::logf("Invalid update mode during DMA");
			break;
		}

		*_ctrl -= 1;

		//SwitchToThread();
	};
	logging<_DEFAULT>::logf("DMA finished");
	*_ctrl = 0;
}

template <typename T> struct cur_maps
{
	static unsigned long mapping[10];
};

template <typename T> unsigned long cur_maps<T>::mapping[10];

template <typename T>
void unmap_bank(unsigned long idx, memory_region_base *b)
{
	if (cur_maps<T>::mapping[idx] != 0)
	{
		logging<T>::logf("Unmapping %s", b->name);
		memory_map<T>::unmap( b );
		cur_maps<T>::mapping[idx] = 0;
	}
}

template <typename T>
void map_bank(unsigned long idx, memory_region_base *b, unsigned long addr)
{
	assert(addr != 0);
	if (cur_maps<T>::mapping[idx] != addr)
	{
		unmap_bank<T>(idx, b);
		logging<T>::logf("Mapping %s to %08X", b->name, addr);
		memory_map<T>::map_region( b, addr >> PAGING::SIZE_BITS );
		cur_maps<T>::mapping[idx] = addr;
	}
}



void invalid_vcnt(unsigned long idx, unsigned long cnt)
{
	logging<_ARM9>::logf("Warning: invalid VRAMCNT specified for BANK-%c (0x%02X)", 'A' + idx, cnt);
}



// [16:02] <mukunda> the banks with the higher letters have priority i believe
void map_vcnt(unsigned long cnt, unsigned long mst_mask, unsigned long ofs_mask, unsigned long idx)
{
	unsigned long mst, ofs;
	static const unsigned long lut_fg[4] = {0x0, 0x4000, 0x10000, 0x14000};
	if (cnt & (1 << 7)) \
	{ 
		mst = cnt & (mst_mask);
		ofs = (cnt & (ofs_mask));
		if (cnt & ~(ofs_mask | mst_mask | 0x80))
			invalid_vcnt(idx, cnt);
		switch (idx)
		{
		case 0: // BANK-A
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_a, 0x06800000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_a, 0x06000000 | (ofs << 14));
			case 2: return map_bank<_ARM9>(idx, &memory::vram_a, 0x06400000 | (ofs << 14));
			//case 3: texture slot
			}
			break;
		case 1: // BANK-B
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_b, 0x06820000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_b, 0x06000000 | (ofs << 14));
			case 2: return map_bank<_ARM9>(idx, &memory::vram_b, 0x06400000 | (ofs << 14));
			//case 3: texture slot
			}
			break;
		case 2: // BANK-C
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_c, 0x06840000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_c, 0x06000000 | (ofs << 14));
			case 2: return map_bank<_ARM7>(idx, &memory::vram_c, 0x06000000 | (ofs << 14));
			//case 3: texture slot
			case 4: return map_bank<_ARM9>(idx, &memory::vram_c, 0x06200000);
			}
		case 3: // BANK-D
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_d, 0x06860000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_d, 0x06000000 | (ofs << 14));
			case 2: return map_bank<_ARM7>(idx, &memory::vram_d, 0x06000000 | (ofs << 14));
			//case 3: texture slot
			case 4: return map_bank<_ARM9>(idx, &memory::vram_d, 0x06600000);
			}
			break;
		case 4: // BANK-E
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_e, 0x06880000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_e, 0x06000000);
			case 2: return map_bank<_ARM9>(idx, &memory::vram_e, 0x06400000);
			//case 3: texture slot
			//case 4: texture slot
			}
			break;
		case 5: // BANK-F
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_f, 0x06890000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_f, 0x06000000 | lut_fg[ofs]);
			case 2: return map_bank<_ARM9>(idx, &memory::vram_f, 0x06400000 | lut_fg[ofs]);
			//case 3: texture slot
			//case 4: texture slot
			//case 5: texture slot
			}
			break;
		case 6: // BANK-G
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_g, 0x06894000);
			case 2: return map_bank<_ARM9>(idx, &memory::vram_g, 0x06000000 | lut_fg[ofs]);
			case 3: return map_bank<_ARM9>(idx, &memory::vram_g, 0x06400000 | lut_fg[ofs]);
			//case 3: texture slot
			//case 4: texture slot
			//case 5: texture slot
			}
			break;
		case 8: // BANK-H
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_h, 0x06898000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_h, 0x06200000);
			//case 2: texture slot
			}
			break;
		case 9: // BANK-I
			switch (mst)
			{
			case 0: return map_bank<_ARM9>(idx, &memory::vram_i, 0x068A0000);
			case 1: return map_bank<_ARM9>(idx, &memory::vram_i, 0x06208000);
			case 2: return map_bank<_ARM9>(idx, &memory::vram_i, 0x06600000);
			}
			//case 3: texture slot
			break;
		}
		invalid_vcnt(idx, cnt);
	} else 
	{
		static memory_region_base* const banks[9] = {
			&memory::vram_a,
			&memory::vram_b,
			&memory::vram_c,
			&memory::vram_d,
			&memory::vram_e,
			&memory::vram_f,
			&memory::vram_g,
			&memory::vram_h,
			&memory::vram_i
		};
		unmap_bank<_ARM7>(idx, banks[idx]);
		unmap_bank<_ARM9>(idx, banks[idx]);
	}
}

void remap_vmem()
{
	unsigned char* vramcnt = (unsigned char*)reg_at(0x240);
	map_vcnt(vramcnt[0], 0x3, 0x18, 0); // BANK-A
	map_vcnt(vramcnt[1], 0x3, 0x18, 1); // BANK-B
	map_vcnt(vramcnt[2], 0x7, 0x18, 2); // BANK-C
	map_vcnt(vramcnt[3], 0x7, 0x18, 3); // BANK-D
	map_vcnt(vramcnt[4], 0x7, 0x00, 4); // BANK-E
	map_vcnt(vramcnt[5], 0x7, 0x18, 5); // BANK-F
	map_vcnt(vramcnt[6], 0x7, 0x18, 6); // BANK-G
	//map_wcnt(vramcnt[7]); // WRAM
	map_vcnt(vramcnt[8], 0x3, 0x00, 7); // BANK-H
	map_vcnt(vramcnt[9], 0x3, 0x00, 8); // BANK-I
}


void fake_ipc_sync()
{
	unsigned long *pfifo = reinterpret_cast<unsigned long*>(&memory::registers9_1.blocks[0].mem[0x180]);
	*pfifo = (*pfifo & ~0xF) | (((*pfifo >> 8) + 1) & 0xF);
}


void io_observer::process9()
{
	bool remap_v = false;
	for (unsigned int i = 0; i < memory::registers9_1.pages; i++)
	{
		memory_block &b = memory::registers9_1.blocks[i];
		reactor_page &p = reactor_data9[i];
		if (!b.react())
			continue;

		// pull the new data
		unsigned long *bmem = (unsigned long*)&b.mem;
		unsigned long addr_base = i * PAGING::SIZE;
		for (unsigned int j = 0; j < PAGING::SIZE/sizeof(unsigned long); j++)
		{
			if (p[j] == bmem[j])
				continue;

			const char *name = "<unknown>";
			unsigned long addr = addr_base + j * sizeof(unsigned long);
			switch (addr)
			{
			case 0x00:
				name ="[DISPCNT] 2D Graphics Engine A display control";
				break;
			case 0x04:
				name = "[DISPSTAT/VCOUNT] Display Status / V count comparison";
				break;
			case 0x08:
				name = "[BG0CNT/BB1CNT] 2D Graphics Engine A BG0/BG1 control";
				break;
			case 0x0C:
				name = "[BG2CNT/BB3CNT] 2D Graphics Engine A BG2/BG3 control";
				break;
			case 0x10:
				name = "[BG0HOFS/BG0VOFS] 2D Graphics Engine A BG0 offsets";
				break;
			case 0x30:
				name = "[BG3PA/BG3PB] 2D Graphics Engine A BG3 affine transformation (dx/dmx)";
				break;
			case 0x34:
				name = "[BG3PC/BG3PD] 2D Graphics Engine A BG3 affine transformation (dy/dmy)";
				break;
			case 0x38:
				name = "[BG3X] 2D Graphics Engine A BG3 reference start point (x coordinate)";
				break;
			case 0x3C:
				name = "[BG3Y] 2D Graphics Engine A BG3 reference start point (y coordinate)";
				break;
			case 0xD4:
				name = "[DMA3SAD] DMA3 source address";
				break;
			case 0xD8:
				name = "[DMA3DAD] DMA3 destination address";
				break;
			case 0xDC:
				name = "[DMA3CNT] DMA3 control";
				start_dma3();
				break;
			case 0x180:
				name = "IPC Synchronize Register";
				// sync to arm7 IPC
				// = bmem[j]
				//*(unsigned short*)(memory::registers7_1.start->mem + 0x180) =
				//*(unsigned short*)(memory::registers9_1.start->mem + 0x180);
				fake_ipc_sync();
				break;
			case 0x184:
				name = "IPC Fifo Control Register";
				break;
			case 0x1C0:
				name = "[SPICNT] SPI Bus Control/Status Register";
				break;
			case 0x204:
				name = "[EXMEMCNT] External memory control";
				break;
			case 0x208:
				name = "[IME] Interrupt master flag";
				break;
			case 0x210:
				name = "[IE] Interrupt enable flag";
				break;
			case 0x214:
				name = "[IF] Interrupt request flag";
				break;
			case 0x240:
				name = "[VRAMCNT] RAM bank control 0";
				remap_v = true;
				break;
			case 0x244:
				name = "[WRAMCNT] RAM bank control 1";
				remap_v = true;
				break;
			case 0x248:
				name = "[VRAM_HI_CNT] RAM bank control 2";
				remap_v = true;
				break;

			case 0x304:
				name = "[POWCNT] Power control";
				break;
			case 0x1000:
				name = "[DB_DISPCNT] 2D Graphics Engine B display control";
				break;
			case 0x1008:
				name = "[DB_BG0CNT/DB_BG1CNT] 2D Graphics Engine B BG0/BG1 control";
				break;
			case 0x100C:
				name = "[DB_BG2CNT/DB_BG3CNT] 2D Graphics Engine B BG2/BG3 control";
				break;
			}
			
			logging<_DEFAULT>::logf("IO reactor thread detected change at ARM9:%08X from %08X to %08X [%s]", 
				addr, p[j], bmem[j], name);

			// update
			p[j] = bmem[j];
		}
	}
	if (remap_v)
		remap_vmem();
}

void io_observer::process7()
{
	bool remap_v = false;
	for (unsigned int i = 0; i < memory::registers7_1.pages; i++)
	{
		memory_block &b = memory::registers7_1.blocks[i];
		reactor_page &p = reactor_data7[i];
		if (!b.react())
			continue;

		// pull the new data
		unsigned long *bmem = (unsigned long*)&b.mem;
		unsigned long addr_base = i * PAGING::SIZE;
		for (unsigned int j = 0; j < PAGING::SIZE/sizeof(unsigned long); j++)
		{
			if (p[j] == bmem[j])
				continue;

			const char *name = "<unknown>";
			unsigned long addr = addr_base + j * sizeof(unsigned long);
			
			/*
			switch (addr)
			{
			// 0x208 = IME
			// 0x1C0 = SPICNT
			}
			*/
			
			logging<_DEFAULT>::logf("IO reactor thread detected change at ARM7:%08X from %08X to %08X [%s]", 
				addr, p[j], bmem[j], name);

			// update
			p[j] = bmem[j];
		}
	}
	if (remap_v)
		remap_vmem();
}

void io_observer::io_write9(memory_block*)
{
	process9();
}

void io_observer::io_write7(memory_block*)
{
	process7();
}



void io_observer::init()
{
	memset( reactor_data9, 0, sizeof(reactor_data9) );
	memset( reactor_data7, 0, sizeof(reactor_data7) );
	//reactor_data[0][0x180 >> 2] = 0xEFEFEFEF; // fifo fake sync init

	memset( cur_maps<_ARM7>::mapping, 0, sizeof(cur_maps<_ARM7>::mapping) );
	memset( cur_maps<_ARM9>::mapping, 0, sizeof(cur_maps<_ARM9>::mapping) );

	// register direct callbacks needed to properly sync
	// arm9 pages init
	for (unsigned long i = 0; i < memory::registers9_1.pages; i++)
	{
		memory_block &b = memory::registers9_1.blocks[i];
		b.writecb = io_write9;
	}

	// arm7 pages init
	for (unsigned long i = 0; i < memory::registers7_1.pages; i++)
	{
		memory_block &b = memory::registers7_1.blocks[i];
		b.writecb = io_write7;
	}

	//static io_observer i;
}
