/*
 * This C file models the CD-ROM module of the PlayStation hardware as a class.
 * 
 * CDROMDrive.c - Copyright Phillip Potter, 2020, under GPLv3
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "../headers/CDROMDrive.h"
#include "../headers/SystemInterlink.h"
#include "../headers/CD.h"

// Forward declarations for functions private to this class
// CDROMDrive-related stuff:
static bool CDROMDrive_allowCddaRead(CDROMDrive *cdrom);
static bool CDROMDrive_autoPause(CDROMDrive *cdrom);
static bool CDROMDrive_cddaPlaying(CDROMDrive *cdrom);
static void CDROMDrive_clearDataFifo(CDROMDrive *cdrom);
static void CDROMDrive_clearParameterFifo(CDROMDrive *cdrom);
static void CDROMDrive_clearResponseFifo(CDROMDrive *cdrom);
static bool CDROMDrive_commandError(CDROMDrive *cdrom);
static void CDROMDrive_command_Demute(CDROMDrive *cdrom);
static void CDROMDrive_command_GetID(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_Getstat(CDROMDrive *cdrom);
static void CDROMDrive_command_Init(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_Pause(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_ReadN(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_ReadTOC(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_SeekL(CDROMDrive *cdrom, bool secondResponse);
static void CDROMDrive_command_Setloc(CDROMDrive *cdrom);
static void CDROMDrive_command_Setmode(CDROMDrive *cdrom);
static void CDROMDrive_command_Test(CDROMDrive *cdrom);
static bool CDROMDrive_doubleSpeed(CDROMDrive *cdrom);
static bool CDROMDrive_enableReportInterrupts(CDROMDrive *cdrom);
static void CDROMDrive_executeCommand(CDROMDrive *cdrom, int32_t commandNum,
		bool secondResponse);
static int8_t CDROMDrive_getInterruptEnableRegister(CDROMDrive *cdrom);
static int8_t CDROMDrive_getInterruptFlagRegister(CDROMDrive *cdrom);
static int8_t CDROMDrive_getStatusCode(CDROMDrive *cdrom);
static bool CDROMDrive_idError(CDROMDrive *cdrom);
static bool CDROMDrive_ignoreBit(CDROMDrive *cdrom);
static bool CDROMDrive_isReading(CDROMDrive *cdrom);
static bool CDROMDrive_isSeeking(CDROMDrive *cdrom);
static bool CDROMDrive_motorStatus(CDROMDrive *cdrom);
static bool CDROMDrive_seekError(CDROMDrive *cdrom);
static bool CDROMDrive_shellOpen(CDROMDrive *cdrom);
static void CDROMDrive_triggerInterrupt(CDROMDrive *cdrom, int32_t interruptNum,
		int32_t delay);
static bool CDROMDrive_wholeSector(CDROMDrive *cdrom);
static bool CDROMDrive_xaAdpcm(CDROMDrive *cdrom);
static bool CDROMDrive_xaFilter(CDROMDrive *cdrom);

/*
 * This struct encapsulates the state of the CD-ROM module.
 */
struct CDROMDrive {

	// System reference
	SystemInterlink *system;

	// This controls what we are reading/writing 
	int32_t portIndex;

	// This stores parameters for commands
	int8_t parameterFifo[16];
	int32_t parameterCount;

	// This stores command responses
	int8_t responseFifo[16];
	int32_t responseCount;
	int32_t responseIndex;

	// This stores data from the CD
	int8_t *dataFifo; // Allocated dynamically due to size
	int32_t dataCount;
	int32_t dataIndex;

	// This references the actual CD
	CD *cd;

	// Interrupt registers
	int32_t interruptEnableRegister;
	int32_t interruptFlagRegister;

	// Busy flag and current command
	int32_t busy;
	int32_t currentCommand;
	bool needsSecondResponse;

	// These flags allow the composition of a status byte
	bool cddaPlaying;
	bool isSeeking;
	bool isReading;
	bool shellOpen;
	bool idError;
	bool seekError;
	bool motorStatus;
	bool commandError;

