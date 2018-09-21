/*
 * This C file handles DMA within the emulator as a class, and is intended to 
 * mimic DMA behaviour of the real hardware closely. This is useful for getting
 * things into and out of various hardware like the GPU. It doesn't use a PIO
 * channel as no software actually uses it, but the constant is defined for
 * consistency.
 * 
 * DMAArbiter.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/DMAArbiter.h"
#include "../headers/BusInterfaceUnit.h"
#include "../headers/CDROMDrive.h"
#include "../headers/R3051.h"
#include "../headers/GPU.h"
#include "../headers/SystemInterlink.h"
#include "../headers/Cop0.h"
#include "../headers/Components.h"
#include "../headers/math_utils.h"

// Constant for number of DMA channels
#define PHILPSX_DMA_CHANNEL_COUNT 7

// Constants for each DMA channel
#define PHILPSX_DMA_MDEC_IN 0
#define PHILPSX_DMA_MDEC_OUT 1
#define PHILPSX_DMA_GPU 2
#define PHILPSX_DMA_CDROM 3
#define PHILPSX_DMA_SPU 4
#define PHILPSX_DMA_PIO 5
#define PHILPSX_DMA_OTC 6

/*
 * This struct contains the state to model DMA transfers
 * within the PlayStation.
 */
struct DMAArbiter {

	// Store reference to interlink object
	SystemInterlink *system;

	// Store device references
	R3051 *cpu;
	BusInterfaceUnit *biu;
	GPU *gpu;
	CDROMDrive *cdrom;

	// Global DMA registers
	int32_t dmaControlRegister;
	int32_t dmaInterruptRegister;

	// Per-channel registers
	int32_t *channelRegisters;
};

/*
 * This function constructs a default arbiter.
 */
DMAArbiter *construct_DMAArbiter(void)
{
	// Allocate DMAArbiter struct
	DMAArbiter *dma = malloc(sizeof(DMAArbiter));
	if (!dma) {
		fprintf(stderr, "PhilPSX: DMAArbiter: Couldn't allocate memory for "
				"DMAArbiter struct\n");
		goto end;
	}
	
	// Allocate channelRegisters array
	dma->channelRegisters =
			calloc(3 * PHILPSX_DMA_CHANNEL_COUNT, sizeof(int32_t));
	if (!dma->channelRegisters) {
		fprintf(stderr, "PhilPSX: DMAArbiter: Couldn't allocate memory for "
				"channelRegisters array\n");
		goto cleanup_dmaarbiter;
	}
	
	// Setup other registers
	dma->dmaControlRegister = 0;
	dma->dmaInterruptRegister = 0;
	
	// Set all component references to NULL
	dma->system = NULL;
	dma->cpu = NULL;
	dma->biu = NULL;
	dma->gpu = NULL;
	dma->cdrom = NULL;
	
	// Normal return:
	return dma;
	
	// Cleanup path:
	cleanup_dmaarbiter:
	free(dma);
	dma = NULL;
	
	end:
	return dma;
}

/*
 * This function destructs a DMAArbiter object.
 */
void destruct_DMAArbiter(DMAArbiter *dma)
{
	free(dma->channelRegisters);
	free(dma);
}

/*
 * This function handles CD-ROM DMA transfers - it assumes a sync mode of 0.
 */
