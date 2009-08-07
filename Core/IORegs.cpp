#include "IORegs.h"
#include "Logging.h"
#include "Compiler.h" // for DebugBreak_
#include "MemMap.h"
#include "vram.h"
#include "dma.h"

#include "Processor.h"
#include "Interrupt.h"

// todo: outsource name lookup to an array ....


union endian_access
{
	unsigned long  w;
	unsigned short h[2];
	unsigned char  b[4];
};

////////////////////////////////////////////////////////////////////////////////

ioregs::ioregs()
{
	ipc = 0;
	ipc_fifocnt = 0;
	ipc_fifo.reset();
}

void ioregs::set_ipc(unsigned long remote_ipc)
{
	ipc = (ipc & ~0xF) | ((remote_ipc >> 8) & 0xF);
}

unsigned long ioregs::get_ipc() const
{
	return ipc;
}

unsigned long ioregs::get_fifostate() const
{
	unsigned long state = 0;
	if (ipc_fifo.empty())
		state |= 0x1;
	if (ipc_fifo.full())
		state |= 0x2;
	return state;
}

bool ioregs::pop_fifo(unsigned long &value)
{
	if (ipc_fifo.empty())
		return false;
	value = ipc_fifo.read();
	return true;
}

unsigned long ioregs::get_fifocnt() const
{
	return ipc_fifocnt & 0xFFFFFFFC | get_fifostate();
}

void ioregs::push_fifo(unsigned long value)
{
	if (ipc_fifo.full())
	{
		logging<_DEFAULT>::log("FIFO overflow");
		DebugBreak_();
	}
	ipc_fifo.write(value);
}

void ioregs::flag_fifo(unsigned long state)
{
	// need to guard
	ipc_fifocnt = (ipc_fifocnt & 0xFFFFFCFF) | (state << 8);
}

////////////////////////////////////////////////////////////////////////////////

static const char* NONAME = "NoName";
static const char* NONAME_NOLOG = "^NoName";

////////////////////////////////////////////////////////////////////////////////

#define DEF_NAME_1(offset, name) \
	names[offset] = name; \
	names[offset+1] = NONAME_NOLOG; \
	names[offset+2] = NONAME_NOLOG; \
	names[offset+3] = NONAME_NOLOG
#define DEF_NAME_2(offset, name) \
	names[offset] = name; \
	names[offset+1] = name; \
	names[offset+2] = NONAME_NOLOG; \
	names[offset+3] = NONAME_NOLOG
#define DEF_NAME_4(offset, name) \
	names[offset] = name; \
	names[offset+1] = name; \
	names[offset+2] = name; \
	names[offset+3] = name

