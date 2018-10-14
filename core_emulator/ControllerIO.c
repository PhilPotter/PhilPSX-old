/*
 * This C file models the controllers and memory cards of the PlayStation
 * hardware as a class.
 * 
 * ControllerIO.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/ControllerIO.h"
#include "../headers/math_utils.h"

// Forward declarations for functions private to this class
// ControllerIO-related stuff:
static void ControllerIO_updateBaudrateTimer(ControllerIO *cio);
static void ControllerIO_updateJoyStat(ControllerIO *cio);

/*
 * This struct encapsulates the state of the IO subsystem.
 */
struct ControllerIO {
	
	// System object reference
	SystemInterlink *system;

	// RX fifo
	int8_t rxFifo[4];
	int32_t rxCount;

	// Controller related variables
	int32_t joyBaud; // JOY_BAUD
	int32_t joyTxData; // JOY_TX_DATA
	int32_t joyStat; // JOY_STAT
	int32_t joyMode; // JOY_MODE
	int32_t joyCtrl; // JOY_CTRL

	// Store number of cycles to change timer by
	int32_t cycles;
};

/*
 * This function constructs a ControllerIO object.
 */
ControllerIO *construct_ControllerIO(void)
{
	// Allocate ControllerIO object
	ControllerIO *cio = malloc(sizeof(ControllerIO));
	if (!cio) {
		fprintf(stderr, "PhilPSX: ControllerIO: Couldn't allocate memory for "
				"ControllerIO struct\n");
		goto end;
	}
	
	// Zero out RX fifo
	memset(cio->rxFifo, 0, sizeof(cio->rxFifo));
	cio->rxCount = 0;

	// Setup controller related variables
	cio->joyBaud = 0;
	cio->joyTxData = 0;
	cio->joyStat = 0;
	cio->joyMode = 0;
	cio->joyCtrl = 0;

	// Setup cycle store
	cio->cycles = 0;
	
	// Normal return:
	return cio;
	
	// Cleanup path:	
	end:
	return cio;
}

/*
 * This function destructs a ControllerIO object.
 */
void destruct_ControllerIO(ControllerIO *cio)
{
	free(cio);
}

/*
 * This function appends the cycles we need to sync the baudrate timer by.
 */
void ControllerIO_appendSyncCycles(ControllerIO *cio, int32_t cycles)
{
	cio->cycles += cycles;
}

/*
 * This function reads bytes from the right place in the object.
 */
int8_t ControllerIO_readByte(ControllerIO *cio, int32_t address)
{
	// Update baudrate timer
	ControllerIO_updateBaudrateTimer(cio);

	// Split least significant byte off address value
	int32_t tempAddress = address & 0xFF;

	// Declare return value
	int8_t retVal = 0;

	// Determine what to read based on this
	switch (tempAddress) {
		case 0x40: // JOY_RX_DATA 1st fifo entry
			if (cio->rxCount > 0) {
				retVal = cio->rxFifo[0];
				--cio->rxCount;
			}
			break;
		case 0x44: // JOY_STAT 1st (lowest) byte
			ControllerIO_updateJoyStat(cio);
			retVal = (int8_t)(cio->joyStat & 0xFF);
			break;
		case 0x45: // JOY_STAT 2nd byte
			retVal = (int8_t)(logical_rshift(cio->joyStat, 8) & 0xFF);
			break;
		case 0x46: // JOY_STAT 3rd byte
			retVal = (int8_t)(logical_rshift(cio->joyStat, 16) & 0xFF);
			break;
		case 0x47: // JOY_STAT 4th (highest) byte
			retVal = (int8_t)(logical_rshift(cio->joyStat, 24) & 0xFF);
			break;
		case 0x48: // JOY_MODE lower byte
			retVal = (int8_t)(cio->joyMode & 0xFF);
			break;
		case 0x49: // JOY_MODE higher byte
			retVal = (int8_t)(logical_rshift(cio->joyMode, 8) & 0xFF);
			break;
		case 0x4A: // JOY_CTRL lower byte
			retVal = (int8_t)(cio->joyCtrl & 0xFF);
			break;
		case 0x4B: // JOY_CTRL higher byte
			retVal = (int8_t)(logical_rshift(cio->joyCtrl, 8) & 0xFF);
			break;
		case 0x4E: // JOY_BAUD lower byte
			retVal = (int8_t)(cio->joyBaud & 0xFF);
			break;
		case 0x4F: // JOY_BAUD higher byte
			retVal = (int8_t)(logical_rshift(cio->joyBaud, 8) & 0xFF);
			break;
	}

	return retVal;
}

/*
 * This function writes bytes to the right place in the object.
 */
void ControllerIO_writeByte(ControllerIO *cio, int32_t address, int8_t value)
{
	// Update baudrate timer
	ControllerIO_updateBaudrateTimer(cio);

	// Split least significant byte off address value
	int32_t tempAddress = address & 0xFF;

	// Determine what to write based on this
	switch (tempAddress) {
		case 0x40: // JOY_TX_DATA lower byte
			cio->joyTxData = value & 0xFF;
			break;
		case 0x48: // JOY_MODE lower byte
			cio->joyMode = (cio->joyMode & 0xFF00) | (value & 0xFF);
			break;
		case 0x49: // JOY_MODE higher byte
			cio->joyMode = ((value & 0xFF) << 8) | (cio->joyMode & 0xFF);
			break;
		case 0x4A: // JOY_CTRL lower byte
			cio->joyCtrl = (cio->joyCtrl & 0xFF00) | (value & 0xFF);
			break;
		case 0x4B: // JOY_CTRL higher byte
			cio->joyCtrl = ((value & 0xFF) << 8) | (cio->joyCtrl & 0xFF);
			break;
		case 0x4E: // JOY_BAUD lower byte
		{
			cio->joyBaud = (cio->joyBaud & 0xFF00) | (value & 0xFF);
			int32_t baudRate = cio->joyBaud * (cio->joyMode & 0x3) / 2;
			cio->joyStat = (baudRate << 11) | (cio->joyStat & 0x7FF);
		}
			break;
		case 0x4F: // JOY_BAUD higher byte
		{
			cio->joyBaud = ((value & 0xFF) << 8) | (cio->joyBaud & 0xFF);
			int32_t baudRate = cio->joyBaud * (cio->joyMode & 0x3) / 2;
			cio->joyStat = (baudRate << 11) | (cio->joyStat & 0x7FF);
		}
			break;
	}
}

/*
 * This function sets the system reference to that of the supplied argument.
 */
void ControllerIO_setMemoryInterface(ControllerIO *cio, SystemInterlink *smi)
{
	cio->system = smi;
}

/*
 * This function updates the baudrate timer.
 */
static void ControllerIO_updateBaudrateTimer(ControllerIO *cio)
{
	int32_t baudRate = logical_rshift(cio->joyStat, 11) & 0x1FFFFF;
	baudRate -= cio->cycles;
	cio->cycles = 0;
	if (baudRate < 0) {
		baudRate = cio->joyBaud * (cio->joyMode & 0x3) / 2;
	}
	cio->joyStat = (baudRate << 11) | (cio->joyStat & 0x7FF);
}

/*
 * This function updates joyStat.
 */
static void ControllerIO_updateJoyStat(ControllerIO *cio)
{
	cio->joyStat |= 0x7;
}