int32_t DMAArbiter_handleCDROM(DMAArbiter *dma)
{
	// Get DMA base address and correct endianness
	int32_t baseAddress = dma->channelRegisters[PHILPSX_DMA_CDROM * 3];
	baseAddress = ((baseAddress << 24) & 0xFF000000) |
			((baseAddress << 8) & 0xFF0000) |
			(logical_rshift(baseAddress, 8) & 0xFF00) |
			(logical_rshift(baseAddress, 24) & 0xFF);
	baseAddress = Cop0_virtualToPhysical(R3051_getCop0(dma->cpu), baseAddress);

	// Get number of words and correct endianness
	int32_t numberOfWords = dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 1];
	numberOfWords = ((numberOfWords << 24) & 0xFF000000) |
			((numberOfWords << 8) & 0xFF0000) |
			(logical_rshift(numberOfWords, 8) & 0xFF00) |
			(logical_rshift(numberOfWords, 24) & 0xFF);
	numberOfWords &= 0xFFFF;
	if (numberOfWords == 0)
		numberOfWords = 0x10000;

	// Calculate time to run system for (40 clocks per word for CDROM)
	int32_t dmaCycles = 40 * numberOfWords;

	// Perform CD-ROM transfer
	int64_t startingByte = 0xFFFFFFFFL & baseAddress;
	int64_t endingByte = (startingByte + numberOfWords * 4) - 1;
	int32_t numberOfBytes = numberOfWords * 4;

	// If DMA transfer is destined for RAM, then take shortcut and
	// transfer whole lot
	switch ((int32_t)(startingByte & 0xFFE00000)) {
		case 0: // Starts in RAM
			switch ((int32_t)(endingByte & 0xFFE00000)) {
				case 0: // Ends in RAM, transfer whole lot in one go
					CDROMDrive_chunkCopy(
							dma->cdrom,
							SystemInterlink_getRamArray(dma->system),
							(int32_t)startingByte,
							numberOfBytes
							);
					break;
				default: // Doesn't end in RAM, handle normally
					for (int32_t i = 0; i < numberOfBytes; ++i) {
						SystemInterlink_writeByte(
								dma->system,
								(int32_t)startingByte,
								SystemInterlink_readByte(
								dma->system,
								0x1F801802)
								);
						++startingByte;
					}
					break;
			}
			break;
		default: // Doesn't start in RAM, handle normally
			for (int32_t i = 0; i < (numberOfWords * 4); ++i) {
				SystemInterlink_writeByte(
						dma->system,
						(int32_t)startingByte,
						SystemInterlink_readByte(dma->system, 0x1F801802)
						);
				++startingByte;
			}
			break;
	}

	// Decrement BC if chopping enabled (detect in little-endian mode
	// for speed)
	switch (dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 2] & 0x10000) {
		case 0x10000:
			// Set BC (again in little-endian mode for speed) to 0
			dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 1] &= 0xFFFF;
			break;
	}

	return dmaCycles;
}

/*
 * This function handles DMA and makes data go to the right place.
 */