REGISTERS9_1::REGISTERS9_1( const char *name, unsigned long color, unsigned long priority )
	: memory_region(name, color, priority)
{
	for (int i = 0; i < PAGES; i++)
		blocks[i].flags |= memory_block::PAGE_ACCESSHANDLER;
	for (int i = 0; i < SIZE; i++)
		names[i] = NONAME;

	DEF_NAME_4(0x0000, "^[DISPCNT] 2D Graphics Engine A display control");
	DEF_NAME_2(0x0004, "^[DISPSTAT] Display Status");
	DEF_NAME_2(0x0006, "^[VCOUNT] V count comparison");
	DEF_NAME_2(0x0008, "^[BG0CNT] 2D Graphics Engine A BG0 control");
	DEF_NAME_2(0x000A, "^[BG1CNT] 2D Graphics Engine A BG1 control");
	DEF_NAME_2(0x000C, "^[BG2CNT] 2D Graphics Engine A BG2 control");
	DEF_NAME_2(0x000E, "^[BG3CNT] 2D Graphics Engine A BG3 control");
	DEF_NAME_2(0x0010, "^[BG0HOFS] 2D Graphics Engine A BG0 display H offset");
	DEF_NAME_2(0x0012, "^[BG0VOFS] 2D Graphics Engine A BG0 display V offset");
	DEF_NAME_2(0x0014, "^[BG1HOFS] 2D Graphics Engine A BG1 display H offset");
	DEF_NAME_2(0x0016, "^[BG1VOFS] 2D Graphics Engine A BG1 display V offset");
	DEF_NAME_2(0x0018, "^[BG2HOFS] 2D Graphics Engine A BG2 display H offset");
	DEF_NAME_2(0x001A, "^[BG2VOFS] 2D Graphics Engine A BG2 display V offset");
	DEF_NAME_2(0x001C, "^[BG3HOFS] 2D Graphics Engine A BG3 display H offset");
	DEF_NAME_2(0x001E, "^[BG3VOFS] 2D Graphics Engine A BG3 display V offset");
	DEF_NAME_2(0x0020, "^[BG2PA] 2D Graphics Engine A BG2 affine parameter A");
	DEF_NAME_2(0x0022, "^[BG2PB] 2D Graphics Engine A BG2 affine parameter B");
	DEF_NAME_2(0x0024, "^[BG2PB] 2D Graphics Engine A BG2 affine parameter C");
	DEF_NAME_2(0x0026, "^[BG2PB] 2D Graphics Engine A BG2 affine parameter D");
	DEF_NAME_4(0x0028, "^[BG2X] 2D Graphics Engine A BG2 reference X");
	DEF_NAME_4(0x002C, "^[BG2Y] 2D Graphics Engine A BG2 reference Y");
	DEF_NAME_2(0x0030, "^[BG3PA] 2D Graphics Engine A BG3 affine parameter A");
	DEF_NAME_2(0x0032, "^[BG3PB] 2D Graphics Engine A BG3 affine parameter B");
	DEF_NAME_2(0x0034, "^[BG3PB] 2D Graphics Engine A BG3 affine parameter C");
	DEF_NAME_2(0x0036, "^[BG3PB] 2D Graphics Engine A BG3 affine parameter D");
	DEF_NAME_4(0x0038, "^[BG3X] 2D Graphics Engine A BG3 reference X");
	DEF_NAME_4(0x003C, "^[BG3Y] 2D Graphics Engine A BG3 reference Y");
	DEF_NAME_2(0x0040, "^[WIN0H] 2D Graphics Engine A window 0 H size");
	DEF_NAME_2(0x0042, "^[WIN1H] 2D Graphics Engine A window 1 H size");
	DEF_NAME_2(0x0044, "^[WIN0V] 2D Graphics Engine A window 0 V size");
	DEF_NAME_2(0x0046, "^[WIN1V] 2D Graphics Engine A window 1 V size");
	DEF_NAME_2(0x0048, "^[WININ] 2D Graphics Engine A window inside");
	DEF_NAME_2(0x004A, "^[WINOUT] 2D Graphics Engine A window outside");
	DEF_NAME_2(0x004C, "^[MOSAIC] 2D Graphics Engine A mosaic size");
	DEF_NAME_2(0x0050, "^[BLDCNT] 2D Graphics Engine A color special effects");
	DEF_NAME_2(0x0052, "^[BLDALPHA] 2D Graphics Engine A alpha blending factor");
	DEF_NAME_2(0x0054, "^[BLDY] 2D Graphics Engine A brightness change factor");

	DEF_NAME_4(0x00B0, "^[DM0SAD] DMA0 source address");
	DEF_NAME_4(0x00B4, "^[DM0DAD] DMA0 destination address");
	DEF_NAME_4(0x00B8, "^[DM0CNT] DMA0 control");
	DEF_NAME_4(0x00BC, "^[DM1SAD] DMA1 source address");
	DEF_NAME_4(0x00C0, "^[DM1DAD] DMA1 destination address");
	DEF_NAME_4(0x00C4, "^[DM1CNT] DMA1 control");
	DEF_NAME_4(0x00C8, "^[DM2SAD] DMA2 source address");
	DEF_NAME_4(0x00CC, "^[DM2DAD] DMA2 destination address");
	DEF_NAME_4(0x00D0, "^[DM2CNT] DMA2 control");
	DEF_NAME_4(0x00D4, "^[DM3SAD] DMA3 source address");
	DEF_NAME_4(0x00D8, "^[DM3DAD] DMA3 destination address");
	DEF_NAME_4(0x00DC, "^[DM3CNT] DMA3 control");

	DEF_NAME_4(0x00E0, "^[DM0FILL] DMA 0 Filldata");
	DEF_NAME_4(0x00E4, "^[DM1FILL] DMA 1 Filldata");
	DEF_NAME_4(0x00E8, "^[DM2FILL] DMA 2 Filldata");
	DEF_NAME_4(0x00EC, "^[DM3FILL] DMA 3 Filldata");

	DEF_NAME_2(0x0100, "^[TM0CNT_L] Timer 0 counter");
	DEF_NAME_2(0x0102, "^[TM0CNT_H] Timer 0 control");
	DEF_NAME_2(0x0104, "^[TM1CNT_L] Timer 1 counter");
	DEF_NAME_2(0x0106, "^[TM1CNT_H] Timer 1 control");
	DEF_NAME_2(0x0108, "^[TM2CNT_L] Timer 2 counter");
	DEF_NAME_2(0x010A, "^[TM2CNT_H] Timer 2 control");
	DEF_NAME_2(0x010C, "^[TM3CNT_L] Timer 3 counter");
	DEF_NAME_2(0x010E, "^[TM3CNT_H] Timer 3 control");

	DEF_NAME_2(0x0180, "^[IPCSYNC] IPC Synchronize Register");
	DEF_NAME_2(0x0184, "^[IPCFIFOCNT] IPC Fifo Control");
	DEF_NAME_4(0x0188, "^[IPCFIFOSEND] IPC Send Fifo");

	DEF_NAME_4(0x0208, "^[IME] Interrupt Master Enable");
	DEF_NAME_4(0x0210, "^[IE] Interrupt Enable");
	DEF_NAME_4(0x0214, "^[IF] Interrupt Flag");
	DEF_NAME_4(0x0240, "^[VRAMCNT] RAM bank control 0");
	DEF_NAME_4(0x0244, "^[WRAMCNT] RAM bank control 1");
	DEF_NAME_2(0x0248, "^[VRAM_HI_CNT] RAM bank control 2");
	DEF_NAME_2(0x0304, "^[POWCNT] Power control");

	DEF_NAME_4(0x1000, "^[DB_DISPCNT] 2D Graphics Engine B display control");
	DEF_NAME_2(0x1008, "^[DB_BG0CNT] 2D Graphics Engine B BG0 control");
	DEF_NAME_2(0x100A, "^[DB_BG1CNT] 2D Graphics Engine B BG1 control");
	DEF_NAME_2(0x100C, "^[DB_BG2CNT] 2D Graphics Engine B BG2 control");
	DEF_NAME_2(0x100E, "^[DB_BG3CNT] 2D Graphics Engine B BG3 control");
	DEF_NAME_2(0x1010, "^[DB_BG0HOFS] 2D Graphics Engine B BG0 display H offset");
	DEF_NAME_2(0x1012, "^[DB_BG0VOFS] 2D Graphics Engine B BG0 display V offset");
	DEF_NAME_2(0x1014, "^[DB_BG1HOFS] 2D Graphics Engine B BG1 display H offset");
	DEF_NAME_2(0x1016, "^[DB_BG1VOFS] 2D Graphics Engine B BG1 display V offset");
	DEF_NAME_2(0x1018, "^[DB_BG2HOFS] 2D Graphics Engine B BG2 display H offset");
	DEF_NAME_2(0x101A, "^[DB_BG2VOFS] 2D Graphics Engine B BG2 display V offset");
	DEF_NAME_2(0x101C, "^[DB_BG3HOFS] 2D Graphics Engine B BG3 display H offset");
	DEF_NAME_2(0x101E, "^[DB_BG3VOFS] 2D Graphics Engine B BG3 display V offset");
	DEF_NAME_2(0x1020, "^[DB_BG2PA] 2D Graphics Engine B BG2 affine parameter A");
	DEF_NAME_2(0x1022, "^[DB_BG2PB] 2D Graphics Engine B BG2 affine parameter B");
	DEF_NAME_2(0x1024, "^[DB_BG2PB] 2D Graphics Engine B BG2 affine parameter C");
	DEF_NAME_2(0x1026, "^[DB_BG2PB] 2D Graphics Engine B BG2 affine parameter D");
	DEF_NAME_4(0x1028, "^[DB_BG2X] 2D Graphics Engine B BG2 reference X");
	DEF_NAME_4(0x102C, "^[DB_BG2Y] 2D Graphics Engine B BG2 reference Y");
	DEF_NAME_2(0x1030, "^[DB_BG3PA] 2D Graphics Engine B BG3 affine parameter A");
	DEF_NAME_2(0x1032, "^[DB_BG3PB] 2D Graphics Engine B BG3 affine parameter B");
	DEF_NAME_2(0x1034, "^[DB_BG3PB] 2D Graphics Engine B BG3 affine parameter C");
	DEF_NAME_2(0x1036, "^[DB_BG3PB] 2D Graphics Engine B BG3 affine parameter D");
	DEF_NAME_4(0x1038, "^[DB_BG3X] 2D Graphics Engine B BG3 reference X");
	DEF_NAME_4(0x103C, "^[DB_BG3Y] 2D Graphics Engine B BG3 reference Y");
	DEF_NAME_2(0x1040, "^[DB_WIN0H] 2D Graphics Engine B window 0 H size");
	DEF_NAME_2(0x1042, "^[DB_WIN1H] 2D Graphics Engine B window 1 H size");
	DEF_NAME_2(0x1044, "^[DB_WIN0V] 2D Graphics Engine B window 0 V size");
	DEF_NAME_2(0x1046, "^[DB_WIN1V] 2D Graphics Engine B window 1 V size");
	DEF_NAME_2(0x1048, "^[DB_WININ] 2D Graphics Engine B window inside");
	DEF_NAME_2(0x104A, "^[DB_WINOUT] 2D Graphics Engine B window outside");
	DEF_NAME_2(0x104C, "^[DB_MOSAIC] 2D Graphics Engine B mosaic size");
	DEF_NAME_2(0x1050, "^[DB_BLDCNT] 2D Graphics Engine B color special effects");
	DEF_NAME_2(0x1052, "^[DB_BLDALPHA] 2D Graphics Engine B alpha blending factor");
	DEF_NAME_2(0x1054, "^[DB_BLDY] 2D Graphics Engine B brightness change factor");
}

