.PHONY: all

CFLAGS += -g -ggdb -Wall

all:	m68em

m68em:	m68_ops.o m68emu.o m68test.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

m68_ops.o:	m68_optab_hc05.h m68_internal.h m68emu.h
m68emu.o:	m68_internal.h m68emu.h

#m68_internal_template.h:	optable/opcodes_m68hc05.csv optable/makeoptab.py m68emu.h
#	./optable/makeoptab.py $< m68op prototypes > $@
#
#m68_op_template.c:	optable/opcodes_m68hc05.csv m68emu.h optable/makeoptab.py
#	./optable/makeoptab.py $< m68op boilerplate > $@

m68_optab_hc05.h:	optable/opcodes_m68hc05.csv m68emu.h optable/makeoptab.py
	./optable/makeoptab.py $< m68hc05 optable > $@