	// These flags control behaviour and are set via the Setmode command
	bool doubleSpeed;
	bool xaAdpcm;
	bool wholeSector;
	bool ignoreBit;
	bool xaFilter;
	bool enableReportInterrupts;
	bool autoPause;
	bool allowCddaRead;

	// This tells us if a response has been received
	int32_t responseReceived;

	// This stores the setloc position as a byte index, and whether
	// this sector has been read
	int64_t setlocPosition;
	bool setlocProcessed;

	// This handles the read retry in ReadN
	bool beenRead;
};

/*
 * This function constructs a CDROMDrive object.
 */
CDROMDrive *construct_CDROMDrive(void)
{
	// Allocate memory for CDROMDrive struct
	CDROMDrive *cdrom = malloc(sizeof(CDROMDrive));
	if (!cdrom) {
		fprintf(stderr, "PhilPSX: CDROMDrive: Couldn't allocate memory for "
				"CDROMDrive struct\n");
		goto end;
	}
	
	// Create empty CD object
	cdrom->cd = construct_CD();
	if (!cdrom->cd) {
		fprintf(stderr, "PhilPSX: CDROMDrive: Couldn't construct CD object\n");
		goto cleanup_cdromdrive;
	}
	
	// Allocate data fifo
	cdrom->dataFifo = calloc(0x924, sizeof(int8_t));
	if (!cdrom->dataFifo) {
		fprintf(stderr, "PhilPSX: CDROMDrive: Couldn't allocate memory for "
				"dataFifo array\n");
		goto cleanup_cd;
	}
	
	// Zero out parameterFifo and responseFifo arrays
	memset(cdrom->parameterFifo, 0, sizeof(cdrom->parameterFifo));
	memset(cdrom->responseFifo, 0, sizeof(cdrom->responseFifo));
	
	// Set system reference to NULL
	cdrom->system = NULL;

	// Set port index to 0
	cdrom->portIndex = 0;

	// Setup fifo indexes and counts
	cdrom->parameterCount = 0;
	cdrom->responseCount = 0;
	cdrom->responseIndex = 0;
	cdrom->dataCount = 0;
	cdrom->dataIndex = 0;

	// Setup status flags
	cdrom->cddaPlaying = false;
	cdrom->isSeeking = false;
	cdrom->isReading = false;
	cdrom->shellOpen = false;
	cdrom->idError = false;
	cdrom->seekError = false;
	cdrom->motorStatus = true;
	cdrom->commandError = false;

	// Setup mode flags
	cdrom->doubleSpeed = false;
	cdrom->xaAdpcm = false;
	cdrom->wholeSector = false;
	cdrom->ignoreBit = false;
	cdrom->xaFilter = false;
	cdrom->enableReportInterrupts = false;
	cdrom->autoPause = false;
	cdrom->allowCddaRead = false;

	// Setup interrupt registers
	cdrom->interruptEnableRegister = 0;
	cdrom->interruptFlagRegister = 0;

	// Set status to non-busy and current command to 0, as well as
	// second response flag
	cdrom->busy = 0;
	cdrom->currentCommand = 0;
	cdrom->needsSecondResponse = false;

	// Setup response received
	cdrom->responseReceived = 0;

	// Set position
	cdrom->setlocPosition = 0;
	cdrom->setlocProcessed = false;

	// Handle read retry in ReadN command
	cdrom->beenRead = true;
	
	// Normal return:
	return cdrom;
	
	// Cleanup path:
	cleanup_cd:
	destruct_CD(cdrom->cd);
	
	cleanup_cdromdrive:
	free(cdrom);
	cdrom = NULL;
	
	end:
	return cdrom;
}

/*
 * This function destructs a CDROMDrive object.
 */
void destruct_CDROMDrive(CDROMDrive *cdrom)
{
	free(cdrom->dataFifo);
	destruct_CD(cdrom->cd);
	free(cdrom);
}

/*
 * This function reads chunks at a time from the data fifo into an array.
 */
