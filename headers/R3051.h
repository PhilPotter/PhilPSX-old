/*
 * This header file provides the public API for the MIPS R3051 implementation
 * of PhilPSX.
 * 
 * R3051.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_R3051_HEADER
#define PHILPSX_R3051_HEADER

// System includes
#include <stdint.h>
#include <stdbool.h>

// Typedefs
typedef struct R3051 R3051;

// Includes
#include "Cop0_public.h"
#include "Cop2_public.h"
#include "SystemInterlink.h"

// Public functions
R3051 *construct_R3051(void);
void destruct_R3051(R3051 *cpu);
int64_t R3051_executeInstructions(R3051 *cpu);
int32_t R3051_getBusHolder(R3051 *cpu);
Cop0 *R3051_getCop0(R3051 *cpu);
Cop2 *R3051_getCop2(R3051 *cpu);
void R3051_setBusHolder(R3051 *cpu, int32_t holder);
void R3051_setMemoryInterface(R3051 *cpu, SystemInterlink *system);

#endif