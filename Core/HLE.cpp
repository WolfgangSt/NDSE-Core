#include "HLE.h"
#include "Logging.h"
#include "Mem.h" // for namespaces _ARM
#include "MemMap.h"
#include "Compiler.h"
#include "Processor.h"
#include "PhysMem.h"
#include "lz77.h"
#include <QThread>

template <typename>
struct CPU_NAME { static const char *name; };

template<> const char *CPU_NAME<_ARM7>::name = "Arm7";
template<> const char *CPU_NAME<_ARM9>::name = "Arm9";


template <typename T>
void HLE<T>::invalid_read(unsigned long addr)
{
	logging<T>::logf("Invalid read access of %s:%08X", CPU_NAME<T>::name, addr);
	DebugBreak_();
}
template <typename T>
void HLE<T>::invalid_write(unsigned long addr)
{
	logging<T>::logf("Invalid write access of %s:%08X", CPU_NAME<T>::name, addr);
	DebugBreak_();
}

template <typename T>
void HLE<T>::invalid_branch(unsigned long addr)
{
	logging<T>::logf("Invalid branch to %s:%08X", CPU_NAME<T>::name, addr);
	DebugBreak_();
}


// read 32bit bit from variable address
template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load32(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	return *(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load16u(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	return *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load16s(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	return *(signed short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load8u(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	return *(unsigned char*)(&b->mem[addr & (PAGING::ADDRESS_MASK)]);
}


// write 32bit to variable address
template <typename T>
void FASTCALL_IMPL(HLE<T>::store32(unsigned long addr, unsigned long value))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	*(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]) = value;
	b->dirty();
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::store16(unsigned long addr, unsigned long value))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	*(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]) = (unsigned short)value;
	b->dirty();
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::store8(unsigned long addr, unsigned long value))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	*(unsigned char*)(&b->mem[addr & (PAGING::ADDRESS_MASK)]) = (unsigned char)value;
	b->dirty();
}

template <typename T>
void HLE<T>::store32_array(unsigned long addr, int num, unsigned long *data)
{	
	//logging<_ARM9>::logf("storing %i words to %08x", num, addr);
	const unsigned long *end = data + num;
	// load first page
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	while (data != end)
	{
		*(unsigned long*)(&b->mem[subaddr]) = *data; // store word
		data++;       // advance data 
		subaddr += 4; // advance in block
		if (subaddr == PAGING::SIZE) // reached end of page?
		{
			// dirty old page
			b->dirty();
			// load next page
			addr += PAGING::SIZE;
			b = memory_map<T>::addr2page(addr);
			if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
				return invalid_write(addr & ~PAGING::ADDRESS_MASK);
			subaddr = 0;
		}
	}
	// dirty old page
	b->dirty();
}

template <typename T>
void HLE<T>::load32_array(unsigned long addr, int num, unsigned long *data)
{
	//logging<_ARM9>::logf("loading %i words from %08x", num, addr);
	const unsigned long *end = data + num;
	// load first page
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		return invalid_read(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	while (data != end)
	{
		*data = *(unsigned long*)(&b->mem[subaddr]); // load word
		data++;       // advance data 
		subaddr += 4; // advance in block
		if (subaddr == PAGING::SIZE) // reached end of page?
		{
			// load next page
			addr += PAGING::SIZE;
			b = memory_map<T>::addr2page(addr);
			if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
				return invalid_read(addr & ~PAGING::ADDRESS_MASK);
			subaddr = 0;
		}
	}
}


// execute a jump out of page boundaries
template <typename T>
char* FASTCALL_IMPL(HLE<T>::compile_and_link_branch_a_real(unsigned long addr))
{
	// resolve destination
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_EXECPROT))
		invalid_branch(addr);
	// recompile if dirty
	if (b->flags & memory_block::PAGE_DIRTY_J9)
	{
		b->flags &= ~memory_block::PAGE_DIRTY_J9;
		if ( b->get_jit<T, IS_ARM>() )
			b->recompile<T, IS_ARM>();
		if ( b->get_jit<T, IS_THUMB>() )
			b->recompile<T, IS_THUMB>();
	}

	if (addr & 1)
	{
		compiled_block<IS_THUMB>* &block = b->get_jit<T, IS_THUMB>();
		if ( !block )                         // if not compiled yet
			b->recompile<T, IS_THUMB>();  // compile the block
		unsigned long inst = (addr & PAGING::ADDRESS_MASK) >> 1;
		return block->remap[inst];
	} else
	{
		compiled_block<IS_ARM>* &block = b->get_jit<T, IS_ARM>();
		if ( !block )                       // if not compiled yet
			b->recompile<T, IS_ARM>();  // compile the block
		unsigned long inst = (addr & PAGING::ADDRESS_MASK) >> 2;
		return block->remap[inst];
	}
}

