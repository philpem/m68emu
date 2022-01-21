#!/usr/bin/env python3

# create an empty opcode-table CSV file

print("op,instr,amode,cycles")
for i in range(256):
    print('"%02X",,' % i)
