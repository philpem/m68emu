#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "m68emu.h"
#include "m68_internal.h"

static uint8_t m68op_BRA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param);

/****************************************************************************
 * HELPER FUNCTIONS
 ****************************************************************************/

/**
 * Carry flag calculation method selection.
 *
 * Instructions calculate the carry flag state in different ways:
 *
 *   * ADC, ADD: (A7 && M7) || (M7 && !R7) || (!R7 && A7)
 *   * ASL, LSL, ROL: b7
 *   * ASR, LSR, ROR: b0
 *   * BRCLR, BRSET: Mn
 *   * CLC, MUL: 0
 *   * NEG: R != 0
 *   * CMP, CPX*, SBC: (!A7 && M7) || (M7 && R7) || (R7 && !A7)
 *     (* CPX: substitute X7 in place of A7)
 *
 * The shift and negation instructions handle carry themselves.
 * This means update_flags() only needs to implement Add and Subtract carry
 * logic.
 *
 * CARRY_ADD: Addition carry
 * CARRY_SUB: Subtraction carry
 * CARRY_UNDEFINED: Raise an assertion failure if this is ever encountered
 *
 * We could in theory have CARRY_CLEAR and CARRY_SET in here, but I think it's
 * preferable to have an explicit force_flags() call for the carry instead of
 * hiding carry set/clear inside this call.
 */
typedef enum {
	CARRY_ADD,
	CARRY_SUB,
	CARRY_UNDEFINED
} M68_CARRY_TYPE;

/**
 * Update the CCR flag bits after an arithmetic operation.
 *
 * @param	ctx			Emulation context
 * @param	ccr_bits	CCR bit map (OR of M68_CCR_x constants)
 * @param	a			Accumulator initial value (or X-register for CPX)
 * @param	m			Operation parameter
 * @param	r			Operation result
 */
static inline void update_flags(M68_CTX *ctx, const uint8_t ccr_bits, const uint8_t a, const uint8_t m, const uint8_t r, const M68_CARRY_TYPE carryMode)
{
	// Half Carry
	if (ccr_bits & M68_CCR_H) {
		if (( (a & 0x08) &&  (m & 0x08)) ||
				( (m & 0x08) && !(r & 0x08)) ||
				(!(r & 0x08) &&  (a & 0x08))) {
			ctx->reg_ccr |= M68_CCR_H;
		} else {
			ctx->reg_ccr &= ~M68_CCR_H;
		}
	}

	// Negative
	if (ccr_bits & M68_CCR_N) {
		if (r & 0x80) {
			ctx->reg_ccr |= M68_CCR_N;
		} else {
			ctx->reg_ccr &= ~M68_CCR_N;
		}
	}

	// Zero
	if (ccr_bits & M68_CCR_Z) {
		if (r == 0) {
			ctx->reg_ccr |= M68_CCR_Z;
		} else {
			ctx->reg_ccr &= ~M68_CCR_Z;
		}
	}

	// Carry
	// TODO:  Carry is calculated in different ways depending on instruction. Implement the different modes.
	if (ccr_bits & M68_CCR_C) {
		bool newCarry;
		switch (carryMode) {
			case CARRY_ADD:
				newCarry =
					(( (a & 0x80) &&  (m & 0x80)) ||
					 ( (m & 0x80) && !(r & 0x80)) ||
					 (!(r & 0x80) &&  (a & 0x80)));
				break;

			case CARRY_SUB:
				newCarry =
					((!(a & 0x80) &&  (m & 0x80)) ||
					 ( (m & 0x80) &&  (r & 0x80)) ||
					 ( (r & 0x80) && !(a & 0x80)));
				break;

			default:
				assert(0);
		}

		if (newCarry) {
			ctx->reg_ccr |= M68_CCR_C;
		} else {
			ctx->reg_ccr &= ~M68_CCR_C;
		}
	}

	// Force CCR top 3 bits to 1
	ctx->reg_ccr |= 0xE0;
}

/**
 * Force one or more flag bits to a given state
 *
 * @param	ctx			Emulation context
 * @param	ccr_bits	CCR bit map (OR of M68_CCR_x constants)
 * @param	state		State to set -- true for set, false for clear
 */
static inline void force_flags(M68_CTX *ctx, const uint8_t ccr_bits, const bool state)
{
	if (state) {
		ctx->reg_ccr |= ccr_bits;
	} else {
		ctx->reg_ccr &= ~ccr_bits;
	}

	// Force CCR top 3 bits to 1
	ctx->reg_ccr |= 0xE0;
}

/**
 * Get the state of a CCR flag bit
 *
 * @param	ctx			Emulation context
 * @param	ccr_bits	CCR bit (M68_CCR_x constant)
 */
