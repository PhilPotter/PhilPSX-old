/*
 * This C file models the Geometry Transformation Engine of the PlayStation as a
 * class.
 * 
 * Cop2.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/Cop2.h"
#include "../headers/math_utils.h"

// Unsigned Newton-Raphson algorithm array - values taken from NOPSX 
// documentation to mimic NOPSX results (but with my own code of course)
static const int32_t unrResults[] = {
	0xFF, 0xFD, 0xFB, 0xF9, 0xF7, 0xF5, 0xF3, 0xF1, 0xEF, 0xEE, 0xEC, 0xEA,
	0xE8, 0xE6, 0xE4, 0xE3, 0xE1, 0xDF, 0xDD, 0xDC, 0xDA, 0xD8, 0xD6, 0xD5,
	0xD3, 0xD1, 0xD0, 0xCE, 0xCD, 0xCB, 0xC9, 0xC8, 0xC6, 0xC5, 0xC3, 0xC1,
	0xC0, 0xBE, 0xBD, 0xBB, 0xBA, 0xB8, 0xB7, 0xB5, 0xB4, 0xB2, 0xB1, 0xB0,
	0xAE, 0xAD, 0xAB, 0xAA, 0xA9, 0xA7, 0xA6, 0xA4, 0xA3, 0xA2, 0xA0, 0x9F,
	0x9E, 0x9C, 0x9B, 0x9A, 0x99, 0x97, 0x96, 0x95, 0x94, 0x92, 0x91, 0x90,
	0x8F, 0x8D, 0x8C, 0x8B, 0x8A, 0x89, 0x87, 0x86, 0x85, 0x84, 0x83, 0x82,
	0x81, 0x7F, 0x7E, 0x7D, 0x7C, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x75, 0x74,
	0x73, 0x72, 0x71, 0x70, 0x6F, 0x6E, 0x6D, 0x6C, 0x6B, 0x6A, 0x69, 0x68,
	0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60, 0x5F, 0x5E, 0x5D, 0x5D,
	0x5C, 0x5B, 0x5A, 0x59, 0x58, 0x57, 0x56, 0x55, 0x54, 0x53, 0x53, 0x52,
	0x51, 0x50, 0x4F, 0x4E, 0x4D, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x48,
	0x47, 0x46, 0x45, 0x44, 0x43, 0x43, 0x42, 0x41, 0x40, 0x3F, 0x3F, 0x3E,
	0x3D, 0x3C, 0x3C, 0x3B, 0x3A, 0x39, 0x39, 0x38, 0x37, 0x36, 0x36, 0x35,
	0x34, 0x33, 0x33, 0x32, 0x31, 0x31, 0x30, 0x2F, 0x2E, 0x2E, 0x2D, 0x2C,
	0x2C, 0x2B, 0x2A, 0x2A, 0x29, 0x28, 0x28, 0x27, 0x26, 0x26, 0x25, 0x24,
	0x24, 0x23, 0x22, 0x22, 0x21, 0x20, 0x20, 0x1F, 0x1E, 0x1E, 0x1D, 0x1D,
	0x1C, 0x1B, 0x1B, 0x1A, 0x19, 0x19, 0x18, 0x18, 0x17, 0x16, 0x16, 0x15,
	0x15, 0x14, 0x14, 0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x0F, 0x0F, 0x0E,
	0x0E, 0x0D, 0x0D, 0x0C, 0x0C, 0x0B, 0x0A, 0x0A, 0x09, 0x09, 0x08, 0x08,
	0x07, 0x07, 0x06, 0x06, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03, 0x02, 0x02,
	0x01, 0x01, 0x00, 0x00, 0x00
};

// Forward declarations for functions private to this class
static void Cop2_handleRTPS(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCLIP(Cop2 *gte, int32_t opcode);
static void Cop2_handleOP(Cop2 *gte, int32_t opcode);
static void Cop2_handleDPCS(Cop2 *gte, int32_t opcode);
static void Cop2_handleINTPL(Cop2 *gte, int32_t opcode);
static void Cop2_handleMVMVA(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCDS(Cop2 *gte, int32_t opcode);
static void Cop2_handleCDP(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCDT(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCCS(Cop2 *gte, int32_t opcode);
static void Cop2_handleCC(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCS(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCT(Cop2 *gte, int32_t opcode);
static void Cop2_handleSQR(Cop2 *gte, int32_t opcode);
static void Cop2_handleDCPL(Cop2 *gte, int32_t opcode);
static void Cop2_handleDPCT(Cop2 *gte, int32_t opcode);
static void Cop2_handleAVSZ3(Cop2 *gte, int32_t opcode);
static void Cop2_handleAVSZ4(Cop2 *gte, int32_t opcode);
static void Cop2_handleRTPT(Cop2 *gte, int32_t opcode);
static void Cop2_handleGPF(Cop2 *gte, int32_t opcode);
static void Cop2_handleGPL(Cop2 *gte, int32_t opcode);
static void Cop2_handleNCCT(Cop2 *gte, int32_t opcode);

/*
 * The Cop2 struct models the Geometry Transformation Engine, which is a
 * co-processor in the PlayStation responsible for matrix calculations amongst
 * other things.
 */
