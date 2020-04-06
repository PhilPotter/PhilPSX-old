/*
 * This C file models a system interlink that ties RAM, CPU and other components
 * together.
 * 
 * SystemInterlink.c - Copyright Phillip Potter, 2020
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "../headers/SystemInterlink.h"
#include "../headers/CDROMDrive.h"
#include "../headers/ControllerIO.h"
#include "../headers/R3051.h"
#include "../headers/DMAArbiter.h"
#include "../headers/GPU.h"
#include "../headers/SPU.h"
#include "../headers/math_utils.h"

// Forward declarations for functions and subcomponents private to this class
// SystemInterlink-related stuff:
static bool SystemInterlink_loadBiosFileToMemory(const char *biosPath,
		int8_t *biosMemory);

// TimerModule-related stuff:
typedef struct TimerModule TimerModule;
static void TimerModule_appendSyncCycles(TimerModule *timerModule,
		int32_t cycles);
static int32_t TimerModule_readCounterValue(TimerModule *timerModule,
		int32_t timer);
static int32_t TimerModule_readMode(TimerModule *timerModule,
		int32_t timer, bool override);
static int32_t TimerModule_readTargetValue(TimerModule *timerModule,
		int32_t timer);
static void TimerModule_resync(TimerModule *timerModule);
static int32_t TimerModule_swapEndianness(int32_t word);
static void TimerModule_triggerTimerInterrupt(TimerModule *timerModule,
		int32_t timer);
static void TimerModule_writeCounterValue(TimerModule *timerModule,
		int32_t timer, int32_t value);
static void TimerModule_writeMode(TimerModule *timerModule,
		int32_t timer, int32_t value);
static void TimerModule_writeTargetValue(TimerModule *timerModule,
		int32_t timer, int32_t value);

/*
 * This struct models all three timers within a single object.
 */
struct TimerModule {

	// Reference back to SystemInterlink container object
	SystemInterlink *smi;
	
	// Variables for the three timers
	int32_t timerMode[3];
	int32_t timerCounterValue[3];
	int32_t timerTargetValue[3];
	int32_t clockSource[3];
	int32_t incrementBy[3];
	int32_t newValue[3];
	bool interruptHappenedOnceOrMore[3];

	// Variables to track CPU cycles and GPU cycles
	int32_t cpuCyclesToSync[3];
	int32_t gpuCyclesToSync[3];
	int32_t cpuTopup[3];
	int32_t gpuTopup[3];
	bool hblankHappened[3];
	bool vblankHappened[3];
};

/*
 * This struct models the PlayStation components as linked components in a
 * motherboard of sorts.
 */
struct SystemInterlink {
	
	// Variables to keep track of components
	DMAArbiter *dma;
	R3051 *cpu;
	GPU *gpu;
	SPU *spu;
	CDROMDrive *cdrom;
	ControllerIO *cio;
	int8_t *ram;		// Allocated dynamically due to size
	int8_t *scratchpad;	// Allocated dynamically due to size
	int8_t *bios;		// Allocated dynamically due to size

	// Timers declaration
	TimerModule timerModule;

	// Register declarations
	int32_t cacheControlReg;
	int32_t interruptStatusReg;
	int32_t interruptMaskReg;
	int32_t expansion1BaseAddress;
	int32_t expansion2BaseAddress;
	int32_t expansion1DelaySize;
	int32_t expansion3DelaySize;
	int32_t biosRomDelaySize;
	int32_t spuDelaySize;
	int32_t cdromDelaySize;
	int32_t expansion2DelaySize;
	int32_t commonDelay;
	int32_t ramSize;
	int8_t biosPost;

	// These registers can allow us to delay interrupts so they trigger at
	// the proper time
	int64_t gpuInterruptDelay;
	int64_t dmaInterruptDelay;
	int64_t cdromInterruptDelay;
	int64_t gpuInterruptCounter;
	int64_t dmaInterruptCounter;
	int64_t cdromInterruptCounter;
	int64_t timersInterruptDelay[3];
	int64_t timersInterruptCounter[3];
	int32_t cdromInterruptNumber;
	bool cdromInterruptEnabled;
	int32_t interruptCycles;
};

/*
 * This function constructs a SystemInterlink object using the file specified
 * by biosPath as its system BIOS.
 */
SystemInterlink *construct_SystemInterlink(const char *biosPath)
{
	// Allocate SystemInterlink struct
	SystemInterlink *smi = malloc(sizeof(SystemInterlink));
	if (!smi) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't allocate memory "
				"for SystemInterlink struct\n");
		goto end;
	}
	
	// Allocate 2 MB system RAM
	smi->ram = calloc(2097152, sizeof(int8_t));
	if (!smi->ram) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't allocate memory "
				"for ram array\n");
		goto cleanup_systeminterlink;
	}

	// Allocate 512 KB BIOS area
	smi->bios = calloc(524288, sizeof(int8_t));
	if (!smi->bios) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't allocate memory "
				"for bios array\n");
		goto cleanup_ram;
	}

	// Allocate scratchpad
	smi->scratchpad = calloc(1024, sizeof(int8_t));
	if (!smi->scratchpad) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't allocate memory "
				"for scratchpad array\n");
		goto cleanup_bios;
	}
	
	// Load BIOS
	if (!SystemInterlink_loadBiosFileToMemory(biosPath, smi->bios)) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't copy BIOS data "
				"to memory\n");
		goto cleanup_scratchpad;
	}
	
	// Zero out timer module and set interlink reference
	memset(&smi->timerModule, 0, sizeof(smi->timerModule));
	smi->timerModule.smi = smi;
	
	// Set all component references to NULL
	smi->dma = NULL;
	smi->cpu = NULL;
	smi->gpu = NULL;
	smi->spu = NULL;
	smi->cdrom = NULL;
	smi->cio = NULL;
	
	// Setup interrupt delays
	smi->gpuInterruptDelay = -1L;
	smi->dmaInterruptDelay = -1L;
	smi->cdromInterruptDelay = -1L;
	smi->gpuInterruptCounter = 0;
	smi->dmaInterruptCounter = 0;
	smi->cdromInterruptCounter = 0;
	for (int32_t i = 0; i < 3; ++i) {
		smi->timersInterruptDelay[i] = -1L;
		smi->timersInterruptCounter[i] = 0;
	}
	smi->cdromInterruptNumber = 0;
	smi->cdromInterruptEnabled = false;
	smi->interruptCycles = 0;

	// Setup registers
	smi->cacheControlReg = 0;
	smi->interruptStatusReg = 0;
	smi->interruptMaskReg = 0;
	smi->expansion1BaseAddress = 0;
	smi->expansion2BaseAddress = 0;
	smi->expansion1DelaySize = 0;
	smi->expansion3DelaySize = 0;
	smi->biosRomDelaySize = 0;
	smi->spuDelaySize = 0;
	smi->cdromDelaySize = 0;
	smi->expansion2DelaySize = 0;
	smi->commonDelay = 0;
	smi->ramSize = 0;
	smi->biosPost = 0;
	
	// Normal return:
	return smi;
	
	// Cleanup path:
	cleanup_scratchpad:
	free(smi->scratchpad);
	
	cleanup_bios:
	free(smi->bios);
	
	cleanup_ram:
	free(smi->ram);
	
	cleanup_systeminterlink:
	free(smi);
	smi = NULL;
	
	end:
	return smi;
}

/*
 * This function destructs a SystemInterlink object.
 */
void destruct_SystemInterlink(SystemInterlink *smi)
{
	free(smi->scratchpad);
	free(smi->bios);
	free(smi->ram);
	free(smi);
}

/*
 * This function appends CPU cycles to the timer module.
 */
void SystemInterlink_appendSyncCycles(SystemInterlink *smi, int32_t cycles)
{
	smi->interruptCycles += cycles;
	GPU_appendSyncCycles(smi->gpu, cycles);
	ControllerIO_appendSyncCycles(smi->cio, cycles);
	TimerModule_appendSyncCycles(&smi->timerModule, cycles);
}

/*
 * This function wraps the GPU function to execute accumulated cycles.
 */
void SystemInterlink_executeGPUCycles(SystemInterlink *smi)
{
	GPU_executeGPUCycles(smi->gpu);
}

