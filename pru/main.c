#include <stdint.h>
#include <pru_cfg.h>
#include "resource_table_empty.h"

#define PRU0_DRAM 0x00000000
volatile uint32_t *shared = (unsigned int *) (PRU0_DRAM);

extern void START(void);

void main(void) {
    shared[0] = 1500;   // The number of samples
    shared[1] = 2;      // Sample delay in ms
	START();
}