void DMAArbiter_handleDMATransactions(DMAArbiter *dma)
{
	// Check if we need to start any DMA requests
	int32_t dmaChannelControl[7] = { 0, 0, 0, 0, 0, 0, 0 };
	int32_t dmaChannelStarted[7] = { 0, 0, 0, 0, 0, 0, 0 };
	for (int32_t i = 0; i < 7; ++i) {
		int32_t word = dma->channelRegisters[i * 3 + 2];
		dmaChannelControl[i] = ((word << 24) & 0xFF000000) |
				((word << 8) & 0xFF0000) |
				(logical_rshift(word, 8) & 0xFF00) |
				(logical_rshift(word, 24) & 0xFF);

		switch (logical_rshift((dmaChannelControl[i] & 0x600), 9)) {
			case 0: // Sync mode 0
				if ((dmaChannelControl[i] & 0x11000000) == 0x11000000)
					dmaChannelStarted[i] = 0x1;
				break;
			default: // Other
				dmaChannelStarted[i] =
						logical_rshift(dmaChannelControl[i], 24) & 0x1;
				break;
		}
	}

	// Check if each one is actually enabled
	int32_t tempControlRegister =
			((dma->dmaControlRegister << 24) & 0xFF000000) |
			((dma->dmaControlRegister << 8) & 0xFF0000) |
			(logical_rshift(dma->dmaControlRegister, 8) & 0xFF00) |
			(logical_rshift(dma->dmaControlRegister, 24) & 0xFF);

	int32_t highestPriority = 8;
	int32_t highestPriorityChannelSoFar = -1;
	for (int32_t i = 0; i < 7; ++i) {
		switch (dmaChannelStarted[i]) {
			case 1:
			{
				int32_t priority =
						logical_rshift(tempControlRegister, (i * 4)) & 0x7;
				int32_t enabled =
						logical_rshift(tempControlRegister, (i * 4)) & 0x8;

				if (enabled > 0) {
					if (priority <= highestPriority) {
						highestPriority = priority;
						highestPriorityChannelSoFar = i;
					}
				}
			}
			break;
		}
	}

	int32_t cpuCycles = 0;

	// Perform DMA transfer itself if one is needed
	switch (highestPriorityChannelSoFar) {
		case -1: // Do nothing
			break;
		default: // Do DMA transfer
			// Set bus holder to DMA Arbiter
			BusInterfaceUnit_setHolder(dma->biu, PHILPSX_COMPONENTS_DMA);

			// Clear bit 28 of channel control register (remember it is
			// stored in little-endian mode)
			dma->channelRegisters[highestPriorityChannelSoFar * 3 + 2] &=
					0xFFFFFFEF;

			// Call correct method depending on channel or mode
			switch (highestPriorityChannelSoFar) {
				case 0: // MDECin
					fprintf(stdout, "PhilPSX: DMAArbiter: MDECin DMA "
							"triggered\n");
					break;
				case 1: // MDECout
					fprintf(stdout, "PhilPSX: DMAArbiter: MDECout DMA "
							"triggered\n");
					break;
				case 2: // GPU DMA
					cpuCycles = DMAArbiter_handleGPU(dma);
					break;
				case 3: // CD-ROM
					cpuCycles = DMAArbiter_handleCDROM(dma);
					break;
				case 4: // SPU
					fprintf(stdout, "PhilPSX: DMAArbiter: SPU DMA "
							"triggered\n");
					break;
				case 5: // PIO
					fprintf(stdout, "PhilPSX: DMAArbiter: PIO DMA "
							"triggered\n");
					break;
				case 6: // OTC DMA
					cpuCycles = DMAArbiter_handleOTC(dma);
					break;
			}

			// Clear bit 24 of channel control register (again, remember
			// endianness
			dma->channelRegisters[highestPriorityChannelSoFar * 3 + 2] &=
					0xFFFFFFFE;

			// Set bus holder back to CPU
			BusInterfaceUnit_setHolder(dma->biu, PHILPSX_COMPONENTS_CPU);

			// Trigger interrupt by masking correct bit (remember endianness)
			int32_t tempInterruptRegister =
					((dma->dmaInterruptRegister << 24) & 0xFF000000) |
					((dma->dmaInterruptRegister << 8) & 0xFF0000) |
					(logical_rshift(dma->dmaInterruptRegister, 8) & 0xFF00) |
					(logical_rshift(dma->dmaInterruptRegister, 24) & 0xFF);
			int32_t intMask = 0x00010000 << highestPriorityChannelSoFar;
			intMask |= 0x00800000;

			if ((tempInterruptRegister & intMask) == intMask) {
				intMask = 0x01000000 << highestPriorityChannelSoFar;
				tempInterruptRegister |= intMask;
				dma->dmaInterruptRegister =
						((tempInterruptRegister << 24) & 0xFF000000) |
						((tempInterruptRegister << 8) & 0xFF0000) |
						(logical_rshift(tempInterruptRegister, 8) & 0xFF00) |
						(logical_rshift(tempInterruptRegister, 24) & 0xFF);

				// Set flag in system's Interrupt Status Register
				SystemInterlink_setDMAInterruptDelay(dma->system, 0);
			}

			break;
	}

	//SystemInterlink_appendSyncCycles(dma->system, cpuCycles);
}

/*
 * This function handles GPU DMA transfers.
 */
