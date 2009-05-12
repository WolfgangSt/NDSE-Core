#ifndef _NDSE_H_
#define _NDSE_H_

#include "Mem.h"
#include "MemRegionBase.h"
#include "Util.h"
#include "BreakpointBase.h"
#include "ArmContext.h"
#include "HostContext.h"
#include "CompiledBlock.h"
#include "jitcode.h"

#ifdef WIN32
#define STDCALL __stdcall 
#else
#define STDCALL
#endif

#ifndef EXPORT
	#ifdef WIN32
	#define IMPORT __declspec(dllimport)
	#else
	#define IMPORT
	#endif
#else
	#define IMPORT EXPORT
#endif

struct memory_block;

extern "C"{

typedef void (STDCALL *stream_callback)(memory_block*, char*, int, void*);
typedef void (STDCALL *log_callback)(const char* log);

IMPORT void STDCALL Init();

IMPORT memory_block* STDCALL ARM7_GetPage(unsigned long addr);
IMPORT const char* STDCALL ARM7_DisassembleA(unsigned long op, unsigned long addr);
IMPORT const char* STDCALL ARM7_DisassembleT(unsigned long op, unsigned long addr);
IMPORT void STDCALL ARM7_Log(log_callback cb);
IMPORT bool STDCALL ARM7_Run(unsigned long addr);
IMPORT bool STDCALL ARM7_Continue();
IMPORT void STDCALL ARM7_Step(breakpoint_defs::stepmode mode);
IMPORT emulation_context* ARM7_GetContext();
IMPORT host_context* STDCALL ARM7_GetException();
IMPORT breakpoint_defs::break_info* STDCALL ARM7_GetDebuggerInfo();
IMPORT void STDCALL ARM7_ToggleBreakpointA(unsigned long addr, unsigned long subcode);
IMPORT void STDCALL ARM7_ToggleBreakpointT(unsigned long addr, unsigned long subcode);
IMPORT const breakpoint_defs::break_data* STDCALL ARM7_GetBreakpointA(unsigned long addr);
IMPORT const breakpoint_defs::break_data* STDCALL ARM7_GetBreakpointT(unsigned long addr);
IMPORT void STDCALL ARM7_GetJITA(unsigned long addr, jit_code *code);
IMPORT void STDCALL ARM7_GetJITT(unsigned long addr, jit_code *code);
IMPORT void STDCALL ARM7_SetPC(unsigned long addr);

IMPORT memory_block* STDCALL ARM9_GetPage(unsigned long addr);
IMPORT const char* STDCALL ARM9_DisassembleA(unsigned long op, unsigned long addr);
IMPORT const char* STDCALL ARM9_DisassembleT(unsigned long op, unsigned long addr);
IMPORT void STDCALL ARM9_Log(log_callback cb);
IMPORT bool STDCALL ARM9_Run(unsigned long addr);
IMPORT bool STDCALL ARM9_Continue();
IMPORT void STDCALL ARM9_Step(breakpoint_defs::stepmode mode);
IMPORT emulation_context* ARM9_GetContext();
IMPORT host_context* STDCALL ARM9_GetException();
IMPORT breakpoint_defs::break_info* STDCALL ARM9_GetDebuggerInfo();
IMPORT void STDCALL ARM9_ToggleBreakpointA(unsigned long addr, unsigned long subcode);
IMPORT void STDCALL ARM9_ToggleBreakpointT(unsigned long addr, unsigned long subcode);
IMPORT const breakpoint_defs::break_data* STDCALL ARM9_GetBreakpointA(unsigned long addr);
IMPORT const breakpoint_defs::break_data* STDCALL ARM9_GetBreakpointT(unsigned long addr);
IMPORT void STDCALL ARM9_GetJITA(unsigned long addr, jit_code *code);
IMPORT void STDCALL ARM9_GetJITT(unsigned long addr, jit_code *code);
IMPORT void STDCALL ARM9_SetPC(unsigned long addr);

IMPORT bool STDCALL UTIL_LoadFile(const char *filename, util::load_result *result);
IMPORT memory_region_base* STDCALL MEM_GetVRAM(int bank);
IMPORT unsigned long STDCALL PageSize();
IMPORT unsigned long STDCALL DebugMax();
IMPORT const memory_region_base* STDCALL MEM_GetRegionInfo(memory_block *p);
IMPORT void STDCALL DEFAULT_Log(log_callback cb);

IMPORT const char* STDCALL DEBUGGER_GetSymbol(void *addr);

}

#endif
