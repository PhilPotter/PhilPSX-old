/*
 * This C file models the System Control Co-Processor of the PlayStation as a
 * class.
 * 
 * Cop0.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/Cop0.h"
#include "../headers/math_utils.h"

/*
 * The Cop0 struct models the System Control Co-Processor (Cop0), which
 * is responsible for memory management and exceptions.
 */
struct Cop0 {

	// Register definitions
	int32_t *copRegisters;

	// Condition line
	bool conditionLine;
};

/*
 * This constructs a Cop0 object.
 */
Cop0 *construct_Cop0(void)
{
	// Allocate Cop0 struct
	Cop0 *sccp = malloc(sizeof(Cop0));
	if (sccp == NULL) {
		fprintf(stderr, "PhilPSX: Cop0: Couldn't allocate memory for "
				"Cop0 struct\n");
		goto end;
	}
	
	sccp->copRegisters = calloc(32, sizeof(int32_t));
	if (sccp->copRegisters == NULL) {
		fprintf(stderr, "PhilPSX: Cop0: Couldn't allocate memory for "
				"copRegisters array\n");
		goto cleanup_cop0;
	}
	
	Cop0_reset(sccp);
	
	// Normal return:
	return sccp;
	
	// Cleanup path:
	cleanup_cop0:
	free(sccp);
	sccp = NULL;
	
	end:
	return sccp;
}

/*
 * This destructs a Cop0 object.
 */
void destruct_Cop0(Cop0 *sccp)
{
	free(sccp->copRegisters);
	free(sccp);
}

/*
 * This function resets the state of the co-processor as per the
 * reset exception.
 */
void Cop0_reset(Cop0 *sccp)
{
	sccp->copRegisters[1] = 63 << 8;		// Set random register to 63
	sccp->copRegisters[12] &= 0xFF9FFFFF;	// Set BEV and TS bits of status register to 0 and 0
											// (BEV should be 1 but PSX doesn't run this way)
											// (TS should be 1 but other emus don't seem to do this)
	sccp->copRegisters[12] &= 0xFFFDFFFC;	// SSSet SWc, KUc and IEc bits of status register to 0

	sccp->conditionLine = false;
}

/*
 * This function gets the state of the condition line.
 */
bool Cop0_getConditionLineStatus(Cop0 *sccp)
{
	return sccp->conditionLine;
}

/*
 * This function sets the state of the condition line.
 */
void Cop0_setConditionLineStatus(Cop0 *sccp, bool status)
{
	sccp->conditionLine = status;
}

/**
 * This executes the RFE Cop0 instruction.
 */
void Cop0_rfe(Cop0 *sccp)
{
	// Get copy of status bits
	int32_t tempReg = Cop0_readReg(sccp, 12);
	int32_t newBits = logical_rshift(tempReg, 2) & 0xF;
	tempReg &= 0xFFFFFFF0;
	tempReg |= newBits;

	// Write new value back
	Cop0_writeReg(sccp, 12, tempReg, false);
}

/*
 * This function returns the reset exception vector's virtual address.
 */
int32_t Cop0_getResetExceptionVector(Cop0 *sccp)
{
	return 0xBFC00000;
}

/*
 * This function returns the general exception vector's virtual address.
 */
int32_t Cop0_getGeneralExceptionVector(Cop0 *sccp)
{
	// Isolate BEV bit and define return variable
	int32_t bev = logical_rshift((sccp->copRegisters[12] & 0x00400000), 22);
	int32_t retVal = 0;

	switch (bev) {
		case 1: retVal = 0xBFC00180;
			break;
		default: retVal = 0x80000080;
			break;
	}

	return retVal;
}

/*
 * This function reads from a register.
 */
int32_t Cop0_readReg(Cop0 *sccp, int32_t reg)
{
	// Declare holding variable
	int32_t retVal = 0;

	// Determine which register we are after
	switch (reg) {
		case 12: // Status register
			retVal = sccp->copRegisters[reg] & 0xF27FFF3F; // Mask out 0-read bits
			//retVal |= 0x00200000; // Merge in TS bit (commented out to copy observed behaviour of other emus)
			break;
		case 13: // Cause register
			retVal = sccp->copRegisters[reg] & 0xB000FF7C; // mask out 0-read bits
			break;
		case 14: // Exception PC register
			retVal = sccp->copRegisters[reg];
			break;
		case 8: // Bad virtual address register
			retVal = sccp->copRegisters[reg];
			break;
		case 15: // PrId register
			retVal = 0x00000002; // PSX specific value
			break;
		case 1: // Random register
			retVal = sccp->copRegisters[reg];
			break;
		default: break;
	}

	return retVal;
}

/*
 * This function writes to a register. It allows override of write protection
 * on certain bits.
 */
