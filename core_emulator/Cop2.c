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
	if (!gte) {
		fprintf(stderr, "PhilPSX: Cop2: Couldn't allocate memory for "
				"Cop2 struct\n");
		goto end;
	}

	// Setup register arrays
	gte->controlRegisters = calloc(32, sizeof(int32_t));
	if (!gte->controlRegisters) {
		fprintf(stderr, "PhilPSX: Cop2: Couldn't allocate memory for "
				"controlRegisters array\n");
		goto cleanup_cop2;
	}
	gte->dataRegisters = calloc(32, sizeof(int32_t));
	if (!gte->dataRegisters) {
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
		}
			break;
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
				gte->dataRegisters[11] =
						logical_rshift((0x7C00 & value), 3); // IR3
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
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Retrieve rotation matrix values
	int64_t rt11 =
			0xFFFF & gte->controlRegisters[0]; // RT11
	int64_t rt12 =
			0xFFFF & logical_rshift(gte->controlRegisters[0], 16); // RT12
	int64_t rt13 =
			0xFFFF & gte->controlRegisters[1]; // RT13
	int64_t rt21 =
			0xFFFF & logical_rshift(gte->controlRegisters[1], 16); // RT21
	int64_t rt22 =
			0xFFFF & gte->controlRegisters[2]; // RT22
	int64_t rt23 =
			0xFFFF & logical_rshift(gte->controlRegisters[2], 16); // RT23
	int64_t rt31 =
			0xFFFF & gte->controlRegisters[3]; // RT31
	int64_t rt32 =
			0xFFFF & logical_rshift(gte->controlRegisters[3], 16); // RT32
	int64_t rt33 =
			0xFFFF & gte->controlRegisters[4]; // RT33

	// Sign extend rotation matrix values if necessary
	if ((rt11 & 0x8000L) == 0x8000L)
		rt11 |= 0xFFFFFFFFFFFF0000L;
	if ((rt12 & 0x8000L) == 0x8000L)
		rt12 |= 0xFFFFFFFFFFFF0000L;
	if ((rt13 & 0x8000L) == 0x8000L)
		rt13 |= 0xFFFFFFFFFFFF0000L;
	if ((rt21 & 0x8000L) == 0x8000L)
		rt21 |= 0xFFFFFFFFFFFF0000L;
	if ((rt22 & 0x8000L) == 0x8000L)
		rt22 |= 0xFFFFFFFFFFFF0000L;
	if ((rt23 & 0x8000L) == 0x8000L)
		rt23 |= 0xFFFFFFFFFFFF0000L;
	if ((rt31 & 0x8000L) == 0x8000L)
		rt31 |= 0xFFFFFFFFFFFF0000L;
	if ((rt32 & 0x8000L) == 0x8000L)
		rt32 |= 0xFFFFFFFFFFFF0000L;
	if ((rt33 & 0x8000L) == 0x8000L)
		rt33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve vector 0 values
	int64_t vx0 = 0xFFFF & gte->dataRegisters[0]; // VX0
	int64_t vy0 = 0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
	int64_t vz0 = 0xFFFF & gte->dataRegisters[1]; // VZ0

	// Sign extend vector 0 values if necessary
	if ((vx0 & 0x8000L) == 0x8000L)
		vx0 |= 0xFFFFFFFFFFFF0000L;
	if ((vy0 & 0x8000L) == 0x8000L)
		vy0 |= 0xFFFFFFFFFFFF0000L;
	if ((vz0 & 0x8000L) == 0x8000L)
		vz0 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve translation vector values
	int64_t trX = 0xFFFFFFFFL & gte->controlRegisters[5]; // TRX
	int64_t trY = 0xFFFFFFFFL & gte->controlRegisters[6]; // TRY
	int64_t trZ = 0xFFFFFFFFL & gte->controlRegisters[7]; // TRZ

	// Sign extend translation vector values if necessary
	if ((trX & 0x80000000L) == 0x80000000L)
		trX |= 0xFFFFFFFF00000000L;
	if ((trY & 0x80000000L) == 0x80000000L)
		trY |= 0xFFFFFFFF00000000L;
	if ((trZ & 0x80000000L) == 0x80000000L)
		trZ |= 0xFFFFFFFF00000000L;

	// Retrieve offset and distance values
	int64_t ofx = 0xFFFFFFFFL & gte->controlRegisters[24];
	int64_t ofy = 0xFFFFFFFFL & gte->controlRegisters[25];
	int64_t h = 0xFFFF & gte->controlRegisters[26];
	int64_t dqa = 0xFFFF & gte->controlRegisters[27];
	int64_t dqb = 0xFFFFFFFFL & gte->controlRegisters[28];

	// Sign extend signed values if needed
	if ((ofx & 0x80000000L) == 0x80000000L)
		ofx |= 0xFFFFFFFF00000000L;
	if ((ofy & 0x80000000L) == 0x80000000L)
		ofy |= 0xFFFFFFFF00000000L;
	if ((dqa & 0x8000L) == 0x8000L)
		dqa |= 0xFFFFFFFFFFFF0000L;
	if ((dqb & 0x80000000L) == 0x80000000L)
		dqb |= 0xFFFFFFFF00000000L;

	// Perform first stage of calculations
	int64_t mac1 = trX * 0x1000 + rt11 * vx0 + rt12 * vy0 + rt13 * vz0;
	int64_t mac2 = trY * 0x1000 + rt21 * vx0 + rt22 * vy0 + rt23 * vz0;
	int64_t mac3 = trZ * 0x1000 + rt31 * vx0 + rt32 * vy0 + rt33 * vz0;

	// Shift all three results by (sf * 12), preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set MAC1, MAC2 and MAC3 flags as needed

	// MAC1
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	// MAC2
	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	// MAC3
	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Set IR1, IR2 and IR3 - dealing with flags too,
	// saturation should be -0x8000..0x7FFF, regardless of lm bit

	// IR1
	int64_t ir1 = 0, ir2 = 0, ir3 = 0;
	if (mac1 < -0x8000) {
		ir1 = -0x8000;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 > 0x7FFF) {
		ir1 = 0x7FFF;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = (int32_t)mac1;
	}

	// IR2
	if (mac2 < -0x8000) {
		ir2 = -0x8000;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 > 0x7FFF) {
		ir2 = 0x7FFF;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = (int32_t)mac2;
	}

	// IR3
	if (mac3 < -0x8000) {
		ir3 = -0x8000;

		// Deal with quirk in IR3 flag handling
		if (sf == 0) {
			int64_t temp = mac3 >> 12;
			if (temp > 0x7FFF || temp < -0x8000)
				gte->controlRegisters[31] |= 0x400000;
		} else {
			gte->controlRegisters[31] |= 0x400000;
		}
	} else if (mac3 > 0x7FFF) {
		ir3 = 0x7FFF;

		// Deal with quirk in IR3 flag handling
		if (sf == 0) {
			int64_t temp = mac3 >> 12;
			if (temp > 0x7FFF || temp < -0x8000)
				gte->controlRegisters[31] |= 0x400000;
		} else {
			gte->controlRegisters[31] |= 0x400000;
		}
	} else {
		ir3 = (int32_t)mac3;
	}

	// Write back to real registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3
	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	// Calculate SZ3 and move FIFO along, also setting SZ3 flag if needed
	gte->dataRegisters[16] = gte->dataRegisters[17]; // SZ1 to SZ0
	gte->dataRegisters[17] = gte->dataRegisters[18]; // SZ2 to SZ1
	gte->dataRegisters[18] = gte->dataRegisters[19]; // SZ3 to SZ2
	int64_t temp_sz3 = mac3 >> ((1 - sf) * 12);
	if (temp_sz3 < 0) {
		temp_sz3 = 0;
		gte->controlRegisters[31] |= 0x40000;
	} else if (temp_sz3 > 0xFFFF) {
		temp_sz3 = 0xFFFF;
		gte->controlRegisters[31] |= 0x40000;
	}
	gte->dataRegisters[19] = (int32_t)temp_sz3;

	// Begin second phase of calculations - use Unsigned Newton-Raphson
	// division algorithm from NOPSX documentation
	int64_t mac0 = 0, sx2 = 0, sy2 = 0, ir0 = 0;
	int64_t divisionResult = 0;
	if (h < temp_sz3 * 2) {

		// Count leading zeroes in SZ3
		int64_t z = 0, temp_sz3_2 = temp_sz3;
		for (int32_t i = 0; i < 16; ++i) {
			if ((temp_sz3_2 & 0x8000) == 0) {
				++z;
				temp_sz3_2 = temp_sz3_2 << 1;
			} else {
				break;
			}
		}

		divisionResult = h << z;
		int64_t d = temp_sz3 << z;
		int64_t u =
				unrResults[logical_rshift(((int32_t)d - 0x7FC0), 7)] + 0x101;
		d = logical_rshift((0x2000080 - (d * u)), 8);
		d = logical_rshift((0x80 + (d * u)), 8);
		divisionResult = min_value(0x1FFFFL,
				logical_rshift(((divisionResult * d) + 0x8000L), 16));

	} else {
		divisionResult = 0x1FFFFL;
		gte->controlRegisters[31] |= 0x20000;
	}

	// Use division result and set MAC0 flag if needed
	mac0 = divisionResult * ir1 + ofx;
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;
	mac0 &= 0xFFFFFFFFL;
	if ((mac0 & 0x80000000L) == 0x80000000L)
		mac0 |= 0xFFFFFFFF00000000L;
	sx2 = mac0 / 0x10000;
	mac0 = divisionResult * ir2 + ofy;
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;
	mac0 &= 0xFFFFFFFFL;
	if ((mac0 & 0x80000000L) == 0x80000000L)
		mac0 |= 0xFFFFFFFF00000000L;
	sy2 = mac0 / 0x10000;
	mac0 = divisionResult * dqa + dqb;
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;
	mac0 &= 0xFFFFFFFFL;
	if ((mac0 & 0x80000000L) == 0x80000000L)
		mac0 |= 0xFFFFFFFF00000000L;
	ir0 = mac0 / 0x1000;

	// Adjust results for saturation and set flags if needed
	if (sx2 > 0x3FFL) {
		sx2 = 0x3FFL;
		gte->controlRegisters[31] |= 0x4000;
	} else if (sx2 < -0x400) {
		sx2 = -0x400;
		gte->controlRegisters[31] |= 0x4000;
	}

	if (sy2 > 0x3FFL) {
		sy2 = 0x3FFL;
		gte->controlRegisters[31] |= 0x2000;
	} else if (sy2 < -0x400) {
		sy2 = -0x400;
		gte->controlRegisters[31] |= 0x2000;
	}

	if (ir0 < 0) {
		ir0 = 0;
		gte->controlRegisters[31] |= 0x1000;
	} else if (ir0 > 0x1000) {
		ir0 = 0x1000;
		gte->controlRegisters[31] |= 0x1000;
	}

	// Store values back to correct registers

	// SXY FIFO registers
	gte->dataRegisters[12] = gte->dataRegisters[13]; // SXY1 to SXY0
	gte->dataRegisters[13] = gte->dataRegisters[14]; // SXY2 to SXY1
	gte->dataRegisters[14] =
			(((int32_t)sy2 & 0xFFFF) << 16) | ((int32_t)sx2 & 0xFFFF); // SXY2
	gte->dataRegisters[15] = gte->dataRegisters[14]; // SXYP mirror of SXY2

	// MAC0
	gte->dataRegisters[24] = (int32_t)mac0;

	// IR0
	gte->dataRegisters[8] = (int32_t)ir0;

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;
}