extern void start_dma3();

void REGISTERS9_1::store32(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM9>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	unsigned long &current = *(unsigned long*)(&b->mem[subaddr]);
	bool remap_vram = false;
	unsigned long addr4k = addr & 0x1FFF;

	switch (addr4k)
	{
	case 0xB8:
	case 0xC4:
	case 0xD0:
	case 0xDC:
		current = value;
		start_dma((unsigned long*)(&b->mem[addr4k - 8]));
		return;	
	case 0x180:
		memory::registers7_1.set_ipc(value);
		break;
	case 0x188:
		logging<_ARM9>::logf("Pushing %08X to ARM9::FIFO", value);
		push_fifo(value);
		memory::registers7_1.flag_fifo(get_fifostate());
		interrupt<_ARM7>::fire(18); // IPC Recv FIFO Not Empty
		break;
	case 0x240:
	case 0x244:
	case 0x248:
		remap_vram = true;
		break;
	}

	// debug output names
	const char* lastname = "";
	for (int i = 0; i < 4; i++)
	{
		const char* name = names[addr4k+i];
		if (name == lastname)
			continue;
		lastname = name;
		if (name == NONAME) logging<_ARM9>::logf("Unhandled write to ARM9::IO::%08X", addr+i);
		else if (name[0] != '^') logging<_ARM9>::logf("Write to ARM9::IO::%08X [%s]", addr+i, name);
	}

	if (current != value)
	{
		current = value;
		if (remap_vram)
			vram::remap();
		b->dirty();
	}
}