int32_t DMAArbiter_handleGPU(DMAArbiter *dma)
{
	// Get DMA base address and correct endianness
	int32_t baseAddress = dma->channelRegisters[PHILPSX_DMA_GPU * 3];
	baseAddress = ((baseAddress << 24) & 0xFF000000) |
			((baseAddress << 8) & 0xFF0000) |
			(logical_rshift(baseAddress, 8) & 0xFF00) |
			(logical_rshift(baseAddress, 24) & 0xFF);
	baseAddress = Cop0_virtualToPhysical(R3051_getCop0(dma->cpu), baseAddress);

	// Get block control and correct endianness
	int32_t blockControl = dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 1];
	blockControl = ((blockControl << 24) & 0xFF000000) |
			((blockControl << 8) & 0xFF0000) |
			(logical_rshift(blockControl, 8) & 0xFF00) |
			(logical_rshift(blockControl, 24) & 0xFF);

	// Get channel control register and correct endianness
	int32_t channelControl = dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 2];
	channelControl = ((channelControl << 24) & 0xFF000000) |
			((channelControl << 8) & 0xFF0000) |
			(logical_rshift(channelControl, 8) & 0xFF00) |
			(logical_rshift(channelControl, 24) & 0xFF);

	// Act according to specified mode
	int32_t dmaCycles = 0;
	switch (logical_rshift((channelControl & 0x600), 9)) {
		case 1:
		{
			// Store base address as long
			int64_t tempAddress = baseAddress & 0xFFFFFFFFL;

			// Get block size in words
			int32_t blockSize = 0xFFFF & blockControl;
			switch (blockSize) {
				case 0:
					blockSize = 0x10000;
					break;
			}

			// Get number of blocks
			int32_t numOfBlocks = 0xFFFF & logical_rshift(blockControl, 16);
			switch (numOfBlocks) {
				case 0:
					numOfBlocks = 0x10000;
					break;
			}

			// Calculate total number of words and cycles
			int32_t numOfWords = blockSize * numOfBlocks;
			dmaCycles = numOfWords;

			// Read from or write to GPU depending on channel control register
			int32_t writeToGPU = channelControl & 0x1;
			int32_t backward = logical_rshift(channelControl, 1) & 0x1;
			switch (writeToGPU) {
				case 0: // Read from GPU to RAM
					for (int32_t i = 0; i < numOfWords; ++i) {
						SystemInterlink_writeWord(
								dma->system,
								(int32_t)tempAddress,
								GPU_readResponse(dma->gpu)
								);
						switch (backward) {
							case 0: // Go forward to next address
								tempAddress += 4;
								break;
							case 1: // Go backward to next address
								tempAddress -= 4;
								break;
						}
					}
					break;
				case 1: // Write to GPU from RAM
					for (int32_t i = 0; i < numOfWords; ++i) {
						GPU_submitToGP0(
								dma->gpu,
								SystemInterlink_readWord(
								dma->system,
								(int32_t)tempAddress)
								);
						switch (backward) {
							case 0: // Go forward to next address
								tempAddress += 4;
								break;
							case 1: // Go backward to next address
								tempAddress -= 4;
								break;
						}
					}
					break;
			}

			// Set BA to 0 directly in register (little-endian) for speed
			dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 1] &= 0xFFFF0000;

		}
		break;
		case 2:
		{
			// Iterate over linked list, sending commands to GPU GP0
			int32_t nextAddress = baseAddress;
			do {
				// Store as current address
				int32_t currentAddress = nextAddress;

				// Read word into nextAddress, update base address
				// register and correct endianness
				nextAddress =
						SystemInterlink_readWord(dma->system, nextAddress);
				dma->channelRegisters[PHILPSX_DMA_GPU * 3] =
						nextAddress & 0xFFFFFF00;
				nextAddress = ((nextAddress << 24) & 0xFF000000) |
						((nextAddress << 8) & 0xFF0000) |
						(logical_rshift(nextAddress, 8) & 0xFF00) |
						(logical_rshift(nextAddress, 24) & 0xFF);

				// Get number of words we need and mask them from nextAddress
				int32_t numOfWords =
						logical_rshift((nextAddress & 0xFF000000), 24);
				nextAddress &= 0xFFFFFF;

				// Iterate current block and send commands
				for (int32_t i = 1; i <= numOfWords; ++i) {
					GPU_submitToGP0(
							dma->gpu,
							SystemInterlink_readWord(
							dma->system,
							currentAddress + i * 4)
							);
					++dmaCycles;
				}
			} while (nextAddress != 0xFFFFFF);
		}
		break;
		default:
			fprintf(stderr, "This transfer mode is not implemented for "
					"GPU DMA\n");
			exit(1);
			break;
	}

	return dmaCycles;
}

/*
 * This function handles OTC DMA transfers - it assumes a sync mode of 0.
 */
