#pragma warning(disable: 4244)

#include "Logging.h"
#include "Mem.h" // for namespaces _ARM
#include "MemMap.h"
#include "Compiler.h"
#include "Processor.h"
#include "PhysMem.h"
#include "lz77.h"
#include "runner.h"
#include "HLE.h"
#include "Interrupt.h"

#include <boost/thread.hpp>

// TODO: could give the compiler hints about the
// b->flags & memory_block::PAGE_ACCESSHANDLER
// checks as they are almost always evaluating to false

template <typename>
struct CPU_NAME { static const char *name; };

template<> const char *CPU_NAME<_ARM7>::name = "Arm7";
template<> const char *CPU_NAME<_ARM9>::name = "Arm9";

#ifdef DEBUGGING
void DEBUG_STORE(unsigned long addr, unsigned long sz)
{
// add memory debugger here
	if ((addr >= 0x01000100) &&  (addr <= 0x01010200))
		DebugBreak_();
}
#else
#define DEBUG_STORE(addr,sz)
#endif

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
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->load32(addr);
	return _rotr(*(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]),
		(addr & 3) << 3);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load16u(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->load16u(addr);
	return *(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load16s(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->load16s(addr);
	return *(signed short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]);
}

template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::load8u(unsigned long addr))
{
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		invalid_read(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->load8u(addr);
	return *(unsigned char*)(&b->mem[addr & (PAGING::ADDRESS_MASK)]);
}

template <typename T>
void HLE<T>::load32_array(unsigned long addr, int num, unsigned long *data)
{
	//logging<_ARM9>::logf("loading %i words from %08x", num, addr);
	const unsigned long *end = data + num;
	// load first page

	addr &= ~3;
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		return invalid_read(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->load32_array(addr, num, data);

	unsigned long subaddr = addr & PAGING::ADDRESS_MASK;
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


// write 32bit to variable address
template <typename T>
void FASTCALL_IMPL(HLE<T>::store32(unsigned long addr, unsigned long value))
{
	DEBUG_STORE(addr, 4);
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->store32(addr, value);

	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	*(unsigned long*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~3))]) = value;	
	b->dirty();
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::store16(unsigned long addr, unsigned long value))
{
	DEBUG_STORE(addr, 2);
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->store16(addr, value);

	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT))
		return invalid_write(addr);
	*(unsigned short*)(&b->mem[addr & (PAGING::ADDRESS_MASK & (~1))]) = (unsigned short)value;
	b->dirty();
}

template <typename T>
void FASTCALL_IMPL(HLE<T>::store8(unsigned long addr, unsigned long value))
{
	DEBUG_STORE(addr, 1);
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->store8(addr, value);

	if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_WRITEPROT | 
		memory_block::PAGE_WRITEPROT8))
		return invalid_write(addr);
	*(unsigned char*)(&b->mem[addr & (PAGING::ADDRESS_MASK)]) = (unsigned char)value;
	b->dirty();
}