/*
 * This function returns the CD-ROM object of the system.
 */
CDROMDrive *SystemInterlink_getCdrom(SystemInterlink *smi)
{
	return smi->cdrom;
}

/*
 * This function returns the ControllerIO object of the system.
 */
ControllerIO *SystemInterlink_getControllerIO(SystemInterlink *smi)
{
	return smi->cio;
}

/*
 * This function returns the R3051 CPU object of the system.
 */
R3051 *SystemInterlink_getCpu(SystemInterlink *smi)
{
	return smi->cpu;
}

/*
 * This function returns the DMAArbiter object of the system.
 */
DMAArbiter *SystemInterlink_getDma(SystemInterlink *smi)
{
	return smi->dma;
}

/*
 * This function returns the GPU object of the system.
 */
GPU *SystemInterlink_getGpu(SystemInterlink *smi)
{
	return smi->gpu;
}

/*
 * This function allows us to return a reference to the RAM array.
 */
int8_t *SystemInterlink_getRamArray(SystemInterlink *smi)
{
	return smi->ram;
}

/*
 * This function returns the SPU object of the system.
 */
SPU *SystemInterlink_getSpu(SystemInterlink *smi)
{
	return smi->spu;
}

/*
 * This tells us how many stall cycles to use when accessing a given address.
 */
int32_t SystemInterlink_howManyStallCycles(SystemInterlink *smi,
		int32_t address)
{
	// Convert address to long so we can do arithmetic on it, aligning to
	// word boundary, and declare return value
	int64_t tempAddress = 0xFFFFFFFCL & address;

	// Set to 4 as this is the delay of most registers
	int32_t cycles = 4;

	// Check which area the address is in and set cycles accordingly
	if (tempAddress >= 0L && tempAddress < 0x200000L) {
		// RAM
		cycles = 6;
	} else if (tempAddress >= 0x1FC00000L && tempAddress < 0x1FC80000L) {
		// BIOS
		cycles = 1;
	} else if (tempAddress == 0xFFFE0130L) {
		// Cache control register
		cycles = 1;
	}

	return cycles;
}

/*
 * This function updates the counters of all the peripherals capable of
 * generating interrupts, and sets flags in the interrupt status register at
 * the right time.
 */
void SystemInterlink_incrementInterruptCounters(SystemInterlink *smi)
{
	smi->gpuInterruptCounter += smi->interruptCycles;
	smi->dmaInterruptCounter += smi->interruptCycles;
	smi->cdromInterruptCounter += smi->interruptCycles;
	for (int32_t i = 0; i < 3; ++i) {
		smi->timersInterruptCounter[i] += smi->interruptCycles;
	}

	// Handle GPU
	if (smi->gpuInterruptDelay != -1L &&
			smi->gpuInterruptCounter > smi->gpuInterruptDelay) {
		// Set in little-endian mode for speed
		smi->interruptStatusReg |= 0x01000000;
		smi->gpuInterruptDelay = -1L;
	}

	// Handle DMA
	if (smi->dmaInterruptDelay != -1L &&
			smi->dmaInterruptCounter > smi->dmaInterruptDelay) {
		// Set in little-endian mode for speed
		smi->interruptStatusReg |= 0x08000000;
		smi->dmaInterruptDelay = -1L;
	}

	// Handle CD-ROM
	if (smi->cdromInterruptDelay != -1L &&
			smi->cdromInterruptCounter > smi->cdromInterruptDelay) {
		// Set in little-endian mode for speed
		if (smi->cdromInterruptEnabled) {
			smi->interruptStatusReg |= 0x04000000;
		}

		// Set this interrupt handler back to inactive
		smi->cdromInterruptDelay = -1L;

		// Also set interrupt number in CD-ROM interrupt flag register
		CDROMDrive_setInterruptNumber(smi->cdrom, smi->cdromInterruptNumber);
	}

	// Handle Timer 0
	if (smi->timersInterruptDelay[0] != -1L &&
			smi->timersInterruptCounter[0] > smi->timersInterruptDelay[0]) {
		// Set in little-endian mode for speed
		smi->interruptStatusReg |= 0x10000000;
		smi->timersInterruptDelay[0] = -1L;
	}

	// Handle Timer 1
	if (smi->timersInterruptDelay[1] != -1L &&
			smi->timersInterruptCounter[1] > smi->timersInterruptDelay[1]) {
		// Set in little-endian mode for speed
		smi->interruptStatusReg |= 0x20000000;
		smi->timersInterruptDelay[1] = -1L;
	}

	// Handle Timer 2
	if (smi->timersInterruptDelay[2] != -1L &&
			smi->timersInterruptCounter[2] > smi->timersInterruptDelay[2]) {
		// Set in little-endian mode for speed
		smi->interruptStatusReg |= 0x40000000;
		smi->timersInterruptDelay[2] = -1L;
	}

	// Reset interrupt cycles
	smi->interruptCycles = 0;
}

/*
 * This tests the cache control register to see if the instruction
 * cache is enabled.
 */
bool SystemInterlink_instructionCacheEnabled(SystemInterlink *smi)
{
	// swap endianness of reg
	int32_t tempReg = ((smi->cacheControlReg << 24) & 0xFF000000) | 
			((smi->cacheControlReg << 8) & 0xFF0000) | 
			(logical_rshift(smi->cacheControlReg, 8) & 0xFF00) | 
			(logical_rshift(smi->cacheControlReg, 24) & 0xFF);

	return((tempReg & 0x800) == 0x800);
}

/*
 * This function lets us know whether or not an address should be incremented -
 * it is mainly useful for halfword accesses where each byte should come from
 * the same address.
 */
bool SystemInterlink_okToIncrement(SystemInterlink *smi, int64_t address)
{
	return !(address >= 0x1F801800 && address <= 0x1F801803);
}

/*
 * This reads from the correct area depending on the address.
 */