int32_t DMAArbiter_handleOTC(DMAArbiter *dma)
{
	// Get DMA base address and correct endianness
	int32_t baseAddress = dma->channelRegisters[PHILPSX_DMA_OTC * 3];
	baseAddress = ((baseAddress << 24) & 0xFF000000) |
			((baseAddress << 8) & 0xFF0000) |
			(logical_rshift(baseAddress, 8) & 0xFF00) |
			(logical_rshift(baseAddress, 24) & 0xFF);
	baseAddress = Cop0_virtualToPhysical(R3051_getCop0(dma->cpu), baseAddress);

	// Get number of words and correct endianness
	int32_t numberOfWords = dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 1];
	numberOfWords = ((numberOfWords << 24) & 0xFF000000) |
			((numberOfWords << 8) & 0xFF0000) |
			(logical_rshift(numberOfWords, 8) & 0xFF00) |
			(logical_rshift(numberOfWords, 24) & 0xFF);
	numberOfWords &= 0xFFFF;
	if (numberOfWords == 0)
		numberOfWords = 0x10000;

	// Calculate time to run system for (1 clock per word for OTC)
	int32_t dmaCycles = numberOfWords;

	// Perform OTC transfer
	int64_t currentWord = 0xFFFFFFFFL & baseAddress;
	int32_t destinationWord = (int32_t)(currentWord - 4L);
	destinationWord = ((destinationWord << 24) & 0xFF000000) |
			((destinationWord << 8) & 0xFF0000) |
			(logical_rshift(destinationWord, 8) & 0xFF00) |
			(logical_rshift(destinationWord, 24) & 0xFF);

	for (int32_t i = 0; i < numberOfWords - 1; ++i) {
		SystemInterlink_writeWord(
				dma->system,
				(int32_t)currentWord,
				destinationWord
				);
		currentWord -= 4L;
		destinationWord = (int32_t)(currentWord - 4L);
		destinationWord = ((destinationWord << 24) & 0xFF000000) |
				((destinationWord << 8) & 0xFF0000) |
				(logical_rshift(destinationWord, 8) & 0xFF00) |
				(logical_rshift(destinationWord, 24) & 0xFF);
	}
	SystemInterlink_writeWord(dma->system, (int32_t)currentWord, 0xFFFFFF00);

	// Decrement BC if chopping enabled (detect in little-endian mode
	// for speed)
	switch (dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 2] & 0x10000) {
		case 0x10000:
			// Set BC (again in little-endian mode for speed) to 0
			dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 1] &= 0xFFFF;
			break;
	}

	return dmaCycles;
}

/*
 * This function reads bytes from the DMA Arbiter.
 */
int8_t DMAArbiter_readByte(DMAArbiter *dma, int32_t address)
{
	// Get word address and byte index
	int32_t wordAddress = address & 0xFFFFFFFC;
	int32_t byteIndex = address & 0x3;

	// Read original word
	int32_t tempWord = DMAArbiter_readWord(dma, wordAddress);

	// Return correct byte of word
	int8_t tempByte =
			(int8_t)logical_rshift(tempWord, ((~byteIndex & 0x3) * 8));
	return tempByte;
}

/*
 * This function reads words from the DMA Arbiter.
 */