void REGISTERS9_1::store16(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM9>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~1));
	// uncomment as soon as needed
	//unsigned short &current = *(unsigned short*)(&b->mem[subaddr]);

	// special 16bit handling can be added here
	switch (addr & 0x1FFF)
	{
	case 0:
	default:
		endian_access e = *(endian_access*)(&b->mem[subaddr]);
		e.h[(addr >> 1) & 1] = (unsigned short)value;
		return store32(addr & ~3, e.w);
	}

	// uncomment when adding special case
	//if (current != (unsigned short)value)
	//	b->dirty();
}

void REGISTERS9_1::store8(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM9>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK);
	// uncomment as soon as needed
	//unsigned char &current = *(unsigned char*)(&b->mem[subaddr]);

	// special 8bit handling can be added here
	switch (addr & 0x1FFF)
	{
	case 0:
	default:
		endian_access e = *(endian_access*)(&b->mem[subaddr & (~3)]);
		e.b[addr & 3] = (unsigned char)value;
		return store32(addr & ~3, e.w);
	}
}

void REGISTERS9_1::store32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/)
{
	logging<_DEFAULT>::logf("Store Multiple not supported at %08X", addr);
	DebugBreak_();
}

unsigned long REGISTERS9_1::load32(unsigned long addr)
{
	memory_block *b = memory_map<_ARM9>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	unsigned long &current = *(unsigned long*)(&b->mem[subaddr]);
	unsigned long addr4k = addr & 0x1FFF;

	// debug output names
	const char* lastname = "";
	for (int i = 0; i < 4; i++)
	{
		const char* name = names[addr4k+i];
		if (name == lastname)
			continue;
		lastname = name;
		if (name == NONAME) logging<_ARM9>::logf("Unhandled load from ARM9::IO::%08X", addr+i);
		else if (name[0] != '^') logging<_ARM9>::logf("Load from ARM9::IO::%08X [%s]", addr+i, name);
	}

	switch (addr4k)
	{
	case 0x180: return get_ipc();
	case 0x184: return get_fifocnt();
	}
	return current;
}