template <typename T>
void HLE<T>::store32_array(unsigned long addr, int num, unsigned long *data)
{	
	DEBUG_STORE(addr, num*4);
	//logging<_ARM9>::logf("storing %i words to %08x", num, addr);
	// load first page
	memory_block *b = memory_map<T>::addr2page(addr);
	if (b->flags & memory_block::PAGE_ACCESSHANDLER) // need special handling?
		return b->base->store32_array(addr, num, data);

	const unsigned long *end = data + num;
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




// flags swizzle mask
// eax-BIT x86-FLAG  PSR-BIT
//    0       OF       28
//    8       CF       29
//    14      ZF       30
//    15      SF       31

// ................SZ.....C.......O (x86 flag dump)
// SZ.....C.......O................ (<< 16)
// ..C.......O..................... (<< 21)
// ...O............................ (<< 28)
// -------------------------------- (OR'ed)
// SZCO...C..O....O................

// CPSR read request
template <typename T>
unsigned long FASTCALL_IMPL(HLE<T>::storecpsr())
{
	unsigned long cpsr = processor<T>::ctx().cpsr;
	unsigned long x86f = processor<T>::ctx().x86_flags;
	cpsr &= 0x07FFFFFF;
	x86f &= 0x0C101;
	cpsr |= ((x86f << 16) | (x86f << 21) | (x86f << 28)) & 0x0F8000000;
	return cpsr;
}

static const unsigned long mode_regs[CPU_MAX_MODES] = 
{
	15,
	13,
	13,
	13,
	13,
	8
}; // same for ARM9/ARM7

template <typename T>
emulation_context* FASTCALL_IMPL(HLE<T>::loadcpsr(unsigned long value, unsigned long mask))
{
	// update the CPSR this can trigger several things
	// toggling thumb bit => rebranch
	// toggling mode      => remap registers
	// toggle flags       => update x86flags

	unsigned long &cpsr = processor<T>::ctx().cpsr;
	cpsr &= ~mask;
	cpsr |= value & mask;

	// sync x86 flags as needed
	if (mask & 0xF8000000)
	{
		unsigned long &x86f = processor<T>::ctx().x86_flags;
		unsigned long flags = cpsr & 0xF8000000;
		x86f = ((flags >> 16) | (flags >> 21) | (flags >> 28)) & 0x0C101;
	}

	// this has to be done last as cpsr migh need to be duplicated to new mode
	if (mask & 0x1F)
	{
		// mode change
		unsigned long mode = cpsr & 0x1F;
		unsigned long cpumode = 0;
		switch (cpsr & 0x1F)
		{
		case 0x11: cpumode++; // FIQ
		case 0x12: cpumode++; // IRQ
		case 0x1B: cpumode++; // Undefined
		case 0x14: cpumode++; // Abort
		case 0x13: cpumode++; // Supervisor
		case 0x1F: // System
		case 0x10: // User
			{
				unsigned long oldmode = processor<T>::mode;
				if (oldmode == cpumode)
					break; // no change
	
				emulation_context &old_mode = processor<T>::context[oldmode];
				emulation_context &new_mode = processor<T>::context[cpumode];

				// copy registers
				unsigned long m = std::min( mode_regs[oldmode], mode_regs[cpumode] );
				for (unsigned int i = 0; i < m; i++)
					new_mode.regs[i] = old_mode.regs[i];
				new_mode.regs[15] = old_mode.regs[15];
				new_mode.cpsr = old_mode.cpsr;

				// switch to new mode
				processor<T>::mode = (cpu_mode)cpumode;
				break;
			}
		default:
			logging<T>::logf("Modeswitch to invalid mode: %02x", mode);
			DebugBreak_();
		}
	}
	return &processor<T>::ctx();
}

template <typename T> struct dirty_flag {};
template <> struct dirty_flag<_ARM9> { enum { VALUE = memory_block::PAGE_DIRTY_J9 }; };
template <> struct dirty_flag<_ARM7> { enum { VALUE = memory_block::PAGE_DIRTY_J7 }; };


// execute a jump out of page boundaries
template <typename T>
char* FASTCALL_IMPL(HLE<T>::compile_and_link_branch_a_real(unsigned long addr))
{
	interrupt<T>::poll_process();
	// resolve destination
	memory_block *b = memory_map<T>::addr2page(addr);
	processor<T>::last_page = b;

	if (b->flags & (memory_block::PAGE_EXECPROT))
		invalid_branch(addr);
	// recompile if dirty
	if (b->flags & dirty_flag<T>::VALUE)
	{
		b->flags &= ~dirty_flag<T>::VALUE;
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
template <typename T> char HLE<T>::compile_and_link_branch_a[7+HLE<T>::SECURITY_PADDING];
template <typename T> char HLE<T>::invoke_arm[19+HLE<T>::SECURITY_PADDING];
template <typename T> char HLE<T>::read_tsc[3+HLE<T>::SECURITY_PADDING];

// if possible remove the wrapping!
template <typename T>
void HLE<T>::invoke(unsigned long addr, emulation_context *ctx)
{
	((invoke_fun)&invoke_arm)(addr, ctx);
}

#define OFFSET(z) ((char*)&__context_helper.z - (char*)&__context_helper)
static emulation_context __context_helper; // temporary for OFFSET calculation

static void prepare_stub(char *addr, size_t sz)
{
	if (mprotect( addr, sz, PROT_READ | PROT_WRITE | PROT_EXEC ) != 0)
		logging<_DEFAULT>::log("Failed to adjust JIT memory to executable");
	cacheflush( addr, (int)sz, ICACHE );
}

template <typename T>
void HLE<T>::init()
{
	{
		std::ostringstream s;
		char *data = HLE<T>::compile_and_link_branch_a;
		char *func = (char*)&HLE<T>::compile_and_link_branch_a_real;
		unsigned long d = func - data - 5;
		s << '\xE8'; s.write((char*)&d, sizeof(d)); // call func
		s << "\xFF\xE0";                            // jmp eax
		std::string str = s.str();
		memset( data, 0x90, str.size() + SECURITY_PADDING );
		memcpy( data, str.data(), str.size() );
		prepare_stub( data, str.size() );
	}
	{
		std::ostringstream s;
		char *data = HLE<T>::invoke_arm;
		char *func = (char*)&HLE<T>::compile_and_link_branch_a_real;
		unsigned long d = func - data - 5 - 10; // 3 bytes stackframe
		unsigned long ret = 0xEFEF0000;
		// input: ecx = arm addr, edx = context pointer

		//s << '\x56';                                      // push esi
		//s << '\x55';                                      // push ebp
		s << '\x60';                                      // pushad
		s << "\x8B\xEA";                                  // mov ebp, edx
		s << "\xC7\x45" << (char)OFFSET(regs[14]);        // mov [ebp+LR]
		s.write((char*)&ret, sizeof(ret));                //   , 0xEFEF0000
		s << '\xE8'; s.write((char*)&d, sizeof(d));       // call func
		s << "\xFF\xD0";                                  // call eax
		s << '\x61';                                      // popad
		//s << '\x5D';                                      // pop ebp
		//s << '\x5E';                                      // pop esi
		s << '\xC3';                                      // ret

		std::string str = s.str();
		memset( data, 0x90, str.size() + SECURITY_PADDING );
		memcpy( data, str.data(), str.size() );		
		prepare_stub( data, str.size() );
	}

	{
		std::ostringstream s;
		char *data = HLE<T>::read_tsc;		
		s << "\x8B\xC3"; // mov eax, ebx
		s << '\xC3';     // ret
		std::string str = s.str();
		memset( data, 0x90, str.size() + SECURITY_PADDING );
		memcpy( data, str.data(), str.size() );	
		prepare_stub( data, str.size() );
	}
}

#undef OFFSET

bool InitHLE()
{
	HLE<_ARM7>::init();
	HLE<_ARM9>::init();
	// init default mapping
	HLE<_ARM9>::remap_tcm(0x80000A, 0);
	return true;
}

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

		// TODO: handle cpu mode
		HLE<_ARM9>::invoke(f_open, &processor<_ARM9>::ctx());
		unsigned long hdr = processor<_ARM9>::ctx().regs[0];
		return hdr;
	}
	unsigned char get8()
	{
		// TODO: handle cpu mode
		processor<_ARM9>::ctx().regs[0] = src++;
		HLE<_ARM9>::invoke(f_get8, &processor<_ARM9>::ctx());
		return (unsigned char)processor<_ARM9>::ctx().regs[0];
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
		// TODO: handle CPU mode
		src = processor<_ARM9>::ctx().regs[0];
		dst = processor<_ARM9>::ctx().regs[1];
		cbp = processor<_ARM9>::ctx().regs[2];
		lr = processor<_ARM9>::ctx().regs[14];
		pc = processor<_ARM9>::ctx().regs[15];
		unsigned long cb = processor<_ARM9>::ctx().regs[3];

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
		// TODO: handle CPU mode
		flush();
		processor<_ARM9>::ctx().regs[14] = lr;
		processor<_ARM9>::ctx().regs[15] = pc;
	}
};

template <> void HLE<_ARM9>::LZ77UnCompVram()
{
	logging<_ARM9>::logf("SWI 12h [LZ77UnCompVram] called");
	static lz77<decomp> dc;
	dc.init();
	dc.decode();
	dc.finish();
}

template <> void HLE<_ARM9>::sqrt()
{
	logging<_ARM9>::logf("SWI 8h [sqrt] called (noimpl)");
	DebugBreak_(); // not yet supported
}

template <typename T> void HLE<T>::div()
{
	emulation_context &ctx = processor<_ARM9>::ctx();
	if (ctx.regs[1] == 0)
	{
		logging<T>::log("SWI 9h division by zero");
		DebugBreak_();
	}
	signed long r0 = (signed)ctx.regs[0]/(signed)ctx.regs[1];
	signed long r1 = ctx.regs[0] - r0;
	ctx.regs[0] = (unsigned long)r0;
	ctx.regs[1] = (unsigned long)r1;
	if (r0 < 0)
		ctx.regs[2] = (unsigned long)-r0;
	else ctx.regs[2] = (unsigned long)r0;
}

template <> void HLE<_ARM9>::CpuSet()
{
	logging<_ARM9>::logf("SWI Bh [CpuSet] called (noimpl)");
	DebugBreak_(); // not yet supported
}


struct stream_crc16
{
	struct context
	{
		unsigned long crc;
	};

	static void process(memory_block *b, char *mem, int sz, context &ctx)
	{
		if (b->flags & (memory_block::PAGE_INVALID | memory_block::PAGE_READPROT))
		{
			logging<_DEFAULT>::logf("Invalid page accessed during crc16 swi");
			DebugBreak_();
		}
		ctx.crc = util::get_crc16(ctx.crc, mem, sz);
	}
};

template <typename T> void HLE<T>::crc16()
{
	// r0  Initial CRC value (16bit, usually FFFFh)
	// r1  Start Address   (must be aligned by 2)
	// r2  Length in bytes (must be aligned by 2)
	logging<T>::logf("SWI Eh [crc16] called");

	// TODO: handle CPU mode
	stream_crc16::context ctx;
	ctx.crc  = processor<T>::ctx().regs[0];
	unsigned long addr = processor<T>::ctx().regs[1];
	unsigned long len  = processor<T>::ctx().regs[2];
	if (addr & 1)
	{
		logging<T>::logf("Address not aligned: %08X", addr);
		DebugBreak_();
	}
	if (len & 1)
	{
		logging<T>::logf("Length aligned: %08X", len);
		DebugBreak_();
	}
	memory_map<T>::process_memory<stream_crc16>( addr, len, ctx );
	processor<T>::ctx().regs[0] = ctx.crc;
}



class QPseudoThread
{
private:
	static boost::xtime delay(int secs, int msecs=0, int nsecs=0) 
	{
		const int MILLISECONDS_PER_SECOND = 1000; 
		const int NANOSECONDS_PER_SECOND = 1000000000; 
		const int NANOSECONDS_PER_MILLISECOND = 1000000; 

		boost::xtime xt; 
		if (boost::TIME_UTC != boost::xtime_get (&xt, boost::TIME_UTC)) 
			assert(0);
		nsecs += xt.nsec; 
		msecs += nsecs / NANOSECONDS_PER_MILLISECOND; 
		secs += msecs / MILLISECONDS_PER_SECOND; 
		nsecs += (msecs % MILLISECONDS_PER_SECOND) * 
		NANOSECONDS_PER_MILLISECOND; 
		xt.nsec = nsecs % NANOSECONDS_PER_SECOND; 
		xt.sec += secs + (nsecs / NANOSECONDS_PER_SECOND); 

		return xt; 
	}
public:
	static void do_sleep(int ms)
	{
		boost::thread::sleep(delay(0, ms));
	}

	static void yieldCurrentThread()
	{
		boost::thread::yield();
	}
};

template <typename T> void HLE<T>::IntrWait()
{
	// nospam...
	//logging<T>::logf("SWI 4h [IntrWait] called (noimpl)");
	//QPseudoThread::do_sleep(10);
	QPseudoThread::yieldCurrentThread();
	//DebugBreak_(); // not yet supported
}


template <typename T> void HLE<T>::delay()
{
	// does a tiny busy idle loop
	// for R0*4 tacts

	// delays for approx r0/8388 msecs
	// TODO: handle CPU mode
	QPseudoThread::do_sleep(processor<T>::ctx().regs[0] / 8388);
}

template <typename T> void HLE<T>::wait_vblank()
{
	QPseudoThread::do_sleep(20); //Sleep(20);
}

template <>
void FASTCALL_IMPL(HLE<_ARM9>::swi(unsigned long idx))
{
	switch (idx)
	{
	case 0x4: return HLE<_ARM9>::IntrWait();
	case 0x5: return HLE<_ARM9>::wait_vblank();
	case 0x8: return HLE<_ARM9>::sqrt();
	case 0x9: return HLE<_ARM9>::div();
	case 0xB: return HLE<_ARM9>::CpuSet();
	case 0x12: return HLE<_ARM9>::LZ77UnCompVram();
	}
	logging<_ARM9>::logf("Unhandled SWI %08X called", idx);
	DebugBreak_();
}

template <>
void FASTCALL_IMPL(HLE<_ARM7>::swi(unsigned long idx))
{
	switch (idx)
	{
	case 0x3: return HLE<_ARM7>::delay();
	case 0x4: return HLE<_ARM7>::IntrWait();
	case 0x5: return HLE<_ARM7>::wait_vblank();
	case 0x9: return HLE<_ARM7>::div();
	case 0xE: return HLE<_ARM7>::crc16();
	}
	logging<_ARM7>::logf("Unhandled SWI %08X called", idx);
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
void FASTCALL_IMPL(HLE<_ARM7>::remap_tcm(unsigned long /*value*/, unsigned long /*mode*/))
{
	logging<_ARM7>::logf("Invalid TCM command (ARM7?)");
	DebugBreak_();
}


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
	syms[(void*)HLE<_ARM9>::debug_magic]               = "arm9::debugmagic";
	syms[(void*)HLE<_ARM9>::loadcpsr]                  = "arm9::cpsr::load";
	syms[(void*)HLE<_ARM9>::storecpsr]                 = "arm9::cpsr::store";

	syms[(void*)HLE<_ARM7>::load32]                    = "arm7::mem::load32";
	syms[(void*)HLE<_ARM7>::load16u]                   = "arm7::mem::load16u";
	syms[(void*)HLE<_ARM7>::load16s]                   = "arm7::mem::load16s";
	syms[(void*)HLE<_ARM7>::load8u]                    = "arm7::mem::load8u";
	syms[(void*)HLE<_ARM7>::store32]                   = "arm7::mem::store32";
	syms[(void*)HLE<_ARM7>::store16]                   = "arm7::mem::store16";
	syms[(void*)HLE<_ARM7>::store8]                    = "arm7::mem::store8";
	syms[(void*)HLE<_ARM7>::store32_array]             = "arm7::mem::store32_array";
	syms[(void*)HLE<_ARM7>::load32_array]              = "arm7::mem::load32_array";
	syms[(void*)HLE<_ARM7>::compile_and_link_branch_a] = "arm7::arm::branch";
	syms[(void*)HLE<_ARM7>::is_priviledged]            = "arm7::is_priviledged";
	syms[(void*)HLE<_ARM7>::remap_tcm]                 = "arm7::TCM";
	syms[(void*)HLE<_ARM7>::pushcallstack]             = "arm7::dbg::callstack::push";
	syms[(void*)HLE<_ARM7>::popcallstack]              = "arm7::dbg::callstack::pop";
	syms[(void*)HLE<_ARM7>::swi]                       = "arm7::swi";
	syms[(void*)HLE<_ARM7>::debug_magic]               = "arm7::debugmagic";
	syms[(void*)HLE<_ARM7>::loadcpsr]                  = "arm7::cpsr::load";
	syms[(void*)HLE<_ARM7>::storecpsr]                 = "arm7::cpsr::store";
}
