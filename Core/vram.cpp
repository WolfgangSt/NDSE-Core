#include "MemMap.h"
#include "Logging.h"
#include "vram.h"

unsigned long *reg_at(unsigned long offset)
{
	return (unsigned long*)&memory::registers9_1.blocks[
		offset / PAGING::SIZE].mem[offset & PAGING::ADDRESS_MASK
		];
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



// [16:02] <mukunda> the banks with the higher letters have priority i believe

void invalid_vcnt(unsigned long idx, unsigned long cnt)
{
	logging<_ARM9>::logf("Warning: invalid VRAMCNT specified for BANK-%c (0x%02X)", 'A' + idx, cnt);
}

void map_vcnt(unsigned long cnt, unsigned long mst_mask, unsigned long ofs_mask, unsigned long idx)
{
	unsigned long mst, ofs;
	static const unsigned long lut_fg[4] = {0x0, 0x4000, 0x10000, 0x14000};
	if (cnt & (1 << 7))
	{ 
		//enable
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
			case 1: return map_bank<_ARM9>(idx, &memory::vram_g, 0x06000000 | lut_fg[ofs]);
			case 2: return map_bank<_ARM9>(idx, &memory::vram_g, 0x06400000 | lut_fg[ofs]);
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
		// disable
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

void vram::remap()
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

void vram::init()
{
	memset( cur_maps<_ARM7>::mapping, 0, sizeof(cur_maps<_ARM7>::mapping) );
	memset( cur_maps<_ARM9>::mapping, 0, sizeof(cur_maps<_ARM9>::mapping) );
}