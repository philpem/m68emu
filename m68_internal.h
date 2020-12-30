#ifndef M68_INTERNAL_H
#define M68_INTERNAL_H

#include <stdint.h>

//! Addressing modes
typedef enum {
	AMODE_DIRECT,
	AMODE_DIRECT_REL,
	AMODE_EXTENDED,
	AMODE_IMMEDIATE,
	AMODE_INDEXED0,
	AMODE_INDEXED1,
	AMODE_INDEXED2,
	AMODE_INHERENT,
	AMODE_INHERENT_A,
	AMODE_INHERENT_X,
	AMODE_RELATIVE,
	AMODE_ILLEGAL,
	AMODE_MAX
} M68_AMODE;


typedef struct {
	char *			mnem;		/* instruction mnemonic */
	M68_AMODE		amode;		/* addressing mode */
	uint8_t			cycles;		/* number of cycles to execute */
	uint8_t (*opfunc)(M68_CTX *ctx, const uint8_t opcode, const uint8_t param);	/* opcode exec function */
} M68_OPTABLE_ENT;


extern M68_OPTABLE_ENT m68hc05_optable[256];

// M68HC vector addresses.
static const uint16_t _M68_RESET_VECTOR = 0xFFFE;
static const uint16_t _M68_SWI_VECTOR   = 0xFFFC;
static const uint16_t _M68_INT_VECTOR   = 0xFFFA;

// TODO - need to have an m68_int function which allows vector address to be passed at runtime

#endif // M68_INTERNAL_H