void CDROMDrive_chunkCopy(CDROMDrive *cdrom, int8_t *destination,
		int32_t startIndex, int32_t length)
{
	// Copy data fifo pointer and update it as well as destination with the
	// correct offsets
	int8_t *tempDataFifo = cdrom->dataFifo;
	tempDataFifo += cdrom->dataIndex;
	destination += startIndex;
	
	// Check if length would bring us over the data fifo bounds
	if (cdrom->dataIndex + length > cdrom->dataCount) {
		// Copy as much as we can, then copy the rest as specified
		memcpy(destination, tempDataFifo,
				(cdrom->dataCount - cdrom->dataIndex) * sizeof(int8_t));
		destination += cdrom->dataCount - cdrom->dataIndex;
		length -= cdrom->dataCount - cdrom->dataIndex;
		cdrom->dataIndex = cdrom->dataCount;
		int8_t fillValue = CDROMDrive_wholeSector(cdrom) ?
			cdrom->dataFifo[0x920] : cdrom->dataFifo[0x7F8];
		memset(destination, fillValue, length * sizeof(int8_t));
	} else { // We are fine, just do the copy
		memcpy(destination, tempDataFifo, length * sizeof(int8_t));
		cdrom->dataIndex += length;
	}
	cdrom->beenRead = true;
}

/*
 * This function sets the contained CD object to reference the specified
 * image file.
 */
bool CDROMDrive_loadCD(CDROMDrive *cdrom, const char *cdPath)
{
	return CD_loadCD(cdrom->cd, cdPath);
}

/*
 * This function reads a byte from the index/status register.
 */
int8_t CDROMDrive_read1800(CDROMDrive *cdrom)
{
	int8_t retVal = (int8_t)(cdrom->portIndex & 0x3);

	int32_t bit3 = (cdrom->parameterCount == 0) ? 1 : 0;
	int32_t bit4 = (cdrom->parameterCount == 16) ? 0 : 1;
	int32_t bit5 = (cdrom->responseCount == 0) ? 0 : 1;
	int32_t bit6 = (cdrom->dataCount == 0) ? 0 : 1;

	retVal |= (int8_t)(bit3 << 3);
	retVal |= (int8_t)(bit4 << 4);
	retVal |= (int8_t)(bit5 << 5);
	retVal |= (int8_t)(bit6 << 6);
	retVal |= (int8_t)(cdrom->busy << 7);

	return retVal;
}

/*
 * This function reads a byte from port 0x1F801801.
 */
int8_t CDROMDrive_read1801(CDROMDrive *cdrom)
{
	// Return response fifo byte, regardless of port index, as this is
	// mirrored on all four ports.
	int8_t retVal = cdrom->responseFifo[cdrom->responseIndex++];
	if (cdrom->responseIndex == cdrom->responseCount) {
		cdrom->responseCount = 0;
	}
	if (cdrom->responseIndex > 15) {
		cdrom->responseIndex = 0;
	}

	return retVal;
}

/*
 * This function reads a byte from port 0x1F801802.
 */
int8_t CDROMDrive_read1802(CDROMDrive *cdrom)
{
	// All port indexes to this port read from data fifo

	// Declare return value
	int8_t retVal = 0;

	if (cdrom->dataIndex < cdrom->dataCount) {
		retVal = cdrom->dataFifo[cdrom->dataIndex++];
	} else {
		retVal = (CDROMDrive_wholeSector(cdrom)) ?
			cdrom->dataFifo[0x920] : cdrom->dataFifo[0x7F8];
	}
	cdrom->beenRead = true;

	return retVal;
}

/*
 * This function reads a byte from port 0x1F801803.
 */
int8_t CDROMDrive_read1803(CDROMDrive *cdrom)
{
	// Declare return value
	int8_t retVal = 0;

	// Act depending on port index
	switch (cdrom->portIndex) {
		case 0:
			retVal = CDROMDrive_getInterruptEnableRegister(cdrom);
			break;
		case 1:
			retVal = CDROMDrive_getInterruptFlagRegister(cdrom);
			break;
		case 2:
			retVal = CDROMDrive_getInterruptEnableRegister(cdrom);
			break;
		case 3:
			retVal = CDROMDrive_getInterruptFlagRegister(cdrom);
			break;
	}

	return retVal;
}