static inline bool get_flag(M68_CTX *ctx, const uint8_t ccr_bit)
{
	return (ctx->reg_ccr & ccr_bit) ? true : false;
}

/**
 * Push a byte onto the stack
 *
 * @param	ctx			Emulation context
 * @param	value		Byte to PUSH
 */
static inline void push_byte(M68_CTX *ctx, const uint8_t value)
{
	ctx->write_mem(ctx, ctx->reg_sp, value);
	ctx->reg_sp = ((ctx->reg_sp - 1) & ctx->sp_and) | ctx->sp_or;
}

/**
 * Pop a byte off of the stack
 *
 * @param	ctx			Emulation context
 * @return	Byte popped off of the stack
 */
static inline uint8_t pop_byte(M68_CTX *ctx)
{
	ctx->reg_sp = ((ctx->reg_sp + 1) & ctx->sp_and) | ctx->sp_or;
	return ctx->read_mem(ctx, ctx->reg_sp);
}


/****************************************************************************
 * OPCODE IMPLEMENTATION
 ****************************************************************************/

/* **
 * Opcode implementation rules:
 *
 *   - Branch and jump opcodes
 *     - Return true to take the branch.
 *     - Return false to continue execution from the next instruction.
 *   - General opcodes
 *     - Inherent and immediate addressing:
 *       - The opfunc handles all register writes. Return value is ignored.
 *       - e.g. ADC
 *     - Inherent A / Inherent X addressing:
 *       - The opfunc return value is written back to the ACC / X register
 *       - e.g. LSRA, ASRX
 *     - Direct, Extended and Indexed addressing
 *       - The opfunc return value is written back to the memory location.
 *       - e.g. LSR, ASL
 *
 * When the opfunc is entered, the PC will point to the next instruction. This
 * is to make sure the opfunc doesn't need to worry about correcting pushed
 * return addresses -- which would require a case for every opcode.
 */


/// ADC: Add with carry
static uint8_t m68op_ADC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc + param + (ctx->reg_ccr & M68_CCR_C ? 1 : 0);

	update_flags(ctx, M68_CCR_H | M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_acc, param, result, CARRY_ADD);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// ADD: Add
static uint8_t m68op_ADD(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc + param;

	update_flags(ctx, M68_CCR_H | M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_acc, param, result, CARRY_ADD);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// AND: Logical AND
static uint8_t m68op_AND(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc & param;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// ASL: Arithmetic shift left (same as LSL)
static uint8_t m68op_ASL(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = (param << 1) & 0xFE;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, param & 0x80);

	// Result is written back to the source (in-place)
	return result;
}

/// ASR: Arithmetic shift right
static uint8_t m68op_ASR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = (param >> 1) | (param & 0x80);

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, param & 1);

	// Result is written back to the source (in-place)
	return result;
}

/// BCC: Branch carry clear (same as BHS)
static uint8_t m68op_BCC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_C));
}

/// BCLR: Clear bit in memory
static uint8_t m68op_BCLR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// Extract the bit number from the opcode
	uint8_t bitnum = (opcode & 0x0F) >> 1;

	return param & ~(1 << bitnum);
}

/// BCS: Branch carry set
static uint8_t m68op_BCS(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_C));
}

/// BEQ: Branch if equal
static uint8_t m68op_BEQ(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_Z));
}

/// BHCC: Branch if half-carry clear
static uint8_t m68op_BHCC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_H));
}

/// BHCS: Branch if half-carry set
static uint8_t m68op_BHCS(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_H));
}

/// BHI: Branch if higher
static uint8_t m68op_BHI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_C) && !get_flag(ctx, M68_CCR_Z));
}

// BHS: see BCC

/// BIH: Branch if /IRQ high
static uint8_t m68op_BIH(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (ctx->irq);
}

/// BIL: Branch if /IRQ low
static uint8_t m68op_BIL(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!ctx->irq);
}

/// BIT: Bit test memory with accumulator
static uint8_t m68op_BIT(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc & param;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);

	// Don't change the memory or the accumulator
	return param;
}

// BLO: see BCS

/// BLS: Branch if lower or same
static uint8_t m68op_BLS(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_C) || get_flag(ctx, M68_CCR_Z));
}

/// BNC: Branch if interrupt mask is clear
static uint8_t m68op_BMC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_I));
}

/// BMI: Branch if minus
static uint8_t m68op_BMI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_N));
}

/// BMS: Branch if interrupt mask is set
static uint8_t m68op_BMS(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (get_flag(ctx, M68_CCR_I));
}

/// BNE: Branch if not equal
static uint8_t m68op_BNE(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_Z));
}

/// BPL: Branch if plus
static uint8_t m68op_BPL(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return (!get_flag(ctx, M68_CCR_N));
}

