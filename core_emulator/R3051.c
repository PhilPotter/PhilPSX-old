/*
 * This C file models the MIPS R3051 processor of the PlayStation as a class.
 * 
 * R3051.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/R3051.h"
#include "../headers/Components.h"
#include "../headers/Cop0.h"
#include "../headers/Cop2.h"
#include "../headers/BusInterfaceUnit.h"
#include "../headers/SystemInterlink.h"
#include "../headers/math_utils.h"

// Data widths for read and write methods
#define PHILPSX_R3051_BYTE 8
#define PHILPSX_R3051_HALFWORD 16
#define PHILPSX_R3051_WORD 32

// List of exception reasons
#define PHILPSX_EXCEPTION_INT 0
#define PHILPSX_EXCEPTION_ADEL 4
#define PHILPSX_EXCEPTION_ADES 5
#define PHILPSX_EXCEPTION_IBE 6
#define PHILPSX_EXCEPTION_DBE 7
#define PHILPSX_EXCEPTION_SYS 8
#define PHILPSX_EXCEPTION_BP 9
#define PHILPSX_EXCEPTION_RI 10
#define PHILPSX_EXCEPTION_CPU 11
#define PHILPSX_EXCEPTION_OVF 12
#define PHILPSX_EXCEPTION_RESET 13
#define PHILPSX_EXCEPTION_NULL 14

// Cache types
#define PHILPSX_INSTRUCTION_CACHE 0

// Forward declarations for functions and subcomponents
// MIPSException-related stuff:
typedef struct MIPSException MIPSException;
static MIPSException *construct_MIPSException(void);
static void destruct_MIPSException(MIPSException *exception);
static void MIPSException_reset(MIPSException *exception);

// Cache-related stuff:
typedef struct Cache Cache;
static Cache *construct_Cache(int32_t cacheType);
static void destruct_Cache(Cache *cache);
static bool Cache_checkForHit(Cache *cache, int32_t address);
static int32_t Cache_readWord(Cache *cache, int32_t address);
static int8_t Cache_readByte(Cache *cache, int32_t address);
static void Cache_writeWord(Cache *cache, Cop0 *sccp, int32_t address, int32_t value);
static void Cache_writeByte(Cache *cache, Cop0 *sccp, int32_t address, int8_t value);
static void Cache_refillLine(Cache *cache, Cop0 *sccp, SystemInterlink *system, int32_t address);

/*
 * This struct contains registers, and pointers to subcomponents.
 */
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

/*
 * This inner struct models a processor exception, which can occur during
 * any stage of the pipeline.
 */
struct MIPSException {
	
	// Exception variables
	int32_t exceptionReason;
	int32_t programCounterOrigin;
	int32_t badAddress;
	int32_t coProcessorNum;
	bool isInBranchDelaySlot;
};

/*
 * This inner struct allows the R3051 to use instruction caching.
 */
struct Cache {
	
	// Cache variables
	int8_t *cacheData;
	int32_t *cacheTag;
	bool *cacheValid;
	int32_t cacheType;
};

/*
 * This constructs and returns a new R3051 object.
 */