/*
 * This lets us set the interrupt flag register contents manually.
 */
void CDROMDrive_setInterruptNumber(CDROMDrive *cdrom, int32_t interruptNum)
{
	cdrom->interruptFlagRegister = interruptNum;
}

/*
 * This function sets the system reference to that of the supplied argument.
 */
void CDROMDrive_setMemoryInterface(CDROMDrive *cdrom, SystemInterlink *smi)
{
	cdrom->system = smi;
}

/*
 * This function writes a byte to the index/status register.
 */
void CDROMDrive_write1800(CDROMDrive *cdrom, int8_t value)
{
	cdrom->portIndex = value & 0x3;
}

/*
 * This function writes a byte to port 0x1F801801.
 */
void CDROMDrive_write1801(CDROMDrive *cdrom, int8_t value)
{
	// Act depending on port index
	switch (cdrom->portIndex) {
		case 0: // Execute command byte
			if (cdrom->busy == 0) {
				CDROMDrive_clearResponseFifo(cdrom);
				cdrom->busy = 1;
				cdrom->currentCommand = value;
				CDROMDrive_executeCommand(cdrom, value,
						cdrom->needsSecondResponse);
			} else if (value == 0x9) {
				cdrom->currentCommand = value;
				cdrom->needsSecondResponse = false;
				CDROMDrive_executeCommand(cdrom, value,
						cdrom->needsSecondResponse);
			}
			break;
		case 1:
			break;
		case 2:
			break;
		case 3:
			break;
	}
}

/*
 * This function writes a byte to port 0x1F801802.
 */
void CDROMDrive_write1802(CDROMDrive *cdrom, int8_t value)
{
	// Act depending on port index
	switch (cdrom->portIndex) {
		case 0: // Add byte to parameter fifo
			cdrom->parameterFifo[cdrom->parameterCount++] = value;
			break;
		case 1: // Write interrupt enable flags
			cdrom->interruptEnableRegister = value & 0x1F;
			break;
		case 2:
			break;
		case 3:
			break;
	}
}

/*
 * This function writes a byte to port 0x1F801803.
 */
void CDROMDrive_write1803(CDROMDrive *cdrom, int8_t value)
{
	// Act depending on port index
	switch (cdrom->portIndex) {
		case 0: // This is the request register port
			switch (value & 0x80) {
				case 0: // Reset data fifo
					cdrom->dataIndex = 0;
					break;
				case 0x80: // Wants data
					// We have already filled fifo so do nothing
					break;
			}
			break;
		case 1: // Deal with interrupt acknowledgement
		{
			// Clear parameter fifo if specified
			if ((value & 0x40) == 0x40) {
				CDROMDrive_clearParameterFifo(cdrom);
			}

			// Define reset mask
			int32_t interruptResetMask = value;
			interruptResetMask = ~interruptResetMask & 0x1F;
			cdrom->interruptFlagRegister &= interruptResetMask;

			// Check if command has to issue a second response or not
			cdrom->responseReceived = 0;
			if (cdrom->needsSecondResponse) {
				CDROMDrive_clearResponseFifo(cdrom);
				CDROMDrive_executeCommand(cdrom, cdrom->currentCommand,
						cdrom->needsSecondResponse);
			}
		}
			break;
		case 2:
			break;
		case 3:
			break;
	}
}

/*
 * This tells us if we should allow CD-DA (audio CD) sectors to be read.
 */
static bool CDROMDrive_allowCddaRead(CDROMDrive *cdrom)
{
	return cdrom->allowCddaRead;
}

/*
 * This tells us whether we should auto pause on end of track when playing
 * an audio CD.
 */
static bool CDROMDrive_autoPause(CDROMDrive *cdrom)
{
	return cdrom->autoPause;
}

/*
 * This tells us if CD-DA is being played.
 */
static bool CDROMDrive_cddaPlaying(CDROMDrive *cdrom)
{
	return cdrom->cddaPlaying;
}

/*
 * This clears the data fifo.
 */
