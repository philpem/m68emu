#ifndef M68EMU_H
#define M68EMU_H

typedef enum {
	CPUTYPE_M68HC05
} M68_CPUTYPE;

typedef struct {
	uint8_t			reg_acc;
	uint8_t			reg_x;
	uint16_t		reg_sp;
	uint16_t		reg_pc;
	uint8_t			reg_ccr;
	M68_CPUTYPE		cpuType;
} M68_CTX;

/* CCR bits */
#define		CCR_H	0x10		/* Half carry */
#define		CCR_I	0x08		/* Interrupt mask */
#define		CCR_N	0x04		/* Negative */
#define		CCR_C	0x02		/* Carry/borrow */
#define		CCR_Z	0x01		/* Zero */


#endif // M68EMU_H
