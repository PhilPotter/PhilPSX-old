/*
 * This C file models the MIPS R3051 processor of the PlayStation as a class.
 * 
 * R3051.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "headers/Components.h"
#include "headers/Cop0.h"
#include "headers/Cop2.h"
#include "headers/BusInterfaceUnit.h"
#include "headers/SystemInterlink.h"

// Data widths for read and write methods
#define PHILPSX_R3051_BYTE 8
#define PHILPSX_R3051_HALFWORD 16
#define PHILPSX_R3051_WORD 32

// Forward declarations for subcomponents
typedef struct MIPSException MIPSException;
typedef struct Cache Cache;

// This struct contains registers, and pointers to subcomponents
struct R3051 {
    
    // Component ID
    int32_t componentId;
    
    // Register definitions
    int32_t *generalRegisters;
    int32_t programCounter;
    int32_t hiReg;
    int32_t loReg;
    
    // Jump address holder and boolean
    int32_t jumpAddress;
    bool jumpPending;
    
    // Co-processor definitions
    Cop0 *sccp;
    Cop2 *gte;
    
    // Bus Interface Unit definition
    BusInterfaceUnit *biu;
    
    // System link
    SystemInterlink *system;
    
    // This stores the current exception
    MIPSException *exception;
    
    // This stores the instruction cache
    Cache *instructionCache;
    
    // This tells us if the last instruction was a branch/jump instruction
    bool prevWasBranch;
    bool isBranch;
    
    // This counts the cycles of the current instruction
    int32_t cycles;
};

// This constructs and returns a new R3051 object
R3051 *construct_R3051(void)
{
	// Allocate R3051 struct
	R3051 *cpu = malloc(sizeof(R3051));
	if (cpu == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for R3051 struct \n");
		goto end;
	}
	
	// Set component ID
	cpu->componentId = PHILPSX_COMPONENTS_CPU;

	// Setup instruction cycle count
	cpu->cycles = 0;

	// Setup registers
	cpu->generalRegisters = calloc(32, sizeof(int32_t));
	if (cpu->generalRegisters == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for general registers");
		goto cleanup_r3051;
	}
	
	cpu->generalRegisters[0] = 0;// r1 is fixed at 0 (already 0, here for clarity)
	cpu->programCounter = 0;
	cpu->hiReg = 0;
	cpu->loReg = 0;

	// Setup jump variables
	cpu->jumpAddress = 0;
	cpu->jumpPending = false;

	// Setup co-processors
	cpu->sccp = new Cop0();
	cpu->gte = new Cop2();

	// Setup bus interface unit
	cpu->biu = new BusInterfaceUnit(this);

	// Setup empty exception
	cpu->exception = new MIPSException();

	// Setup instruction cache
	cpu->instructionCache = new Cache(Cache.INSTRUCTION_CACHE);

	// Setup the branch marker
	cpu->prevWasBranch = false;
	cpu->isBranch = false;

	// Reset main chip and Cop0
	//reset();
	//sccp.reset();
	
	// Normal return:
	return cpu;
	
	// Cleanup path:
	cleanup_r3051:
	free(cpu);
	cpu = NULL;
	
	end:
	return cpu;
}

// This destructs an R3051 object
void destruct_R3051(R3051 *cpu)
{
	
}