static void CDROMDrive_clearDataFifo(CDROMDrive *cdrom)
{
	// Wipe data fifo with memset
	memset(cdrom->dataFifo, 0, 0x924);
	cdrom->dataCount = 0;
	cdrom->dataIndex = 0;
}

/*
 * This clears the parameter fifo.
 */
static void CDROMDrive_clearParameterFifo(CDROMDrive *cdrom)
{
	// Wipe parameter fifo with memset
	memset(cdrom->parameterFifo, 0, sizeof(cdrom->parameterFifo));
	cdrom->parameterCount = 0;
}

/*
 * This clears the response fifo.
 */
static void CDROMDrive_clearResponseFifo(CDROMDrive *cdrom)
{
	// Wipe response fifo with memset
	memset(cdrom->responseFifo, 0, sizeof(cdrom->responseFifo));
	cdrom->responseCount = 0;
	cdrom->responseIndex = 0;
}

/*
 * This tells us if a command generated an error or not.
 */
static bool CDROMDrive_commandError(CDROMDrive *cdrom)
{
	return cdrom->commandError;
}

/*
 * This handles the Demute command.
 */
static void CDROMDrive_command_Demute(CDROMDrive *cdrom)
{
	// Do nothing other than return status at the moment

	// Store status byte to response fifo
	cdrom->responseFifo[cdrom->responseCount++] =
			CDROMDrive_getStatusCode(cdrom);
	cdrom->busy = 0;
	cdrom->responseReceived = 3;
	CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
}

/*
 * This handles the GetID command.
 */
static void CDROMDrive_command_GetID(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->needsSecondResponse = true;
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
	} else { // Second response, send licensed mode 2 response
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x02;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x00;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x20;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x00;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x53;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x43;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x45;
		cdrom->responseFifo[cdrom->responseCount++] = (int8_t)0x45;
		cdrom->busy = 0;
		cdrom->needsSecondResponse = false;
		cdrom->responseReceived = 2;
		CDROMDrive_triggerInterrupt(cdrom, 2, 16000);
	}
}

/*
 * This handles the Getstat command.
 */
static void CDROMDrive_command_Getstat(CDROMDrive *cdrom)
{
	// Store status byte to response fifo
	cdrom->responseFifo[cdrom->responseCount++] =
			CDROMDrive_getStatusCode(cdrom);
	cdrom->busy = 0;
	cdrom->responseReceived = 3;
	CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
}

/*
 * This handles the Init command.
 */
static void CDROMDrive_command_Init(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, send stat byte

		// Nuke all mode bits
		cdrom->doubleSpeed = false;
		cdrom->xaAdpcm = false;
		cdrom->wholeSector = false;
		cdrom->ignoreBit = false;
		cdrom->xaFilter = false;
		cdrom->enableReportInterrupts = false;
		cdrom->autoPause = false;
		cdrom->allowCddaRead = false;

		// Store status byte to response fifo
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
		cdrom->needsSecondResponse = true;
	} else { // Second response, send stat byte

		// Store status byte to response fifo
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->responseReceived = 2;
		CDROMDrive_triggerInterrupt(cdrom, 2, 16000);
		cdrom->needsSecondResponse = false;
		cdrom->busy = 0;
	}
}

/*
 * This handles the Pause command.
 */
static void CDROMDrive_command_Pause(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->needsSecondResponse = true;
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
	} else { // Second response, send stat byte
		cdrom->isReading = false;
		cdrom->cddaPlaying = false;
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->busy = 0;
		cdrom->needsSecondResponse = false;
		cdrom->responseReceived = 2;
		CDROMDrive_triggerInterrupt(cdrom, 2, 16000);
	}
}

/*
 * This handles the ReadN command.
 */