/*
 * This function handles the NCLIP GTE function.
 */
static void Cop2_handleNCLIP(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Retrieve SXY values
	int64_t sx0 = 0xFFFF & gte->dataRegisters[12]; // SX0
	int64_t sy0 = 0xFFFF & logical_rshift(gte->dataRegisters[12], 16); // SY0
	int64_t sx1 = 0xFFFF & gte->dataRegisters[13]; // SX1
	int64_t sy1 = 0xFFFF & logical_rshift(gte->dataRegisters[13], 16); // SY1
	int64_t sx2 = 0xFFFF & gte->dataRegisters[14]; // SX2
	int64_t sy2 = 0xFFFF & logical_rshift(gte->dataRegisters[14], 16); // SY2

	// Sign extend if necessary
	if ((sx0 & 0x8000L) == 0x8000L)
		sx0 |= 0xFFFFFFFFFFFF0000L;
	if ((sy0 & 0x8000L) == 0x8000L)
		sy0 |= 0xFFFFFFFFFFFF0000L;
	if ((sx1 & 0x8000L) == 0x8000L)
		sx1 |= 0xFFFFFFFFFFFF0000L;
	if ((sy1 & 0x8000L) == 0x8000L)
		sy1 |= 0xFFFFFFFFFFFF0000L;
	if ((sx2 & 0x8000L) == 0x8000L)
		sx2 |= 0xFFFFFFFFFFFF0000L;
	if ((sy2 & 0x8000L) == 0x8000L)
		sy2 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculation
	int64_t mac0 = sx0 * sy1 + sx1 * sy2 + sx2 * sy0 -
			sx0 * sy2 - sx1 * sy0 - sx2 * sy1;

	// Check and set flags if needed
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;

	// Calculate flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store MAC0 result back to register
	gte->dataRegisters[24] = (int32_t)mac0;
}

/*
 * This function handles the OP GTE function.
 */
