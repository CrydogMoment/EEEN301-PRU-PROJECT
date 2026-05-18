rm -vf *.obj *.out
clpru pru_program.asm main.c  \
--include_path=/usr/lib/ti/pru-software-support-package-v6.0/include/am335x/ \
--include_path=/usr/lib/ti/pru-software-support-package-v6.0/include/ \
--include_path=/usr/share/ti/cgt-pru/include/ \
--include_path=/usr/share/ti/cgt-pru/lib \
--run_linker \
--library=libc.a \
--library=/usr/lib/ti/pru-software-support-package-v6.0/lib/rpmsg_lib.lib \
-o pru0_firmware.out