/*
 * This C file models the SPU (sound chip) of the PlayStation as a class.
 * 
 * SPU.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/SPU.h"
#include "../headers/SystemInterlink.h"

/*
 * This struct models the SPU (sound chip) of the PlayStation, and at present
 * is a stub. It is intended merely to store and return register values in
 * combination with the utility functions provided.
 */
struct SPU {

	// This stores the link to the interlink object
	SystemInterlink *system;

	// This stores all values written by the system to the SPU
	int8_t *fakeRegisterSpace;
};

/*
 * This constructs an SPU object.
 */
SPU *construct_SPU(void)
{
	// Allocate SPU struct
	SPU *spu = malloc(sizeof(SPU));
	if (!spu) {
		fprintf(stderr, "PhilPSX: SPU: Couldn't allocate memory for "
				"SPU struct\n");
		goto end;
	}
	
	// Allocate memory for fake register space
	spu->fakeRegisterSpace = calloc(1024, sizeof(int8_t));
	if (!spu->fakeRegisterSpace) {
		fprintf(stderr, "PhilPSX: SPU: Couldn't allocate memory for "
				"fakeRegisterSpace array\n");
		goto cleanup_spu;
	}
	
	// Set SystemInterlink reference to NULL
	spu->system = NULL;
	
	// Normal return:
	return spu;
	
	// Cleanup path:
	cleanup_spu:
	free(spu);
	spu = NULL;
	
	end:
	return spu;
}

/*
 * This destructs an SPU object.
 */
void destruct_SPU(SPU *spu)
{
	free(spu->fakeRegisterSpace);
	free(spu);
}

/*
 * This function writes a byte to the fake store.
 */
void SPU_writeByte(SPU *spu, int32_t address, int8_t value)
{
	int64_t tempAddress = 0xFFFFFFFFL & address;
	tempAddress -= 0x1F801C00L;

	// Store byte
	spu->fakeRegisterSpace[(int32_t)tempAddress] = value;
}

/*
 * This function reads a byte from the fake store.
 */
int8_t SPU_readByte(SPU *spu, int32_t address)
{
	int64_t tempAddress = 0xFFFFFFFFL & address;
	tempAddress -= 0x1F801C00L;

	// Retrieve byte
	return spu->fakeRegisterSpace[(int32_t)tempAddress];
}

/*
 * This function sets the system reference to that of the supplied argument.
 */
void SPU_setMemoryInterface(SPU *spu, SystemInterlink *smi)
{
	spu->system = smi;
}