unsigned long REGISTERS9_1::load16u(unsigned long addr)
{
	endian_access e;
	e.w = REGISTERS9_1::load32(addr & (~3));
	return e.h[(addr >> 1) & 1];
}

unsigned long REGISTERS9_1::load16s(unsigned long addr)
{
	return (signed short)REGISTERS9_1::load16u(addr);
}

unsigned long REGISTERS9_1::load8u(unsigned long addr)
{
	endian_access e;
	e.w = REGISTERS9_1::load32(addr & (~3));
	return e.b[addr & 3];
}

void REGISTERS9_1::load32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/)
{
	logging<_DEFAULT>::logf("Load Multiple not supported at %08X", addr);
	DebugBreak_();
}

////////////////////////////////////////////////////////////////////////////////

REGISTERS7_1::REGISTERS7_1( const char *name, unsigned long color, unsigned long priority )
	: memory_region(name, color, priority)
{
	for (int i = 0; i < PAGES; i++)
		blocks[i].flags |= memory_block::PAGE_ACCESSHANDLER;
	for (int i = 0; i < SIZE; i++)
		names[i] = NONAME;

	DEF_NAME_2(0x0004, "^[DISPSTAT] Display Status");
	DEF_NAME_2(0x0006, "^[VCOUNT] V count comparison");
	DEF_NAME_2(0x0134, "^[DRCNT] Debug RCNT");
	DEF_NAME_2(0x0136, "^[EXTKEYIN] Key X/Y Input");
	DEF_NAME_2(0x0138, "^[RTC] RTC Realtime Clock Bus");
	DEF_NAME_2(0x013A, NONAME_NOLOG);
	DEF_NAME_2(0x0180, "^[IPCSYNC] IPC Synchronize Register");
	DEF_NAME_2(0x0184, "^[IPCFIFOCNT] IPC Fifo Control");
	DEF_NAME_4(0x0188, "^[IPCFIFOSEND] IPC Send Fifo");

	DEF_NAME_2(0x01C0, "^[SPICNT] SPI Bus Control");
	DEF_NAME_2(0x01C2, "^[SPIDATA] SPI Bus Data");
	DEF_NAME_4(0x0208, "^[IME] Interrupt Master Enable");
	DEF_NAME_4(0x0210, "^[IE] Interrupt Enable");
	DEF_NAME_4(0x0214, "^[IF] Interrupt Flag");
	DEF_NAME_2(0x0304, "^[POWCNT2] Sound/Wifi Power Control Register");
}