int8_t SystemInterlink_readByte(SystemInterlink *smi, int32_t address)
{
	int64_t tempAddress = address & 0xFFFFFFFFL;
	int8_t retVal = 0;
	
	if (tempAddress >= 0L && tempAddress < 0x200000L) {
		// RAM
		// Try to read RAM first
		retVal = smi->ram[(int32_t)tempAddress];
	}
	else if (tempAddress >= 0x1FC00000L && tempAddress < 0x1FC80000L) {
		// BIOS ROM
		// Then try to read ROM
		retVal = smi->bios[(int32_t)(tempAddress - 0x1FC00000L)];
	} else { // Everything else

		if (tempAddress >= 0x1F000000L && tempAddress < 0x1F800000L) {
			// Expansion Region 1
			// Do nothing for now
			;
		}
		else if (tempAddress >= 0x1F800000L && tempAddress < 0x1F800400L) {
			// Scratchpad
			// Read from data cache scratchpad if enabled
			if (SystemInterlink_scratchpadEnabled(smi)) {
				tempAddress -= 0x1F800000L;
				retVal = smi->scratchpad[(int32_t)tempAddress];
			}
		}
		else if (tempAddress >= 0x1F801000L && tempAddress < 0x1F802000L) {
			// I/O Ports
			if (tempAddress >= 0x1F801000L && tempAddress < 0x1F801004L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->expansion1BaseAddress,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->expansion1BaseAddress,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->expansion1BaseAddress,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->expansion1BaseAddress;
						break;
				}
			}
			else if (tempAddress >= 0x1F801004L && tempAddress < 0x1F801008L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->expansion2BaseAddress,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->expansion2BaseAddress,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->expansion2BaseAddress,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->expansion2BaseAddress;
						break;
				}
			}
			else if (tempAddress >= 0x1F801008L && tempAddress < 0x1F80100CL) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->expansion1DelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->expansion1DelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->expansion1DelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->expansion1DelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F80100CL && tempAddress < 0x1F801010L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->expansion3DelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->expansion3DelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->expansion3DelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->expansion3DelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F801010L && tempAddress < 0x1F801014L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->biosRomDelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->biosRomDelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->biosRomDelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->biosRomDelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F801014L && tempAddress < 0x1F801018L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->spuDelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->spuDelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->spuDelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->spuDelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F801018L && tempAddress < 0x1F80101CL) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->cdromDelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->cdromDelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->cdromDelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->cdromDelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F80101CL && tempAddress < 0x1F801020L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->expansion2DelaySize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->expansion2DelaySize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->expansion2DelaySize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->expansion2DelaySize;
						break;
				}
			}
			else if (tempAddress >= 0x1F801020L && tempAddress < 0x1F801024L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->commonDelay,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->commonDelay,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->commonDelay,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->commonDelay;
						break;
				}
			}
			else if (tempAddress >= 0x1F801040L && tempAddress < 0x1F801050L) {
				// Read from ControllerIO object
				retVal = ControllerIO_readByte(smi->cio, (int32_t)tempAddress);
			}
			else if (tempAddress >= 0x1F801060L && tempAddress < 0x1F801064L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->ramSize,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->ramSize,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->ramSize,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->ramSize;
						break;
				}
			} // Interrupt Status Register
			else if (tempAddress >= 0x1F801070L && tempAddress < 0x1F801074L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->interruptStatusReg,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->interruptStatusReg,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->interruptStatusReg,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->interruptStatusReg;
						break;
				}
			} // Interrupt Mask Register
			else if (tempAddress >= 0x1F801074L && tempAddress < 0x1F801078L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->interruptMaskReg,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->interruptMaskReg,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->interruptMaskReg,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->interruptMaskReg;
						break;
				}
			}
			else if (tempAddress >= 0x1F801080L && tempAddress < 0x1F801100L) {
				retVal = DMAArbiter_readByte(smi->dma, address);
			}
			else if (tempAddress >= 0x1F801100L && tempAddress < 0x1F801104L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								0),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								0),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								0),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readCounterValue(
								&smi->timerModule,
								0
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801104L && tempAddress < 0x1F801108L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								0,
								false),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								0,
								false),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								0,
								false),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readMode(
								&smi->timerModule,
								0,
								false
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801108L && tempAddress < 0x1F80110CL) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								0),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								0),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								0),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readTargetValue(
								&smi->timerModule,
								0
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801110L && tempAddress < 0x1F801114L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								1),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								1),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								1),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readCounterValue(
								&smi->timerModule,
								1
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801114L && tempAddress < 0x1F801118L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								1,
								false),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								1,
								false),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								1,
								false),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readMode(
								&smi->timerModule,
								1,
								false
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801118L && tempAddress < 0x1F80111CL) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								1),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								1),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								1),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readTargetValue(
								&smi->timerModule,
								1
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801120L && tempAddress < 0x1F801124L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								2),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								2),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readCounterValue(
								&smi->timerModule,
								2),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readCounterValue(
								&smi->timerModule,
								2
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801124L && tempAddress < 0x1F801128L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								2,
								false),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								2,
								false),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readMode(
								&smi->timerModule,
								2,
								false),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readMode(
								&smi->timerModule,
								2,
								false
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801128L && tempAddress < 0x1F80112CL) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								2),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								2),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								TimerModule_readTargetValue(
								&smi->timerModule,
								2),
								8
								);
						break;
					case 3:
						retVal = (int8_t)TimerModule_readTargetValue(
								&smi->timerModule,
								2
								);
						break;
				}
			}
			else if (tempAddress >= 0x1F801810L && tempAddress < 0x1F801814L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								GPU_readResponse(
								smi->gpu),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								GPU_readResponse(
								smi->gpu),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								GPU_readResponse(
								smi->gpu),
								8
								);
						break;
					case 3:
						retVal = (int8_t)GPU_readResponse(smi->gpu);
						break;
				}
			}
			else if (tempAddress >= 0x1F801814L && tempAddress < 0x1F801818L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								GPU_readStatus(
								smi->gpu),
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								GPU_readStatus(
								smi->gpu),
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								GPU_readStatus(
								smi->gpu),
								8
								);
						break;
					case 3:
						retVal = (int8_t)GPU_readStatus(smi->gpu);
						break;
				}
			} // CD-ROM
			else if (tempAddress >= 0x1F801800L && tempAddress < 0x1F801804L) {
				switch ((int32_t)tempAddress & 0xF) {
					case 0:
						retVal = CDROMDrive_read1800(smi->cdrom);
						break;
					case 1:
						retVal = CDROMDrive_read1801(smi->cdrom);
						break;
					case 2:
						retVal = CDROMDrive_read1802(smi->cdrom);
						break;
					case 3:
						retVal = CDROMDrive_read1803(smi->cdrom);
						break;
				}
			}
			else if (tempAddress >= 0x1F801C00L && tempAddress < 0x1F802000L) {
				// Fake SPU read
				retVal = SPU_readByte(smi->spu, address);
			}
		} // Expansion Region 2 (I/O Ports)
		else if (tempAddress >= 0x1F802000L && tempAddress < 0x1F803000L) {
			// Read from BIOS post register
			if (tempAddress == 0x1F802041L) {
				retVal = smi->biosPost;
			}
		} // Expansion Region 3 (Multipurpose)
		else if (tempAddress >= 0x1FA00000L && tempAddress < 0x1FC00000L) {
			// Do nothing for now
			;
		} // I/O Ports (Cache Control)
		else if (tempAddress >= 0xFFFE0000L && tempAddress < 0xFFFE0200L) {
			// Cache Control Register
			if (tempAddress >= 0xFFFE0130L && tempAddress < 0xFFFE0134L) {
				int32_t shift = (int32_t)(tempAddress & 0x3L);
				switch (shift) {
					case 0:
						retVal = (int8_t)logical_rshift(
								smi->cacheControlReg,
								24
								);
						break;
					case 1:
						retVal = (int8_t)logical_rshift(
								smi->cacheControlReg,
								16
								);
						break;
					case 2:
						retVal = (int8_t)logical_rshift(
								smi->cacheControlReg,
								8
								);
						break;
					case 3:
						retVal = (int8_t)smi->cacheControlReg;
						break;
				}
			}
		}
	}

	return retVal;
}

/*
 * This function reads the interrupt status.
 */
int32_t SystemInterlink_readInterruptStatus(SystemInterlink *smi)
{
	return smi->interruptStatusReg;
}

/*
 * This reads a word from the correct area depending on the address.
 */
