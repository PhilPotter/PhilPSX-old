/*
 * This header file provides the public API for the MIPS R3051 implementation
 * of PhilPSX.
 * 
 * R3051.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_R3051_HEADER
#define PHILPSX_R3051_HEADER

// System includes
#include <stdint.h>
#include <stdbool.h>

// Typedefs
typedef struct R3051 R3051;

// Includes
#include "BusInterfaceUnit.h"
#include "Cop0.h"
#include "Cop2.h"
#include "SystemInterlink.h"

// Public functions
R3051 *construct_R3051(void);
void destruct_R3051(R3051 *cpu);
void R3051_executeInstructions(R3051 *cpu);
BusInterfaceUnit *R3051_getBiu(R3051 *cpu);
Cop0 *R3051_getCop0(R3051 *cpu);
Cop2 *R3051_getCop2(R3051 *cpu);
int32_t R3051_getHiReg(R3051 *cpu);
int32_t R3051_getLoReg(R3051 *cpu);
int32_t R3051_getProgramCounter(R3051 *cpu);
int32_t R3051_readDataValue(R3051 *cpu, int32_t width, int32_t address);
int64_t R3051_readInstructionWord(R3051 *cpu, int32_t address,
		int32_t tempBranchAddress);
int32_t R3051_readReg(R3051 *cpu, int32_t reg);
void R3051_reset(R3051 *cpu);
void R3051_setMemoryInterface(R3051 *cpu, SystemInterlink *system);
int32_t R3051_swapWordEndianness(R3051 *cpu, int32_t word);
void R3051_writeDataValue(R3051 *cpu, int32_t width, int32_t address,
		int32_t value);
void R3051_writeReg(R3051 *cpu, int32_t reg, int32_t value, bool override);

#endif