// probably going to outsource SPI

void REGISTERS7_1::set_spicnt(unsigned long value)
{
	spicnt = value & 0xFFFF;
}

void REGISTERS7_1::set_spidat(unsigned long value)
{
	//spidata = rand();
	spidata = 0;
	unsigned long chn = (value >> 4) & 0x7;
	unsigned long differential = (value & 0x4);
	switch ((spicnt >> 8) & 3)
	{
	case 0: break; // Powerman
	case 1: break; // Firmware
	case 2: // Touchscreen
		switch (chn)
		{
		case 0:        // Temperature 0
			//spidata = 0x100;
			break; 
		case 1:        // Touchscreen Y
			if (differential)
				spidata = 0x1;
			else spidata = 0x6B0; // B0-F20 (FFF = disable)
			break; 
		case 2: break; // Battery Voltage
		case 3: break; // Touchscreen Z1
		case 4: break; // Touchscreen Z2
		case 5:        // Touchscreen X
			if (differential)
				spidata = 0x1;
			else spidata = 0x700; // 100-ED0 (0 = disable)
			break;
		case 6: break; // AUX input
		case 7: break; // Temperature 1
		}
		break; 
	case 3: break; // Reserved
	}
	//logging<_ARM7>::logf("SPIDATA=%04X", value & 0xFFFF);
}


void REGISTERS7_1::store32(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM7>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	unsigned long &current = *(unsigned long*)(&b->mem[subaddr]);
	unsigned long addr4k = addr & 0x1FFF;

	switch (addr4k)
	{
	case 0x180:
		memory::registers9_1.set_ipc(value);
		break;
	case 0x188:
		logging<_ARM7>::logf("Pushing %08X to ARM7::FIFO", value);
		push_fifo(value);
		memory::registers9_1.flag_fifo(get_fifostate());
		interrupt<_ARM9>::fire(18); // IPC Recv FIFO Not Empty
		break;
	case 0x01C0: // SPI requires the 16bit handler!
		{
			endian_access e;
			e.w = value;
			store16(addr, e.h[0]);
			store16(addr+2, e.h[1]);
			return;
		}
		break;
	}

	// debug output names
	const char* lastname = "";
	for (int i = 0; i < 4; i++)
	{
		const char* name = names[addr4k+i];
		if (name == lastname)
			continue;
		lastname = name;
		if (name == NONAME) logging<_ARM7>::logf("Unhandled write to ARM7::IO::%08X", addr+i);
		else if (name[0] != '^') logging<_ARM7>::logf("Write to ARM7::IO::%08X [%s]", addr+i, name);
	}

	if (current != value)
	{
		current = value;
		b->dirty();
	}
}