// this stub is needed as the original calling code
// migh get overwritten by the compile call!
// this is compiler dependant as i dont know any way to do this
// highlevel yet ...
template <typename T>
char HLE<T>::compile_and_link_branch_a[7];


// this needs rewrite!
template <typename T>
void HLE<T>::invoke(unsigned long /*addr*/, emulation_context *ctx)
{
	DebugBreak_();
	ctx->regs[14] = 0xEFEF0000;
/*	
	__asm
	{
		push ebp
		mov ecx, addr
		mov ebp, ctx
		call compile_and_link_branch_a_real
		call eax
		pop ebp
	}
*/
}

template <typename T>
void HLE<T>::init()
{
	std::ostringstream s;
	char *data = HLE<T>::compile_and_link_branch_a;
	char *func = (char*)&HLE<T>::compile_and_link_branch_a_real;
	unsigned long d = func - data - 5;
	s << '\xE8'; s.write((char*)&d, sizeof(d)); // call func
	s << "\xFF\xE0";                            // jmp eax
	std::string str = s.str();
	memcpy( data, str.data(), str.size() );
}

bool InitHLE()
{
	HLE<_ARM7>::init();
	HLE<_ARM9>::init();
	return true;
}
bool HLE_Initialized = InitHLE();



template <typename T>
void FASTCALL_IMPL(HLE<T>::is_priviledged())
{
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::pushcallstack(unsigned long addr))
{
	callstack_tracer<T>::push(addr);
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::popcallstack(unsigned long addr))
{
	callstack_tracer<T>::pop(addr);
}

/*
  Open_and_get_32bit (eg. LDR r0,[r0], get header)
  Close              (optional, 0=none)
  Get_8bit           (eg. LDRB r0,[r0])
  Get_16bit          (not used)
  Get_32bit          (used by Huffman only)
*/

// int getHeader(uint8 *source, uint16 *dest, uint32 arg) { return *(uint32*)source;}
// 0
// uint8 readByte(uint8 *source) {return *source;}
// 0
// 0


class decomp
{
	memory_block *b;
	unsigned char *mem, *end;
	unsigned long dst;
	unsigned long src;
	unsigned long cbp;
	unsigned long lr;
	unsigned long pc;
	unsigned long f_open;
	unsigned long f_close;
	unsigned long f_get8;
	//unsigned long get16;
	//unsigned long get32;
public:
	unsigned long get_header()
	{
		// invoke getHeader(source, dest, arg)
		// this only needs a call to cb[0] since r0 r1 r2 are set correct
		// at function entrance
		HLE<_ARM9>::invoke(f_open, &processor<_ARM9>::context);
		unsigned long hdr = processor<_ARM9>::context.regs[0];
		return hdr;
	}
	unsigned char get8()
	{
		processor<_ARM9>::context.regs[0] = src++;
		HLE<_ARM9>::invoke(f_get8, &processor<_ARM9>::context);
		return (unsigned char)processor<_ARM9>::context.regs[0];
	}

	void skip(unsigned int num)
	{
		src += num;
	}

	void put8(unsigned char c)
	{
		*mem++ = c;
		if (mem >= end)
		{
			// flush and load next page
			flush();
			dst += PAGING::SIZE;
			b = memory_map<_ARM9>::addr2page(dst);
			mem = (unsigned char*)b->mem;
			end = mem + PAGING::SIZE;
		}
	}

	void init()
	{
		src = processor<_ARM9>::context.regs[0];
		dst = processor<_ARM9>::context.regs[1];
		cbp = processor<_ARM9>::context.regs[2];
		lr = processor<_ARM9>::context.regs[14];
		pc = processor<_ARM9>::context.regs[15];
		unsigned long cb = processor<_ARM9>::context.regs[3];

		if (src & 3)
			logging<_ARM9>::logf("SWI 12h Warning: Source Address not aligned: %08X", src);
		if (dst & 3)
			logging<_ARM9>::logf("SWI 12h Warning: Destination Address not aligned: %08X", dst);

		f_open  = HLE<_ARM9>::load32(cb + 0x00);
		f_close = HLE<_ARM9>::load32(cb + 0x04);
		f_get8  = HLE<_ARM9>::load32(cb + 0x08);
		//f_get16 = HLE<_ARM9>::load32(cb + 0x0C);
		//f_get32 = HLE<_ARM9>::load32(cb + 0x10);

		/*
		logging<_ARM9>::logf("source   = %08X", src);
		logging<_ARM9>::logf("dest     = %08X", dst);
		logging<_ARM9>::logf("open     = %08X", f_open);
		logging<_ARM9>::logf("close    = %08X", f_close);
		logging<_ARM9>::logf("get8     = %08X", f_get8);
		*/
		//logging<_ARM9>::logf("get16    = %08X", f_get16);
		//logging<_ARM9>::logf("get32    = %08X", f_get32);

		b = memory_map<_ARM9>::addr2page(dst);
		mem = (unsigned char*)(b->mem + (dst & PAGING::ADDRESS_MASK));
		end = mem + PAGING::SIZE;
	}

