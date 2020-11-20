# Motorola 68HC05 CPU core emulator

This is a noddy little cycle-counting emulation of the 68HC05 CPU core. It exists because I couldn't find one which fit my needs!

Features include:

  * 68HC05 core emulation (no peripherals) with cycle counting
  * Memory access is done through hook functions
  * Separate opcode fetch hooks (to handle CPU cores with scrambled opcodes)
