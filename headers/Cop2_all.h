/*
 * This header file provides implementation details regarding the struct for
 * the Geometry Transformation Engine of the PlayStation, and also includes its
 * public header.
 * 
 * Cop2_all.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_COP2_HEADER
#define PHILPSX_COP2_HEADER

/*
 * The Cop2 struct models the Geometry Transformation Engine, which is a
 * co-processor in the PlayStation responsible for matrix calculations amongst
 * other things.
 */
struct Cop2 {
	// Control registers
	int32_t controlRegisters[32];

	// Data registers
	int32_t dataRegisters[32];

	// Condition line
	bool conditionLine;
};

// Includes
#include "Cop2_public.h"

#endif