	void flush()
	{
		b->dirty();
	}

	void finish()
	{
		flush();
		processor<_ARM9>::context.regs[14] = lr;
		processor<_ARM9>::context.regs[15] = pc;
	}
};

template <>
void HLE<_ARM9>::LZ77UnCompVram()
{
	logging<_ARM9>::logf("SWI 12h [LZ77UnCompVram] called");
	static lz77<decomp> dc;
	dc.init();
	dc.decode();
	dc.finish();
}

class QPseudoThread: public QThread
{
public:
	static void do_sleep(int ms)
	{
		usleep(ms);
	}
};

template <>
void FASTCALL_IMPL(HLE<_ARM9>::swi(unsigned long idx))
{
	switch (idx)
	{
	case 0x5: return QPseudoThread::do_sleep(20); //Sleep(20);
	case 0x12: return HLE<_ARM9>::LZ77UnCompVram();
	}
	logging<_ARM9>::logf("Unhandled SWI %08X called", idx);
	DebugBreak_();
}


/*
// 0x02400000-0x027FFFFF is a mirror of 0x02000000-0x023FFFFF
ARM9:[0x02000000-0x023E0000) (RAM, 4MB version)
ARM9:[0x023E0000-0x027FF000) (Invalid)
ARM9:[0x027FF000-0x02800000) (???)

27FFE00 upwards = copy of cart header (=512bytes)

ARM9:[0x02800000-0x02BE0000) (RAM image)

*/

template <>
void FASTCALL_IMPL(HLE<_ARM9>::remap_tcm(unsigned long value, unsigned long mode))
{
	int shift = (value >> 1) & 0x1F;

	unsigned long base = value & 0xFFFFF000;
	unsigned long size = 512 << shift;
	unsigned long end  = base + size;

	switch (mode)
	{
	case 0: // data tcm
		logging<_ARM9>::logf("Mapping DTCM to [%08X-%08X)", base, end);
		memory_map<_ARM9>::unmap( &memory::data_tcm );
		memory_map<_ARM9>::map_region( &memory::data_tcm, PAGING::REGION(base, end) );
		break;
	case 1: // instruction tcm
		if (base != 0)
		{
			logging<_ARM9>::logf("ITCM base specified as %08X, must be 0!", base);
			DebugBreak_();
		}
		logging<_ARM9>::logf("Mapping ITCM to [%08X-%08X)", base, end);
		memory_map<_ARM9>::unmap( &memory::inst_tcm );
		memory_map<_ARM9>::map_region( &memory::inst_tcm, PAGING::REGION(base, end) );
		break;
	default:
		logging<_ARM9>::logf("Invalid TCM command");
		DebugBreak_();
	}
}


////////////////////////////////////////////////////////////////////////////////
// Symbol map

symbols::symmap symbols::syms;

void symbols::init()
{
	syms[(void*)HLE<_ARM9>::load32]                    = "arm9::mem::load32";
	syms[(void*)HLE<_ARM9>::load16u]                   = "arm9::mem::load16u";
	syms[(void*)HLE<_ARM9>::load16s]                   = "arm9::mem::load16s";
	syms[(void*)HLE<_ARM9>::load8u]                    = "arm9::mem::load8u";
	syms[(void*)HLE<_ARM9>::store32]                   = "arm9::mem::store32";
	syms[(void*)HLE<_ARM9>::store16]                   = "arm9::mem::store16";
	syms[(void*)HLE<_ARM9>::store8]                    = "arm9::mem::store8";
	syms[(void*)HLE<_ARM9>::store32_array]             = "arm9::mem::store32_array";
	syms[(void*)HLE<_ARM9>::load32_array]              = "arm9::mem::load32_array";
	syms[(void*)HLE<_ARM9>::compile_and_link_branch_a] = "arm9::arm::branch";
	syms[(void*)HLE<_ARM9>::is_priviledged]            = "arm9::is_priviledged";
	syms[(void*)HLE<_ARM9>::remap_tcm]                 = "arm9::TCM";
	syms[(void*)HLE<_ARM9>::pushcallstack]             = "arm9::dbg::callstack::push";
	syms[(void*)HLE<_ARM9>::popcallstack]              = "arm9::dbg::callstack::pop";
	syms[(void*)HLE<_ARM9>::swi]                       = "arm9::swi";
}