int32_t SystemInterlink_readWord(SystemInterlink *smi, int32_t address)
{
	int64_t tempAddress = address & 0xFFFFFFFCL;
	address = (int32_t)tempAddress;
	int32_t retVal = 0;

	// Handle RAM directly rather than going to readByte method
	if (tempAddress >= 0L && tempAddress < 0x200000L) {
		retVal = (smi->ram[address] & 0xFF) << 24 |
				(smi->ram[address + 1] & 0xFF) << 16 |
				(smi->ram[address + 2] & 0xFF) << 8 |
				(smi->ram[address + 3] & 0xFF);
	} // Handle ROM directly rather than going to readByte method
	else if (tempAddress >= 0x1FC00000L && tempAddress < 0x1FC80000L) {
		address -= 0x1FC00000;
		retVal = (smi->bios[address] & 0xFF) << 24 |
				(smi->bios[address + 1] & 0xFF) << 16 |
				(smi->bios[address + 2] & 0xFF) << 8 |
				(smi->bios[address + 3] & 0xFF);
	} // Handle everything else
	else {
		switch (address) {
			case 0x1F801070:
				retVal = smi->interruptStatusReg;
				break;
			case 0x1F801074:
				retVal = smi->interruptMaskReg;
				break;
			case 0xFFFE0130:
				retVal = smi->cacheControlReg;
				break;
			case 0x1F801080:
			case 0x1F801081:
			case 0x1F801082:
			case 0x1F801083:
			case 0x1F801084:
			case 0x1F801085:
			case 0x1F801086:
			case 0x1F801087:
			case 0x1F801088:
			case 0x1F801089:
			case 0x1F80108A:
			case 0x1F80108B:
			case 0x1F80108C:
			case 0x1F80108D:
			case 0x1F80108E:
			case 0x1F80108F:
			case 0x1F801090:
			case 0x1F801091:
			case 0x1F801092:
			case 0x1F801093:
			case 0x1F801094:
			case 0x1F801095:
			case 0x1F801096:
			case 0x1F801097:
			case 0x1F801098:
			case 0x1F801099:
			case 0x1F80109A:
			case 0x1F80109B:
			case 0x1F80109C:
			case 0x1F80109D:
			case 0x1F80109E:
			case 0x1F80109F:
			case 0x1F8010A0:
			case 0x1F8010A1:
			case 0x1F8010A2:
			case 0x1F8010A3:
			case 0x1F8010A4:
			case 0x1F8010A5:
			case 0x1F8010A6:
			case 0x1F8010A7:
			case 0x1F8010A8:
			case 0x1F8010A9:
			case 0x1F8010AA:
			case 0x1F8010AB:
			case 0x1F8010AC:
			case 0x1F8010AD:
			case 0x1F8010AE:
			case 0x1F8010AF:
			case 0x1F8010B0:
			case 0x1F8010B1:
			case 0x1F8010B2:
			case 0x1F8010B3:
			case 0x1F8010B4:
			case 0x1F8010B5:
			case 0x1F8010B6:
			case 0x1F8010B7:
			case 0x1F8010B8:
			case 0x1F8010B9:
			case 0x1F8010BA:
			case 0x1F8010BB:
			case 0x1F8010BC:
			case 0x1F8010BD:
			case 0x1F8010BE:
			case 0x1F8010BF:
			case 0x1F8010C0:
			case 0x1F8010C1:
			case 0x1F8010C2:
			case 0x1F8010C3:
			case 0x1F8010C4:
			case 0x1F8010C5:
			case 0x1F8010C6:
			case 0x1F8010C7:
			case 0x1F8010C8:
			case 0x1F8010C9:
			case 0x1F8010CA:
			case 0x1F8010CB:
			case 0x1F8010CC:
			case 0x1F8010CD:
			case 0x1F8010CE:
			case 0x1F8010CF:
			case 0x1F8010D0:
			case 0x1F8010D1:
			case 0x1F8010D2:
			case 0x1F8010D3:
			case 0x1F8010D4:
			case 0x1F8010D5:
			case 0x1F8010D6:
			case 0x1F8010D7:
			case 0x1F8010D8:
			case 0x1F8010D9:
			case 0x1F8010DA:
			case 0x1F8010DB:
			case 0x1F8010DC:
			case 0x1F8010DD:
			case 0x1F8010DE:
			case 0x1F8010DF:
			case 0x1F8010E0:
			case 0x1F8010E1:
			case 0x1F8010E2:
			case 0x1F8010E3:
			case 0x1F8010E4:
			case 0x1F8010E5:
			case 0x1F8010E6:
			case 0x1F8010E7:
			case 0x1F8010E8:
			case 0x1F8010E9:
			case 0x1F8010EA:
			case 0x1F8010EB:
			case 0x1F8010EC:
			case 0x1F8010ED:
			case 0x1F8010EE:
			case 0x1F8010EF:
			case 0x1F8010F0:
			case 0x1F8010F1:
			case 0x1F8010F2:
			case 0x1F8010F3:
			case 0x1F8010F4:
			case 0x1F8010F5:
			case 0x1F8010F6:
			case 0x1F8010F7:
			case 0x1F8010F8:
			case 0x1F8010F9:
			case 0x1F8010FA:
			case 0x1F8010FB:
			case 0x1F8010FC:
			case 0x1F8010FD:
			case 0x1F8010FE:
			case 0x1F8010FF:
				retVal = DMAArbiter_readWord(smi->dma, address);
				break;
			case 0x1F801100:
				retVal = TimerModule_readCounterValue(&smi->timerModule, 0);
				break;
			case 0x1F801104:
				retVal = TimerModule_readMode(&smi->timerModule, 0, false);
				break;
			case 0x1F801108:
				retVal = TimerModule_readTargetValue(&smi->timerModule, 0);
				break;
			case 0x1F801110:
				retVal = TimerModule_readCounterValue(&smi->timerModule, 1);
				break;
			case 0x1F801114:
				retVal = TimerModule_readMode(&smi->timerModule, 1, false);
				break;
			case 0x1F801118:
				retVal = TimerModule_readTargetValue(&smi->timerModule, 1);
				break;
			case 0x1F801120:
				retVal = TimerModule_readCounterValue(&smi->timerModule, 2);
				break;
			case 0x1F801124:
				retVal = TimerModule_readMode(&smi->timerModule, 2, false);
				break;
			case 0x1F801128:
				retVal = TimerModule_readTargetValue(&smi->timerModule, 2);
				break;
			case 0x1F801000:
				retVal = smi->expansion1BaseAddress;
				break;
			case 0x1F801004:
				retVal = smi->expansion2BaseAddress;
				break;
			case 0x1F801008:
				retVal = smi->expansion1DelaySize;
				break;
			case 0x1F80100C:
				retVal = smi->expansion3DelaySize;
				break;
			case 0x1F801010:
				retVal = smi->biosRomDelaySize;
				break;
			case 0x1F801014:
				retVal = smi->spuDelaySize;
				break;
			case 0x1F801018:
				retVal = smi->cdromDelaySize;
				break;
			case 0x1F80101C:
				retVal = smi->expansion2DelaySize;
				break;
			case 0x1F801020:
				retVal = smi->commonDelay;
				break;
			case 0x1F801060:
				retVal = smi->ramSize;
				break;
			case 0x1F801810:
				retVal = GPU_readResponse(smi->gpu);
				break;
			case 0x1F801814:
				retVal = GPU_readStatus(smi->gpu);
				break;
			case 0x1F801800:
			case 0x1F801801:
			case 0x1F801802:
			case 0x1F801803:
				// Do nothing as we don't allow word reads from CD-ROM range
				break;
			default:
				// Use readByte method to read four bytes
				retVal = (SystemInterlink_readByte(
						smi, (int32_t)tempAddress) & 0xFF) << 24;
				++tempAddress;
				retVal |= (SystemInterlink_readByte(
						smi, (int32_t)tempAddress) & 0xFF) << 16;
				++tempAddress;
				retVal |= (SystemInterlink_readByte(
						smi, (int32_t)tempAddress) & 0xFF) << 8;
				++tempAddress;
				retVal |= SystemInterlink_readByte(
						smi, (int32_t)tempAddress) & 0xFF;
				break;
		}
	}

	return retVal;
}

/*
 * This function syncs all timers up to the current point.
 */
void SystemInterlink_resyncAllTimers(SystemInterlink *smi)
{
	TimerModule_resync(&smi->timerModule);
}

/*
 * This tests the cache control register to see if scratchpad is enabled.
 */
bool SystemInterlink_scratchpadEnabled(SystemInterlink *smi)
{
	// Swap endianness of reg
	int32_t tempReg = ((smi->cacheControlReg << 24) & 0xFF000000) |
			((smi->cacheControlReg << 8) & 0xFF0000) |
			(logical_rshift(smi->cacheControlReg, 8) & 0xFF00) |
			(logical_rshift(smi->cacheControlReg, 24) & 0xFF);

	return ((tempReg & 0x88) == 0x88);
}

/*
 * This sets the CD-ROM interrupt delay.
 */
void SystemInterlink_setCDROMInterruptDelay(SystemInterlink *smi,
		int32_t delay)
{
	smi->cdromInterruptDelay = delay;
	smi->cdromInterruptCounter = 0;
}

/*
 * This sets whether the CDROM interrupt is actually enabled.
 */
void SystemInterlink_setCDROMInterruptEnabled(SystemInterlink *smi,
		bool enabled)
{
	smi->cdromInterruptEnabled = enabled;
}

/*
 * This sets the CDROM interrupt number.
 */
void SystemInterlink_setCDROMInterruptNumber(SystemInterlink *smi,
		int32_t number)
{
	smi->cdromInterruptNumber = number;
}

/*
 * This function sets the CD-ROM object of the system.
 */
void SystemInterlink_setCdrom(SystemInterlink *smi,
		CDROMDrive *cdrom)
{
	smi->cdrom = cdrom;
}

/*
 * This function sets the ControllerIO object of the system.
 */
void SystemInterlink_setControllerIO(SystemInterlink *smi,
		ControllerIO *cio)
{
	smi->cio = cio;
}

/*
 * This sets the R3051 CPU reference to that supplied by the argument.
 */