void Cop0_writeReg(Cop0 *sccp, int32_t reg, int32_t value, bool override)
{
	// Determine which register we are writing
	if (override) {
		sccp->copRegisters[reg] = value;
	} else {

		// Define temp variable
		int32_t tempVal = 0;

		switch (reg) {
			case 12: // Status register
				tempVal = sccp->copRegisters[reg] & 0x0DB400C0; // Mask out writable bits
				value &= 0xF24BFF3F; // Mask out read-only bits
				sccp->copRegisters[reg] = value | tempVal; // Merge
				break;
			case 13: // Cause register
				tempVal = sccp->copRegisters[reg] & 0xFFFFFCFF; // Mask out writable bits
				value &= 0x00000300; // Mask out read-only bits
				sccp->copRegisters[reg] &= value | tempVal; // Merge
				break;
			case 14: // Exception PC register
				sccp->copRegisters[reg] = value;
				break;
			case 8: // Bad virtual address register
				sccp->copRegisters[reg] = value;
				break;
			default:
				// Other registers
				sccp->copRegisters[reg] = value;
				break;
		}
	}
}

/*
 * Set or reset cache miss flag.
 */
void Cop0_setCacheMiss(Cop0 *sccp, bool value)
{
	// Determine value to merge in
	int32_t cmFlag = value ? 0x00080000 : 0;

	// Merge flag into status reg
	int32_t statusReg = sccp->copRegisters[12] & 0xFFF7FFFF;
	statusReg |= cmFlag;

	// Write back status reg
	Cop0_writeReg(sccp, 12, statusReg, true);
}

/*
 * This function transforms a virtual address into a physical one.
 * It is designed for the base model of the processor which has no TLB.
 */
int32_t Cop0_virtualToPhysical(Cop0 *sccp, int32_t virtualAddress)
{
	// Declare temp variable to hold new address
	// and mask top 32-bits so we always get an unsigned value.
	int64_t physicalAddress = virtualAddress & 0xFFFFFFFFL;

	// Below is the map used in the PS1

	// Make correct modifications to address
	if (physicalAddress >= 0L && physicalAddress < 0x80000000L) { // kuseg address
		// Do nothing
		;
	} else if (physicalAddress >= 0x80000000L && physicalAddress < 0xA0000000L) { // kseg0 address
		// Subtract 0x80000000L
		physicalAddress -= 0x80000000L;
	} else if (physicalAddress >= 0xA0000000L && physicalAddress < 0xC0000000L) { // kseg1 address
		// Subtract 0xA0000000L
		physicalAddress -= 0xA0000000L;
	} else if (physicalAddress >= 0xC0000000L && physicalAddress < 0x100000000L) { // kseg2 address
		// Do nothing, this only includes cache control on PSX
		;
	}

	return (int32_t)physicalAddress;
}

/*
 * This function determines whether the supplied virtual address is cacheable.
 */
bool Cop0_isCacheable(Cop0 *sccp, int32_t virtualAddress)
{
	// Declare temp variable to hold new address
	// and mask top 32-bits so we always get an unsigned value, as well as
	// return variable
	int64_t tempAddress = virtualAddress & 0xFFFFFFFFL;
	bool retVal = false;

	if (tempAddress >= 0L && tempAddress < 0xA0000000L)
		retVal = true;

	return retVal;
}

/*
 * This function tells us whether or not we are in kernel mode.
 */
bool Cop0_areWeInKernelMode(Cop0 *sccp)
{
	bool retVal = false;

	if ((sccp->copRegisters[12] & 0x2) == 0)
		retVal = true;

	return retVal;
}

/**
 * This function tells us if opposite byte ordering is in effect in user mode.
 */
bool Cop0_userModeOppositeByteOrdering(Cop0 *sccp)
{
	bool retVal = false;

	if ((sccp->copRegisters[12] & 0x02000000) == 0x02000000)
		retVal = true;

	return retVal;
}

/*
 * This function allows us to check if a virtual address is allowed to be accessed.
 * It is useful for checking if we are attempting to access a prohibited address
 * whilst in user mode.
 */
bool Cop0_isAddressAllowed(Cop0 *sccp, int32_t virtualAddress)
{
	bool retVal = true;

	if ((virtualAddress & 0x80000000) == 0x80000000 && !Cop0_areWeInKernelMode(sccp))
		retVal = false;

	return retVal;
}

/*
 * This function tells us if the caches have been swapped.
 */
bool Cop0_areCachesSwapped(Cop0 *sccp)
{
	bool retVal = false;

	// Disable cache swapping
	/*if ((sccp->copRegisters[12] & 0x00020000) == 0x00020000)
		retVal = true;*/

	return retVal;
}

/*
 * This function tells us if the data cache is isolated.
 */
bool Cop0_isDataCacheIsolated(Cop0 *sccp)
{
	bool retVal = false;

	if ((sccp->copRegisters[12] & 0x00010000) == 0x00010000)
		retVal = true;

	return retVal;
}

/*
 * This function tells us if a co-processor is usable.
 */
bool Cop0_isCoProcessorUsable(Cop0 *sccp, int32_t copNum)
{
	int32_t usableFlags = logical_rshift(sccp->copRegisters[12], 28);
	usableFlags = logical_rshift(usableFlags, copNum) & 0x1;
	return usableFlags == 1;
}