/*
 * This header file provides the public API for the Geometry Transformation
 * Engine of the PlayStation.
 * 
 * Cop2.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_COP2_HEADER
#define PHILPSX_COP2_HEADER

// System includes
#include <stdbool.h>
#include <stdint.h>

// Typedefs
typedef struct Cop2 Cop2;

// Public functions
Cop2 *construct_Cop2(void);
void destruct_Cop2(Cop2 *gte);
void Cop2_reset(Cop2 *gte);
bool Cop2_getConditionLineStatus(Cop2 *gte);
void Cop2_gteFunction(Cop2 *gte, int32_t opcode);
int32_t Cop2_gteGetCycles(Cop2 *gte, int32_t opcode);
int32_t Cop2_readControlReg(Cop2 *gte, int32_t reg);
int32_t Cop2_readDataReg(Cop2 *gte, int32_t reg);
void Cop2_writeControlReg(Cop2 *gte, int32_t reg, int32_t value, bool override);
void Cop2_writeDataReg(Cop2 *gte, int32_t reg, int32_t value, bool override);

#endif