/// BRA: Branch always
static uint8_t m68op_BRA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return true;
}

/// BRCLR: Branch if bit clear
static uint8_t m68op_BRCLR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// Extract the bit number from the opcode
	uint8_t bitnum = (opcode & 0x0F) >> 1;

	// Carry flag is set to the bit state
	force_flags(ctx, M68_CCR_C, param & (1 << bitnum) ? 1 : 0);

	return !(param & (1 << bitnum));
}

/// BRN: Branch never
static uint8_t m68op_BRN(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return false;
}

/// BRSET: Branch if bit set
static uint8_t m68op_BRSET(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// Extract the bit number from the opcode
	uint8_t bitnum = (opcode & 0x0F) >> 1;

	// Carry flag is set to the bit state
	force_flags(ctx, M68_CCR_C, param & (1 << bitnum) ? 1 : 0);

	return (param & (1 << bitnum));
}

/// BSET: Bit set
static uint8_t m68op_BSET(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// Extract the bit number from the opcode
	uint8_t bitnum = (opcode & 0x0F) >> 1;

	return param | (1 << bitnum);
}

/// BSR: Branch to subroutine
static uint8_t m68op_BSR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// push return address onto stack
	uint16_t ra = ctx->reg_pc;
	push_byte(ctx, ra & 0xff);
	push_byte(ctx, (ra >> 8) & 0xff);

	// take the branch
	return true;
}

/// CLC: Clear carry
static uint8_t m68op_CLC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_C, 0);
	return 0;	// inherent
}

/// CLI: Clear interrupt mask
static uint8_t m68op_CLI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_I, 0);
	return 0;	// inherent
}

/// CLR: Clear accumulator, X or register
static uint8_t m68op_CLR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_N, 0);
	force_flags(ctx, M68_CCR_Z, 1);
	return 0;	// inherent
}

/// CMP: Compare accumulator with memory
static uint8_t m68op_CMP(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = ctx->reg_acc - param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_acc, param, result, CARRY_SUB);

	// CMP affects flags only, not the parameter or accumulator
	return param;
}

/// COM: Complement
static uint8_t m68op_COM(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = ~param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, 1);

	return result;
}

/// CPX: Compare index register with memory
static uint8_t m68op_CPX(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = ctx->reg_x - param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_x, param, result, CARRY_SUB);

	// CPX affects flags only, not the parameter or accumulator
	return param;
}

/// DEC: Decrement
static uint8_t m68op_DEC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = param - 1;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);

	return result;
}

/// EOR: Exclusive-OR accumulator with memory
static uint8_t m68op_EOR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = ctx->reg_acc ^ param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);

	return result;
}

/// INC: Increment
static uint8_t m68op_INC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint8_t result = param + 1;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);

	return result;
}

/// JMP: Long jump
static uint8_t m68op_JMP(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return true;
}

/// JSR: Jump subroutine
static uint8_t m68op_JSR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// push return address onto stack
	uint16_t ra = ctx->reg_pc;
	push_byte(ctx, ra & 0xff);
	push_byte(ctx, (ra >> 8) & 0xff);

	// take the branch
	return true;
}

/// LDA: Load accumulator
static uint8_t m68op_LDA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	ctx->reg_acc = param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, param, CARRY_UNDEFINED);
	// input parameter is unaffected
	return param;
}

/// LDX: Load index
static uint8_t m68op_LDX(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	ctx->reg_x = param;
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, param, CARRY_UNDEFINED);
	// input parameter is unaffected
	return param;
}

/// LSR: Logical shift right
static uint8_t m68op_LSR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = (param >> 1);

	update_flags(ctx, M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_N, 0);
	force_flags(ctx, M68_CCR_C, param & 1);

	// Result is written back to the source (in-place)
	return result;
}

/// MUL: Multiply
static uint8_t m68op_MUL(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = (uint16_t)ctx->reg_x * (uint16_t)ctx->reg_acc;

	ctx->reg_x = (result >> 8) & 0xff;
	ctx->reg_acc = result & 0xff;

	force_flags(ctx, M68_CCR_H | M68_CCR_C, 0);
}

/// NEG: Negate (2's complement)
static uint8_t m68op_NEG(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = 0 - param;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, result != 0);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// NOP: No operation
static uint8_t m68op_NOP(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	return param;
}

/// ORA: Inclusive-OR
static uint8_t m68op_ORA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc | param;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// ROL: Rotate left
static uint8_t m68op_ROL(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ((param << 1) | (get_flag(ctx, M68_CCR_C) ? 1 : 0)) & 0xFF;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, param & 0x80);

	// Result is written back to the source (in-place)
	return result;
}

