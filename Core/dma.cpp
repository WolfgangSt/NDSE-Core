#include "dma.h"
#include "Logging.h"
#include "MemMap.h"
#include "HLE.h"

void start_dma(unsigned long *base)
{
	unsigned long *_src  = base;
	unsigned long *_dst  = base + 1;
	unsigned long *_ctrl = base + 2;
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
			unsigned long value;
			if (bs->flags & memory_block::PAGE_ACCESSHANDLER)
				value = HLE<_ARM9>::load32(src);
			else value = *((unsigned long*)&bs->mem[subaddr_src]);

			if (bd->flags & memory_block::PAGE_ACCESSHANDLER)
				HLE<_ARM9>::store32(dst, value);
			else
				*((unsigned long*)&bd->mem[subaddr_dst]) = value;
		} else
		{
			sz = 2;
			// transfer 16 bits
			if ((subaddr_src | subaddr_dst) & 1) // check alignment
				goto align_error;

			unsigned long value;
			if (bs->flags & memory_block::PAGE_ACCESSHANDLER)
				value = HLE<_ARM9>::load16u(src);
			else value = *((unsigned short*)&bs->mem[subaddr_src]);

			if (bd->flags & memory_block::PAGE_ACCESSHANDLER)
				HLE<_ARM9>::store16(dst, value);
			else
				*((unsigned short*)&bd->mem[subaddr_dst]) = (unsigned short)value;
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
