/*
 * This header file provides implementation details regarding the struct
 * for the System Control Co-Processor of the PlayStation, and also includes
 * its public header.
 * 
 * Cop0_all.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_COP0_ALL_HEADER
#define PHILPSX_COP0_ALL_HEADER

/*
 * The Cop0 struct models the System Control Co-Processor (Cop0), which
 * is responsible for memory management and exceptions.
 */
struct Cop0 {

	// Register definitions
	int32_t copRegisters[32];

	// Condition line
	bool conditionLine;
};

// Includes
#include "Cop0_public.h"

#endif