static void CDROMDrive_command_ReadN(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, just send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->isReading = true;
		cdrom->needsSecondResponse = true;
		cdrom->beenRead = true;
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
	} else { // Second response, read sector into fifo and send stat byte
		if (cdrom->beenRead) {
			CDROMDrive_clearDataFifo(cdrom);

			// Modify Setloc position if needed
			if (cdrom->setlocProcessed) {
				cdrom->setlocPosition += 2352;
			} else {
				cdrom->setlocProcessed = true;
			}

			int64_t startAddress = cdrom->setlocPosition;
			startAddress += CDROMDrive_wholeSector(cdrom) ? 12 : 24;
			int32_t sectorSize = CDROMDrive_wholeSector(cdrom) ? 0x924 : 0x800;

			for (int32_t i = 0; i < sectorSize; ++i) {
				cdrom->dataFifo[cdrom->dataCount++] =
						CD_readByte(cdrom->cd, startAddress++);
			}

			cdrom->beenRead = false;
		}
		
		// Send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->needsSecondResponse = true;
		cdrom->responseReceived = 0;
		CDROMDrive_triggerInterrupt(cdrom, 1, 16000);
	}
}

/*
 * This handles the ReadTOC command.
 */
static void CDROMDrive_command_ReadTOC(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->needsSecondResponse = true;
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
	} else { // Second response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->responseReceived = 2;
		CDROMDrive_triggerInterrupt(cdrom, 2, 16000);
		cdrom->needsSecondResponse = false;
		cdrom->busy = 0;
	}
}

/*
 * This handles the SeekL command.
 */
static void CDROMDrive_command_SeekL(CDROMDrive *cdrom, bool secondResponse)
{
	// Determine what to do
	if (!secondResponse) { // First response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->isSeeking = true;
		cdrom->needsSecondResponse = true;
		cdrom->responseReceived = 3;
		CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
	} else { // Second response, send stat byte
		cdrom->responseFifo[cdrom->responseCount++] =
				CDROMDrive_getStatusCode(cdrom);
		cdrom->isSeeking = false;
		cdrom->needsSecondResponse = false;
		cdrom->responseReceived = 2;
		CDROMDrive_triggerInterrupt(cdrom, 2, 16000);
		cdrom->busy = 0;
	}
}

/*
 * This handles the Setloc command.
 */
static void CDROMDrive_command_Setloc(CDROMDrive *cdrom)
{
	// Get location from parameters
	int32_t minutes = cdrom->parameterFifo[0] & 0xFF;
	int32_t seconds = cdrom->parameterFifo[1] & 0xFF;
	int32_t frames = cdrom->parameterFifo[2] & 0xFF;

	// Convert from BCD to actual numbers
	minutes = (minutes & 0xF) + (((minutes >> 4) & 0xF) * 10);
	seconds = (seconds & 0xF) + (((seconds >> 4) & 0xF) * 10);
	frames = (frames & 0xF) + (((frames >> 4) & 0xF) * 10);

	// Get byte position of the above
	cdrom->setlocPosition =
			(frames * 2352) + (seconds * 176400) + (minutes * 10584000);
	cdrom->setlocProcessed = false;

	// Deal with response code etc.
	cdrom->responseFifo[cdrom->responseCount++] =
			CDROMDrive_getStatusCode(cdrom);
	cdrom->busy = 0;
	cdrom->responseReceived = 3;
	CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
}

/*
 * This handles the Setmode command.
 */
static void CDROMDrive_command_Setmode(CDROMDrive *cdrom)
{
	// Set flags using parameter byte
	int32_t modeFlags = cdrom->parameterFifo[0] & 0xFF;

	cdrom->doubleSpeed = (modeFlags & 0x80) != 0;
	cdrom->xaAdpcm = (modeFlags & 0x40) != 0;
	cdrom->wholeSector = (modeFlags & 0x20) != 0;
	cdrom->ignoreBit = (modeFlags & 0x10) != 0;
	cdrom->xaFilter = (modeFlags & 0x8) != 0;
	cdrom->enableReportInterrupts = (modeFlags & 0x4) != 0;
	cdrom->autoPause = (modeFlags & 0x2) != 0;
	cdrom->allowCddaRead = (modeFlags & 0x1) != 0;

	// Store status byte to response fifo
	cdrom->responseFifo[cdrom->responseCount++] =
			CDROMDrive_getStatusCode(cdrom);
	cdrom->busy = 0;
	cdrom->responseReceived = 3;
	CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
}

/*
 * This handles Test commands.
 */
