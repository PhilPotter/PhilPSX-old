/*
 * This header file provides the public API for the System Control Co-Processor
 * of the PlayStation.
 * 
 * Cop0.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_COP0_HEADER
#define PHILPSX_COP0_HEADER

// Includes
#include <stdbool.h>
#include <stdint.h>

// Typedefs
typedef struct Cop0 Cop0;

// Public functions
Cop0 *construct_Cop0(void);
void destruct_Cop0(Cop0 *sccp);
void Cop0_reset(Cop0 *sccp);
bool Cop0_getConditionLineStatus(Cop0 *sccp);
void Cop0_setConditionLineStatus(Cop0 *sccp, bool status);
void Cop0_rfe(Cop0 *sccp);
int32_t Cop0_getResetExceptionVector(Cop0 *sccp);
int32_t Cop0_getGeneralExceptionVector(Cop0 *sccp);
int32_t Cop0_readReg(Cop0 *sccp, int32_t reg);
void Cop0_writeReg(Cop0 *sccp, int32_t reg, int32_t value, bool override);
void Cop0_setCacheMiss(Cop0 *sccp, bool value);
int32_t Cop0_virtualToPhysical(Cop0 *sccp, int32_t virtualAddress);
bool Cop0_isCacheable(Cop0 *sccp, int32_t virtualAddress);
bool Cop0_areWeInKernelMode(Cop0 *sccp);
bool Cop0_userModeOppositeByteOrdering(Cop0 *sccp);
bool Cop0_isAddressAllowed(Cop0 *sccp, int32_t virtualAddress);
bool Cop0_areCachesSwapped(Cop0 *sccp);
bool Cop0_isDataCacheIsolated(Cop0 *sccp);
bool Cop0_isCoProcessorUsable(Cop0 *sccp, int32_t copNum);

#endif