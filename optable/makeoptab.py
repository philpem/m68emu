#!/usr/bin/env python3

# Make opcode table
#
# Reads opcodes.csv and produces a C file containing the opcode table
#

import csv
import re
import sys
from enum import Enum, auto, unique

@unique
class AddressingMode(Enum):
    DIRECT = auto()
    DIRECT_REL = auto()
    DIRECT_JUMP = auto()
    EXTENDED = auto()
    EXTENDED_JUMP = auto()
    IMMEDIATE = auto()
    INDEXED0 = auto()
    INDEXED0_JUMP = auto()
    INDEXED1 = auto()
    INDEXED1_JUMP = auto()
    INDEXED2 = auto()
    INDEXED2_JUMP = auto()
    INHERENT = auto()
    INHERENT_A = auto()     # special case of INHERENT with A-reg as input
    INHERENT_X = auto()     # special case of INHERENT with X-reg as input
    RELATIVE = auto()
    ILLEGAL = auto()

    @staticmethod
    def from_str(_s: str):
        DIRECT_N_RE = re.compile("direct_[0-9]")
        DIRECT_REL_N_RE = re.compile("direct_rel_[0-9]")

        s = _s.lower()
        if s == 'dir' or DIRECT_N_RE.match(s):
            # Direct (e.g. ADC, JSR) or Direct with implicit bit number (BSET/BCLR)
            return AddressingMode.DIRECT
        elif DIRECT_REL_N_RE.match(s):
            # Direct with relative jump offset (BRSET/BRCLR)
            return AddressingMode.DIRECT_REL
        elif s == 'dir_jump':
            # Direct -- is a jump
            return AddressingMode.DIRECT_JUMP
        elif s == 'ext':
            # Extended -- instruction with 16-bit address parameter
            return AddressingMode.EXTENDED
        elif s == 'ext_jump':
            # Extended -- is a jump
            return AddressingMode.EXTENDED_JUMP
        elif s == 'imm':
            # Immediate -- instruction with 8-bit immediate value parameter
            return AddressingMode.IMMEDIATE
        elif s == 'indexed0':
            # Indexed with no offset
            return AddressingMode.INDEXED0
        elif s == 'indexed0_jump':
            # Indexed with no offset, jump
            return AddressingMode.INDEXED0_JUMP
        elif s == 'indexed1':
            # Indexed with 8-bit offset
            return AddressingMode.INDEXED1
        elif s == 'indexed1_jump':
            # Indexed with 8-bit offset, jump
            return AddressingMode.INDEXED1_JUMP
        elif s == 'indexed2':
            # Indexed with 16-bit offset
            return AddressingMode.INDEXED2
        elif s == 'indexed2_jump':
            # Indexed with 16-bit offset, jump
            return AddressingMode.INDEXED2_JUMP
        elif s == 'inh':
            # Inherent
            return AddressingMode.INHERENT
        elif s == 'inh_a':
            # Inherent, affects A register
            return AddressingMode.INHERENT_A
        elif s == 'inh_x':
            # Inherent, affects X register
            return AddressingMode.INHERENT_X
        elif s == 'rel':
            # Relative jump
            return AddressingMode.RELATIVE
        else:
            raise ValueError("Invalid addressing mode '%s'" % _s)

    def to_c_amode(self):
        return "AMODE_" + self.__str__().split('.')[1]


class Instruction:
    def __init__(self, opcode, mnemonic, addressing_mode, cycles):
        self.opcode = opcode
        self.mnemonic = mnemonic
        self.addressing_mode = addressing_mode
        self.cycles = cycles

    def __repr__(self):
        return "<Instruction '%s', op=%02X, amode=%s cyc=%d>" % (self.mnemonic, self.opcode, self.addressing_mode)

    def root_mnemonic(self):
        """Return the 'root' operation -- remove the A or X suffix for inherent A/X operations"""
        if self.addressing_mode in (AddressingMode.INHERENT_A, AddressingMode.INHERENT_X):
            if self.mnemonic[-1] not in ('A', 'X'):
                raise ValueError("Inherent-A/X opcode 0x%02X ('%s') has an illegal suffix" % (self.opcode, self.mnemonic))
            else:
                return self.mnemonic[:-1]
        else:
            return self.mnemonic


# validate command line parameters
if len(sys.argv) < 4:
    sys.exit("Syntax: %s csvfile prefix outputmode" % sys.argv[0])

g_filename = sys.argv[1]
g_prefix = sys.argv[2]
g_outmode = sys.argv[3]

# create instruction code table
op_table = [None] * 256

with open(g_filename, newline='') as csvfile:
    reader = csv.reader(csvfile)
    header = True
    for row in reader:
        # skip the header row
        if header:
            header = False
            continue

        # opcode is 2-digit hex
        opcode = int(row[0], 16)
        assert(0 <= opcode <= 0xFF)

        # mnemonics may include an alias, drop it
        mnemonic = row[1].split('_')[0]

        # is this an illegal/undefined opcode? (skip if so)
        if mnemonic == '':
            continue

        # addressing mode needs to be an AddressingMode enum
        addr_mode = AddressingMode.from_str(row[2])

        cycles = int(row[3])

        # make sure opcodes aren't duplicated
        if op_table[opcode] is not None:
            sys.exit("Opcode duplicated: 0x%02X" % opcode)

        # into the table it goes
        op_table[opcode] = Instruction(opcode, mnemonic, addr_mode, cycles)


# -- function prototypes
if g_outmode == 'prototypes':
    print("#ifndef M68_INTERNAL_H")
    print("#define M68_INTERNAL_H")
    print()

    # addressing modes
    print("typedef enum {")
    for am in AddressingMode:
        print(f"\t{am.to_c_amode()},")
    print("\tAMODE_MAX")
    print("} M68_AMODE;")
    print()

    # mnemonic prototypes
    unique_mnemonics = set([ins.root_mnemonic() for ins in op_table if ins is not None])
    for m in sorted(unique_mnemonics):
        print(f"uint8_t {g_prefix}_{m}(M68_CTX *ctx, const uint8_t param);")

    print()
    print("#endif // M68_INTERNAL_H")


# -- function boilerplate (empty functions)
if g_outmode == 'boilerplate':
    unique_mnemonics = set([ins.root_mnemonic() for ins in op_table if ins is not None])
    for m in sorted(unique_mnemonics):
        print(f"uint8_t {g_prefix}_{m}(M68_CTX *ctx, const uint8_t param)")
        print("{")
        print("}")
        print()

# -- instruction decode table
if g_outmode == 'optable':
    print(f"M68_OPTABLE_ENT {g_prefix}_optable[256] = {{")
    for opcode in op_table:
        if opcode is None:
            print("\t{ \"ILLEGAL\", AMODE_ILLEGAL, 0, NULL },")
        else:
            fname = f"m68op_{opcode.root_mnemonic()}"
            print(f"\t{{ \"{opcode.mnemonic}\", {opcode.addressing_mode.to_c_amode()}, {opcode.cycles}, &{fname} }},")
    print("};")