void REGISTERS7_1::store16(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM7>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~1));
	// uncomment as soon as needed
	unsigned short &current = *(unsigned short*)(&b->mem[subaddr]);

	// special 16bit handling can be added here
	switch (addr & 0x1FFF)
	{
	case 0x01C0: set_spicnt(value); break;
	case 0x01C2: set_spidat(value); break;
	default:
		endian_access e = *(endian_access*)(&b->mem[subaddr]);
		e.h[(addr >> 1) & 1] = (unsigned short)value;
		return store32(addr & ~3, e.w);
	}

	// uncomment when adding special case
	if (current != value)
	{
		current = (unsigned short)value;
		b->dirty();
	}
}

void REGISTERS7_1::store8(unsigned long addr, unsigned long value)
{
	memory_block *b = memory_map<_ARM7>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK);
	// uncomment as soon as needed
	//unsigned char &current = *(unsigned char*)(&b->mem[subaddr]);

	// special 8bit handling can be added here
	switch (addr & 0x1FFF)
	{
	case 0:
	default:
		endian_access e = *(endian_access*)(&b->mem[subaddr & (~3)]);
		e.b[addr & 3] = (unsigned char)value;
		return store32(addr & ~3, e.w);
	}
}

void REGISTERS7_1::store32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/)
{
	logging<_DEFAULT>::logf("Store Multiple not supported at %08X", addr);
	DebugBreak_();
}

unsigned long REGISTERS7_1::load32(unsigned long addr)
{
	memory_block *b = memory_map<_ARM7>::addr2page(addr);
	unsigned long subaddr = addr & (PAGING::ADDRESS_MASK & (~3));
	unsigned long &current = *(unsigned long*)(&b->mem[subaddr]);
	unsigned long addr4k = addr & 0x1FFF;

	// debug output names
	const char* lastname = "";
	for (int i = 0; i < 4; i++)
	{
		const char* name = names[addr4k+i];
		if (name == lastname)
			continue;
		lastname = name;
		if (name == NONAME) logging<_ARM7>::logf("Unhandled load from ARM7::IO::%08X", addr+i);
		else if (name[0] != '^') logging<_ARM7>::logf("Load from ARM7::IO::%08X [%s]", addr+i, name);
	}

	switch (addr4k)
	{
	case 0x0136: return 0xD; // TODO: this is a temporary hack reporting no X/Y/DEBUG button
	case 0x0180: return get_ipc();
	case 0x0184: return get_fifocnt();
	case 0x01C0: // SPI requires the 16bit handler!
		{
			endian_access e;
			e.h[0] = (unsigned short)load16u(addr);
			e.h[1] = (unsigned short)load16u(addr+2);
			return e.w;
		}
		break;
	}
	return current;
}

unsigned long REGISTERS7_1::load16u(unsigned long addr)
{
	// SPI requires the 16bit handler!
	unsigned long addr4k = addr & 0x1FFF;
	switch (addr4k)
	{
	case 0x01C0: return 0;
	case 0x01C2: return spidata;// SPI requires the 16bit handler!
	default:
		endian_access e;
		e.w = REGISTERS7_1::load32(addr & (~3));
		return e.h[(addr >> 1) & 1];
	}
}

unsigned long REGISTERS7_1::load16s(unsigned long addr)
{
	return (signed short)REGISTERS7_1::load16u(addr);
}

unsigned long REGISTERS7_1::load8u(unsigned long addr)
{
	endian_access e;
	e.w = REGISTERS7_1::load32(addr & (~3));
	return e.b[addr & 3];
}

void REGISTERS7_1::load32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/)
{
	logging<_DEFAULT>::logf("Load Multiple not supported at %08X", addr);
	DebugBreak_();
}

////////////////////////////////////////////////////////////////////////////////

TRANSFER9::TRANSFER9( const char *name, unsigned long color, unsigned long priority )
	: memory_region(name, color, priority)
{
	for (int i = 0; i < PAGES; i++)
		blocks[i].flags |= memory_block::PAGE_ACCESSHANDLER;
}

