#ifndef GLOBALS_H
#define GLOBALS_H

#define NUM_CAPTURES 1000

#include "types.h"

extern UINT16 capture_values[NUM_CAPTURES];
extern UINT16 capture_idx;
extern UINT8 finished_capturing;	  

#define TIOS_INPUT_CAPTURE 0
#define TIOS_OUTPUT_CAPTURE 1

#endif