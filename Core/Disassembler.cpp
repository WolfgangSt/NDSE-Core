#include "Disassembler.h"


#ifdef UPCASE

const char* INST::strings[] = {
	"UD", 
	"%?DEBUG",
	"UNKNOWN",

	// missing U-BIT (-) for register based addressing
	"STR%c %Rd,[%-%Rn],#0x%I",   // OK 
	"LDR%c %Rd,[%-%Rn],#0x%I",   // OK
	"STR%cT %Rd,[%-%Rn],#0x%I",  // OK
	"LDR%cT %Rd,[%-%Rn],#0x%I",  // OK 
	"STR%cB %Rd,[%-%Rn],#0x%I",  // OK
	"LDR%cB %Rd,[%-%Rn],#0x%I",  // OK
	"STR%cBT [%Rd],%-%Rn,#0x%I",  // OK
	"LDR%cBT [%Rd],%-%Rn,#0x%I",  // OK
	"STR%c %Rd,[%-%Rn,#0x%I]",   // OK 
	"LDR%c %Rd,[%-%Rn,#0x%I]",   // OK
	"STR%c %Rd,[%-%Rn,#0x%I]!",  // OK 
	"LDR%c %Rd,[%-%Rn,#0x%I]!",  // OK
	"STR%cB %Rd,[%-%Rn,#0x%I]",  // OK
	"LDR%cB %Rd,[%-%Rn,#0x%I]",  // OK
	"STR%cB %Rd,[%-%Rn,#0x%I]!", // OK
	"LDR%cB %Rd,[%-%Rn,#0x%I]!", // OK

	"STR%c %Rd,[%-%Rn],%Rm %S#0x%I",
	"LDR%c %Rd,[%-%Rn],%Rm %S#0x%I",
	"STR%cT %Rd,[%-%Rn],%Rm %S#0x%I",
	"LDR%cT %Rd,[%-%Rn],%Rm %S#0x%I",
	"STR%cB %Rd,[%-%Rn],%Rm %S#0x%I",
	"LDR%cB %Rd,[%-%Rn],%Rm %S#0x%I",
	"STR%cBT %Rd,[%-%Rn],%Rm %S#0x%I",
	"LDR%cBT %Rd,[%-%Rn],%Rm %S#0x%I",
	"STR%c %Rd,[%-%Rn,%Rm %S#0x%I]",
	"LDR%c %Rd,[%-%Rn,%Rm %S#0x%I]",
	"STR%c %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"LDR%c %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"STR%cB %Rd,[%-%Rn,%Rm %S#0x%I]",
	"LDR%cB %Rd,[%-%Rn,%Rm %S#0x%I]",
	"STR%cB %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"LDR%cB %Rd,[%-%Rn,%Rm %S#0x%I]!",

	"AND%c%s %Rd,%Rn,#0x%I",
	"EOR%c%s %Rd,%Rn,#0x%I",
	"SUB%c%s %Rd,%Rn,#0x%I",
	"RSB%c%s %Rd,%Rn,#0x%I",
	"ADD%c%s %Rd,%Rn,#0x%I",
	"ADC%c%s %Rd,%Rn,#0x%I",
	"SBC%c%s %Rd,%Rn,#0x%I",
	"RSC%c%s %Rd,%Rn,#0x%I",
	"TST%c %Rn,#0x%I", //s-bit here
	"TEQ%c %Rn,#0x%I", // means a 
	"CMP%c %Rn,#0x%I", // different
	"CMN%c %Rn,#0x%I", // instruction
	"ORR%c%s %Rd,%Rn,#0x%I",
	"MOV%c%s %Rd,#0x%I",
	"BIC%c%s %Rd,%Rn,#0x%I",
	"MVN%c%s %Rd,#0x%I",

	"MSR CPSR_%F,#0x%I",
	"MSR SPSR_%F,#0x%I",

	"AND%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"EOR%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"SUB%c%s %Rd,%Rn,%Rm,%S#0x%I", // OK
	"RSB%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"ADD%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"ADC%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"SBC%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"RSC%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"TST%c %Rn,%Rm,%S#0x%I", //s-bit here
	"TEQ%c %Rn,%Rm,%S#0x%I", // means a 
	"CMP%c %Rn,%Rm,%S#0x%I", // different    // OK
	"CMN%c %Rn,%Rm,%S#0x%I", // instruction
	"ORR%c%s %Rd,%Rn,%Rm,%S#0x%I", // OK
	"MOV%c%s %Rd,%Rm,%S#0x%I",     // OK
	"BIC%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"MVN%c%s %Rd,%Rm,%S#0x%I",

	"AND%c%s %Rd,%Rn,%Rm,%S %Rs",
	"EOR%c%s %Rd,%Rn,%Rm,%S %Rs",
	"SUB%c%s %Rd,%Rn,%Rm,%S %Rs",
	"RSB%c%s %Rd,%Rn,%Rm,%S %Rs",
	"ADD%c%s %Rd,%Rn,%Rm,%S %Rs",
	"ADC%c%s %Rd,%Rn,%Rm,%S %Rs",
	"SBC%c%s %Rd,%Rn,%Rm,%S %Rs",
	"RSC%c%s %Rd,%Rn,%Rm,%S %Rs",
	"TST%c %Rn,%Rm,%S %Rs", //s-bit here
	"TEQ%c %Rn,%Rm,%S %Rs", // means a 
	"CMP%c %Rn,%Rm,%S %Rs", // different
	"CMN%c %Rn,%Rm,%S %Rs", // instruction
	"ORR%c%s %Rd,%Rn,%Rm,%S %Rs",
	"MOV%c%s %Rd,%Rm,%S %Rs",
	"%?BIC%c%s %Rd,%Rn,%Rm,%S %Rs",
	"MVN%c%s %Rd,%Rm,%S %Rs",

	"MSR%c CPSR_%F,%Rm",
	"MSR%c SPSR_%F,%Rm",
	"MRS%c %Rd,CPSR",
	"MRS%c %Rd,SPSR",

	"CLZ%c %Rd,%Rm",
	"BKPT #0x%I",
	"SWI #0x%I",

	"BX%c %Rm",
	"BLX%c %Rm",
	"B%c #0x%A",
	"BL%c #0x%A",

	"MRC%c %Cp,%C1,%Rd,%CRn,%CRm,%C2",
	"MCR%c %Cp,%C1,%Rd,%CRn,%CRm,%C2",

	"STM%c%M %Rn,{%RL}%^",
	"LDM%c%M %Rn,{%RL}%^",
	"STM%c%M %Rn!,{%RL}%^",
	"LDM%c%M %Rn!,{%RL}%^",

	"STR%c%E %Rd,[%-%Rn],#0x%I",
	"LDR%c%E %Rd,[%-%Rn],#0x%I",
	"STR%c%E %Rd,[%-%Rn],#0x%I!",
	"LDR%c%E %Rd,[%-%Rn],#0x%I!",
	"STR%c%E %Rd,[%-%Rn,#0x%I]",
	"LDR%c%E %Rd,[%-%Rn,#0x%I]",
	"STR%c%E %Rd,[%-%Rn,#0x%I]!",
	"LDR%c%E %Rd,[%-%Rn,#0x%I]!",
	"STR%c%E %Rd,[%-%Rn],%Rm",
	"LDR%c%E %Rd,[%-%Rn],%Rm",
	"STR%c%E %Rd,[%-%Rn],%Rm!",
	"LDR%c%E %Rd,[%-%Rn],%Rm!",
	"STR%c%E %Rd,[%-%Rn,%Rm]",
	"LDR%c%E %Rd,[%-%Rn,%Rm]",
	"STR%c%E %Rd,[%-%Rn,%Rm]!",
	"LDR%c%E %Rd,[%-%Rn,%Rm]!",

	"BPRE #0x%A",
	"MUL%c%s %Rd,%Rm,%Rs",
	"MLA%c%s %Rd,%Rm,%Rs,%Rn",

	"UMULL%c%s %Rd,%Rn,%Rm,%Rs",
	"UMLAL%c%s %Rd,%Rn,%Rm,%R",
	"SMULL%c%s %Rd,%Rn,%Rm,%Rs",
	"SMLAL%c%s %Rd,%Rn,%Rm,%Rs",

	"SWP%c %Rd,%Rm,[%Rn]",
	"SWPB%c %Rd,%Rm,[%Rn]",

	"BLX%c #0x%A".
	"LDR%cD_R %Rd [%-%Rn]",
	"LDR%cD_RW %Rd [%-%Rn]",
	"LDR%cD_RI %Rd [%-%Rn]",
	"LDR%cD_RIW %Rd [%-%Rn]",
	"LDR%cD_RP %Rd [%-%Rn]",
	"LDR%cD_RPW %Rd [%-%Rn]",
	"LDR%cD_RIP %Rd [%-%Rn]",
	"LDR%cD_RIPW %Rd [%-%Rn]",

};

const char* SR_MASK::strings[16] = { 
	"",  "c",  "x",  "xc",  "s",  "sc",  "sx",  "sxc", 
	"f", "fc", "fx", "fxc", "fs", "fsc", "fsx", "fsxc"
};

const char* CONDITION::strings[16] = { 
	"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", 
	"HI", "LS", "GE", "LT", "GT", "LE", "",   "NV"
};

const char* SHIFT::strings[5] = { 
	"LSL", "LSR", "ASR", "ROR", "RXX"
};

const char* ADDRESSING_MODE::strings[4] = { 
	"DA", "IA", "DB", "IB", 
};

const char* EXTEND_MODE::strings[4] = {
	"", "H", "SB", "SH"
};

const char STRINGS::S_BIT = 'S';
const char STRINGS::R_PREFIX = 'R';

#else


const char* INST::strings[] = {
	"ud", 
	"%?debug",
	"unknown",

	// missing U-BIT (-) for register based addressing
	"str%c %Rd,[%-%Rn],#0x%I",   // OK 
	"ldr%c %Rd,[%-%Rn],#0x%I",   // OK
	"str%ct %Rd,[%-%Rn],#0x%I",  // OK
	"ldr%ct %Rd,[%-%Rn],#0x%I",  // OK 
	"str%cb %Rd,[%-%Rn],#0x%I",  // OK
	"ldr%cb %Rd,[%-%Rn],#0x%I",  // OK
	"str%cbt [%Rd],%-%Rn,#0x%I",  // OK
	"ldr%cbt [%Rd],%-%Rn,#0x%I",  // OK
	"str%c %Rd,[%-%Rn,#0x%I]",   // OK 
	"ldr%c %Rd,[%-%Rn,#0x%I]",   // OK
	"str%c %Rd,[%-%Rn,#0x%I]!",  // OK 
	"ldr%c %Rd,[%-%Rn,#0x%I]!",  // OK
	"str%cb %Rd,[%-%Rn,#0x%I]",  // OK
	"ldr%cb %Rd,[%-%Rn,#0x%I]",  // OK
	"str%cb %Rd,[%-%Rn,#0x%I]!", // OK
	"ldr%cb %Rd,[%-%Rn,#0x%I]!", // OK

	"str%c %Rd,[%-%Rn],%Rm %S#0x%I",
	"ldr%c %Rd,[%-%Rn],%Rm %S#0x%I",
	"str%ct %Rd,[%-%Rn],%Rm %S#0x%I",
	"ldr%ct %Rd,[%-%Rn],%Rm %S#0x%I",
	"str%cb %Rd,[%-%Rn],%Rm %S#0x%I",
	"ldr%cb %Rd,[%-%Rn],%Rm %S#0x%I",
	"str%cbt %Rd,[%-%Rn],%Rm %S#0x%I",
	"ldr%cbt %Rd,[%-%Rn],%Rm %S#0x%I",
	"str%c %Rd,[%-%Rn,%Rm %S#0x%I]",
	"ldr%c %Rd,[%-%Rn,%Rm %S#0x%I]",
	"str%c %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"ldr%c %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"str%cb %Rd,[%-%Rn,%Rm %S#0x%I]",
	"ldr%cb %Rd,[%-%Rn,%Rm %S#0x%I]",
	"str%cb %Rd,[%-%Rn,%Rm %S#0x%I]!",
	"ldr%cb %Rd,[%-%Rn,%Rm %S#0x%I]!",

	"and%c%s %Rd,%Rn,#0x%I",
	"eor%c%s %Rd,%Rn,#0x%I",
	"sub%c%s %Rd,%Rn,#0x%I",
	"rsb%c%s %Rd,%Rn,#0x%I",
	"add%c%s %Rd,%Rn,#0x%I",
	"adc%c%s %Rd,%Rn,#0x%I",
	"sbc%c%s %Rd,%Rn,#0x%I",
	"rsc%c%s %Rd,%Rn,#0x%I",
	"tst%c %Rn,#0x%I", //s-bit here
	"teq%c %Rn,#0x%I", // means a 
	"cmp%c %Rn,#0x%I", // different
	"cmn%c %Rn,#0x%I", // instruction
	"orr%c%s %Rd,%Rn,#0x%I",
	"mov%c%s %Rd,#0x%I",
	"bic%c%s %Rd,%Rn,#0x%I",
	"mvn%c%s %Rd,#0x%I",

	"msr cpsr_%F,#0x%I",
	"msr spsr_%F,#0x%I",

	"and%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"eor%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"sub%c%s %Rd,%Rn,%Rm,%S#0x%I", // OK
	"rsb%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"add%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"adc%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"sbc%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"rsc%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"tst%c %Rn,%Rm,%S#0x%I", //s-bit here
	"teq%c %Rn,%Rm,%S#0x%I", // means a 
	"cmp%c %Rn,%Rm,%S#0x%I", // different    // OK
	"cmn%c %Rn,%Rm,%S#0x%I", // instruction
	"orr%c%s %Rd,%Rn,%Rm,%S#0x%I", // OK
	"mov%c%s %Rd,%Rm,%S#0x%I",     // OK
	"bic%c%s %Rd,%Rn,%Rm,%S#0x%I",
	"mvn%c%s %Rd,%Rm,%S#0x%I",

	"and%c%s %Rd,%Rn,%Rm,%S %Rs",
	"eor%c%s %Rd,%Rn,%Rm,%S %Rs",
	"sub%c%s %Rd,%Rn,%Rm,%S %Rs",
	"rsb%c%s %Rd,%Rn,%Rm,%S %Rs",
	"add%c%s %Rd,%Rn,%Rm,%S %Rs",
	"adc%c%s %Rd,%Rn,%Rm,%S %Rs",
	"sbc%c%s %Rd,%Rn,%Rm,%S %Rs",
	"rsc%c%s %Rd,%Rn,%Rm,%S %Rs",
	"tst%c %Rn,%Rm,%S %Rs", //s-bit here
	"teq%c %Rn,%Rm,%S %Rs", // means a 
	"cmp%c %Rn,%Rm,%S %Rs", // different
	"cmn%c %Rn,%Rm,%S %Rs", // instruction
	"orr%c%s %Rd,%Rn,%Rm,%S %Rs",
	"mov%c%s %Rd,%Rm,%S %Rs",
	"%?bic%c%s %Rd,%Rn,%Rm,%S %Rs",
	"mvn%c%s %Rd,%Rm,%S %Rs",

	"msr%c cpsr_%F,%Rm",
	"msr%c spsr_%F,%Rm",
	"msr%c %Rd,cpsr",
	"mrs%c %Rd,spsr",

	"clz%c %Rd,%Rm",
	"bkpt #0x%I",
	"swi #0x%I",

	"bx%c %Rm",
	"blx%c %Rm",
	"b%c #0x%A",
	"bl%c #0x%A",

	"mrc%c %Cp,%C1,%Rd,%CRn,%CRm,%C2",
	"mcr%c %Cp,%C1,%Rd,%CRn,%CRm,%C2",

	"stm%c%M %Rn,{%RL}%^",
	"ldm%c%M %Rn,{%RL}%^",
	"stm%c%M %Rn!,{%RL}%^",
	"ldm%c%M %Rn!,{%RL}%^",

	"str%c%E %Rd,[%-%Rn],#0x%I",
	"ldr%c%E %Rd,[%-%Rn],#0x%I",
	"str%c%E %Rd,[%-%Rn],#0x%I!",
	"ldr%c%E %Rd,[%-%Rn],#0x%I!",
	"str%c%E %Rd,[%-%Rn,#0x%I]",
	"ldr%c%E %Rd,[%-%Rn,#0x%I]",
	"str%c%E %Rd,[%-%Rn,#0x%I]!",
	"ldr%c%E %Rd,[%-%Rn,#0x%I]!",
	"str%c%E %Rd,[%-%Rn],%Rm",
	"ldr%c%E %Rd,[%-%Rn],%Rm",
	"str%c%E %Rd,[%-%Rn],%Rm!",
	"ldr%c%E %Rd,[%-%Rn],%Rm!",
	"str%c%E %Rd,[%-%Rn,%Rm]",
	"ldr%c%E %Rd,[%-%Rn,%Rm]",
	"str%c%E %Rd,[%-%Rn,%Rm]!",
	"ldr%c%E %Rd,[%-%Rn,%Rm]!",

	"bpre #0x%A",
	"mul%c%s %Rd,%Rm,%Rs",
	"mla%c%s %Rd,%Rm,%Rs,%Rn",

	"umull%c%s %Rd,%Rn,%Rm,%Rs",
	"umlal%c%s %Rd,%Rn,%Rm,%R",
	"smull%c%s %Rd,%Rn,%Rm,%Rs",
	"smlal%c%s %Rd,%Rn,%Rm,%Rs",

	"swp%c %Rd,%Rm,[%Rn]",
	"swpb%c %Rd,%Rm,[%Rn]",

	"blx%c #0x%A",
	"? ldr%cd_R %Rd,[%-%Rn]",
	"? ldr%cd_RW %Rd,[%-%Rn]", // ! form
	"ldr%cd %Rd,[%Rn],%I",
	"? ldr%cd_RIW %Rd,[%-%Rn]", // ! form
	"? ldr%cd_RP %Rd,[%-%Rn]",
	"? ldr%cd_RPW %Rd,[%-%Rn]", // ! form
	"? ldr%cd_RIP %Rd,[%-%Rn]",
	"? ldr%cd_RIPW %Rd,[%-%Rn]", // !form
};

const char* SR_MASK::strings[16] = { 
	"",  "c",  "x",  "xc",  "s",  "sc",  "sx",  "sxc", 
	"f", "fc", "fx", "fxc", "fs", "fsc", "fsx", "fsxc"
};

const char* CONDITION::strings[16] = { 
	"eq", "ne", "cs", "cc", "mi", "pl", "vc", "vc", 
	"hi", "ls", "ge", "lt", "gt", "le", "",   "nv"
};

const char* SHIFT::strings[5] = { 
	"lsl", "lsr", "asr", "ror", "rxx"
};

const char* ADDRESSING_MODE::strings[4] = { 
	"da", "ia", "db", "ib", 
};

const char* EXTEND_MODE::strings[4] = {
	"", "h", "sb", "sh"
};

const char STRINGS::S_BIT = 's';
const char STRINGS::R_PREFIX = 'r';

template <>
void disassembler::decode<IS_ARM>(unsigned int op, unsigned int addr)
{
	ctx.op = op;
	ctx.flags = 0;
	ctx.addr = addr;
	ctx.shift = SHIFT::LSL;
	
	decode_condition();
	decode_instruction();
}

template <>
void disassembler::decode<IS_THUMB>(unsigned int op, unsigned int addr)
{
	ctx.op = op & 0xFFFF;
	ctx.flags = 0;
	ctx.addr = addr;
	ctx.shift = SHIFT::LSL;
	decode_instruction_thumb();
}


#endif