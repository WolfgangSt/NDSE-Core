#ifndef _HLCORE_H_
#define _HLCORE_H_

#include "Disassembler.h"

// this is a proprietary new interface
// replacing all JIT with HLE

typedef unsigned long u32;
template <typename T> struct HLCore
{
	static void* funcs[INST::MAX_INSTRUCTIONS];
	static void init();

	// helper
	static int check_flags();

	// functions
	static void undefined();

	static void STR_I();
	static void LDR_I();
	static void STR_IW();
	static void LDR_IW();
	static void STRB_I();
	static void LDRB_I();
	static void STRB_IW();
	static void LDRB_IW();
	static void STR_IP(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void LDR_IP(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void STR_IPW(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void LDR_IPW(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void STRB_IP(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void LDRB_IP(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void STRB_IPW();
	static void LDRB_IPW();
		
	static void STR_R();
	static void LDR_R();
	static void STR_RW();
	static void LDR_RW();
	static void STRB_R();
	static void LDRB_R();
	static void STRB_RW();
	static void LDRB_RW();
	static void STR_RP();
	static void LDR_RP();
	static void STR_RPW();
	static void LDR_RPW();
	static void STRB_RP();
	static void LDRB_RP();
	static void STRB_RPW();
	static void LDRB_RPW();

	static void AND_I();
	static void EOR_I();
	static void SUB_I();
	static void RSB_I();
	static void ADD_I(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void ADC_I();
	static void SBC_I();
	static void RSC_I();
	static void TST_I();
	static void TEQ_I();
	static void CMP_I();
	static void CMN_I();
	static void ORR_I(u32 flags, u32 imm, u32 Rn, u32 Rd);
	static void MOV_I(u32 flags, u32 imm, u32 Rd);
	static void BIC_I();
	static void MVN_I();

	static void MSR_CPSR_I(u32 flags, u32 mask, u32 imm);
	static void MSR_SPSR_I();

	static void AND_R();
	static void EOR_R();
	static void SUB_R();
	static void RSB_R();
	static void ADD_R();
	static void ADC_R();
	static void SBC_R();
	static void RSC_R();
	static void TST_R();
	static void TEQ_R();
	static void CMP_R();
	static void CMN_R();
	static void ORR_R();
	static void MOV_R(u32 flags, u32 imm, SHIFT::CODE shift, u32 Rm, u32 Rd);
	static void BIC_R();
	static void MVN_R();

	static void AND_RR();
	static void EOR_RR();
	static void SUB_RR();
	static void RSB_RR();
	static void ADD_RR();
	static void ADC_RR();
	static void SBC_RR();
	static void RSC_RR();
	static void TST_RR();
	static void TEQ_RR();
	static void CMP_RR();
	static void CMN_RR();
	static void ORR_RR();
	static void MOV_RR();
	static void BIC_RR();
	static void MVN_RR();

	static void MSR_CPSR_R(u32 flags, u32 Rm, u32 mask);
	static void MSR_SPSR_R();
	static void MRS_CPSR();
	static void MRS_SPSR();

	static void CLZ();
	static void BKPT();
	static void SWI();

	static void BX();
	static void BLX();
	static void B();
	static void BL();

	static void MRC();
	static void MCR(u32 flags, u32 C2, u32 CRm, u32 CRn, u32 Rd, u32 C1, u32 Cp);

	static void STM();
	static void LDM();
	static void STM_W();
	static void LDM_W();


	static void STRX_I();
	static void LDRX_I();
	static void STRX_IW();
	static void LDRX_IW();
	static void STRX_IP(u32 flags, u32 imm, u32 Rn, u32 Rd, EXTEND_MODE::CODE mode);
	static void LDRX_IP(u32 flags, u32 imm, u32 Rn, u32 Rd, EXTEND_MODE::CODE mode);
	static void STRX_IPW();
	static void LDRX_IPW();
	static void STRX_R();
	static void LDRX_R();
	static void STRX_RW();
	static void LDRX_RW();
	static void STRX_RP();
	static void LDRX_RP();
	static void STRX_RPW();
	static void LDRX_RPW();

	static void BPRE();

	static void MUL_R();
	static void MLA_R();

	static void UMULL();
	static void UMLAL();
	static void SMULL();
	static void SMLAL();

	static void SWP();
	static void SWPB();

	static void BLX_I();

	static void LDRD_R();
	static void LDRD_RW();
	static void LDRD_RI();
	static void LDRD_RIW();
	static void LDRD_RP();
	static void LDRD_RPW();
	static void LDRD_RIP();
	static void LDRD_RIPW();

	static void PLD_I();
	static void PLD_R();
};

bool InitHLCore();

#endif