static void Cop2_handleOP(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Fetch IR values
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Fetch RT11, RT22, RT33 values
	int64_t d1 = 0xFFFF & gte->controlRegisters[0]; // RT11
	int64_t d2 = 0xFFFF & gte->controlRegisters[2]; // RT22
	int64_t d3 = 0xFFFF & gte->controlRegisters[4]; // RT33

	// Sign extend both sets of values
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;
	if ((d1 & 0x8000L) == 0x8000L)
		d1 |= 0xFFFFFFFFFFFF0000L;
	if ((d2 & 0x8000L) == 0x8000L)
		d2 |= 0xFFFFFFFFFFFF0000L;
	if ((d3 & 0x8000L) == 0x8000L)
		d3 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculation
	int64_t temp1 = ir3 * d2 - ir2 * d3;
	int64_t temp2 = ir1 * d3 - ir3 * d1;
	int64_t temp3 = ir2 * d1 - ir1 * d2;

	// Shift result by (sf * 12) bits, preserving sign bit
	temp1 = temp1 >> (sf * 12);
	temp2 = temp2 >> (sf * 12);
	temp3 = temp3 >> (sf * 12);

	// Store results in MAC1, MAC2 and MAC3 registers
	gte->dataRegisters[25] = (int32_t)temp1; // MAC1
	gte->dataRegisters[26] = (int32_t)temp2; // MAC2
	gte->dataRegisters[27] = (int32_t)temp3; // MAC3

	// Set relevant MAC1, MAC2 and MAC3 flag bits

	// MAC1
	if (temp1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (temp1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	// MAC2
	if (temp2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (temp2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	// MAC3
	if (temp3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (temp3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Set IR1, IR2 and IR3 registers and saturation flag bits
	switch (lm) {
		case 0: // Saturate to -0x8000..0x7FFF
		{
			// IR1
			if (temp1 < -0x8000) {
				gte->dataRegisters[9] = -0x8000;
				gte->controlRegisters[31] |= 0x1000000;
			} else if (temp1 > 0x7FFF) {
				gte->dataRegisters[9] = 0x7FFF;
				gte->controlRegisters[31] |= 0x1000000;
			} else {
				gte->dataRegisters[9] = (int32_t)temp1;
			}

			// IR2
			if (temp2 < -0x8000) {
				gte->dataRegisters[10] = -0x8000;
				gte->controlRegisters[31] |= 0x800000;
			} else if (temp2 > 0x7FFF) {
				gte->dataRegisters[10] = 0x7FFF;
				gte->controlRegisters[31] |= 0x800000;
			} else {
				gte->dataRegisters[10] = (int32_t)temp2;
			}

			// IR3
			if (temp3 < -0x8000) {
				gte->dataRegisters[11] = -0x8000;
				gte->controlRegisters[31] |= 0x400000;
			} else if (temp3 > 0x7FFF) {
				gte->dataRegisters[11] = 0x7FFF;
				gte->controlRegisters[31] |= 0x400000;
			} else {
				gte->dataRegisters[11] = (int32_t)temp3;
			}
		}
			break;
		case 1: // Saturate to 0..0x7FFF
		{
			// IR1
			if (temp1 < 0) {
				gte->dataRegisters[9] = 0;
				gte->controlRegisters[31] |= 0x1000000;
			} else if (temp1 > 0x7FFF) {
				gte->dataRegisters[9] = 0x7FFF;
				gte->controlRegisters[31] |= 0x1000000;
			} else {
				gte->dataRegisters[9] = (int32_t)temp1;
			}

			// IR2
			if (temp2 < 0) {
				gte->dataRegisters[10] = 0;
				gte->controlRegisters[31] |= 0x800000;
			} else if (temp2 > 0x7FFF) {
				gte->dataRegisters[10] = 0x7FFF;
				gte->controlRegisters[31] |= 0x800000;
			} else {
				gte->dataRegisters[10] = (int32_t)temp2;
			}

			// IR3
			if (temp3 < 0) {
				gte->dataRegisters[11] = 0;
				gte->controlRegisters[31] |= 0x400000;
			} else if (temp3 > 0x7FFF) {
				gte->dataRegisters[11] = 0x7FFF;
				gte->controlRegisters[31] |= 0x400000;
			} else {
				gte->dataRegisters[11] = (int32_t)temp3;
			}
		}
			break;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;
}

/*
 * This function handles the DPCS GTE function.
 */
static void Cop2_handleDPCS(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve IR0 value and define IR1, IR2 and IR3
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0, ir2 = 0, ir3 = 0;

	// Sign extend IR0 value
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21];
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22];
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23];

	// Sign extend far colour values
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Perform DPCS-only calculation
	int64_t mac1 = r << 16;
	int64_t mac2 = g << 16;
	int64_t mac3 = b << 16;

	// Check for and set MAC1, MAC2 and MAC3 flags if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform first common stage of calculation
	ir1 = ((rfc << 12) - mac1) >> (sf * 12);
	ir2 = ((gfc << 12) - mac2) >> (sf * 12);
	ir3 = ((bfc << 12) - mac3) >> (sf * 12);

	// Saturate IR1, IR2 and IR3 results, setting flags as needed
	if (ir1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (ir1 < -0x8000L) {
		ir1 = -0x8000L;
		gte->controlRegisters[31] |= 0x1000000;
	}

	if (ir2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (ir2 < -0x8000L) {
		ir2 = -0x8000L;
		gte->controlRegisters[31] |= 0x800000;
	}

	if (ir3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (ir3 < -0x8000L) {
		ir3 = -0x8000L;
		gte->controlRegisters[31] |= 0x400000;
	}

	// Continue first common stage of calculation
	mac1 = ir1 * ir0 + mac1;
	mac2 = ir2 * ir0 + mac2;
	mac3 = ir3 * ir0 + mac3;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Shift MAC1, MAC2 and MAC3 by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Generate colour FIFO values and check/set flags as needed
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store values back to registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the INTPL GTE function.
 */
static void Cop2_handleINTPL(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve IR0, IR1, IR2 and IR3 values
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend IR0, IR1, IR2 and IR3 values
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21];
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22];
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23];

	// Sign extend far colour values
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Retrieve code value from RGBC
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Perform INTPL-only calculation
	int64_t mac1 = ir1 << 12;
	int64_t mac2 = ir2 << 12;
	int64_t mac3 = ir3 << 12;

	// Check for and set MAC1, MAC2 and MAC3 flags if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform first common stage of calculation
	ir1 = ((rfc << 12) - mac1) >> (sf * 12);
	ir2 = ((gfc << 12) - mac2) >> (sf * 12);
	ir3 = ((bfc << 12) - mac3) >> (sf * 12);

	// Saturate IR1, IR2 and IR3 results, setting flags as needed
	if (ir1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (ir1 < -0x8000L) {
		ir1 = -0x8000L;
		gte->controlRegisters[31] |= 0x1000000;
	}

	if (ir2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (ir2 < -0x8000L) {
		ir2 = -0x8000L;
		gte->controlRegisters[31] |= 0x800000;
	}

	if (ir3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (ir3 < -0x8000L) {
		ir3 = -0x8000L;
		gte->controlRegisters[31] |= 0x400000;
	}

	// Continue first common stage of calculation
	mac1 = ir1 * ir0 + mac1;
	mac2 = ir2 * ir0 + mac2;
	mac3 = ir3 * ir0 + mac3;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Shift MAC1, MAC2 and MAC3 by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Generate colour FIFO values and check/set flags as needed
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store values back to registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the MVMVA GTE function.
 */
static void Cop2_handleMVMVA(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Filter out translation vector value
	int32_t tVec = logical_rshift((opcode & 0x6000), 13);

	// Filter out multiply vector value
	int32_t mVec = logical_rshift((opcode & 0x18000), 15);

	// Filter out multiply matrix value
	int32_t mMatrix = logical_rshift((opcode & 0x60000), 17);

	// Declare and store correct translation vector values
	int64_t tv1 = 0, tv2 = 0, tv3 = 0;
	switch (tVec) {
		case 0: // TR
			tv1 = 0xFFFFFFFFL & gte->controlRegisters[5]; // TRX
			tv2 = 0xFFFFFFFFL & gte->controlRegisters[6]; // TRY
			tv3 = 0xFFFFFFFFL & gte->controlRegisters[7]; // TRZ
			break;
		case 1: // BK
			tv1 = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
			tv2 = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
			tv3 = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK
			break;
		case 2: // FC
			tv1 = 0xFFFFFFFFL & gte->controlRegisters[21]; // RFC
			tv2 = 0xFFFFFFFFL & gte->controlRegisters[22]; // GFC
			tv3 = 0xFFFFFFFFL & gte->controlRegisters[23]; // BFC
			break;
		case 3: // None (do nothing as 0 already set)
			break;
	}

	// Sign extend translation vector values
	if ((tv1 & 0x80000000L) == 0x80000000L)
		tv1 |= 0xFFFFFFFF00000000L;
	if ((tv2 & 0x80000000L) == 0x80000000L)
		tv2 |= 0xFFFFFFFF00000000L;
	if ((tv3 & 0x80000000L) == 0x80000000L)
		tv3 |= 0xFFFFFFFF00000000L;

	// Declare and store correct multiply vector values
	int64_t mv1 = 0, mv2 = 0, mv3 = 0;
	switch (mVec) {
		case 0: // V0
			mv1 = 0xFFFF & gte->dataRegisters[0]; // VX0
			mv2 = 0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
			mv3 = 0xFFFF & gte->dataRegisters[1]; // VZ0
			break;
		case 1: // V1
			mv1 = 0xFFFF & gte->dataRegisters[2]; // VX1
			mv2 = 0xFFFF & logical_rshift(gte->dataRegisters[2], 16); // VY1
			mv3 = 0xFFFF & gte->dataRegisters[3]; // VZ1
			break;
		case 2: // V2
			mv1 = 0xFFFF & gte->dataRegisters[4]; // VX2
			mv2 = 0xFFFF & logical_rshift(gte->dataRegisters[4], 16); // VY2
			mv3 = 0xFFFF & gte->dataRegisters[5]; // VZ2
			break;
		case 3: // [IR1,IR2,IR3]
			mv1 = 0xFFFF & gte->dataRegisters[9]; // IR1
			mv2 = 0xFFFF & gte->dataRegisters[10]; // IR2
			mv3 = 0xFFFF & gte->dataRegisters[11]; // IR3
			break;
	}

	// Sign extend multiply vector values
	if ((mv1 & 0x8000L) == 0x8000L)
		mv1 |= 0xFFFFFFFFFFFF0000L;
	if ((mv2 & 0x8000L) == 0x8000L)
		mv2 |= 0xFFFFFFFFFFFF0000L;
	if ((mv3 & 0x8000L) == 0x8000L)
		mv3 |= 0xFFFFFFFFFFFF0000L;

	// Declare and store correct multiply matrix values
	int64_t mx11 = 0, mx12 = 0, mx13 = 0, mx21 = 0,
			mx22 = 0, mx23 = 0, mx31 = 0, mx32 = 0, mx33 = 0;
	switch (mMatrix) {
		case 0: // Rotation matrix
			mx11 =
				0xFFFF & gte->controlRegisters[0]; // RT11;
			mx12 =
				0xFFFF & logical_rshift(gte->controlRegisters[0], 16); // RT12
			mx13 =
				0xFFFF & gte->controlRegisters[1]; // RT13
			mx21 =
				0xFFFF & logical_rshift(gte->controlRegisters[1], 16); // RT21
			mx22 =
				0xFFFF & gte->controlRegisters[2]; // RT22
			mx23 =
				0xFFFF & logical_rshift(gte->controlRegisters[2], 16); // RT23
			mx31 =
				0xFFFF & gte->controlRegisters[3]; // RT31
			mx32 =
				0xFFFF & logical_rshift(gte->controlRegisters[3], 16); // RT32
			mx33 =
				0xFFFF & gte->controlRegisters[4]; // RT33
			break;
		case 1: // Light matrix
			mx11 =
				0xFFFF & gte->controlRegisters[8]; // L11;
			mx12 =
				0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
			mx13 =
				0xFFFF & gte->controlRegisters[9]; // L13
			mx21 =
				0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
			mx22 =
				0xFFFF & gte->controlRegisters[10]; // L22
			mx23 =
				0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
			mx31 =
				0xFFFF & gte->controlRegisters[11]; // L31
			mx32 =
				0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
			mx33 =
				0xFFFF & gte->controlRegisters[12]; // L33
			break;
		case 2: // Colour matrix
			mx11 =
				0xFFFF & gte->controlRegisters[16]; // LR1;
			mx12 =
				0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
			mx13 =
				0xFFFF & gte->controlRegisters[17]; // LR3
			mx21 =
				0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
			mx22 =
				0xFFFF & gte->controlRegisters[18]; // LG2
			mx23 =
				0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
			mx31 =
				0xFFFF & gte->controlRegisters[19]; // LB1
			mx32 =
				0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
			mx33 =
				0xFFFF & gte->controlRegisters[20]; // LB3
			break;
		case 3: // Reserved (garbage matrix)
			mx11 = -0x60;
			mx12 = 0x60;
			mx13 = 0xFFFF & gte->dataRegisters[8]; // IR0
			mx21 = 0xFFFF & gte->controlRegisters[1]; // RT13
			mx22 = 0xFFFF & gte->controlRegisters[1]; // RT13
			mx23 = 0xFFFF & gte->controlRegisters[1]; // RT13
			mx31 = 0xFFFF & gte->controlRegisters[2]; // RT22
			mx32 = 0xFFFF & gte->controlRegisters[2]; // RT22
			mx33 = 0xFFFF & gte->controlRegisters[2]; // RT22                
			break;
	}

	// Sign extend multiply matrix values
	if ((mx11 & 0x8000L) == 0x8000L && mMatrix != 3)
		mx11 |= 0xFFFFFFFFFFFF0000L;
	if ((mx12 & 0x8000L) == 0x8000L && mMatrix != 3)
		mx12 |= 0xFFFFFFFFFFFF0000L;
	if ((mx13 & 0x8000L) == 0x8000L)
		mx13 |= 0xFFFFFFFFFFFF0000L;
	if ((mx21 & 0x8000L) == 0x8000L)
		mx21 |= 0xFFFFFFFFFFFF0000L;
	if ((mx22 & 0x8000L) == 0x8000L)
		mx22 |= 0xFFFFFFFFFFFF0000L;
	if ((mx23 & 0x8000L) == 0x8000L)
		mx23 |= 0xFFFFFFFFFFFF0000L;
	if ((mx31 & 0x8000L) == 0x8000L)
		mx31 |= 0xFFFFFFFFFFFF0000L;
	if ((mx32 & 0x8000L) == 0x8000L)
		mx32 |= 0xFFFFFFFFFFFF0000L;
	if ((mx33 & 0x8000L) == 0x8000L)
		mx33 |= 0xFFFFFFFFFFFF0000L;

	// Declare temporary variables and do calculations
	int64_t temp1 = 0, temp2 = 0, temp3 = 0;
	if (tVec != 2) {
		temp1 = tv1 * 0x1000L + mx11 * mv1 + mx12 * mv2 + mx13 * mv3;
		temp2 = tv2 * 0x1000L + mx21 * mv1 + mx22 * mv2 + mx23 * mv3;
		temp3 = tv3 * 0x1000L + mx31 * mv1 + mx32 * mv2 + mx33 * mv3;
	} else { // Account for faulty FC vector calculation on real hardware
		temp1 = mx13 * mv3;
		temp2 = mx23 * mv3;
		temp3 = mx33 * mv3;
	}

	// Shift results right by (sf * 12) bits, preserving sign bit
	temp1 = temp1 >> (sf * 12);
	temp2 = temp2 >> (sf * 12);
	temp3 = temp3 >> (sf * 12);

	// Set MAC1, MAC2 and MAC3 registers
	gte->dataRegisters[25] = (int32_t)temp1; // MAC1
	gte->dataRegisters[26] = (int32_t)temp2; // MAC2
	gte->dataRegisters[27] = (int32_t)temp3; // MAC3

	// Set relevant MAC1, MAC2 and MAC3 flag bits

	// MAC1
	if (temp1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (temp1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	// MAC2
	if (temp2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (temp2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	// MAC3
	if (temp3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (temp3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Set IR1, IR2 and IR3 registers and saturation flag bits
	switch (lm) {
		case 0: // Saturate to -0x8000..0x7FFF
		{
			// IR1
			if (temp1 < -0x8000) {
				gte->dataRegisters[9] = -0x8000;
				gte->controlRegisters[31] |= 0x1000000;
			} else if (temp1 > 0x7FFF) {
				gte->dataRegisters[9] = 0x7FFF;
				gte->controlRegisters[31] |= 0x1000000;
			} else {
				gte->dataRegisters[9] = (int32_t)temp1;
			}

			// IR2
			if (temp2 < -0x8000) {
				gte->dataRegisters[10] = -0x8000;
				gte->controlRegisters[31] |= 0x800000;
			} else if (temp2 > 0x7FFF) {
				gte->dataRegisters[10] = 0x7FFF;
				gte->controlRegisters[31] |= 0x800000;
			} else {
				gte->dataRegisters[10] = (int32_t)temp2;
			}

			// IR3
			if (temp3 < -0x8000) {
				gte->dataRegisters[11] = -0x8000;
				gte->controlRegisters[31] |= 0x400000;
			} else if (temp3 > 0x7FFF) {
				gte->dataRegisters[11] = 0x7FFF;
				gte->controlRegisters[31] |= 0x400000;
			} else {
				gte->dataRegisters[11] = (int32_t)temp3;
			}
		}
			break;
		case 1: // Saturate to 0..0x7FFF
		{
			// IR1
			if (temp1 < 0) {
				gte->dataRegisters[9] = 0;
				gte->controlRegisters[31] |= 0x1000000;
			} else if (temp1 > 0x7FFF) {
				gte->dataRegisters[9] = 0x7FFF;
				gte->controlRegisters[31] |= 0x1000000;
			} else {
				gte->dataRegisters[9] = (int32_t)temp1;
			}

			// IR2
			if (temp2 < 0) {
				gte->dataRegisters[10] = 0;
				gte->controlRegisters[31] |= 0x800000;
			} else if (temp2 > 0x7FFF) {
				gte->dataRegisters[10] = 0x7FFF;
				gte->controlRegisters[31] |= 0x800000;
			} else {
				gte->dataRegisters[10] = (int32_t)temp2;
			}

			// IR3
			if (temp3 < 0) {
				gte->dataRegisters[11] = 0;
				gte->controlRegisters[31] |= 0x400000;
			} else if (temp3 > 0x7FFF) {
				gte->dataRegisters[11] = 0x7FFF;
				gte->controlRegisters[31] |= 0x400000;
			} else {
				gte->dataRegisters[11] = (int32_t)temp3;
			}
		}
			break;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;
}

/*
 * This function handles the NCDS GTE function.
 */
static void Cop2_handleNCDS(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21]; // RFC
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22]; // GFC
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23]; // BFC

	// Sign extend far colour values if needed
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Retrieve IR0 value
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0

	// Sign extend IR0 value if necessary
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Retrieve V0 values
	int64_t vx0 = 0xFFFF & gte->dataRegisters[0]; // VX0
	int64_t vy0 = 0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
	int64_t vz0 = 0xFFFF & gte->dataRegisters[1]; // VZ0

	// Sign extend V0 values if necessary
	if ((vx0 & 0x8000L) == 0x8000L)
		vx0 |= 0xFFFFFFFFFFFF0000L;
	if ((vy0 & 0x8000L) == 0x8000L)
		vy0 |= 0xFFFFFFFFFFFF0000L;
	if ((vz0 & 0x8000L) == 0x8000L)
		vz0 |= 0xFFFFFFFFFFFF0000L;

	// Perform first stage of calculation
	int64_t mac1 = l11 * vx0 + l12 * vy0 + l13 * vz0;
	int64_t mac2 = l21 * vx0 + l22 * vy0 + l23 * vz0;
	int64_t mac3 = l31 * vx0 + l32 * vy0 + l33 * vz0;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3
	int64_t ir1 = 0, ir2 = 0, ir3 = 0;
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform second stage of calculation
	mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
	mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
	mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3 (again)
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform first NCCx stage calculation
	mac1 = r * ir1;
	mac2 = g * ir2;
	mac3 = b * ir3;

	// Shift results left by four bits
	mac1 = mac1 << 4;
	mac2 = mac2 << 4;
	mac3 = mac3 << 4;

	// Check for and set MAC1, MAC2 and MAC3 flags again if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform NCDx only calculation
	ir1 = ((rfc << 12) - mac1) >> (sf * 12);
	ir2 = ((gfc << 12) - mac2) >> (sf * 12);
	ir3 = ((bfc << 12) - mac3) >> (sf * 12);

	// Saturate IR1, IR2 and IR3 if needed, but ignoring lm bit
	if (ir1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (ir1 < -0x8000L) {
		ir1 = -0x8000L;
		gte->controlRegisters[31] |= 0x1000000;
	}

	if (ir2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (ir2 < -0x8000L) {
		ir2 = -0x8000L;
		gte->controlRegisters[31] |= 0x800000;
	}

	if (ir3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (ir3 < -0x8000L) {
		ir3 = -0x8000L;
		gte->controlRegisters[31] |= 0x400000;
	}

	// Next stage of NCDx only calculation
	mac1 = ir1 * ir0 + mac1;
	mac2 = ir2 * ir0 + mac2;
	mac3 = ir3 * ir0 + mac3;

	// Check for and set MAC1, MAC2 and MAC3 flags again if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform second NCCx stage calculation,
	// shifting MAC1, MAC2 and MAC3 by (sf * 12), and preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for and set MAC1, MAC2 and MAC3 flags again if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results back to IR1, IR2 and IR3
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Calculate result to be stored to colour FIFO
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Check values and saturate + set flags if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate and set flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store all values back
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the CDP GTE function.
 */
static void Cop2_handleCDP(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21]; // RFC
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22]; // GFC
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23]; // BFC

	// Sign extend far colour values if needed
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Load IR0, IR1, IR2 and IR3 values
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend IR0, IR1, IR2 and IR3 values if needed
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Perform first stage of calculation
	int64_t mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
	int64_t mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
	int64_t mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

	// Shift results right by (sf * 12), preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results to IR1, IR2 and IR3        
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform second stage of calculation
	mac1 = (r * ir1) << 4;
	mac2 = (g * ir2) << 4;
	mac3 = (b * ir3) << 4;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform first part of CDP-only stage of calculation
	ir1 = ((rfc << 12) - mac1) >> (sf * 12);
	ir2 = ((gfc << 12) - mac2) >> (sf * 12);
	ir3 = ((bfc << 12) - mac3) >> (sf * 12);

	// Check for IR1, IR2 and IR3 values, saturating and flag setting if needed
	if (ir1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (ir1 < -0x8000L) {
		ir1 = -0x8000L;
		gte->controlRegisters[31] |= 0x1000000;
	}

	if (ir2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (ir2 < -0x8000L) {
		ir2 = -0x8000L;
		gte->controlRegisters[31] |= 0x800000;
	}

	if (ir3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (ir3 < -0x8000L) {
		ir3 = -0x8000L;
		gte->controlRegisters[31] |= 0x400000;
	}

	// Perform second part of CDP-only stage of calculation
	mac1 = ir1 * ir0 + mac1;
	mac2 = ir2 * ir0 + mac2;
	mac3 = ir3 * ir0 + mac3;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Shift MAC1, MAC2 and MAC3 right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results to IR1, IR2 and IR3 again    
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Generate colour FIFO values
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Check for colour FIFO flags and set if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store values back to registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the NCDT GTE function.
 */
static void Cop2_handleNCDT(Cop2 *gte, int32_t opcode)
{
	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21]; // RFC
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22]; // GFC
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23]; // BFC

	// Sign extend far colour values if needed
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Retrieve IR0 value
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0

	// Sign extend IR0 value if necessary
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Repeat calculation for V0, V1 and V2
	for (int32_t i = 0; i < 3; ++i) {

		// Clear flag register
		gte->controlRegisters[31] = 0;

		// Retrieve Vx values
		int64_t vxAny = 0, vyAny = 0, vzAny = 0;
		switch (i) {
			case 0:
				vxAny =
					0xFFFF & gte->dataRegisters[0]; // VX0
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
				vzAny =
					0xFFFF & gte->dataRegisters[1]; // VZ0
				break;
			case 1:
				vxAny =
					0xFFFF & gte->dataRegisters[2]; // VX1
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[2], 16); // VY1
				vzAny =
					0xFFFF & gte->dataRegisters[3]; // VZ1
				break;
			case 2:
				vxAny =
					0xFFFF & gte->dataRegisters[4]; // VX2
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[4], 16); // VY2
				vzAny =
					0xFFFF & gte->dataRegisters[5]; // VZ2
				break;
		}

		// Sign extend Vx values if necessary
		if ((vxAny & 0x8000L) == 0x8000L) {
			vxAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vyAny & 0x8000L) == 0x8000L) {
			vyAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vzAny & 0x8000L) == 0x8000L) {
			vzAny |= 0xFFFFFFFFFFFF0000L;
		}

		// Perform first stage of calculation
		int64_t mac1 = l11 * vxAny + l12 * vyAny + l13 * vzAny;
		int64_t mac2 = l21 * vxAny + l22 * vyAny + l23 * vzAny;
		int64_t mac3 = l31 * vxAny + l32 * vyAny + l33 * vzAny;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3
		int64_t ir1 = 0, ir2 = 0, ir3 = 0;
		int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Perform second stage of calculation
		mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
		mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
		mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3 (again)
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Perform first NCCx stage calculation
		mac1 = r * ir1;
		mac2 = g * ir2;
		mac3 = b * ir3;

		// Shift results left by four bits
		mac1 = mac1 << 4;
		mac2 = mac2 << 4;
		mac3 = mac3 << 4;

		// Check for and set MAC1, MAC2 and MAC3 flags again if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Perform NCDx only calculation
		ir1 = ((rfc << 12) - mac1) >> (sf * 12);
		ir2 = ((gfc << 12) - mac2) >> (sf * 12);
		ir3 = ((bfc << 12) - mac3) >> (sf * 12);

		// Saturate IR1, IR2 and IR3 if needed, but ignoring lm bit
		if (ir1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (ir1 < -0x8000L) {
			ir1 = -0x8000L;
			gte->controlRegisters[31] |= 0x1000000;
		}

		if (ir2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (ir2 < -0x8000L) {
			ir2 = -0x8000L;
			gte->controlRegisters[31] |= 0x800000;
		}

		if (ir3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (ir3 < -0x8000L) {
			ir3 = -0x8000L;
			gte->controlRegisters[31] |= 0x400000;
		}

		// Next stage of NCDx only calculation
		mac1 = ir1 * ir0 + mac1;
		mac2 = ir2 * ir0 + mac2;
		mac3 = ir3 * ir0 + mac3;

		// Check for and set MAC1, MAC2 and MAC3 flags again if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Perform second NCCx stage calculation,
		// shifting MAC1, MAC2 and MAC3 by (sf * 12), and preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Check for and set MAC1, MAC2 and MAC3 flags again if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Store results back to IR1, IR2 and IR3
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Calculate result to be stored to colour FIFO
		int64_t rOut = mac1 / 16;
		int64_t gOut = mac2 / 16;
		int64_t bOut = mac3 / 16;

		// Check values and saturate + set flags if needed
		if (rOut < 0) {
			rOut = 0;
			gte->controlRegisters[31] |= 0x200000;
		} else if (rOut > 0xFF) {
			rOut = 0xFF;
			gte->controlRegisters[31] |= 0x200000;
		}

		if (gOut < 0) {
			gOut = 0;
			gte->controlRegisters[31] |= 0x100000;
		} else if (gOut > 0xFF) {
			gOut = 0xFF;
			gte->controlRegisters[31] |= 0x100000;
		}

		if (bOut < 0) {
			bOut = 0;
			gte->controlRegisters[31] |= 0x80000;
		} else if (bOut > 0xFF) {
			bOut = 0xFF;
			gte->controlRegisters[31] |= 0x80000;
		}

		// Calculate and set flag bit 31
		if ((gte->controlRegisters[31] & 0x7F87E000) != 0) {
			gte->controlRegisters[31] |= 0x80000000;
		}

		// Store all values back
		gte->dataRegisters[25] = (int32_t)mac1; // MAC1
		gte->dataRegisters[26] = (int32_t)mac2; // MAC2
		gte->dataRegisters[27] = (int32_t)mac3; // MAC3

		gte->dataRegisters[9] = (int32_t)ir1; // IR1
		gte->dataRegisters[10] = (int32_t)ir2; // IR2
		gte->dataRegisters[11] = (int32_t)ir3; // IR3

		gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
		gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
		gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
	}
}

/*
 * This function handles the NCCS GTE function.
 */
static void Cop2_handleNCCS(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Retrieve V0 values
	int64_t vx0 = 0xFFFF & gte->dataRegisters[0]; // VX0
	int64_t vy0 = 0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
	int64_t vz0 = 0xFFFF & gte->dataRegisters[1]; // VZ0

	// Sign extend V0 values if necessary
	if ((vx0 & 0x8000L) == 0x8000L)
		vx0 |= 0xFFFFFFFFFFFF0000L;
	if ((vy0 & 0x8000L) == 0x8000L)
		vy0 |= 0xFFFFFFFFFFFF0000L;
	if ((vz0 & 0x8000L) == 0x8000L)
		vz0 |= 0xFFFFFFFFFFFF0000L;

	// Perform first stage of calculation
	int64_t mac1 = l11 * vx0 + l12 * vy0 + l13 * vz0;
	int64_t mac2 = l21 * vx0 + l22 * vy0 + l23 * vz0;
	int64_t mac3 = l31 * vx0 + l32 * vy0 + l33 * vz0;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3
	int64_t ir1 = 0, ir2 = 0, ir3 = 0;
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform second stage of calculation
	mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
	mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
	mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3 (again)
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform first NCCx stage calculation
	mac1 = r * ir1;
	mac2 = g * ir2;
	mac3 = b * ir3;

	// Shift results left by four bits
	mac1 = mac1 << 4;
	mac2 = mac2 << 4;
	mac3 = mac3 << 4;

	// Check for and set MAC1, MAC2 and MAC3 flags again if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform second NCCx stage calculation,
	// shifting MAC1, MAC2 and MAC3 by (sf * 12), and preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for and set MAC1, MAC2 and MAC3 flags again if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results back to IR1, IR2 and IR3
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Calculate result to be stored to colour FIFO
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Check values and saturate + set flags if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate and set flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store all values back
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the CC GTE function.
 */
static void Cop2_handleCC(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Load IR1, IR2 and IR3 values
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend IR1, IR2 and IR3 values if needed
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Perform first stage of calculation
	int64_t mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
	int64_t mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
	int64_t mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

	// Shift results right by (sf * 12), preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results to IR1, IR2 and IR3 
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform second stage of calculation
	mac1 = (r * ir1) << 4;
	mac2 = (g * ir2) << 4;
	mac3 = (b * ir3) << 4;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Shift MAC1, MAC2 and MAC3 right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store results to IR1, IR2 and IR3 again    
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Generate colour FIFO values
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Check for colour FIFO flags and set if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store values back to registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the NCS GTE function.
 */
static void Cop2_handleNCS(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Retrieve V0 values
	int64_t vx0 = 0xFFFF & gte->dataRegisters[0]; // VX0
	int64_t vy0 = 0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
	int64_t vz0 = 0xFFFF & gte->dataRegisters[1]; // VZ0

	// Sign extend V0 values if necessary
	if ((vx0 & 0x8000L) == 0x8000L)
		vx0 |= 0xFFFFFFFFFFFF0000L;
	if ((vy0 & 0x8000L) == 0x8000L)
		vy0 |= 0xFFFFFFFFFFFF0000L;
	if ((vz0 & 0x8000L) == 0x8000L)
		vz0 |= 0xFFFFFFFFFFFF0000L;

	// Perform first stage of calculation
	int64_t mac1 = l11 * vx0 + l12 * vy0 + l13 * vz0;
	int64_t mac2 = l21 * vx0 + l22 * vy0 + l23 * vz0;
	int64_t mac3 = l31 * vx0 + l32 * vy0 + l33 * vz0;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3
	int64_t ir1 = 0, ir2 = 0, ir3 = 0;
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Perform second stage of calculation
	mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
	mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
	mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

	// Shift right by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Setup IR1, IR2 and IR3 (again)
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Calculate result to be stored to colour FIFO
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Check values and saturate + set flags if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate and set flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store all values back
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the NCT GTE function.
 */
static void Cop2_handleNCT(Cop2 *gte, int32_t opcode)
{
	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Repeat function three times, using V0, V1 and V2
	for (int32_t i = 0; i < 3; ++i) {

		// Clear flag register
		gte->controlRegisters[31] = 0;

		// Retrieve Vx values
		int64_t vxAny = 0, vyAny = 0, vzAny = 0;
		switch (i) {
			case 0:
				vxAny =
					0xFFFF & gte->dataRegisters[0]; // VX0
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
				vzAny =
					0xFFFF & gte->dataRegisters[1]; // VZ0
				break;
			case 1:
				vxAny =
					0xFFFF & gte->dataRegisters[2]; // VX1
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[2], 16); // VY1
				vzAny =
					0xFFFF & gte->dataRegisters[3]; // VZ1
				break;
			case 2:
				vxAny =
					0xFFFF & gte->dataRegisters[4]; // VX2
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[4], 16); // VY2
				vzAny =
					0xFFFF & gte->dataRegisters[5]; // VZ2
				break;
		}

		// Sign extend Vx values if necessary
		if ((vxAny & 0x8000L) == 0x8000L) {
			vxAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vyAny & 0x8000L) == 0x8000L) {
			vyAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vzAny & 0x8000L) == 0x8000L) {
			vzAny |= 0xFFFFFFFFFFFF0000L;
		}

		// Perform first stage of calculation
		int64_t mac1 = l11 * vxAny + l12 * vyAny + l13 * vzAny;
		int64_t mac2 = l21 * vxAny + l22 * vyAny + l23 * vzAny;
		int64_t mac3 = l31 * vxAny + l32 * vyAny + l33 * vzAny;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3
		int64_t ir1 = 0, ir2 = 0, ir3 = 0;
		int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Perform second stage of calculation
		mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
		mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
		mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3 (again)
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Calculate result to be stored to colour FIFO
		int64_t rOut = mac1 / 16;
		int64_t gOut = mac2 / 16;
		int64_t bOut = mac3 / 16;

		// Check values and saturate + set flags if needed
		if (rOut < 0) {
			rOut = 0;
			gte->controlRegisters[31] |= 0x200000;
		} else if (rOut > 0xFF) {
			rOut = 0xFF;
			gte->controlRegisters[31] |= 0x200000;
		}

		if (gOut < 0) {
			gOut = 0;
			gte->controlRegisters[31] |= 0x100000;
		} else if (gOut > 0xFF) {
			gOut = 0xFF;
			gte->controlRegisters[31] |= 0x100000;
		}

		if (bOut < 0) {
			bOut = 0;
			gte->controlRegisters[31] |= 0x80000;
		} else if (bOut > 0xFF) {
			bOut = 0xFF;
			gte->controlRegisters[31] |= 0x80000;
		}

		// Calculate and set flag bit 31
		if ((gte->controlRegisters[31] & 0x7F87E000) != 0) {
			gte->controlRegisters[31] |= 0x80000000;
		}

		// Store all values back
		gte->dataRegisters[25] = (int32_t)mac1; // MAC1
		gte->dataRegisters[26] = (int32_t)mac2; // MAC2
		gte->dataRegisters[27] = (int32_t)mac3; // MAC3

		gte->dataRegisters[9] = (int32_t)ir1; // IR1
		gte->dataRegisters[10] = (int32_t)ir2; // IR2
		gte->dataRegisters[11] = (int32_t)ir3; // IR3

		gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
		gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
		gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
	}
}

/*
 * This function handles the SQR GTE function.
 */
static void Cop2_handleSQR(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Fetch IR1, IR2 and IR3
	int64_t temp1 = gte->dataRegisters[9] & 0xFFFF; // IR1
	int64_t temp2 = gte->dataRegisters[10] & 0xFFFF; // IR2
	int64_t temp3 = gte->dataRegisters[11] & 0xFFFF; // IR3

	// Extend sign bit if necessary
	if ((temp1 & 0x8000L) == 0x8000L)
		temp1 |= 0xFFFFFFFFFFFF0000L;
	if ((temp2 & 0x8000L) == 0x8000L)
		temp2 |= 0xFFFFFFFFFFFF0000L;
	if ((temp3 & 0x8000L) == 0x8000L)
		temp3 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculations
	temp1 = temp1 * temp1;
	temp2 = temp2 * temp2;
	temp3 = temp3 * temp3;

	// Shift if specified
	temp1 = logical_rshift(temp1, (12 * sf));
	temp2 = logical_rshift(temp2, (12 * sf));
	temp3 = logical_rshift(temp3, (12 * sf));

	// Set MAC1, MAC2 and MAC3 registers
	gte->dataRegisters[25] = (int32_t)temp1; // MAC1
	gte->dataRegisters[26] = (int32_t)temp2; // MAC2
	gte->dataRegisters[27] = (int32_t)temp3; // MAC3

	// Set IR1, IR2 and IR3 registers
	gte->dataRegisters[9] = (temp1 > 0x7FFF) ? 0x7FFF : (int32_t)temp1; // IR1
	gte->dataRegisters[10] = (temp2 > 0x7FFF) ? 0x7FFF : (int32_t)temp2; // IR2
	gte->dataRegisters[11] = (temp3 > 0x7FFF) ? 0x7FFF : (int32_t)temp3; // IR3

	// Set flags
	if (temp1 > 0x7FFF)
		gte->controlRegisters[31] |= 0x1000000;
	if (temp2 > 0x7FFF)
		gte->controlRegisters[31] |= 0x800000;
	if (temp3 > 0x7FFF)
		gte->controlRegisters[31] |= 0x400000;
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;
}

/*
 * This function handles the DCPL GTE function.
 */
static void Cop2_handleDCPL(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve IR0, IR1, IR2 and IR3 values
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend IR0, IR1, IR2 and IR3 values
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21];
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22];
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23];

	// Sign extend far colour values
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Perform DCPL-only calculation
	int64_t mac1 = (r * ir1) << 4;
	int64_t mac2 = (g * ir2) << 4;
	int64_t mac3 = (b * ir3) << 4;

	// Check for and set MAC1, MAC2 and MAC3 flags if needed
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Perform first common stage of calculation
	ir1 = ((rfc << 12) - mac1) >> (sf * 12);
	ir2 = ((gfc << 12) - mac2) >> (sf * 12);
	ir3 = ((bfc << 12) - mac3) >> (sf * 12);

	// Saturate IR1, IR2 and IR3 results, setting flags as needed
	if (ir1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (ir1 < -0x8000L) {
		ir1 = -0x8000L;
		gte->controlRegisters[31] |= 0x1000000;
	}

	if (ir2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (ir2 < -0x8000L) {
		ir2 = -0x8000L;
		gte->controlRegisters[31] |= 0x800000;
	}

	if (ir3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (ir3 < -0x8000L) {
		ir3 = -0x8000L;
		gte->controlRegisters[31] |= 0x400000;
	}

	// Continue first common stage of calculation
	mac1 = ir1 * ir0 + mac1;
	mac2 = ir2 * ir0 + mac2;
	mac3 = ir3 * ir0 + mac3;

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Shift MAC1, MAC2 and MAC3 by (sf * 12) bits, preserving sign bit
	mac1 = mac1 >> (sf * 12);
	mac2 = mac2 >> (sf * 12);
	mac3 = mac3 >> (sf * 12);

	// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Generate colour FIFO values and check/set flags as needed
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store values back to registers
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the DPCT GTE function.
 */
static void Cop2_handleDPCT(Cop2 *gte, int32_t opcode)
{
	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve IR0 value and define IR1, IR2 and IR3
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0

	// Sign extend IR0 value
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve far colour values
	int64_t rfc = 0xFFFFFFFFL & gte->controlRegisters[21];
	int64_t gfc = 0xFFFFFFFFL & gte->controlRegisters[22];
	int64_t bfc = 0xFFFFFFFFL & gte->controlRegisters[23];

	// Sign extend far colour values
	if ((rfc & 0x80000000L) == 0x80000000L)
		rfc |= 0xFFFFFFFF00000000L;
	if ((gfc & 0x80000000L) == 0x80000000L)
		gfc |= 0xFFFFFFFF00000000L;
	if ((bfc & 0x80000000L) == 0x80000000L)
		bfc |= 0xFFFFFFFF00000000L;

	// Repeat calculation three times
	for (int32_t i = 0; i < 3; ++i) {

		// Clear flag register
		gte->controlRegisters[31] = 0;

		// Retrieve CODE value from RGBC and RGB values from RGB0
		int64_t r = 0xFF & gte->dataRegisters[20]; // R0
		int64_t g = 0xFF & logical_rshift(gte->dataRegisters[20], 8); // G0
		int64_t b = 0xFF & logical_rshift(gte->dataRegisters[20], 16); // B0
		int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

		// Perform DPCT-only calculation
		int64_t mac1 = r << 16;
		int64_t mac2 = g << 16;
		int64_t mac3 = b << 16;

		// Check for and set MAC1, MAC2 and MAC3 flags if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Perform first common stage of calculation
		int64_t ir1 = 0, ir2 = 0, ir3 = 0;
		ir1 = ((rfc << 12) - mac1) >> (sf * 12);
		ir2 = ((gfc << 12) - mac2) >> (sf * 12);
		ir3 = ((bfc << 12) - mac3) >> (sf * 12);

		// Saturate IR1, IR2 and IR3 results, setting flags as needed
		if (ir1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (ir1 < -0x8000L) {
			ir1 = -0x8000L;
			gte->controlRegisters[31] |= 0x1000000;
		}

		if (ir2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (ir2 < -0x8000L) {
			ir2 = -0x8000L;
			gte->controlRegisters[31] |= 0x800000;
		}

		if (ir3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (ir3 < -0x8000L) {
			ir3 = -0x8000L;
			gte->controlRegisters[31] |= 0x400000;
		}

		// Continue first common stage of calculation
		mac1 = ir1 * ir0 + mac1;
		mac2 = ir2 * ir0 + mac2;
		mac3 = ir3 * ir0 + mac3;

		// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Shift MAC1, MAC2 and MAC3 by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Check for MAC1, MAC2 and MAC3 flags again and set accordingly
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
		int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Generate colour FIFO values and check/set flags as needed
		int64_t rOut = mac1 / 16;
		int64_t gOut = mac2 / 16;
		int64_t bOut = mac3 / 16;

		if (rOut < 0) {
			rOut = 0;
			gte->controlRegisters[31] |= 0x200000;
		} else if (rOut > 0xFF) {
			rOut = 0xFF;
			gte->controlRegisters[31] |= 0x200000;
		}

		if (gOut < 0) {
			gOut = 0;
			gte->controlRegisters[31] |= 0x100000;
		} else if (gOut > 0xFF) {
			gOut = 0xFF;
			gte->controlRegisters[31] |= 0x100000;
		}

		if (bOut < 0) {
			bOut = 0;
			gte->controlRegisters[31] |= 0x80000;
		} else if (bOut > 0xFF) {
			bOut = 0xFF;
			gte->controlRegisters[31] |= 0x80000;
		}

		// Calculate bit 31 of flag register
		if ((gte->controlRegisters[31] & 0x7F87E000) != 0) {
			gte->controlRegisters[31] |= 0x80000000;
		}

		// Store values back to registers
		gte->dataRegisters[25] = (int32_t)mac1; // MAC1
		gte->dataRegisters[26] = (int32_t)mac2; // MAC2
		gte->dataRegisters[27] = (int32_t)mac3; // MAC3

		gte->dataRegisters[9] = (int32_t)ir1; // IR1
		gte->dataRegisters[10] = (int32_t)ir2; // IR2
		gte->dataRegisters[11] = (int32_t)ir3; // IR3

		gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
		gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
		gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
	}
}

/*
 * This function handles the AVSZ3 GTE function.
 */
static void Cop2_handleAVSZ3(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Retrieve ZSF3, SZ1, SZ2 and SZ3
	int64_t zsf3 = 0xFFFF & gte->controlRegisters[29]; // ZSF3
	int64_t sz1 = 0xFFFF & gte->dataRegisters[17]; // SZ1
	int64_t sz2 = 0xFFFF & gte->dataRegisters[18]; // SZ2
	int64_t sz3 = 0xFFFF & gte->dataRegisters[19]; // SZ3

	// Sign extend ZSF3 if needed
	if ((zsf3 & 0x8000L) == 0x8000L)
		zsf3 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculation
	int64_t mac0 = zsf3 * (sz1 + sz2 + sz3);
	int64_t otz = mac0 / 0x1000;

	// Set flags where needed, and apply saturation to OTZ if needed
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;

	if (otz < 0) {
		otz = 0;
		gte->controlRegisters[31] |= 0x40000;
	} else if (otz > 0xFFFFL) {
		otz = 0xFFFFL;
		gte->controlRegisters[31] |= 0x40000;
	}

	// Calculate flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store results back to registers
	gte->dataRegisters[24] = (int32_t)mac0;
	gte->dataRegisters[7] = (int32_t)otz;
}

/*
 * This function handles the AVSZ4 GTE function.
 */
static void Cop2_handleAVSZ4(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Retrieve ZSF4, SZ0, SZ1, SZ2 and SZ3
	int64_t zsf4 = 0xFFFF & gte->controlRegisters[30]; // ZSF4
	int64_t sz0 = 0xFFFF & gte->dataRegisters[16]; // SZ0
	int64_t sz1 = 0xFFFF & gte->dataRegisters[17]; // SZ1
	int64_t sz2 = 0xFFFF & gte->dataRegisters[18]; // SZ2
	int64_t sz3 = 0xFFFF & gte->dataRegisters[19]; // SZ3

	// Sign extend ZSF4 if needed
	if ((zsf4 & 0x8000L) == 0x8000L)
		zsf4 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculation
	int64_t mac0 = zsf4 * (sz0 + sz1 + sz2 + sz3);
	int64_t otz = mac0 / 0x1000;

	// Set flags where needed, and apply saturation to OTZ if needed
	if (mac0 > 0x80000000L)
		gte->controlRegisters[31] |= 0x10000;
	else if (mac0 < -0x80000000L)
		gte->controlRegisters[31] |= 0x8000;

	if (otz < 0) {
		otz = 0;
		gte->controlRegisters[31] |= 0x40000;
	} else if (otz > 0xFFFFL) {
		otz = 0xFFFFL;
		gte->controlRegisters[31] |= 0x40000;
	}

	// Calculate flag bit 31
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store results back to registers
	gte->dataRegisters[24] = (int32_t)mac0;
	gte->dataRegisters[7] = (int32_t)otz;
}

/*
 * This function handles the RTPT GTE function.
 */
static void Cop2_handleRTPT(Cop2 *gte, int32_t opcode)
{
	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve rotation matrix values
	int64_t rt11 =
			0xFFFF & gte->controlRegisters[0]; // RT11
	int64_t rt12 =
			0xFFFF & logical_rshift(gte->controlRegisters[0], 16); // RT12
	int64_t rt13 =
			0xFFFF & gte->controlRegisters[1]; // RT13
	int64_t rt21 =
			0xFFFF & logical_rshift(gte->controlRegisters[1], 16); // RT21
	int64_t rt22 =
			0xFFFF & gte->controlRegisters[2]; // RT22
	int64_t rt23 =
			0xFFFF & logical_rshift(gte->controlRegisters[2], 16); // RT23
	int64_t rt31 =
			0xFFFF & gte->controlRegisters[3]; // RT31
	int64_t rt32 =
			0xFFFF & logical_rshift(gte->controlRegisters[3], 16); // RT32
	int64_t rt33 =
			0xFFFF & gte->controlRegisters[4]; // RT33

	// Sign extend rotation matrix values if necessary
	if ((rt11 & 0x8000L) == 0x8000L)
		rt11 |= 0xFFFFFFFFFFFF0000L;
	if ((rt12 & 0x8000L) == 0x8000L)
		rt12 |= 0xFFFFFFFFFFFF0000L;
	if ((rt13 & 0x8000L) == 0x8000L)
		rt13 |= 0xFFFFFFFFFFFF0000L;
	if ((rt21 & 0x8000L) == 0x8000L)
		rt21 |= 0xFFFFFFFFFFFF0000L;
	if ((rt22 & 0x8000L) == 0x8000L)
		rt22 |= 0xFFFFFFFFFFFF0000L;
	if ((rt23 & 0x8000L) == 0x8000L)
		rt23 |= 0xFFFFFFFFFFFF0000L;
	if ((rt31 & 0x8000L) == 0x8000L)
		rt31 |= 0xFFFFFFFFFFFF0000L;
	if ((rt32 & 0x8000L) == 0x8000L)
		rt32 |= 0xFFFFFFFFFFFF0000L;
	if ((rt33 & 0x8000L) == 0x8000L)
		rt33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve translation vector values
	int64_t trX = 0xFFFFFFFFL & gte->controlRegisters[5]; // TRX
	int64_t trY = 0xFFFFFFFFL & gte->controlRegisters[6]; // TRY
	int64_t trZ = 0xFFFFFFFFL & gte->controlRegisters[7]; // TRZ

	// Sign extend translation vector values if necessary
	if ((trX & 0x80000000L) == 0x80000000L)
		trX |= 0xFFFFFFFF00000000L;
	if ((trY & 0x80000000L) == 0x80000000L)
		trY |= 0xFFFFFFFF00000000L;
	if ((trZ & 0x80000000L) == 0x80000000L)
		trZ |= 0xFFFFFFFF00000000L;

	// Retrieve offset and distance values
	int64_t ofx = 0xFFFFFFFFL & gte->controlRegisters[24];
	int64_t ofy = 0xFFFFFFFFL & gte->controlRegisters[25];
	int64_t h = 0xFFFF & gte->controlRegisters[26];
	int64_t dqa = 0xFFFF & gte->controlRegisters[27];
	int64_t dqb = 0xFFFFFFFFL & gte->controlRegisters[28];

	// Sign extend signed values if needed
	if ((ofx & 0x80000000L) == 0x80000000L)
		ofx |= 0xFFFFFFFF00000000L;
	if ((ofy & 0x80000000L) == 0x80000000L)
		ofy |= 0xFFFFFFFF00000000L;
	if ((dqa & 0x8000L) == 0x8000L)
		dqa |= 0xFFFFFFFFFFFF0000L;
	if ((dqb & 0x80000000L) == 0x80000000L)
		dqb |= 0xFFFFFFFF00000000L;

	// Do calculation three times, using a different vector
	// with each iteration
	for (int32_t i = 0; i < 3; ++i) {

		// Clear flag register
		gte->controlRegisters[31] = 0;

		// Retrieve correct vector values
		int64_t vxAny = 0, vyAny = 0, vzAny = 0;

		switch (i) {
			case 0:
				vxAny =
					0xFFFF & gte->dataRegisters[0]; // VX0
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
				vzAny =
					0xFFFF & gte->dataRegisters[1]; // VZ0
				break;
			case 1:
				vxAny =
					0xFFFF & gte->dataRegisters[2]; // VX1
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[2], 16); // VY1
				vzAny =
					0xFFFF & gte->dataRegisters[3]; // VZ1
				break;
			case 2:
				vxAny =
					0xFFFF & gte->dataRegisters[4]; // VX2
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[4], 16); // VY2
				vzAny =
					0xFFFF & gte->dataRegisters[5]; // VZ2
				break;
		}


		// Sign extend vector values if necessary
		if ((vxAny & 0x8000L) == 0x8000L) {
			vxAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vyAny & 0x8000L) == 0x8000L) {
			vyAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vzAny & 0x8000L) == 0x8000L) {
			vzAny |= 0xFFFFFFFFFFFF0000L;
		}

		// Perform first stage of calculations
		int64_t mac1 =
				trX * 0x1000 + rt11 * vxAny + rt12 * vyAny + rt13 * vzAny;
		int64_t mac2 =
				trY * 0x1000 + rt21 * vxAny + rt22 * vyAny + rt23 * vzAny;
		int64_t mac3 =
				trZ * 0x1000 + rt31 * vxAny + rt32 * vyAny + rt33 * vzAny;

		// Shift all three results by (sf * 12), preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set MAC1, MAC2 and MAC3 flags as needed
		// MAC1
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		// MAC2
		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		// MAC3
		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Set IR1, IR2 and IR3 - dealing with flags too
		// saturation should be -0x8000..0x7FFF, regardless of lm bit
		// IR1
		int64_t ir1 = 0, ir2 = 0, ir3 = 0;
		if (mac1 < -0x8000) {
			ir1 = -0x8000;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 > 0x7FFF) {
			ir1 = 0x7FFF;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = (int32_t)mac1;
		}

		// IR2
		if (mac2 < -0x8000) {
			ir2 = -0x8000;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 > 0x7FFF) {
			ir2 = 0x7FFF;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = (int32_t)mac2;
		}

		// IR3
		if (mac3 < -0x8000) {
			ir3 = -0x8000;

			// Deal with quirk in IR3 flag handling
			if (sf == 0) {
				int64_t temp = mac3 >> 12;
				if (temp > 0x7FFF || temp < -0x8000) {
					gte->controlRegisters[31] |= 0x400000;
				}
			} else {
				gte->controlRegisters[31] |= 0x400000;
			}
		} else if (mac3 > 0x7FFF) {
			ir3 = 0x7FFF;

			// Deal with quirk in IR3 flag handling
			if (sf == 0) {
				int64_t temp = mac3 >> 12;
				if (temp > 0x7FFF || temp < -0x8000) {
					gte->controlRegisters[31] |= 0x400000;
				}
			} else {
				gte->controlRegisters[31] |= 0x400000;
			}
		} else {
			ir3 = (int32_t)mac3;
		}

		// Write back to real registers
		gte->dataRegisters[25] = (int32_t)mac1; // MAC1
		gte->dataRegisters[26] = (int32_t)mac2; // MAC2
		gte->dataRegisters[27] = (int32_t)mac3; // MAC3
		gte->dataRegisters[9] = (int32_t)ir1; // IR1
		gte->dataRegisters[10] = (int32_t)ir2; // IR2
		gte->dataRegisters[11] = (int32_t)ir3; // IR3

		// Calculate SZ3 and move FIFO along, also setting SZ3 flag if needed
		gte->dataRegisters[16] = gte->dataRegisters[17]; // SZ1 to SZ0
		gte->dataRegisters[17] = gte->dataRegisters[18]; // SZ2 to SZ1
		gte->dataRegisters[18] = gte->dataRegisters[19]; // SZ3 to SZ2
		int64_t temp_sz3 = mac3 >> ((1 - sf) * 12);
		if (temp_sz3 < 0) {
			temp_sz3 = 0;
			gte->controlRegisters[31] |= 0x40000;
		} else if (temp_sz3 > 0xFFFF) {
			temp_sz3 = 0xFFFF;
			gte->controlRegisters[31] |= 0x40000;
		}
		gte->dataRegisters[19] = (int32_t)temp_sz3;

		// Begin second phase of calculations - use Unsigned Newton-Raphson
		// division algorithm from NOPSX documentation
		int64_t mac0 = 0, sx2 = 0, sy2 = 0, ir0 = 0;
		int64_t divisionResult = 0;
		if (h < temp_sz3 * 2) {

			// Count leading zeroes in SZ3
			int64_t z = 0, temp_sz3_2 = temp_sz3;
			for (int32_t j = 0; j < 16; ++j) {
				if ((temp_sz3_2 & 0x8000) == 0) {
					++z;
					temp_sz3_2 = temp_sz3_2 << 1;
				} else {
					break;
				}
			}

			divisionResult = h << z;
			int64_t d = temp_sz3 << z;
			int64_t u =
				unrResults[logical_rshift(((int32_t)d - 0x7FC0), 7)] + 0x101;
			d = logical_rshift((0x2000080 - (d * u)), 8);
			d = logical_rshift((0x80 + (d * u)), 8);
			divisionResult = min_value(0x1FFFFL,
					logical_rshift(((divisionResult * d) + 0x8000L), 16));

		} else {
			divisionResult = 0x1FFFFL;
			gte->controlRegisters[31] |= 0x20000;
		}

		// Use division result and set MAC0 flags if needed
		mac0 = divisionResult * ir1 + ofx;
		if (mac0 > 0x80000000L) {
			gte->controlRegisters[31] |= 0x10000;
		} else if (mac0 < -0x80000000L) {
			gte->controlRegisters[31] |= 0x8000;
		}
		mac0 &= 0xFFFFFFFFL;
		if ((mac0 & 0x80000000L) == 0x80000000L)
			mac0 |= 0xFFFFFFFF00000000L;
		sx2 = mac0 / 0x10000;
		mac0 = divisionResult * ir2 + ofy;
		if (mac0 > 0x80000000L) {
			gte->controlRegisters[31] |= 0x10000;
		} else if (mac0 < -0x80000000L) {
			gte->controlRegisters[31] |= 0x8000;
		}
		mac0 &= 0xFFFFFFFFL;
		if ((mac0 & 0x80000000L) == 0x80000000L)
			mac0 |= 0xFFFFFFFF00000000L;
		sy2 = mac0 / 0x10000;
		mac0 = divisionResult * dqa + dqb;
		if (mac0 > 0x80000000L) {
			gte->controlRegisters[31] |= 0x10000;
		} else if (mac0 < -0x80000000L) {
			gte->controlRegisters[31] |= 0x8000;
		}
		mac0 &= 0xFFFFFFFFL;
		if ((mac0 & 0x80000000L) == 0x80000000L)
			mac0 |= 0xFFFFFFFF00000000L;
		ir0 = mac0 / 0x1000;

		// Adjust results for saturation and set flags if needed
		if (sx2 > 0x3FFL) {
			sx2 = 0x3FFL;
			gte->controlRegisters[31] |= 0x4000;
		} else if (sx2 < -0x400) {
			sx2 = -0x400;
			gte->controlRegisters[31] |= 0x4000;
		}

		if (sy2 > 0x3FFL) {
			sy2 = 0x3FFL;
			gte->controlRegisters[31] |= 0x2000;
		} else if (sy2 < -0x400) {
			sy2 = -0x400;
			gte->controlRegisters[31] |= 0x2000;
		}

		if (ir0 < 0) {
			ir0 = 0;
			gte->controlRegisters[31] |= 0x1000;
		} else if (ir0 > 0x1000) {
			ir0 = 0x1000;
			gte->controlRegisters[31] |= 0x1000;
		}

		// Store values back to correct registers
		// SXY FIFO registers
		gte->dataRegisters[12] =
			gte->dataRegisters[13]; // SXY1 to SXY0
		gte->dataRegisters[13] =
			gte->dataRegisters[14]; // SXY2 to SXY1
		gte->dataRegisters[14] =
			(((int32_t)sy2 & 0xFFFF) << 16) | ((int32_t)sx2 & 0xFFFF); // SXY2
		gte->dataRegisters[15] =
			gte->dataRegisters[14]; // SXYP mirror of SXY2

		// MAC0
		gte->dataRegisters[24] = (int32_t)mac0;

		// IR0
		gte->dataRegisters[8] = (int32_t)ir0;

	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;
}

/*
 * This function handles the GPF GTE function.
 */
static void Cop2_handleGPF(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Define MAC1, MAC2 and MAC3 and set to 0
	int64_t mac1 = 0, mac2 = 0, mac3 = 0;

	// Retrieve IR0, IR1, IR2 and IR3
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend these four if necessary
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculations
	mac1 = ((ir1 * ir0) + mac1) >> (sf * 12);
	mac2 = ((ir2 * ir0) + mac2) >> (sf * 12);
	mac3 = ((ir3 * ir0) + mac3) >> (sf * 12);

	// Check/set flags for MAC1, MAC2 and MAC3
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Fetch code value from RGBC
	int64_t code = logical_rshift(gte->dataRegisters[6], 24);

	// Calculate colour FIFO entries
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Saturate FIFO entries and set flags if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store all values back
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the GPL GTE function.
 */
static void Cop2_handleGPL(Cop2 *gte, int32_t opcode)
{
	// Clear flag register
	gte->controlRegisters[31] = 0;

	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve MAC1, MAC2 and MAC3
	int64_t mac1 = 0xFFFFFFFFL & gte->dataRegisters[25];
	int64_t mac2 = 0xFFFFFFFFL & gte->dataRegisters[26];
	int64_t mac3 = 0xFFFFFFFFL & gte->dataRegisters[27];

	// Sign extend these values if needed
	if ((mac1 & 0x80000000L) == 0x80000000L)
		mac1 |= 0xFFFFFFFF00000000L;
	if ((mac2 & 0x80000000L) == 0x80000000L)
		mac2 |= 0xFFFFFFFF00000000L;
	if ((mac3 & 0x80000000L) == 0x80000000L)
		mac3 |= 0xFFFFFFFF00000000L;

	// Shift these values left by (sf * 12)
	mac1 = mac1 << (sf * 12);
	mac2 = mac2 << (sf * 12);
	mac3 = mac3 << (sf * 12);

	// Check/set flags for MAC1, MAC2 and MAC3
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Retrieve IR0, IR1, IR2 and IR3
	int64_t ir0 = 0xFFFF & gte->dataRegisters[8]; // IR0
	int64_t ir1 = 0xFFFF & gte->dataRegisters[9]; // IR1
	int64_t ir2 = 0xFFFF & gte->dataRegisters[10]; // IR2
	int64_t ir3 = 0xFFFF & gte->dataRegisters[11]; // IR3

	// Sign extend these four if necessary
	if ((ir0 & 0x8000L) == 0x8000L)
		ir0 |= 0xFFFFFFFFFFFF0000L;
	if ((ir1 & 0x8000L) == 0x8000L)
		ir1 |= 0xFFFFFFFFFFFF0000L;
	if ((ir2 & 0x8000L) == 0x8000L)
		ir2 |= 0xFFFFFFFFFFFF0000L;
	if ((ir3 & 0x8000L) == 0x8000L)
		ir3 |= 0xFFFFFFFFFFFF0000L;

	// Perform calculations
	mac1 = ((ir1 * ir0) + mac1) >> (sf * 12);
	mac2 = ((ir2 * ir0) + mac2) >> (sf * 12);
	mac3 = ((ir3 * ir0) + mac3) >> (sf * 12);

	// Check/set flags for MAC1, MAC2 and MAC3 (again)
	if (mac1 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x40000000;
	else if (mac1 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x8000000;

	if (mac2 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x20000000;
	else if (mac2 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x4000000;

	if (mac3 > 0x80000000000L)
		gte->controlRegisters[31] |= 0x10000000;
	else if (mac3 < -0x80000000000L)
		gte->controlRegisters[31] |= 0x2000000;

	// Store MAC1, MAC2 and MAC3 to IR1, IR2 and IR3
	int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

	if (mac1 > 0x7FFFL) {
		ir1 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x1000000;
	} else if (mac1 < lowerBound) {
		ir1 = lowerBound;
		gte->controlRegisters[31] |= 0x1000000;
	} else {
		ir1 = mac1;
	}

	if (mac2 > 0x7FFFL) {
		ir2 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x800000;
	} else if (mac2 < lowerBound) {
		ir2 = lowerBound;
		gte->controlRegisters[31] |= 0x800000;
	} else {
		ir2 = mac2;
	}

	if (mac3 > 0x7FFFL) {
		ir3 = 0x7FFFL;
		gte->controlRegisters[31] |= 0x400000;
	} else if (mac3 < lowerBound) {
		ir3 = lowerBound;
		gte->controlRegisters[31] |= 0x400000;
	} else {
		ir3 = mac3;
	}

	// Fetch code value from RGBC
	int64_t code = logical_rshift(gte->dataRegisters[6], 24);

	// Calculate colour FIFO entries
	int64_t rOut = mac1 / 16;
	int64_t gOut = mac2 / 16;
	int64_t bOut = mac3 / 16;

	// Saturate FIFO entries and set flags if needed
	if (rOut < 0) {
		rOut = 0;
		gte->controlRegisters[31] |= 0x200000;
	} else if (rOut > 0xFF) {
		rOut = 0xFF;
		gte->controlRegisters[31] |= 0x200000;
	}

	if (gOut < 0) {
		gOut = 0;
		gte->controlRegisters[31] |= 0x100000;
	} else if (gOut > 0xFF) {
		gOut = 0xFF;
		gte->controlRegisters[31] |= 0x100000;
	}

	if (bOut < 0) {
		bOut = 0;
		gte->controlRegisters[31] |= 0x80000;
	} else if (bOut > 0xFF) {
		bOut = 0xFF;
		gte->controlRegisters[31] |= 0x80000;
	}

	// Calculate bit 31 of flag register
	if ((gte->controlRegisters[31] & 0x7F87E000) != 0)
		gte->controlRegisters[31] |= 0x80000000;

	// Store all values back
	gte->dataRegisters[25] = (int32_t)mac1; // MAC1
	gte->dataRegisters[26] = (int32_t)mac2; // MAC2
	gte->dataRegisters[27] = (int32_t)mac3; // MAC3

	gte->dataRegisters[9] = (int32_t)ir1; // IR1
	gte->dataRegisters[10] = (int32_t)ir2; // IR2
	gte->dataRegisters[11] = (int32_t)ir3; // IR3

	gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
	gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
	gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
}

/*
 * This function handles the NCCT GTE function.
 */
static void Cop2_handleNCCT(Cop2 *gte, int32_t opcode)
{
	// Filter out sf bit
	int32_t sf = (opcode & 0x80000) == 0x80000 ? 1 : 0;

	// Filter out lm bit
	int32_t lm = (opcode & 0x400) == 0x400 ? 1 : 0;

	// Retrieve light matrix values
	int64_t l11 = 0xFFFF & gte->controlRegisters[8]; // L11
	int64_t l12 = 0xFFFF & logical_rshift(gte->controlRegisters[8], 16); // L12
	int64_t l13 = 0xFFFF & gte->controlRegisters[9]; // L13
	int64_t l21 = 0xFFFF & logical_rshift(gte->controlRegisters[9], 16); // L21
	int64_t l22 = 0xFFFF & gte->controlRegisters[10]; // L22
	int64_t l23 = 0xFFFF & logical_rshift(gte->controlRegisters[10], 16); // L23
	int64_t l31 = 0xFFFF & gte->controlRegisters[11]; // L31
	int64_t l32 = 0xFFFF & logical_rshift(gte->controlRegisters[11], 16); // L32
	int64_t l33 = 0xFFFF & gte->controlRegisters[12]; // L33

	// Sign extend light matrix values if needed
	if ((l11 & 0x8000L) == 0x8000L)
		l11 |= 0xFFFFFFFFFFFF0000L;
	if ((l12 & 0x8000L) == 0x8000L)
		l12 |= 0xFFFFFFFFFFFF0000L;
	if ((l13 & 0x8000L) == 0x8000L)
		l13 |= 0xFFFFFFFFFFFF0000L;
	if ((l21 & 0x8000L) == 0x8000L)
		l21 |= 0xFFFFFFFFFFFF0000L;
	if ((l22 & 0x8000L) == 0x8000L)
		l22 |= 0xFFFFFFFFFFFF0000L;
	if ((l23 & 0x8000L) == 0x8000L)
		l23 |= 0xFFFFFFFFFFFF0000L;
	if ((l31 & 0x8000L) == 0x8000L)
		l31 |= 0xFFFFFFFFFFFF0000L;
	if ((l32 & 0x8000L) == 0x8000L)
		l32 |= 0xFFFFFFFFFFFF0000L;
	if ((l33 & 0x8000L) == 0x8000L)
		l33 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve light colour matrix values
	int64_t lr1 = 0xFFFF & gte->controlRegisters[16]; // LR1
	int64_t lr2 = 0xFFFF & logical_rshift(gte->controlRegisters[16], 16); // LR2
	int64_t lr3 = 0xFFFF & gte->controlRegisters[17]; // LR3
	int64_t lg1 = 0xFFFF & logical_rshift(gte->controlRegisters[17], 16); // LG1
	int64_t lg2 = 0xFFFF & gte->controlRegisters[18]; // LG2
	int64_t lg3 = 0xFFFF & logical_rshift(gte->controlRegisters[18], 16); // LG3
	int64_t lb1 = 0xFFFF & gte->controlRegisters[19]; // LB1
	int64_t lb2 = 0xFFFF & logical_rshift(gte->controlRegisters[19], 16); // LB2
	int64_t lb3 = 0xFFFF & gte->controlRegisters[20]; // LB3

	// Sign extend light colour matrix values if needed
	if ((lr1 & 0x8000L) == 0x8000L)
		lr1 |= 0xFFFFFFFFFFFF0000L;
	if ((lr2 & 0x8000L) == 0x8000L)
		lr2 |= 0xFFFFFFFFFFFF0000L;
	if ((lr3 & 0x8000L) == 0x8000L)
		lr3 |= 0xFFFFFFFFFFFF0000L;
	if ((lg1 & 0x8000L) == 0x8000L)
		lg1 |= 0xFFFFFFFFFFFF0000L;
	if ((lg2 & 0x8000L) == 0x8000L)
		lg2 |= 0xFFFFFFFFFFFF0000L;
	if ((lg3 & 0x8000L) == 0x8000L)
		lg3 |= 0xFFFFFFFFFFFF0000L;
	if ((lb1 & 0x8000L) == 0x8000L)
		lb1 |= 0xFFFFFFFFFFFF0000L;
	if ((lb2 & 0x8000L) == 0x8000L)
		lb2 |= 0xFFFFFFFFFFFF0000L;
	if ((lb3 & 0x8000L) == 0x8000L)
		lb3 |= 0xFFFFFFFFFFFF0000L;

	// Retrieve background colour values
	int64_t rbk = 0xFFFFFFFFL & gte->controlRegisters[13]; // RBK
	int64_t gbk = 0xFFFFFFFFL & gte->controlRegisters[14]; // GBK
	int64_t bbk = 0xFFFFFFFFL & gte->controlRegisters[15]; // BBK

	// Sign extend background colour values if needed
	if ((rbk & 0x80000000L) == 0x80000000L)
		rbk |= 0xFFFFFFFF00000000L;
	if ((gbk & 0x80000000L) == 0x80000000L)
		gbk |= 0xFFFFFFFF00000000L;
	if ((bbk & 0x80000000L) == 0x80000000L)
		bbk |= 0xFFFFFFFF00000000L;

	// Retrieve RGBC values
	int64_t r = 0xFF & gte->dataRegisters[6]; // R
	int64_t g = 0xFF & logical_rshift(gte->dataRegisters[6], 8); // G
	int64_t b = 0xFF & logical_rshift(gte->dataRegisters[6], 16); // B
	int64_t code = 0xFF & logical_rshift(gte->dataRegisters[6], 24); // CODE

	// Repeat for vectors V0, V1 and V2
	for (int32_t i = 0; i < 3; ++i) {

		// Clear flag register
		gte->controlRegisters[31] = 0;

		// Retrieve Vx values
		int64_t vxAny = 0, vyAny = 0, vzAny = 0;
		switch (i) {
			case 0:
				vxAny =
					0xFFFF & gte->dataRegisters[0]; // VX0
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[0], 16); // VY0
				vzAny =
					0xFFFF & gte->dataRegisters[1]; // VZ0
				break;
			case 1:
				vxAny =
					0xFFFF & gte->dataRegisters[2]; // VX1
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[2], 16); // VY1
				vzAny =
					0xFFFF & gte->dataRegisters[3]; // VZ1
				break;
			case 2:
				vxAny =
					0xFFFF & gte->dataRegisters[4]; // VX2
				vyAny =
					0xFFFF & logical_rshift(gte->dataRegisters[4], 16); // VY2
				vzAny =
					0xFFFF & gte->dataRegisters[5]; // VZ2
				break;
		}

		// Sign extend V0 values if necessary
		if ((vxAny & 0x8000L) == 0x8000L) {
			vxAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vyAny & 0x8000L) == 0x8000L) {
			vyAny |= 0xFFFFFFFFFFFF0000L;
		}
		if ((vzAny & 0x8000L) == 0x8000L) {
			vzAny |= 0xFFFFFFFFFFFF0000L;
		}

		// Perform first stage of calculation
		int64_t mac1 = l11 * vxAny + l12 * vyAny + l13 * vzAny;
		int64_t mac2 = l21 * vxAny + l22 * vyAny + l23 * vzAny;
		int64_t mac3 = l31 * vxAny + l32 * vyAny + l33 * vzAny;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3
		int64_t ir1 = 0, ir2 = 0, ir3 = 0;
		int64_t lowerBound = (lm == 1) ? 0 : -0x8000L;

		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Perform second stage of calculation
		mac1 = rbk * 0x1000 + lr1 * ir1 + lr2 * ir2 + lr3 * ir3;
		mac2 = gbk * 0x1000 + lg1 * ir1 + lg2 * ir2 + lg3 * ir3;
		mac3 = bbk * 0x1000 + lb1 * ir1 + lb2 * ir2 + lb3 * ir3;

		// Shift right by (sf * 12) bits, preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Set flags and mask bits for MAC1, MAC2 and MAC3 (again)
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Setup IR1, IR2 and IR3 (again)
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Perform first NCCx stage calculation
		mac1 = r * ir1;
		mac2 = g * ir2;
		mac3 = b * ir3;

		// Shift results left by four bits
		mac1 = mac1 << 4;
		mac2 = mac2 << 4;
		mac3 = mac3 << 4;

		// Check for and set MAC1, MAC2 and MAC3 flags again if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Perform second NCCx stage calculation,
		// shifting MAC1, MAC2 and MAC3 by (sf * 12), and preserving sign bit
		mac1 = mac1 >> (sf * 12);
		mac2 = mac2 >> (sf * 12);
		mac3 = mac3 >> (sf * 12);

		// Check for and set MAC1, MAC2 and MAC3 flags again if needed
		if (mac1 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x40000000;
		} else if (mac1 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x8000000;
		}

		if (mac2 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x20000000;
		} else if (mac2 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x4000000;
		}

		if (mac3 > 0x80000000000L) {
			gte->controlRegisters[31] |= 0x10000000;
		} else if (mac3 < -0x80000000000L) {
			gte->controlRegisters[31] |= 0x2000000;
		}

		// Store results back to IR1, IR2 and IR3
		if (mac1 > 0x7FFFL) {
			ir1 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x1000000;
		} else if (mac1 < lowerBound) {
			ir1 = lowerBound;
			gte->controlRegisters[31] |= 0x1000000;
		} else {
			ir1 = mac1;
		}

		if (mac2 > 0x7FFFL) {
			ir2 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x800000;
		} else if (mac2 < lowerBound) {
			ir2 = lowerBound;
			gte->controlRegisters[31] |= 0x800000;
		} else {
			ir2 = mac2;
		}

		if (mac3 > 0x7FFFL) {
			ir3 = 0x7FFFL;
			gte->controlRegisters[31] |= 0x400000;
		} else if (mac3 < lowerBound) {
			ir3 = lowerBound;
			gte->controlRegisters[31] |= 0x400000;
		} else {
			ir3 = mac3;
		}

		// Calculate result to be stored to colour FIFO
		int64_t rOut = mac1 / 16;
		int64_t gOut = mac2 / 16;
		int64_t bOut = mac3 / 16;

		// Check values and saturate + set flags if needed
		if (rOut < 0) {
			rOut = 0;
			gte->controlRegisters[31] |= 0x200000;
		} else if (rOut > 0xFF) {
			rOut = 0xFF;
			gte->controlRegisters[31] |= 0x200000;
		}

		if (gOut < 0) {
			gOut = 0;
			gte->controlRegisters[31] |= 0x100000;
		} else if (gOut > 0xFF) {
			gOut = 0xFF;
			gte->controlRegisters[31] |= 0x100000;
		}

		if (bOut < 0) {
			bOut = 0;
			gte->controlRegisters[31] |= 0x80000;
		} else if (bOut > 0xFF) {
			bOut = 0xFF;
			gte->controlRegisters[31] |= 0x80000;
		}

		// Calculate and set flag bit 31
		if ((gte->controlRegisters[31] & 0x7F87E000) != 0) {
			gte->controlRegisters[31] |= 0x80000000;
		}

		// Store all values back
		gte->dataRegisters[25] = (int32_t)mac1; // MAC1
		gte->dataRegisters[26] = (int32_t)mac2; // MAC2
		gte->dataRegisters[27] = (int32_t)mac3; // MAC3

		gte->dataRegisters[9] = (int32_t)ir1; // IR1
		gte->dataRegisters[10] = (int32_t)ir2; // IR2
		gte->dataRegisters[11] = (int32_t)ir3; // IR3

		gte->dataRegisters[20] = gte->dataRegisters[21]; // RGB1 to RGB0
		gte->dataRegisters[21] = gte->dataRegisters[22]; // RGB2 to RGB1
		gte->dataRegisters[22] =
			(int32_t)((code << 24) | (bOut << 16) | (gOut << 8) | rOut); // RGB2
	}
}