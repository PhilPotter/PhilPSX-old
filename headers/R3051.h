/*
 * This header file provides the public API for the MIPS R3051 implementation
 * of PhilPSX.
 * 
 * R3051.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_R3051_HEADER
#define PHILPSX_R3051_HEADER

// Typedefs
typedef struct R3051 R3051;

// Public functions
R3051 *construct_R3051(void);
void destruct_R3051(R3051 *cpu);

#endif