R3051 *construct_R3051(void)
{
	// Allocate R3051 struct
	R3051 *cpu = malloc(sizeof(R3051));
	if (cpu == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for R3051 "
				"struct \n");
		goto end;
	}

	// Set component ID
	cpu->componentId = PHILPSX_COMPONENTS_CPU;

	// Setup instruction cycle count
	cpu->cycles = 0;

	// Setup registers
	cpu->generalRegisters = calloc(32, sizeof(int32_t));
	if (cpu->generalRegisters == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for general "
				"registers");
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
	cpu->sccp = construct_Cop0();
	if (cpu->sccp == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct Cop0");
		goto cleanup_registers;
	}
	cpu->gte = construct_Cop2();
	if (cpu->gte == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct Cop2");
		goto cleanup_cop0;
	}

	// Setup bus interface unit
	cpu->biu = construct_BusInterfaceUnit(cpu);
	if (cpu->biu == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct BusInterfaceUnit");
		goto cleanup_cop2;
	}

	// Setup empty exception
	cpu->exception = construct_MIPSException();
	if (cpu->exception == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct MIPSException");
		goto cleanup_biu;
	}

	// Setup instruction cache
	cpu->instructionCache = construct_Cache(PHILPSX_INSTRUCTION_CACHE);
	if (cpu->exception == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct instruction cache");
		goto cleanup_exception;
	}

	// Setup the branch marker
	cpu->prevWasBranch = false;
	cpu->isBranch = false;

	// Reset main chip and Cop0
	reset();
	sccp.reset();

	// Normal return:
	return cpu;

	// Cleanup path:
	cleanup_exception:
	destruct_MIPSException(cpu->exception);
	
	cleanup_biu:
	destruct_BusInterfaceUnit(cpu->biu);

	cleanup_cop2:
	destruct_Cop2(cpu->gte);

	cleanup_cop0:
	destruct_Cop0(cpu->sccp);

	cleanup_registers:
	free(cpu->generalRegisters);

	cleanup_r3051:
	free(cpu);
	cpu = NULL;

	end:
	return cpu;
}

/*
 * This destructs an R3051 object.
 */
void destruct_R3051(R3051 *cpu)
{
	destruct_Cache(cpu->instructionCache);
	destruct_MIPSException(cpu->exception);
	destruct_BusInterfaceUnit(cpu->biu);
	destruct_Cop2(cpu->gte);
	destruct_Cop0(cpu->sccp);
	free(cpu->generalRegisters);
	free(cpu);
}

/*
 * This constructs a MIPSException object.
 */
static MIPSException *construct_MIPSException(void)
{
	// Allocate MIPSException struct
	MIPSException *exception = calloc(1, sizeof(MIPSException));
	if (exception == NULL) {
		fprintf(stderr, "PhilPSX: R3051: MIPSException: Couldn't allocate "
				"memory for R3051 struct \n");
		goto end;
	}

	// Set default exception reason
	exception->exceptionReason = PHILPSX_EXCEPTION_NULL;

	end:
	return exception;
}

/*
 * This destructs a MIPSException object.
 */
static void destruct_MIPSException(MIPSException *exception)
{
	free(exception);
}

/*
 * This function resets an exception to its initial empty state.
 */
static void MIPSException_reset(MIPSException *exception)
{
	exception->exceptionReason = PHILPSX_EXCEPTION_NULL;
	exception->programCounterOrigin = 0;
	exception->badAddress = 0;
	exception->coProcessorNum = 0;
	exception->isInBranchDelaySlot = false;
}

/*
 * This constructs an instruction cache object.
 */
static Cache *construct_Cache(int32_t cacheType)
{
	// Allocate Cache struct
	Cache *cache = malloc(sizeof(Cache));
	if (cache == NULL) {
		fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
				"memory for Cache struct \n");
		goto end;
	}

	// Setup appropriate cache type
	switch (cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			cache->cacheData = calloc(4096, sizeof(int8_t));
			if (cache->cacheData == NULL) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheData array \n");
				goto cleanup_cache;
			}
			
			cache->cacheTag = calloc(256, sizeof(int32_t));
			if (cache->cacheTag == NULL) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheTag array \n");
				goto cleanup_cachedata;
			}
			
			cache->cacheValid = calloc(256, sizeof(bool));
			if (cache->cacheValid == NULL) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheValid array \n");
				goto cleanup_cachetag;
			}
			
			cache->cacheType = cacheType;
		}
		break;
	}

	// Normal return:
	return cache;

	// Cleanup path:
	cleanup_cachetag:
	free(cache->cacheTag);
	
	cleanup_cachedata:
	free(cache->cacheData);

	cleanup_cache:
	free(cache);
	cache = NULL;
	
	end:
	return cache;
}