void SystemInterlink_setCpu(SystemInterlink *smi, R3051 *cpu)
{
	smi->cpu = cpu;
}

/*
 * This sets the DMA interrupt delay.
 */
void SystemInterlink_setDMAInterruptDelay(SystemInterlink *smi, int32_t delay)
{
	smi->dmaInterruptDelay = delay;
	smi->dmaInterruptCounter = 0;
}

/*
 * This sets the DMAArbiter reference to that supplied by the argument.
 */
void SystemInterlink_setDma(SystemInterlink *smi, DMAArbiter *dma)
{
	smi->dma = dma;
}

/*
 * This sets the GPU interrupt delay.
 */
void SystemInterlink_setGPUInterruptDelay(SystemInterlink *smi, int32_t delay)
{
	smi->gpuInterruptDelay = delay;
	smi->gpuInterruptCounter = 0;
}

/*
 * This sets the GPU reference to that supplied by the argument.
 */
void SystemInterlink_setGpu(SystemInterlink *smi, GPU *gpu)
{
	smi->gpu = gpu;
}

/*
 * This sets the SPU reference to that supplied by the argument.
 */
void SystemInterlink_setSpu(SystemInterlink *smi, SPU *spu)
{
	smi->spu = spu;
}

/*
 * This tests the cache control register to see if tag test mode is enabled.
 */
bool SystemInterlink_tagTestEnabled(SystemInterlink *smi)
{
	// Swap endianness of reg
	int32_t tempReg = ((smi->cacheControlReg << 24) & 0xFF000000) |
			((smi->cacheControlReg << 8) & 0xFF0000) |
			(logical_rshift(smi->cacheControlReg, 8) & 0xFF00) |
			(logical_rshift(smi->cacheControlReg, 24) & 0xFF);

	return ((tempReg & 0x4) == 0x4);
}

/*
 * This writes to the correct area depending on the address.
 */