/// ROR: Rotate right
static uint8_t m68op_ROR(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ((param >> 1) | (get_flag(ctx, M68_CCR_C) ? 0x80 : 0)) & 0xFF;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, result, CARRY_UNDEFINED);
	force_flags(ctx, M68_CCR_C, param & 1);

	// Result is written back to the source (in-place)
	return result;
}

/// RSP: Reset stack pointer
static uint8_t m68op_RSP(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	ctx->reg_sp = 0xFF;	// TODO INITIAL_SP constant?
	return param;
}

/// RTI: Return from interrupt
static uint8_t m68op_RTI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// pop CCR, ACCA, X
	ctx->reg_ccr = pop_byte(ctx);
	ctx->reg_acc = pop_byte(ctx);
	ctx->reg_x   = pop_byte(ctx);

	// pop PCH:PCL
	// This is split up to force instruction sequencing (ref C99, sequence points!)
	uint16_t new_pc;
	new_pc = (uint16_t)pop_byte(ctx) << 8;
	new_pc |= pop_byte(ctx);
	ctx->reg_pc = new_pc & ctx->pc_and;

	return param;
}

/// RTS: Return from subroutine
static uint8_t m68op_RTS(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// pop PCH:PCL
	// This is split up to force instruction sequencing (ref C99, sequence points!)
	uint16_t new_pc;
	new_pc = (uint16_t)pop_byte(ctx) << 8;
	new_pc |= pop_byte(ctx);
	ctx->reg_pc = new_pc & ctx->pc_and;

	return param;
}

/// SBC: Subtract with carry
static uint8_t m68op_SBC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc - param - (ctx->reg_ccr & M68_CCR_C ? 1 : 0);

	update_flags(ctx, M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_acc, param, result, CARRY_SUB);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// SEC: Set carry flag
static uint8_t m68op_SEC(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_C, 1);
	return param;	// inherent
}

/// SEI: Set interrupt mask flag (disable interrupts)
static uint8_t m68op_SEI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_I, 1);
}

/// STA: Store accumulator
static uint8_t m68op_STA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_acc, param, ctx->reg_acc, CARRY_UNDEFINED);

	return ctx->reg_acc;	// store A in {param}
}

/// STOP: Enable IRQs, stop oscillator
static uint8_t m68op_STOP(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_I, 0);
	ctx->is_stopped = true;
	return param; // inherent
}

/// STX: Store X register
static uint8_t m68op_STX(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, ctx->reg_x, param, ctx->reg_x, CARRY_UNDEFINED);

	return ctx->reg_acc;	// store A in {param}
}

/// SUB: Subtract
static uint8_t m68op_SUB(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	uint16_t result = ctx->reg_acc - param;

	update_flags(ctx, M68_CCR_N | M68_CCR_Z | M68_CCR_C, ctx->reg_acc, param, result, CARRY_SUB);
	ctx->reg_acc = result;

	// ACC is always the affected register
	return param;
}

/// SWI: Software Interrupt
static uint8_t m68op_SWI(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	// TODO Lift this code for use by IRQ?
	// PC will already have been advanced by the emulation loop
	push_byte(ctx, ctx->reg_pc & 0xFF);
	push_byte(ctx, ctx->reg_pc >> 8);
	push_byte(ctx, ctx->reg_x);
	push_byte(ctx, ctx->reg_acc);
	push_byte(ctx, ctx->reg_ccr);

	// Mask further interrupts
	force_flags(ctx, M68_CCR_I, 1);

	// Vector fetch
	uint16_t vector;
	vector = (uint16_t)ctx->read_mem(ctx, 0xFFFC & ctx->pc_and) << 8;
	vector |= ctx->read_mem(ctx, 0xFFFD & ctx->pc_and);
	ctx->reg_pc = vector;
}

/// TAX: Transfer A to X
static uint8_t m68op_TAX(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	ctx->reg_x = ctx->reg_acc;
	return param;	// inherent
}

/// TST: Test if negative or zero
static uint8_t m68op_TST(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	update_flags(ctx, M68_CCR_N | M68_CCR_Z, 0, 0, param, CARRY_UNDEFINED);

	// Contents of the tested register or memory location are left unaltered.
	return param;
}

/// TXA: Transfer X to ACC
static uint8_t m68op_TXA(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	ctx->reg_acc = ctx->reg_x;
	return param;	// inherent
}

/// WAIT: Wait for interrupt. Like STOP, but leaves peripherals running.
static uint8_t m68op_WAIT(M68_CTX *ctx, const uint8_t opcode, const uint8_t param)
{
	force_flags(ctx, M68_CCR_I, 0);
	ctx->is_waiting = true;
	return param; // inherent
}


/****************************************************************************
 * OPCODE TABLES
 ****************************************************************************/

#include "m68_optab_hc05.h"