static void CDROMDrive_command_Test(CDROMDrive *cdrom)
{
	// Get parameter from fifo
	int8_t parameter1 = cdrom->parameterFifo[0];
	CDROMDrive_clearParameterFifo(cdrom);

	switch (parameter1) {
		case 0x20:
			// Get the date (y/m/d) in BCD and also the version of
			// the CD-ROM controller BIOS
			// fake version PSX/PSone (PU-23, PM-41)
			cdrom->responseFifo[cdrom->responseCount++] =
					(int8_t)0x99; // (1999)
			cdrom->responseFifo[cdrom->responseCount++] =
					(int8_t)0x02; // (February)
			cdrom->responseFifo[cdrom->responseCount++] =
					(int8_t)0x01; // (1st)
			cdrom->responseFifo[cdrom->responseCount++] =
					(int8_t)0xC3; // (version vC3)
			cdrom->busy = 0;
			cdrom->responseReceived = 3;
			CDROMDrive_triggerInterrupt(cdrom, 3, 16000);
			break;
	}
}

/*
 * This tells us if the motor is at double speed setting.
 */
static bool CDROMDrive_doubleSpeed(CDROMDrive *cdrom)
{
	return cdrom->doubleSpeed;
}

/*
 * This tells us whether we should enable report-interrupts for audio play.
 */
static bool CDROMDrive_enableReportInterrupts(CDROMDrive *cdrom)
{
	return cdrom->enableReportInterrupts;
}

/*
 * This executes a CD-ROM command.
 */
static void CDROMDrive_executeCommand(CDROMDrive *cdrom, int32_t commandNum,
		bool secondResponse)
{
	// Execute command or deal with second response
	if (!secondResponse) {
		switch (commandNum) {
			case 0x01: // Getstat
				CDROMDrive_command_Getstat(cdrom);
				break;
			case 0x02: // Setloc
				CDROMDrive_command_Setloc(cdrom);
				break;
			case 0x06: // ReadN
				CDROMDrive_command_ReadN(cdrom, secondResponse);
				break;
			case 0x09: // Pause
				CDROMDrive_command_Pause(cdrom, secondResponse);
				break;
			case 0x0A: // Init
				CDROMDrive_command_Init(cdrom, secondResponse);
				break;
			case 0x0C: // Demute
				CDROMDrive_command_Demute(cdrom);
				break;
			case 0x0E: // Setmode
				CDROMDrive_command_Setmode(cdrom);
				break;
			case 0x15: // SeekL
				CDROMDrive_command_SeekL(cdrom, secondResponse);
				break;
			case 0x19: // Test
				CDROMDrive_command_Test(cdrom);
				break;
			case 0x1A: // GetID
				CDROMDrive_command_GetID(cdrom, secondResponse);
				break;
			case 0x1E: // ReadTOC
				CDROMDrive_command_ReadTOC(cdrom, secondResponse);
				break;
			default: // Unimplemented command
				fprintf(stderr,
						"PhilPSX: CDROMDrive: Unimplemented command: %2X\n",
						commandNum & 0xFF);
				break;
		}
	} else {
		switch (commandNum) {
			case 0x06: // ReadN
				CDROMDrive_command_ReadN(cdrom, secondResponse);
				break;
			case 0x09: // Pause
				CDROMDrive_command_Pause(cdrom, secondResponse);
				break;
			case 0x0A: // Init
				CDROMDrive_command_Init(cdrom, secondResponse);
				break;
			case 0x15: // SeekL
				CDROMDrive_command_SeekL(cdrom, secondResponse);
				break;
			case 0x1A: // GetID
				CDROMDrive_command_GetID(cdrom, secondResponse);
				break;
			case 0x1E: // ReadTOC
				CDROMDrive_command_ReadTOC(cdrom, secondResponse);
				break;
		}
	}
}

/*
 * This returns the interrupt enable register.
 */
static int8_t CDROMDrive_getInterruptEnableRegister(CDROMDrive *cdrom)
{
	return (int8_t)(cdrom->interruptEnableRegister | 0xE0);
}

/*
 * This returns the interrupt flag register.
 */
