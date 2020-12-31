#include <stdio.h>
#include "m68emu.h"

uint8_t memspace[0x2000];

uint64_t clockcount = 0;
double ns_per_clock = 1.e9 / 3.5e6;

uint8_t readfunc(struct M68_CTX *ctx, const uint16_t addr)
{
	if (addr == 0x15c7) {
		ctx->trace = true;
	}
	if (ctx->trace) {
		printf("  MEM RD %04X = %02X\n", addr, memspace[addr]);
	}

	if (addr == 0) {
		return 1;	// I/O pad always high
	}

	return memspace[addr];
}

void writefunc(struct M68_CTX *ctx, const uint16_t addr, const uint8_t data)
{
	if (ctx->trace) {
		printf("  MEM WR %04X = %02X\n", addr, data);
	}
	memspace[addr] = data;

	if (addr == 0) {
		printf("#%lu\n", clockcount);
		printf("%d#", data & 1);
	}
}

int main(void)
{
	M68_CTX ctx;

	FILE *fp=fopen("tacdump.bin", "rb");
	fread(memspace, 1, sizeof(memspace), fp);
	fclose(fp);

	ctx.read_mem  = &readfunc;
	ctx.write_mem = &writefunc;
	ctx.opdecode  = NULL;
	m68_init(&ctx, M68_CPU_HC05C4);
//	ctx.trace = true;

	printf("$timescale 1ns $end\n");
	printf("$var wire 1 # dq $end\n");

	do {
		clockcount += m68_exec_cycle(&ctx);
	} while (clockcount < 10000);
	printf("PC: %04x\n", ctx.reg_pc);
}

