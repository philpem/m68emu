#include <stdint.h>
#include <stddef.h>
#include "m68emu.h"
#include "m68_internal.h"

/****************************************************************************
 * PROTOTYPES
 ****************************************************************************/

static uint8_t m68op_ADC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ADD(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_AND(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ASL(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ASR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BCC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BCLR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BCS(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BEQ(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BHCC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BHCS(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BHI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BIH(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BIL(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BIT(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BLS(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BMC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BMI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BMS(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BNE(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BPL(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BRA(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BRCLR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BRN(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BRSET(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BSET(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_BSR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_CLC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_CLI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_CLR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_CMP(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_COM(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_CPX(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_DEC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_EOR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_INC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_JMP(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_JSR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_LDA(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_LDX(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_LSR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_MUL(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_NEG(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_NOP(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ORA(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ROL(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_ROR(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_RSP(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_RTI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_RTS(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_SBC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_SEC(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_SEI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_STA(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_STOP(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_STX(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_SUB(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_SWI(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_TAX(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_TST(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_TXA(M68_CTX *ctx, const uint8_t param);
static uint8_t m68op_WAIT(M68_CTX *ctx, const uint8_t param);


/****************************************************************************
 * HELPER FUNCTIONS
 ****************************************************************************/

/// Update CCR half-carry bit
static inline void update_halfcarry(M68_CTX *ctx, const uint8_t a, const uint8_t m, const uint8_t r)
{
	ctx->reg_ccr =
		(( (a & 8) &&  (m & 0x08)) ||
		 ( (m & 8) && !(r & 0x08)) ||
		 (!(r & 8) &&  (a & 0x08)))
		? ctx->reg_ccr | CCR_H
		: ctx->reg_ccr & ~CCR_H;
}


/****************************************************************************
 * OPCODE IMPLEMENTATION
 ****************************************************************************/

/// ADC: Add with carry
uint8_t m68op_ADC(M68_CTX *ctx, const uint8_t param)
{
	uint16_t acc = ctx->reg_acc;

	acc += param;
	acc += ctx->reg_ccr & CCR_C ? 1 : 0;

	ctx->reg_acc = acc;

}

uint8_t m68op_ADD(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_AND(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_ASL(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_ASR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BCC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BCLR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BCS(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BEQ(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BHCC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BHCS(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BHI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BIH(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BIL(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BIT(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BLS(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BMC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BMI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BMS(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BNE(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BPL(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BRA(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BRCLR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BRN(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BRSET(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BSET(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_BSR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_CLC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_CLI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_CLR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_CMP(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_COM(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_CPX(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_DEC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_EOR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_INC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_JMP(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_JSR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_LDA(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_LDX(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_LSR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_MUL(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_NEG(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_NOP(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_ORA(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_ROL(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_ROR(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_RSP(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_RTI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_RTS(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_SBC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_SEC(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_SEI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_STA(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_STOP(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_STX(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_SUB(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_SWI(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_TAX(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_TST(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_TXA(M68_CTX *ctx, const uint8_t param)
{
}

uint8_t m68op_WAIT(M68_CTX *ctx, const uint8_t param)
{
}


/****************************************************************************
 * OPCODE TABLES
 ****************************************************************************/

#include "m68_optab_hc05.h"