static int8_t CDROMDrive_getInterruptFlagRegister(CDROMDrive *cdrom)
{
	// Define return value
	int32_t retVal = 0xE0;

	// Merge in response received
	retVal |= cdrom->interruptFlagRegister & 0x7;

	return (int8_t)retVal;
}

/*
 * This returns the status code.
 */
static int8_t CDROMDrive_getStatusCode(CDROMDrive *cdrom)
{
	// Declare return value
	int32_t retVal = 0;

	// Test each bit
	if (CDROMDrive_cddaPlaying(cdrom)) {
		retVal |= 0x80;
	}
	if (CDROMDrive_isSeeking(cdrom)) {
		retVal |= 0x40;
	}
	if (CDROMDrive_isReading(cdrom)) {
		retVal |= 0x20;
	}
	if (CDROMDrive_shellOpen(cdrom)) {
		retVal |= 0x10;
	}
	if (CDROMDrive_idError(cdrom)) {
		retVal |= 0x8;
	}
	if (CDROMDrive_seekError(cdrom)) {
		retVal |= 0x4;
	}
	if (CDROMDrive_motorStatus(cdrom)) {
		retVal |= 0x2;
	}
	if (CDROMDrive_commandError(cdrom)) {
		retVal |= 0x1;
	}

	return (int8_t)retVal;
}

/*
 * This tells us if the GetID command was denied.
 */
static bool CDROMDrive_idError(CDROMDrive *cdrom)
{
	return cdrom->idError;
}

/*
 * This tells us if we should ignore sector size and setloc position.
 */
static bool CDROMDrive_ignoreBit(CDROMDrive *cdrom)
{
	return cdrom->ignoreBit;
}

/*
 * This tells us if the drive is currently reading.
 */
static bool CDROMDrive_isReading(CDROMDrive *cdrom)
{
	return cdrom->isReading;
}

/*
 * This tells us if the drive is currently seeking.
 */
static bool CDROMDrive_isSeeking(CDROMDrive *cdrom)
{
	return cdrom->isSeeking;
}

/*
 * This tells us if the CD motor is off/not yet ready, or if it is on.
 */
static bool CDROMDrive_motorStatus(CDROMDrive *cdrom)
{
	return cdrom->motorStatus;
}

/*
 * This tells us if there was a seek error.
 */
static bool CDROMDrive_seekError(CDROMDrive *cdrom)
{
	return cdrom->seekError;
}

/*
 * This tells us if the CD drive is open.
 */
static bool CDROMDrive_shellOpen(CDROMDrive *cdrom)
{
	return CD_isEmpty(cdrom->cd);
}

/*
 * This triggers a CD-ROM interrupt.
 */
static void CDROMDrive_triggerInterrupt(CDROMDrive *cdrom, int32_t interruptNum,
		int32_t delay)
{
	CDROMDrive_clearParameterFifo(cdrom);

	if (interruptNum != 0 && (cdrom->interruptEnableRegister & interruptNum)
			== interruptNum) {
		SystemInterlink_setCDROMInterruptEnabled(cdrom->system, true);
	} else {
		SystemInterlink_setCDROMInterruptEnabled(cdrom->system, false);
	}

	// Set interrupt delay and number
	SystemInterlink_setCDROMInterruptDelay(cdrom->system, delay);
	SystemInterlink_setCDROMInterruptNumber(cdrom->system, interruptNum);
}

/*
 * This tells us if we should be reading whole sector (0x924 bytes) or just data
 * (0x800 bytes).
 */
static bool CDROMDrive_wholeSector(CDROMDrive *cdrom)
{
	return cdrom->wholeSector;
}

/*
 * This tells us if we should be sending XA-ADPCM sectors to the SPU.
 */
static bool CDROMDrive_xaAdpcm(CDROMDrive *cdrom)
{
	return cdrom->xaAdpcm;
}

/*
 * This tells us if we should process only XA-ADPCM sectors that match
 * Setfilter.
 */
static bool CDROMDrive_xaFilter(CDROMDrive *cdrom)
{
	return cdrom->xaFilter;
}