#ifndef M68EMU_H
#define M68EMU_H

typedef enum {
	CPUTYPE_M68HC05
} M68_CPUTYPE;

/**
 * Emulation context structure
 */
typedef struct M68_CTX {
	uint8_t			reg_acc;				///< Accumulator register
	uint8_t			reg_x;					///< X-index register
	uint16_t		reg_sp;					///< Stack pointer
	uint16_t		reg_pc;					///< Program counter
	uint8_t			reg_ccr;				///< Condition code register
	M68_CPUTYPE		cpuType;				///< CPU type
	bool			irq;					///< IRQ input state
	uint16_t		sp_and, sp_or;			///< Stack pointer AND/OR masks
	uint16_t		pc_and;					///< Program counter AND mask
	bool			is_stopped;				///< True if processor is stopped
	bool			is_waiting;				///< True if processor is WAITing
	uint8_t (*read_mem) (const struct M68_CTX *ctx, const uint16_t addr);	/* memory read function */
	void    (*write_mem)(const struct M68_CTX *ctx, const uint16_t addr, const uint8_t data);	/* memory write function */
} M68_CTX;

/* CCR bits */
#define		M68_CCR_H	0x10		/* Half carry */
#define		M68_CCR_I	0x08		/* Interrupt mask */
#define		M68_CCR_N	0x04		/* Negative */
#define		M68_CCR_C	0x02		/* Carry/borrow */
#define		M68_CCR_Z	0x01		/* Zero */


#endif // M68EMU_H