void SystemInterlink_writeByte(SystemInterlink *smi, int32_t address,
		int8_t value)
{
	int64_t tempAddress = address & 0xFFFFFFFFL;

	// RAM
	if (tempAddress >= 0L && tempAddress < 0x200000L) {
		smi->ram[(int32_t)tempAddress] = value;
	} // Expansion Region 1
	else if (tempAddress >= 0x1F000000L && tempAddress < 0x1F800000L) {
		// Do nothing for now
		;
	} // Scratchpad
	else if (tempAddress >= 0x1F800000L && tempAddress < 0x1F800400L) {
		// Write to data cache scratchpad
		if (SystemInterlink_scratchpadEnabled(smi)) {
			tempAddress -= 0x1F800000L;
			smi->scratchpad[(int32_t)tempAddress] = value;
		}
	} // I/O Ports
	else if (tempAddress >= 0x1F801000L && tempAddress < 0x1F802000L) {
		if (tempAddress >= 0x1F801000L && tempAddress < 0x1F801004L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->expansion1BaseAddress = (value & 0xFF) << 24;
					break;
				case 1:
					smi->expansion1BaseAddress =
							(smi->expansion1BaseAddress & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801004L && tempAddress < 0x1F801008L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->expansion2BaseAddress = (value & 0xFF) << 24;
					break;
				case 1:
					smi->expansion2BaseAddress =
							(smi->expansion2BaseAddress & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801008L && tempAddress < 0x1F80100CL) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->expansion1DelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->expansion1DelaySize =
							(smi->expansion1DelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F80100CL && tempAddress < 0x1F801010L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->expansion3DelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->expansion3DelaySize =
							(smi->expansion3DelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801010L && tempAddress < 0x1F801014L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->biosRomDelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->biosRomDelaySize =
							(smi->biosRomDelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801014L && tempAddress < 0x1F801018L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->spuDelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->spuDelaySize =
							(smi->spuDelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801018L && tempAddress < 0x1F80101CL) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->cdromDelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->cdromDelaySize =
							(smi->cdromDelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F80101CL && tempAddress < 0x1F801020L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->expansion2DelaySize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->expansion2DelaySize =
							(smi->expansion2DelaySize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801020L && tempAddress < 0x1F801024L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->commonDelay = (value & 0xFF) << 24;
					break;
				case 1:
					smi->commonDelay =
							(smi->commonDelay & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801040L && tempAddress < 0x1F801050L) {
			// Write to ControllerIO object
			ControllerIO_writeByte(smi->cio, (int32_t)tempAddress, value);
		}
		else if (tempAddress >= 0x1F801060L && tempAddress < 0x1F801064L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					smi->ramSize = (value & 0xFF) << 24;
					break;
				case 1:
					smi->ramSize =
							(smi->ramSize & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		} // Interrupt Status Register
		else if (tempAddress >= 0x1F801070L && tempAddress < 0x1F801074L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					// Mask first byte
					smi->interruptStatusReg &=
							((value & 0xFF) << 24) |
							(smi->interruptStatusReg & 0xFF0000);
					break;
				case 1:
					// Mask second byte
					smi->interruptStatusReg &=
							(smi->interruptStatusReg & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		} // Interrupt Mask Register
		else if (tempAddress >= 0x1F801074L && tempAddress < 0x1F801078L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					// Write full 32-bits, with zeroes for remaining
					// three bytes
					smi->interruptMaskReg = (value & 0xFF) << 24;
					break;
				case 1:
					// Keep first byte of interrupt mask, merge with this
					// one, and set last two to zeroes
					smi->interruptMaskReg =
							(smi->interruptMaskReg & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
		else if (tempAddress >= 0x1F801080L && tempAddress < 0x1F801100L) {
			DMAArbiter_writeByte(smi->dma, address, value);
		}
		else if (tempAddress >= 0x1F801100L && tempAddress < 0x1F801104L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							0,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							0,
							(TimerModule_readCounterValue(&smi->timerModule, 0)
							& 0xFF000000) | 
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801104L && tempAddress < 0x1F801108L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeMode(
							&smi->timerModule,
							0,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeMode(
							&smi->timerModule,
							0,
							(TimerModule_readMode(&smi->timerModule, 0, true)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801108L && tempAddress < 0x1F80110CL) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							0,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							0,
							(TimerModule_readTargetValue(&smi->timerModule, 0)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801110L && tempAddress < 0x1F801114L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							1,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							1,
							(TimerModule_readCounterValue(&smi->timerModule, 1)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801114L && tempAddress < 0x1F801118L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeMode(
							&smi->timerModule,
							1,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeMode(
							&smi->timerModule,
							1,
							(TimerModule_readMode(&smi->timerModule, 1, true)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801118L && tempAddress < 0x1F80111CL) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							1,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							1,
							(TimerModule_readTargetValue(&smi->timerModule, 1)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801120L && tempAddress < 0x1F801124L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							2,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeCounterValue(
							&smi->timerModule,
							2,
							(TimerModule_readCounterValue(&smi->timerModule, 2)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801124L && tempAddress < 0x1F801128L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeMode(
							&smi->timerModule,
							2,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeMode(
							&smi->timerModule,
							2,
							(TimerModule_readMode(&smi->timerModule, 2, true)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		}
		else if (tempAddress >= 0x1F801128L && tempAddress < 0x1F80112CL) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							2,
							(value & 0xFF) << 24
							);
					break;
				case 1:
					TimerModule_writeTargetValue(
							&smi->timerModule,
							2,
							(TimerModule_readTargetValue(&smi->timerModule, 2)
							& 0xFF000000) |
							((value & 0xFF) << 16)
							);
					break;
			}
		} // CD-ROM
		else if (tempAddress >= 0x1F801800L && tempAddress < 0x1F801804L) {
			switch ((int32_t)tempAddress & 0xF) {
				case 0:
					CDROMDrive_write1800(smi->cdrom, value);
					break;
				case 1:
					CDROMDrive_write1801(smi->cdrom, value);
					break;
				case 2:
					CDROMDrive_write1802(smi->cdrom, value);
					break;
				case 3:
					CDROMDrive_write1803(smi->cdrom, value);
					break;
			}
		}
		else if (tempAddress >= 0x1F801C00L && tempAddress < 0x1F802000L) {
			// Fake SPU write
			SPU_writeByte(smi->spu, address, value);
		}
	} // Expansion Region 2 (I/O Ports)
	else if (tempAddress >= 0x1F802000L && tempAddress < 0x1F803000L) {
		if (tempAddress == 0x1F802041L) {
			smi->biosPost = value;
		}
	} // Expansion Region 3 (Multipurpose)
	else if (tempAddress >= 0x1FA00000L && tempAddress < 0x1FC00000L) {
		// Do nothing for now
		;
	} // BIOS ROM
	else if (tempAddress >= 0x1FC00000L && tempAddress < 0x1FC80000L) {
		// Read only, do nothing
		;
	} // I/O Ports (Cache Control)
	else if (tempAddress >= 0xFFFE0000L && tempAddress < 0xFFFE0200L) {
		// Cache Control Register
		if (tempAddress >= 0xFFFE0130L && tempAddress < 0xFFFE0134L) {
			int32_t shift = (int32_t)(tempAddress & 0x3L);
			switch (shift) {
				case 0:
					// Write full 32-bits, with zeroes for remaining
					// three bytes
					smi->cacheControlReg = (value & 0xFF) << 24;
					break;
				case 1:
					// Keep first byte of cache control, merge with this
					// one, and set last two to zeroes
					smi->cacheControlReg =
							(smi->cacheControlReg & 0xFF000000) |
							((value & 0xFF) << 16);
					break;
			}
		}
	}
}

/*
 * This function writes the interrupt status.
 */
void SystemInterlink_writeInterruptStatus(SystemInterlink *smi,
		int32_t interruptStatus)
{
	smi->interruptStatusReg = interruptStatus;
}

/*
 * This writes a word to the correct area depending on the address.
 */
void SystemInterlink_writeWord(SystemInterlink *smi, int32_t address,
		int32_t word)
{
	int64_t tempAddress = address & 0xFFFFFFFC;
	address = (int32_t)tempAddress;

	// Handle RAM specially due to its frequent use
	if (tempAddress >= 0L && tempAddress < 0x200000L) {
		smi->ram[address] = (int8_t)logical_rshift(word, 24);
		smi->ram[address + 1] = (int8_t)logical_rshift(word, 16);
		smi->ram[address + 2] = (int8_t)logical_rshift(word, 8);
		smi->ram[address + 3] = (int8_t)word;
	} // Everything else
	else {
		switch (address) {
			case 0x1F801070:
				// Mask interrupt bits
				smi->interruptStatusReg &= word;
				break;
			case 0x1F801074:
				smi->interruptMaskReg = word;
				break;
			case 0xFFFE0130:
				smi->cacheControlReg = word;
				break;
			case 0x1F801080:
			case 0x1F801081:
			case 0x1F801082:
			case 0x1F801083:
			case 0x1F801084:
			case 0x1F801085:
			case 0x1F801086:
			case 0x1F801087:
			case 0x1F801088:
			case 0x1F801089:
			case 0x1F80108A:
			case 0x1F80108B:
			case 0x1F80108C:
			case 0x1F80108D:
			case 0x1F80108E:
			case 0x1F80108F:
			case 0x1F801090:
			case 0x1F801091:
			case 0x1F801092:
			case 0x1F801093:
			case 0x1F801094:
			case 0x1F801095:
			case 0x1F801096:
			case 0x1F801097:
			case 0x1F801098:
			case 0x1F801099:
			case 0x1F80109A:
			case 0x1F80109B:
			case 0x1F80109C:
			case 0x1F80109D:
			case 0x1F80109E:
			case 0x1F80109F:
			case 0x1F8010A0:
			case 0x1F8010A1:
			case 0x1F8010A2:
			case 0x1F8010A3:
			case 0x1F8010A4:
			case 0x1F8010A5:
			case 0x1F8010A6:
			case 0x1F8010A7:
			case 0x1F8010A8:
			case 0x1F8010A9:
			case 0x1F8010AA:
			case 0x1F8010AB:
			case 0x1F8010AC:
			case 0x1F8010AD:
			case 0x1F8010AE:
			case 0x1F8010AF:
			case 0x1F8010B0:
			case 0x1F8010B1:
			case 0x1F8010B2:
			case 0x1F8010B3:
			case 0x1F8010B4:
			case 0x1F8010B5:
			case 0x1F8010B6:
			case 0x1F8010B7:
			case 0x1F8010B8:
			case 0x1F8010B9:
			case 0x1F8010BA:
			case 0x1F8010BB:
			case 0x1F8010BC:
			case 0x1F8010BD:
			case 0x1F8010BE:
			case 0x1F8010BF:
			case 0x1F8010C0:
			case 0x1F8010C1:
			case 0x1F8010C2:
			case 0x1F8010C3:
			case 0x1F8010C4:
			case 0x1F8010C5:
			case 0x1F8010C6:
			case 0x1F8010C7:
			case 0x1F8010C8:
			case 0x1F8010C9:
			case 0x1F8010CA:
			case 0x1F8010CB:
			case 0x1F8010CC:
			case 0x1F8010CD:
			case 0x1F8010CE:
			case 0x1F8010CF:
			case 0x1F8010D0:
			case 0x1F8010D1:
			case 0x1F8010D2:
			case 0x1F8010D3:
			case 0x1F8010D4:
			case 0x1F8010D5:
			case 0x1F8010D6:
			case 0x1F8010D7:
			case 0x1F8010D8:
			case 0x1F8010D9:
			case 0x1F8010DA:
			case 0x1F8010DB:
			case 0x1F8010DC:
			case 0x1F8010DD:
			case 0x1F8010DE:
			case 0x1F8010DF:
			case 0x1F8010E0:
			case 0x1F8010E1:
			case 0x1F8010E2:
			case 0x1F8010E3:
			case 0x1F8010E4:
			case 0x1F8010E5:
			case 0x1F8010E6:
			case 0x1F8010E7:
			case 0x1F8010E8:
			case 0x1F8010E9:
			case 0x1F8010EA:
			case 0x1F8010EB:
			case 0x1F8010EC:
			case 0x1F8010ED:
			case 0x1F8010EE:
			case 0x1F8010EF:
			case 0x1F8010F0:
			case 0x1F8010F1:
			case 0x1F8010F2:
			case 0x1F8010F3:
			case 0x1F8010F4:
			case 0x1F8010F5:
			case 0x1F8010F6:
			case 0x1F8010F7:
			case 0x1F8010F8:
			case 0x1F8010F9:
			case 0x1F8010FA:
			case 0x1F8010FB:
			case 0x1F8010FC:
			case 0x1F8010FD:
			case 0x1F8010FE:
			case 0x1F8010FF:
				DMAArbiter_writeWord(smi->dma, address, word);
				break;
			case 0x1F801100:
				TimerModule_writeCounterValue(&smi->timerModule, 0, word);
				break;
			case 0x1F801104:
				TimerModule_writeMode(&smi->timerModule, 0, word);
				break;
			case 0x1F801108:
				TimerModule_writeTargetValue(&smi->timerModule, 0, word);
				break;
			case 0x1F801110:
				TimerModule_writeCounterValue(&smi->timerModule, 1, word);
				break;
			case 0x1F801114:
				TimerModule_writeMode(&smi->timerModule, 1, word);
				break;
			case 0x1F801118:
				TimerModule_writeTargetValue(&smi->timerModule, 1, word);
				break;
			case 0x1F801120:
				TimerModule_writeCounterValue(&smi->timerModule, 2, word);
				break;
			case 0x1F801124:
				TimerModule_writeMode(&smi->timerModule, 2, word);
				break;
			case 0x1F801128:
				TimerModule_writeTargetValue(&smi->timerModule, 2, word);
				break;
			case 0x1F801000:
				smi->expansion1BaseAddress = word;
				break;
			case 0x1F801004:
				smi->expansion2BaseAddress = word;
				break;
			case 0x1F801008:
				smi->expansion1DelaySize = word;
				break;
			case 0x1F80100C:
				smi->expansion3DelaySize = word;
				break;
			case 0x1F801010:
				smi->biosRomDelaySize = word;
				break;
			case 0x1F801014:
				smi->spuDelaySize = word;
				break;
			case 0x1F801018:
				smi->cdromDelaySize = word;
				break;
			case 0x1F80101C:
				smi->expansion2DelaySize = word;
				break;
			case 0x1F801020:
				smi->commonDelay = word;
				break;
			case 0x1F801060:
				smi->ramSize = word;
				break;
			case 0x1F801810:
				GPU_submitToGP0(smi->gpu, word);
				break;
			case 0x1F801814:
				GPU_submitToGP1(smi->gpu, word);
				break;
			case 0x1F801800:
			case 0x1F801801:
			case 0x1F801802:
			case 0x1F801803:
				// Do nothing as we don't allow word writes to CDROM range
				break;
			default:
				// Use writeByte to write four bytes
				SystemInterlink_writeByte(
						smi,
						(int32_t)tempAddress,
						(int8_t)logical_rshift(word, 24)
						);
				++tempAddress;
				SystemInterlink_writeByte(
						smi,
						(int32_t)tempAddress,
						(int8_t)logical_rshift(word, 16)
						);
				++tempAddress;
				SystemInterlink_writeByte(
						smi,
						(int32_t)tempAddress,
						(int8_t)logical_rshift(word, 8)
						);
				++tempAddress;
				SystemInterlink_writeByte(
						smi,
						(int32_t)tempAddress,
						(int8_t)word
						);
				break;
		}
	}
}

/*
 * This function verifies the file at biosPath conforms to requirements, and
 * then copies it to the 512 KB array referenced by the biosMemory pointer.
 */
static bool SystemInterlink_loadBiosFileToMemory(const char *biosPath,
		int8_t *biosMemory)
{
	// Define return value
	bool retVal = false;
	
	// Check path isn't empty
	if (strlen(biosPath) == 0) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Provided BIOS file "
				"path was empty\n");
		goto end;
	}
	
	// Display path
	fprintf(stdout, "PhilPSX: SystemInterlink: BIOS file path "
			"is: %s\n", biosPath);
	
	// Open BIOS file
	int biosFileDescriptor = open(biosPath, O_RDONLY);
	if (biosFileDescriptor == -1) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't open BIOS file\n");
		goto end;
	}
	
	// Get BIOS file size and check it is 512 KB exactly
	ssize_t signedBiosSize = lseek(biosFileDescriptor, 0, SEEK_END);
	if (signedBiosSize == -1) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't seek to end of "
				"BIOS file\n");
		goto cleanup_openfile;
	}
	if (signedBiosSize != 524288) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Size of BIOS file is not "
				"512 KB\n");
		goto cleanup_openfile;
	}
	if (lseek(biosFileDescriptor, 0, SEEK_SET) == -1) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't seek to beginning "
				"of BIOS file\n");
		goto cleanup_openfile;
	}
	
	// Read BIOS file into biosMemory array
	if (read(biosFileDescriptor, biosMemory, 524288) != 524288) {
		fprintf(stderr, "PhilPSX: SystemInterlink: Couldn't copy BIOS file to "
				"biosMemory array\n");
		goto cleanup_openfile;
	}
	
	// Set return value to success
	retVal = true;
	
	// Cleanup begins here:
	cleanup_openfile:
	if (close(biosFileDescriptor)) {
		fprintf(stderr, "PhilPSX: CD: Couldn't close bin file during "
				"error path\n");
	}
	
	end:
	return retVal;
}

/*
 * This tells the timer to add some cycles to the count it needs to sync by.
 */
static void TimerModule_appendSyncCycles(TimerModule *timerModule,
		int32_t cycles)
{
	for (int32_t i = 0; i < 3; ++i)
		timerModule->cpuCyclesToSync[i] += cycles;
}

/*
 * Read from the specified timer's counter value register.
 */
static int32_t TimerModule_readCounterValue(TimerModule *timerModule,
		int32_t timer)
{
	// Catch up to current cycle count
	TimerModule_resync(timerModule);

	// If in pulse mode, set bit 10 back to 1 now
	if ((timerModule->timerMode[timer] & 0x80) == 0) {
		timerModule->timerMode[timer] |= 0x400;
	}

	return TimerModule_swapEndianness(timerModule->timerCounterValue[timer]);
}

/*
 * Read from the specified timer's mode register.
 */
static int32_t TimerModule_readMode(TimerModule *timerModule,
		int32_t timer, bool override)
{
	// Catch up to current cycle count
	TimerModule_resync(timerModule);

	// If in pulse mode, set bit 10 back to 1 now
	if ((timerModule->timerMode[timer] & 0x80) == 0 && !override) {
		timerModule->timerMode[timer] |= 0x400;
	}

	// Store mode register in temporary variable
	int32_t retVal = timerModule->timerMode[timer];

	// Reset bits 11 and 12 if not overriden, as these relate to
	// reaching certain values
	if (!override) {
		timerModule->timerMode[timer] &= 0xFFFFE7FF;
	}

	return TimerModule_swapEndianness(retVal);
}

/*
 * Read from the specified timer's target value register.
 */
static int32_t TimerModule_readTargetValue(TimerModule *timerModule,
		int32_t timer)
{
	// If in pulse mode, set bit 10 back to 1 now
	if ((timerModule->timerMode[timer] & 0x80) == 0) {
		timerModule->timerMode[timer] |= 0x400;
	}

	return TimerModule_swapEndianness(timerModule->timerTargetValue[timer]);
}

/*
 * Resync all timers to the current point.
 */
static void TimerModule_resync(TimerModule *timerModule)
{
	// Get HBlank and VBlank status
	bool hblank = GPU_isInHblank(timerModule->smi->gpu);
	bool vblank = GPU_isInVblank(timerModule->smi->gpu);

	if (hblank) {
		timerModule->hblankHappened[0] = true;
		timerModule->hblankHappened[1] = true;
		timerModule->hblankHappened[2] = true;
	}
	if (vblank) {
		timerModule->vblankHappened[0] = true;
		timerModule->vblankHappened[1] = true;
		timerModule->vblankHappened[2] = true;
	}

	// Convert sync figure to GPU cycles in case needed
	for (int32_t i = 0; i < 3; ++i) {
		timerModule->gpuCyclesToSync[i] +=
				timerModule->cpuCyclesToSync[i] / 7.0f * 11.0f;

		// Add in top up values
		timerModule->cpuCyclesToSync[i] += timerModule->cpuTopup[i];
		timerModule->gpuCyclesToSync[i] += timerModule->gpuTopup[i];
	}

	// Get clock source for all three timers
	timerModule->clockSource[0] =
			logical_rshift(timerModule->timerMode[0], 8) & 0x3;
	timerModule->clockSource[1] =
			logical_rshift(timerModule->timerMode[1], 8) & 0x3;
	timerModule->clockSource[2] =
			logical_rshift(timerModule->timerMode[2], 8) & 0x3;

	// Calculate future topup values
	timerModule->cpuTopup[0] = 0;
	timerModule->gpuTopup[0] = (timerModule->clockSource[0] == 0 ||
			timerModule->clockSource[0] == 2) ?
			0 : GPU_howManyDotclockGpuCyclesLeft(timerModule->smi->gpu,
			timerModule->gpuCyclesToSync[0]);
	timerModule->cpuTopup[1] = 0;
	timerModule->gpuTopup[1] = (timerModule->clockSource[1] == 0 ||
			timerModule->clockSource[1] == 2) ?
			0 : GPU_howManyHblankGpuCyclesLeft(timerModule->smi->gpu,
			timerModule->gpuCyclesToSync[1]);
	timerModule->cpuTopup[2] = (timerModule->clockSource[2] < 2) ?
			0 : timerModule->cpuCyclesToSync[2] % 8;
	timerModule->gpuTopup[2] = 0;

	// Get increment count for all three timers
	timerModule->incrementBy[0] = (timerModule->clockSource[0] == 0 ||
			timerModule->clockSource[0] == 2) ?
			timerModule->cpuCyclesToSync[0] :
			GPU_howManyDotclockIncrements(timerModule->smi->gpu,
			timerModule->gpuCyclesToSync[0]);
	timerModule->incrementBy[1] = (timerModule->clockSource[1] == 0 ||
			timerModule->clockSource[1] == 2) ?
			timerModule->cpuCyclesToSync[1] :
			GPU_howManyHblankIncrements(timerModule->smi->gpu,
			timerModule->gpuCyclesToSync[1]);
	timerModule->incrementBy[2] = (timerModule->clockSource[2] < 2) ?
			timerModule->cpuCyclesToSync[2] :
			timerModule->cpuCyclesToSync[2] / 8;

	// Do increment to temporary value
	timerModule->newValue[0] =
			timerModule->timerCounterValue[0] + timerModule->incrementBy[0];
	timerModule->newValue[1] =
			timerModule->timerCounterValue[1] + timerModule->incrementBy[1];
	timerModule->newValue[2] =
			timerModule->timerCounterValue[2] + timerModule->incrementBy[2];

	// Adjust as required by synchronisation mode
	// Timer 0
	switch (timerModule->timerMode[0] & 0x1) {
		case 1:
			switch (logical_rshift(timerModule->timerMode[0], 1) & 0x3) {
				case 0:
					if (hblank)
						timerModule->newValue[0] =
								timerModule->timerCounterValue[0];
					break;
				case 1:
					if (hblank)
						timerModule->newValue[0] = 0;
					break;
				case 2:
					if (hblank)
						timerModule->newValue[0] = 0;
					else
						timerModule->newValue[0] =
								timerModule->timerCounterValue[0];
					break;
				case 3:
					if (!timerModule->hblankHappened[0])
						timerModule->newValue[0] =
								timerModule->timerCounterValue[0];
					break;
			}
			break;
	}

	// Timer 1
	switch (timerModule->timerMode[1] & 0x1) {
		case 1:
			switch (logical_rshift(timerModule->timerMode[1], 1) & 0x3) {
				case 0:
					if (vblank)
						timerModule->newValue[1] =
								timerModule->timerCounterValue[1];
					break;
				case 1:
					if (vblank)
						timerModule->newValue[1] = 0;
					break;
				case 2:
					if (vblank)
						timerModule->newValue[1] = 0;
					else
						timerModule->newValue[1] =
								timerModule->timerCounterValue[1];
					break;
				case 3:
					if (!timerModule->vblankHappened[1])
						timerModule->newValue[1] =
								timerModule->timerCounterValue[1];
					break;
			}
			break;
	}

	// Timer 2
	switch (timerModule->timerMode[2] & 0x1) {
		case 1:
			switch (logical_rshift(timerModule->timerMode[2], 1) & 0x3) {
				case 0:
				case 3:
					timerModule->newValue[2] =
							timerModule->timerCounterValue[2];
					break;
			}
			break;
	}

	timerModule->timerCounterValue[0] = timerModule->newValue[0];
	timerModule->timerCounterValue[1] = timerModule->newValue[1];
	timerModule->timerCounterValue[2] = timerModule->newValue[2];

	// Deal with interrupts and target values
	for (int32_t i = 0; i < 3; ++i) {
		// Wipe cycles
		timerModule->cpuCyclesToSync[i] = 0;
		timerModule->gpuCyclesToSync[i] = 0;

		// Check for interrupts
		bool intFlag = false;
		if (timerModule->timerCounterValue[i] >=
				timerModule->timerTargetValue[i]) {
			timerModule->timerMode[i] |= 0x800;
			if ((timerModule->timerMode[i] & 0x10) == 0x10) {
				intFlag = true;
			}
		}
		if (timerModule->timerCounterValue[i] >= 0xFFFF) {
			timerModule->timerMode[i] |= 0x1000;
			if ((timerModule->timerMode[i] & 0x20) == 0x20) {
				intFlag = true;
			}
		}
		if (intFlag)
			TimerModule_triggerTimerInterrupt(timerModule, i);

		// Reset values here if needed
		if (timerModule->timerCounterValue[i] > 0xFFFF &&
				(timerModule->timerMode[i] & 0x8) == 0) {
			timerModule->timerCounterValue[i] = 0;
		} else if (timerModule->timerCounterValue[i] >
				timerModule->timerTargetValue[i] &&
				(timerModule->timerMode[i] & 0x8) == 0x8) {
			timerModule->timerCounterValue[i] = 0;
		}
	}
}

/*
 * This swaps the endianness of an incoming or outgoing word.
 */
static int32_t TimerModule_swapEndianness(int32_t word)
{
	return word << 24 | (word & 0xFF00) << 8 |
			logical_rshift(word & 0xFF0000, 8) | logical_rshift(word, 24);
}

/*
 * Handle interrupt logic.
 */
static void TimerModule_triggerTimerInterrupt(TimerModule *timerModule,
		int32_t timer)
{
	// One-shot mode
	if ((timerModule->timerMode[timer] & 0x40) == 0) {
		// If this is first IRQ
		if (!timerModule->interruptHappenedOnceOrMore[timer]) {
			// Check for pulse/toggle mode
			if ((timerModule->timerMode[timer] & 0x80) == 0) {
				// Just set bit 10 to 0 and be done with it,
				// triggering IRQ as well
				timerModule->timerMode[timer] &= 0xFFFFFBFF;
				timerModule->smi->timersInterruptDelay[timer] = 0;
				timerModule->smi->timersInterruptCounter[timer] = 0;
				timerModule->interruptHappenedOnceOrMore[timer] = true;
			} else {
				// Invert flag, triggering IRQ if it is then 0
				if ((timerModule->timerMode[timer] & 0x400) == 0x400) {
					// Flip to 0 and trigger interrupt
					timerModule->timerMode[timer] &= 0xFFFFFBFF;
					timerModule->smi->timersInterruptDelay[timer] = 0;
					timerModule->smi->timersInterruptCounter[timer] = 0;
					timerModule->interruptHappenedOnceOrMore[timer] = true;
				} else {
					// Flip back to 1 and do nothing
					timerModule->timerMode[timer] |= 0x400;
				}
			}
		}
	} else { // Repeated mode - don't check for one-shot flag
		// Check for pulse/toggle mode
		if ((timerModule->timerMode[timer] & 0x80) == 0) {
			// Just set bit 10 to 0 and be done with it,
			// triggering IRQ as well
			timerModule->timerMode[timer] &= 0xFFFFFBFF;
			timerModule->smi->timersInterruptDelay[timer] = 0;
			timerModule->smi->timersInterruptCounter[timer] = 0;
		} else {
			// Invert flag, triggering IRQ if it is then 0
			if ((timerModule->timerMode[timer] & 0x400) == 0x400) {
				// Flip to 0 and trigger interrupt
				timerModule->timerMode[timer] &= 0xFFFFFBFF;
				timerModule->smi->timersInterruptDelay[timer] = 0;
				timerModule->smi->timersInterruptCounter[timer] = 0;
			} else {
				// Flip back to 1 and do nothing
				timerModule->timerMode[timer] |= 0x400;
			}
		}
	}
}

/*
 * Write to the specified timer's counter value register.
 */
static void TimerModule_writeCounterValue(TimerModule *timerModule,
		int32_t timer, int32_t value)
{
	TimerModule_resync(timerModule);
	value = TimerModule_swapEndianness(value);
	timerModule->timerCounterValue[timer] = 0xFFFF & value;
}

/*
 * Write to the specified timer's mode register.
 */
static void TimerModule_writeMode(TimerModule *timerModule,
		int32_t timer, int32_t value)
{
	TimerModule_resync(timerModule);
	value = TimerModule_swapEndianness(value);

	// Set bit 10 to turn off interrupt request
	value |= 0x400;

	// Set bits 13-15 to 0
	value &= 0xFFFF1FFF;

	// Reset hblank and vblank happened markers
	timerModule->hblankHappened[timer] = false;
	timerModule->vblankHappened[timer] = false;

	// Reset one-shot marker
	timerModule->interruptHappenedOnceOrMore[timer] = false;

	timerModule->timerMode[timer] = value;

	// Reset counter value
	timerModule->timerCounterValue[timer] = 0;
}

/*
 * Write to the specified timer's target value register.
 */
static void TimerModule_writeTargetValue(TimerModule *timerModule,
		int32_t timer, int32_t value)
{
	TimerModule_resync(timerModule);
	value = TimerModule_swapEndianness(value);
	timerModule->timerTargetValue[timer] = 0xFFFF & value;
}