unsigned long TRANSFER9::load32(unsigned long addr)
{
	switch (addr & PAGING::ADDRESS_MASK)
	{
	case 0:
		{
			// check the ARM7 fifo state and return a value when available
			unsigned long value;
			if (!memory::registers7_1.pop_fifo(value))
			{
				logging<_ARM9>::logf("ARM7::FIFO underflow", addr);
				DebugBreak_();
			}
			logging<_ARM9>::logf("Poping %08X from ARM7::FIFO", value);
			memory::registers9_1.flag_fifo(memory::registers7_1.get_fifostate());
			return value;
		}
	}
	logging<_ARM9>::logf("Invalid read from %08X", addr);
	DebugBreak_();
	return 0;
}

// default handling starts here
void TRANSFER9::store32(unsigned long addr, unsigned long /*value*/)
{
	logging<_ARM9>::logf("Invalid write to %08X", addr);
}
void TRANSFER9::store16(unsigned long addr, unsigned long /*value*/) { TRANSFER9::store32(addr, 0); }
void TRANSFER9::store8(unsigned long addr, unsigned long /*value*/) { TRANSFER9::store32(addr, 0); };
void TRANSFER9::store32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/) { TRANSFER9::store32(addr, 0); };


unsigned long TRANSFER9::load16u(unsigned long addr)
{
	endian_access e;
	e.w = TRANSFER9::load32(addr & (~3));
	return e.h[(addr >> 1) & 1];
}
unsigned long TRANSFER9::load16s(unsigned long addr)
{
	return (signed short)TRANSFER9::load16u(addr);
}
unsigned long TRANSFER9::load8u(unsigned long addr)
{
	endian_access e;
	e.w = TRANSFER9::load32(addr & (~3));
	return e.b[addr & 3];
}
void TRANSFER9::load32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/) 
{ 
	DebugBreak_(); 
}

////////////////////////////////////////////////////////////////////////////////

TRANSFER7::TRANSFER7( const char *name, unsigned long color, unsigned long priority )
	: memory_region(name, color, priority)
{
	for (int i = 0; i < PAGES; i++)
		blocks[i].flags |= memory_block::PAGE_ACCESSHANDLER;
}

unsigned long TRANSFER7::load32(unsigned long addr)
{
	switch (addr & PAGING::ADDRESS_MASK)
	{
	case 0:
		{
			// check the ARM7 fifo state and return a value when available
			unsigned long value;
			if (!memory::registers9_1.pop_fifo(value))
			{
				logging<_ARM7>::logf("ARM9::FIFO underflow", addr);
				DebugBreak_();
			}
			logging<_ARM7>::logf("Pushing %08X to ARM9::FIFO", value);
			memory::registers7_1.flag_fifo(memory::registers9_1.get_fifostate());
			return value;
		}
	}
	logging<_ARM7>::logf("Invalid read from %08X", addr);
	DebugBreak_();
	return 0;
}

// default handling starts here
void TRANSFER7::store32(unsigned long addr, unsigned long /*value*/)
{
	logging<_ARM7>::logf("Invalid write to %08X", addr);
}
void TRANSFER7::store16(unsigned long addr, unsigned long /*value*/) { TRANSFER7::store32(addr, 0); }
void TRANSFER7::store8(unsigned long addr, unsigned long /*value*/) { TRANSFER7::store32(addr, 0); };
void TRANSFER7::store32_array(unsigned long addr, int /*num*/, unsigned long * /*data*/) { TRANSFER7::store32(addr, 0); };


unsigned long TRANSFER7::load16u(unsigned long addr)
{
	endian_access e;
	e.w = TRANSFER7::load32(addr & (~3));
	return e.h[(addr >> 1) & 1];
}
unsigned long TRANSFER7::load16s(unsigned long addr)
{
	return (signed short)TRANSFER7::load16u(addr);
}
unsigned long TRANSFER7::load8u(unsigned long addr)
{
	endian_access e;
	e.w = TRANSFER7::load32(addr & (~3));
	return e.b[addr & 3];
}
void TRANSFER7::load32_array(unsigned long /*addr*/, int /*num*/, unsigned long * /*data*/) 
{ 
	DebugBreak_(); 
}