struct Cop2 {

	// Control registers
	int32_t *controlRegisters;

	// Data registers
	int32_t *dataRegisters;

	// Condition line
	bool conditionLine;
};

/*
 * This constructs a Cop2 object.
 */
Cop2 *construct_Cop2(void)
{
	// Allocate Cop2 struct
	Cop2 *gte = malloc(sizeof(Cop2));
	if (gte == NULL) {
		fprintf(stderr, "PhilPSX: Cop2: Couldn't allocate memory for "
				"Cop2 struct\n");
		goto end;
	}
	
	// Setup register arrays
	gte->controlRegisters = calloc(32, sizeof(int32_t));
	if (gte->controlRegisters == NULL) {
		fprintf(stderr, "PhilPSX: Cop2: Couldn't allocate memory for "
				"controlRegisters array\n");
		goto cleanup_cop2;
	}
	gte->dataRegisters = calloc(32, sizeof(int32_t));
	if (gte->dataRegisters == NULL) {
		fprintf(stderr, "PhilPSX: Cop2: Couldn't allocate memory for "
				"dataRegisters array\n");
		goto cleanup_creg;
	}

	// Reset
	Cop2_reset(gte);
	
	// Normal return:
	return gte;
	
	// Cleanup path:
	cleanup_creg:
	free(gte->controlRegisters);
	
	cleanup_cop2:
	free(gte);
	gte = NULL;
	
	end:
	return gte;
}

/*
 * This destructs a Cop2 object.
 */
void destruct_Cop2(Cop2 *gte)
{
	free(gte->dataRegisters);
	free(gte->controlRegisters);
	free(gte);
}

/*
 * This function resets the state of the co-processor as per the reset
 * exception.
 */
void Cop2_reset(Cop2 *gte)
{
	gte->conditionLine = false;
}

/*
 * This function gets the state of the condition line.
 */
bool Cop2_getConditionLineStatus(Cop2 *gte)
{
	return gte->conditionLine;
}

/*
 * This function deals with GTE functions that are handed to COP2 from the CPU.
 */
void Cop2_gteFunction(Cop2 *gte, int32_t opcode)
{
	// Determine which function to handle
	switch (opcode & 0x3F) {
		case 0x01:
			Cop2_handleRTPS(gte, opcode);
			break;
		case 0x06:
			Cop2_handleNCLIP(gte, opcode);
			break;
		case 0x0C:
			Cop2_handleOP(gte, opcode);
			break;
		case 0x10:
			Cop2_handleDPCS(gte, opcode);
			break;
		case 0x11:
			Cop2_handleINTPL(gte, opcode);
			break;
		case 0x12:
			Cop2_handleMVMVA(gte, opcode);
			break;
		case 0x13:
			Cop2_handleNCDS(gte, opcode);
			break;
		case 0x14:
			Cop2_handleCDP(gte, opcode);
			break;
		case 0x16:
			Cop2_handleNCDT(gte, opcode);
			break;
		case 0x1B:
			Cop2_handleNCCS(gte, opcode);
			break;
		case 0x1C:
			Cop2_handleCC(gte, opcode);
			break;
		case 0x1E:
			Cop2_handleNCS(gte, opcode);
			break;
		case 0x20:
			Cop2_handleNCT(gte, opcode);
			break;
		case 0x28:
			Cop2_handleSQR(gte, opcode);
			break;
		case 0x29:
			Cop2_handleDCPL(gte, opcode);
			break;
		case 0x2A:
			Cop2_handleDPCT(gte, opcode);
			break;
		case 0x2D:
			Cop2_handleAVSZ3(gte, opcode);
			break;
		case 0x2E:
			Cop2_handleAVSZ4(gte, opcode);
			break;
		case 0x30:
			Cop2_handleRTPT(gte, opcode);
			break;
		case 0x3D:
			Cop2_handleGPF(gte, opcode);
			break;
		case 0x3E:
			Cop2_handleGPL(gte, opcode);
			break;
		case 0x3F:
			Cop2_handleNCCT(gte, opcode);
			break;
		default:
			break;
	}
}