/*
 * This destructs an instruction cache object.
 */
static void destruct_Cache(Cache *cache)
{
	free(cache->cacheValid);
	free(cache->cacheTag);
	free(cache->cacheData);
	free(cache);
}

/*
 * This function checks for a cache hit. The address provided must be
 * physical and not virtual.
 */
static bool Cache_checkForHit(Cache *cache, int32_t address)
{
	// Define return value
	bool retVal = false;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
			int32_t expectedTag = logical_rshift(address, 12) & 0xFFFFF;
			if (cache->cacheTag[tagIndex] == expectedTag && cache->cacheValid[tagIndex])
				retVal = true;
		} break;
	}

	// Return hit status
	return retVal;
}

/*
 * This function retrieves the word from the correct address.
 */
static int32_t Cache_readWord(Cache *cache, int32_t address)
{
	// Define word value
	int32_t wordVal = 0;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			int32_t dataIndex = address & 0xFFC;
			wordVal = (cache->cacheData[dataIndex] & 0xFF) << 24;
			++dataIndex;
			wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 16;
			++dataIndex;
			wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 8;
			++dataIndex;
			wordVal |= cache->cacheData[dataIndex] & 0xFF;
		} break;
	}

	// Return word value
	return wordVal;
}

/*
 * This function retrieves a byte from the correct address.
 */
static int8_t Cache_readByte(Cache *cache, int32_t address)
{
	// Define byte value
	int8_t byteVal = 0;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Get address of byte
			int32_t dataIndex = address & 0xFFF;

			// Retrieve byte
			byteVal = cache->cacheData[dataIndex];
		} break;
	}

	// Return work value
	return byteVal;
}

/*
 * This function writes the word to the correct address.
 */
static void Cache_writeWord(Cache *cache, Cop0 *sccp, int32_t address, int32_t value)
{
	// Act depending on type of cache
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Update correct word
			int32_t dataIndex = address & 0xFFC;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 24);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 16);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 8);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)value;

			// Invalidate line if cache is isolated
			if (Cop0_isDataCacheIsolated(sccp)) {
				int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
				int32_t tag = logical_rshift(address, 12) & 0xFFFFF;
				cache->cacheTag[tagIndex] = tag;
				cache->cacheValid[tagIndex] = false;
			}
		} break;
	}
}

/*
 * This function writes the byte to the correct address, and invalidates the
 * correct cache line.
 */
static void Cache_writeByte(Cache *cache, Cop0 *sccp, int32_t address, int8_t value)
{
	// Act depending on type of cache
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Update correct byte
			int32_t dataIndex = address & 0xFFF;

			// Write byte and invalidate cache line
			cache->cacheData[dataIndex] = value;

			// Invalidate line if cache is isolated
			if (Cop0_isDataCacheIsolated(sccp)) {
				int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
				int32_t tag = logical_rshift(address, 12) & 0xFFFFF;
				cache->cacheTag[tagIndex] = tag;
				cache->cacheValid[tagIndex] = false;
			}
		} break;
	}
}

/*
 * This function refills a cache line using an address.
 */
static void Cache_refillLine(Cache *cache, Cop0 *sccp, SystemInterlink *system, int32_t address)
{
	// Act according to cache type
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Refill a line - four words
			// Check if cache is isolated first
			if (Cop0_isDataCacheIsolated(sccp))
				return;

			// Calculate tag index and tag
			int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
			int32_t tag = logical_rshift(address, 12) & 0xFFFFF;

			// Write tag and valid flag
			cache->cacheTag[tagIndex] = tag;
			cache->cacheValid[tagIndex] = true;

			// Refill cache line
			int64_t startingAddress = address & 0xFFFFFFF0L;
			for (int32_t i = 0; i < 16; ++i) {
				cache->cacheData[(int32_t)(startingAddress & 0xFFF)] =
						SystemInterlink_readByte(system, (int32_t)startingAddress);
				++startingAddress;
			}
		} break;
	}
}