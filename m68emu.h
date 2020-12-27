#ifndef M68EMU_H
#define M68EMU_H

typedef enum {
	CPUTYPE_M68HC05
} M68_CPUTYPE;

typedef struct M68_CTX {
	uint8_t			reg_acc;
	uint8_t			reg_x;
	uint16_t		reg_sp;
	uint16_t		reg_pc;
	uint8_t			reg_ccr;
	M68_CPUTYPE		cpuType;
	bool			irq;
	uint16_t		sp_mask, sp_fixed;		/* Stack pointer mask and fixed bits */
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