int32_t DMAArbiter_readWord(DMAArbiter *dma, int32_t address)
{
	// Determine last byte of address
	int32_t addressByte = address & 0xFF;

	// Declare return value
	int32_t retVal = 0;

	// Store word in correct place
	switch (addressByte) {

			// DMA0 (MDECin)
		case 0x80:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3];
			break;
		case 0x84:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3 + 1];
			break;
		case 0x88:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3 + 2];
			break;

			// DMA1 (MDECout)
		case 0x90:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3];
			break;
		case 0x94:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3 + 1];
			break;
		case 0x98:
			retVal = dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3 + 2];
			break;

			// DMA2 (GPU)
		case 0xA0:
			retVal = dma->channelRegisters[PHILPSX_DMA_GPU * 3];
			break;
		case 0xA4:
			retVal = dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 1];
			break;
		case 0xA8:
			retVal = dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 2];
			break;

			// DMA3 (CDROM)
		case 0xB0:
			retVal = dma->channelRegisters[PHILPSX_DMA_CDROM * 3];
			break;
		case 0xB4:
			retVal = dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 1];
			break;
		case 0xB8:
			retVal = dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 2];
			break;

			// DMA4 (SPU)
		case 0xC0:
			retVal = dma->channelRegisters[PHILPSX_DMA_SPU * 3];
			break;
		case 0xC4:
			retVal = dma->channelRegisters[PHILPSX_DMA_SPU * 3 + 1];
			break;
		case 0xC8:
			retVal = dma->channelRegisters[PHILPSX_DMA_SPU * 3 + 2];
			break;

			// DMA5 (PIO)
		case 0xD0:
			retVal = dma->channelRegisters[PHILPSX_DMA_PIO * 3];
			break;
		case 0xD4:
			retVal = dma->channelRegisters[PHILPSX_DMA_PIO * 3 + 1];
			break;
		case 0xD8:
			retVal = dma->channelRegisters[PHILPSX_DMA_PIO * 3 + 2];
			break;

			// DMA6 (OTC)
		case 0xE0:
			retVal = dma->channelRegisters[PHILPSX_DMA_OTC * 3];
			break;
		case 0xE4:
			retVal = dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 1];
			break;
		case 0xE8:
			// Impose additional restrictions on DMA6 control register
			retVal |= 0x02000000;
			retVal = dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 2];
			break;

			// Global registers
		case 0xF0:
			retVal = dma->dmaControlRegister;
			break;
		case 0xF4:
			retVal = dma->dmaInterruptRegister;
			retVal = ((retVal << 24) & 0xFF000000) |
					((retVal << 8) & 0xFF0000) |
					(logical_rshift(retVal, 8) & 0xFF00) |
					(logical_rshift(retVal, 24) & 0xFF);
			int32_t tempInt = (retVal & 0x800000) |	(
					(retVal & 0x7F0000) &
					logical_rshift((retVal & 0x7F000000), 8)
					);
			if ((retVal & 0x8000) == 0x8000 || tempInt != 0) {
				retVal |= 0x80000000;
			} else {
				retVal &= 0x7FFFFFFF;
			}
			retVal = ((retVal << 24) & 0xFF000000) |
					((retVal << 8) & 0xFF0000) |
					(logical_rshift(retVal, 8) & 0xFF00) |
					(logical_rshift(retVal, 24) & 0xFF);
			break;
	}

	return retVal;
}

/*
 * This function sets the Bus Interface Unit reference.
 */
void DMAArbiter_setBiu(DMAArbiter *dma, BusInterfaceUnit *biu)
{
	dma->biu = biu;
}

/*
 * This function sets the CD-ROM reference.
 */
void DMAArbiter_setCdrom(DMAArbiter *dma, CDROMDrive *cdrom)
{
	dma->cdrom = cdrom;
}

/*
 * This function sets the CPU reference.
 */
void DMAArbiter_setCpu(DMAArbiter *dma, R3051 *cpu)
{
	dma->cpu = cpu;
}

/*
 * This function sets the GPU reference.
 */
void DMAArbiter_setGpu(DMAArbiter *dma, GPU *gpu)
{
	dma->gpu = gpu;
}

/*
 * This function sets the system reference to that of the supplied argument.
 */
void DMAArbiter_setMemoryInterface(DMAArbiter *dma, SystemInterlink *smi)
{
	dma->system = smi;
}

/*
 * This function writes bytes to the DMA Arbiter.
 */
void DMAArbiter_writeByte(DMAArbiter *dma, int32_t address, int8_t value)
{
	// Get word address and byte index
	int32_t wordAddress = address & 0xFFFFFFFC;
	int32_t byteIndex = address & 0x3;

	// Read original word
	int32_t tempWord = DMAArbiter_readWord(dma, wordAddress);

	// Mask out byte we are writing
	tempWord &= ~(0xFF << ((~byteIndex & 0x3) * 8));

	// Merge in our byte
	tempWord |= (value & 0xFF) << ((~byteIndex & 0x3) * 8);

	// Write word back
	DMAArbiter_writeWord(dma, wordAddress, tempWord);
}

/*
 * This function writes words to the DMA Arbiter.
 */