/*
 * This function returns the number of cycles for the specified GTE function.
 */
int32_t Cop2_gteGetCycles(Cop2 *gte, int32_t opcode)
{
	// Define return value
	int32_t retVal = 0;

	// Determine which function to return cycles for
	switch (opcode & 0x3F) {
		case 0x01:
			// RTPS
			retVal = 15;
			break;
		case 0x06:
			// NCLIP
			retVal = 8;
			break;
		case 0x0C:
			// OP
			retVal = 6;
			break;
		case 0x10:
			// DPCS
			retVal = 8;
			break;
		case 0x11:
			// INTPL
			retVal = 8;
			break;
		case 0x12:
			// MVMVA
			retVal = 8;
			break;
		case 0x13:
			// NCDS
			retVal = 19;
			break;
		case 0x14:
			// CDP
			retVal = 13;
			break;
		case 0x16:
			// NCDT
			retVal = 44;
			break;
		case 0x1B:
			// NCCS
			retVal = 17;
			break;
		case 0x1C:
			// CC
			retVal = 11;
			break;
		case 0x1E:
			// NCS
			retVal = 14;
			break;
		case 0x20:
			// NCT
			retVal = 30;
			break;
		case 0x28:
			// SQR
			retVal = 5;
			break;
		case 0x29:
			// DCPL
			retVal = 8;
			break;
		case 0x2A:
			// DPCT
			retVal = 17;
			break;
		case 0x2D:
			// AVSZ3
			retVal = 5;
			break;
		case 0x2E:
			// AVSZ4
			retVal = 6;
			break;
		case 0x30:
			// RTPT
			retVal = 23;
			break;
		case 0x3D:
			// GPF
			retVal = 5;
			break;
		case 0x3E:
			// GPL
			retVal = 5;
			break;
		case 0x3F:
			// NCCT
			retVal = 39;
			break;
		default:
			break;
	}

	// Return number of cycles
	return retVal;
}

/*
 * This function reads from the specified control register.
 */
int32_t Cop2_readControlReg(Cop2 *gte, int32_t reg)
{
	// Declare return value
	int32_t retVal = 0;

	// Set return value to specified register
	switch (reg) {
		case 26:
			// Actually unsigned, but we do sign extension
			// as this is a hardware bug
		case 27:
		case 29:
		case 30:
			retVal = gte->controlRegisters[reg];
			if ((retVal & 0x8000) == 0x8000)
				retVal |= 0xFFFF0000;
			break;
		default:
			retVal = gte->controlRegisters[reg];
			break;
	}

	return retVal;
}

/*
 * This function reads from the specified data register.
 */
int32_t Cop2_readDataReg(Cop2 *gte, int32_t reg)
{
	// Declare return value
	int32_t retVal = 0;

	// Set return value to specified register
	switch (reg) {
		case 1:
		case 3:
		case 5:
		case 8:
		case 9:
		case 10:
		case 11:
			// Read and sign extend if necessary
			retVal = gte->dataRegisters[reg];
			if ((retVal & 0x8000) == 0x8000)
				retVal |= 0xFFFF0000;
			break;
		case 23:
		case 28:
			break;
		case 29:
			retVal = ((gte->dataRegisters[11] << 3) & 0x7C00) |
					 (logical_rshift(gte->dataRegisters[10], 2) & 0x3E0) |
					 (logical_rshift(gte->dataRegisters[9], 7) & 0x1F);
			break;
		case 31: // LZCR
		{
			// Determine whether we are counting leading 1s or 0s
			int32_t bit, lzcs = gte->dataRegisters[30];
			if (lzcs < 0)
				bit = 0x80000000;
			else
				bit = 0;

			for (int32_t i = 0; i < 32; ++i) {
				if ((lzcs & 0x80000000) == bit)
					++retVal;
				else
					break;

				lzcs = lzcs << 1;
			}
		} break;
		default:
			retVal = gte->dataRegisters[reg];
			break;
	}

	return retVal;
}

