#ifndef M68TMR_H
#define M68TMR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct M68TRM_CTX {
	uint8_t tcr;
	uint8_t tdr;
	uint8_t prescaler;
} M68TMR_CTX;


void tmr_init(M68TMR_CTX* ctx);


bool tmr_exec(M68TMR_CTX* ctx, uint64_t cycles,bool pin);
void tmr_read(M68TMR_CTX* ctx,uint16_t addr,uint8_t* val);
void tmr_write(M68TMR_CTX* ctx,uint16_t addr,uint8_t val);


#endif
