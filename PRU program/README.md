A few notes:

- `AM335x_PRU.cmd` is a linker command file for linking PRU programs built with the compiler.
  It contains the memory map and definition for the PRU architecture.
- `resource_table_empty.h` is required by remoteproc to define the required resource table for the PRU cores.
- `prudebug` can be executed in a separate terminal and used to view the registers when the PRU program is halted.
  The PRU debugger can also be used to load binaries into the PRUs, display instruction/data memory spaces,
  disassemble instruction memory space, and start/halt or single-step a PRU. This is useful,
  as it is difficult to debug programs that are running on the PRU because of the absence of a standard output.