/*
 * This function writes to the specified control register.
 */
void Cop2_writeControlReg(Cop2 *gte, int32_t reg, int32_t value, bool override)
{
	if (override) {
		gte->controlRegisters[reg] = value;
	} else {

		// Determine course of action
		switch (reg) {
			default:
				gte->controlRegisters[reg] = value;
				break;
		}
	}
}

/*
 * This function writes to the specified data register.
 */
void Cop2_writeDataReg(Cop2 *gte, int32_t reg, int32_t value, bool override)
{
	if (override) {
		gte->dataRegisters[reg] = value;
	} else {

		// Determine course of action
		switch (reg) {
			case 7:
			case 23:
			case 29:
			case 31:
				break;
			case 14:
				// SXY2 - mirror of SXYP

				// Set SXY2 and SXYP
				gte->dataRegisters[14] = value; // SXY2
				gte->dataRegisters[15] = value; // SXYP

				break;
			case 15:
				// SXYP - mirror of SXY2 but causes SXY1 to move to SXY0,
				// and SXY2 to move to SXY1

				// Move SXY1 to SXY0
				gte->dataRegisters[12] = gte->dataRegisters[13];

				// Move SXY2 to SXY1
				gte->dataRegisters[13] = gte->dataRegisters[14];

				// Set SXY2 and SXYP
				gte->dataRegisters[14] = value; // SXY2
				gte->dataRegisters[15] = value; // SXYP

				break;
			case 28:
				// IRGB
				gte->dataRegisters[9] = (0x1F & value) << 7; // IR1
				gte->dataRegisters[10] = (0x3E0 & value) << 2; // IR2
				gte->dataRegisters[11] = logical_rshift((0x7C00 & value), 3); // IR3
				break;
			default:
				gte->dataRegisters[reg] = value;
				break;
		}
	}
}

/*
 * This function handles the RTPS GTE function.
 */
static void Cop2_handleRTPS(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCLIP GTE function.
 */
static void Cop2_handleNCLIP(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the OP GTE function.
 */
static void Cop2_handleOP(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the DPCS GTE function.
 */
static void Cop2_handleDPCS(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the INTPL GTE function.
 */
static void Cop2_handleINTPL(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the MVMVA GTE function.
 */
static void Cop2_handleMVMVA(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCDS GTE function.
 */
static void Cop2_handleNCDS(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the CDP GTE function.
 */
static void Cop2_handleCDP(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCDT GTE function.
 */
static void Cop2_handleNCDT(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCCS GTE function.
 */
static void Cop2_handleNCCS(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the CC GTE function.
 */
static void Cop2_handleCC(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCS GTE function.
 */
static void Cop2_handleNCS(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCT GTE function.
 */
static void Cop2_handleNCT(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the SQR GTE function.
 */
static void Cop2_handleSQR(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the DCPL GTE function.
 */
static void Cop2_handleDCPL(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the DPCT GTE function.
 */
static void Cop2_handleDPCT(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the AVSZ3 GTE function.
 */
static void Cop2_handleAVSZ3(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the AVSZ4 GTE function.
 */
static void Cop2_handleAVSZ4(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the RTPT GTE function.
 */
static void Cop2_handleRTPT(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the GPF GTE function.
 */
static void Cop2_handleGPF(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the GPL GTE function.
 */
static void Cop2_handleGPL(Cop2 *gte, int32_t opcode)
{
	
}

/*
 * This function handles the NCCT GTE function.
 */
static void Cop2_handleNCCT(Cop2 *gte, int32_t opcode)
{
	
}