void DMAArbiter_writeWord(DMAArbiter *dma, int32_t address, int32_t word)
{
	// Determine last byte of address
	int32_t addressByte = address & 0xFF;

	// Store word in correct place
	switch (addressByte) {

			// DMA0 (MDECin)
		case 0x80:
			dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3] = word;
			break;
		case 0x84:
			dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3 + 1] = word;
			break;
		case 0x88:
			dma->channelRegisters[PHILPSX_DMA_MDEC_IN * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA1 (MDECout)
		case 0x90:
			dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3] = word;
			break;
		case 0x94:
			dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3 + 1] = word;
			break;
		case 0x98:
			dma->channelRegisters[PHILPSX_DMA_MDEC_OUT * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA2 (GPU)
		case 0xA0:
			dma->channelRegisters[PHILPSX_DMA_GPU * 3] = word;
			break;
		case 0xA4:
			dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 1] = word;
			break;
		case 0xA8:
			dma->channelRegisters[PHILPSX_DMA_GPU * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA3 (CDROM)
		case 0xB0:
			dma->channelRegisters[PHILPSX_DMA_CDROM * 3] = word;
			break;
		case 0xB4:
			dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 1] = word;
			break;
		case 0xB8:
			dma->channelRegisters[PHILPSX_DMA_CDROM * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA4 (SPU)
		case 0xC0:
			dma->channelRegisters[PHILPSX_DMA_SPU * 3] = word;
			break;
		case 0xC4:
			dma->channelRegisters[PHILPSX_DMA_SPU * 3 + 1] = word;
			break;
		case 0xC8:
			dma->channelRegisters[PHILPSX_DMA_SPU * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA5 (PIO)
		case 0xD0:
			dma->channelRegisters[PHILPSX_DMA_PIO * 3] = word;
			break;
		case 0xD4:
			dma->channelRegisters[PHILPSX_DMA_PIO * 3 + 1] = word;
			break;
		case 0xD8:
			dma->channelRegisters[PHILPSX_DMA_PIO * 3 + 2] = word;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// DMA6 (OTC)
		case 0xE0:
			dma->channelRegisters[PHILPSX_DMA_OTC * 3] = word;
			break;
		case 0xE4:
			dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 1] = word;
			break;
		case 0xE8:
			// Impose additional restrictions on writable bits
			word = ((word << 24) & 0xFF000000) |
					((word << 8) & 0xFF0000) |
					(logical_rshift(word, 8) & 0xFF00) |
					(logical_rshift(word, 24) & 0xFF);
			int32_t existingWord =
					dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 2];
			existingWord = ((existingWord << 24) & 0xFF000000) |
					((existingWord << 8) & 0xFF0000) |
					(logical_rshift(existingWord, 8) & 0xFF00) |
					(logical_rshift(existingWord, 24) & 0xFF);

			// Mask writable bits out of original and set bit 1
			existingWord &= 0xAEFFFFFF;
			existingWord |= 0x2;

			// Mask non-writable bits out of word and merge into original
			word &= 0x51000000;
			existingWord |= word;

			// Write back
			existingWord = ((existingWord << 24) & 0xFF000000) |
					((existingWord << 8) & 0xFF0000) |
					(logical_rshift(existingWord, 8) & 0xFF00) |
					(logical_rshift(existingWord, 24) & 0xFF);
			dma->channelRegisters[PHILPSX_DMA_OTC * 3 + 2] = existingWord;
			DMAArbiter_handleDMATransactions(dma);
			break;

			// Global registers
		case 0xF0:
			dma->dmaControlRegister = word;
			break;
		case 0xF4:
			// Swap endianness
			word = ((word << 24) & 0xFF000000) |
					((word << 8) & 0xFF0000) |
					(logical_rshift(word, 8) & 0xFF00) |
					(logical_rshift(word, 24) & 0xFF);
			dma->dmaInterruptRegister =
					((dma->dmaInterruptRegister << 24) & 0xFF000000) |
					((dma->dmaInterruptRegister << 8) & 0xFF0000) |
					(logical_rshift(dma->dmaInterruptRegister, 8) & 0xFF00) |
					(logical_rshift(dma->dmaInterruptRegister, 24) & 0xFF);

			// First deal with non-flag bits
			dma->dmaInterruptRegister &= 0x7F000000;
			dma->dmaInterruptRegister |= word & 0x00FFFFFF;

			// now reset flags if necessary
			word = ~word & 0x7F000000;
			dma->dmaInterruptRegister = (dma->dmaInterruptRegister & word) |
					(dma->dmaInterruptRegister & 0x00FFFFFF);

			// Swap endianness back
			dma->dmaInterruptRegister =
					((dma->dmaInterruptRegister << 24) & 0xFF000000) |
					((dma->dmaInterruptRegister << 8) & 0xFF0000) |
					(logical_rshift(dma->dmaInterruptRegister, 8) & 0xFF00) |
					(logical_rshift(dma->dmaInterruptRegister, 24) & 0xFF);
			break;
	}
}