/*
 * This header file provides implementation details regarding the struct
 * for the instruction cache of the R3051 processor, and also includes
 * its public header.
 * 
 * InstructionCache_all.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_INSTRUCTION_CACHE_ALL_HEADER
#define PHILPSX_INSTRUCTION_CACHE_ALL_HEADER

/*
 * This struct models the R3051 instruction cache.
 */
struct InstructionCache {
	
	// Cache variables
	int8_t *cacheData;
	int32_t *cacheTag;
	bool *cacheValid;
};

// Includes
#include "InstructionCache_public.h"

#endif