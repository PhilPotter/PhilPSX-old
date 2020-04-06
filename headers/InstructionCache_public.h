/*
 * This header file provides the public API for the instruction cache of the
 * R3051 processor.
 * 
 * InstructionCache_public.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_INSTRUCTION_CACHE_PUBLIC_HEADER
#define PHILPSX_INSTRUCTION_CACHE_PUBLIC_HEADER

// System includes
#include <stdbool.h>
#include <stdint.h>

// Typedefs
typedef struct InstructionCache InstructionCache;

// Public functions
#include "Cop0_public.h"
#include "SystemInterlink.h"

// construct_InstructionCache needs a pre-allocated memory region
InstructionCache *construct_InstructionCache(InstructionCache *cache);
void destruct_InstructionCache(InstructionCache *cache);
bool InstructionCache_checkForHit(InstructionCache *cache, int32_t address);
int32_t InstructionCache_readWord(InstructionCache *cache, int32_t address);
int8_t InstructionCache_readByte(InstructionCache *cache, int32_t address);
void InstructionCache_writeWord(InstructionCache *cache, Cop0 *sccp,
		int32_t address, int32_t value);
void InstructionCache_writeByte(InstructionCache *cache, Cop0 *sccp,
		int32_t address, int8_t value);
void InstructionCache_refillLine(InstructionCache *cache, Cop0 *sccp,
		SystemInterlink *system, int32_t address);

#endif