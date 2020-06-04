/*
 * This C file models the instruction cache of the R3051 processor as a class.
 * 
 * InstructionCache.c - Copyright Phillip Potter, 2020, under GPLv3
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/InstructionCache_all.h"
#include "../headers/Cop0_public.h"
#include "../headers/math_utils.h"

/*
 * This constructs an instruction cache object, using the pre-allocated struct
 * referenced by cache. It returns cache in the event of success, and NULL
 * in the event of failure.
 */
InstructionCache *construct_InstructionCache(InstructionCache *cache)
{
	// Setup cache arrays
	cache->cacheData = calloc(4096, sizeof(int8_t));
	if (!cache->cacheData) {
		fprintf(stderr, "PhilPSX: R3051: InstructionCache: Couldn't allocate "
				"memory for cacheData array\n");
		goto end;
	}

	cache->cacheTag = calloc(256, sizeof(int32_t));
	if (!cache->cacheTag) {
		fprintf(stderr, "PhilPSX: R3051: InstructionCache: Couldn't allocate "
				"memory for cacheTag array\n");
		goto cleanup_cachedata;
	}

	cache->cacheValid = calloc(256, sizeof(bool));
	if (!cache->cacheValid) {
		fprintf(stderr, "PhilPSX: R3051: InstructionCache: Couldn't allocate "
				"memory for cacheValid array\n");
		goto cleanup_cachetag;
	}

	// Normal return:
	return cache;

	// Cleanup path:
	cleanup_cachetag:
	free(cache->cacheTag);
	
	cleanup_cachedata:
	free(cache->cacheData);
	
	end:
	return NULL;
}

/*
 * This destructs an instruction cache object, but leaves the referenced struct
 * itself allocated.
 */
void destruct_InstructionCache(InstructionCache *cache)
{
	free(cache->cacheValid);
	free(cache->cacheTag);
	free(cache->cacheData);
}

/*
 * This function checks for a cache hit. The address provided must be
 * physical and not virtual.
 */
bool InstructionCache_checkForHit(InstructionCache *cache,
		int32_t address)
{
	// Define return value
	bool retVal = false;

	int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
	int32_t expectedTag = logical_rshift(address, 12) & 0xFFFFF;
	if (cache->cacheTag[tagIndex] ==
			expectedTag && cache->cacheValid[tagIndex])
		retVal = true;

	// Return hit status
	return retVal;
}

/*
 * This function retrieves the word from the correct address.
 */
int32_t InstructionCache_readWord(InstructionCache *cache,
		int32_t address)
{
	// Define word value
	int32_t wordVal = 0;

	int32_t dataIndex = address & 0xFFC;
	wordVal = (cache->cacheData[dataIndex] & 0xFF) << 24;
	++dataIndex;
	wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 16;
	++dataIndex;
	wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 8;
	++dataIndex;
	wordVal |= cache->cacheData[dataIndex] & 0xFF;
	
	// Return word value
	return wordVal;
}

/*
 * This function retrieves a byte from the correct address.
 */
int8_t InstructionCache_readByte(InstructionCache *cache,
		int32_t address)
{
	// Define byte value
	int8_t byteVal = 0;

	// Get address of byte
	int32_t dataIndex = address & 0xFFF;

	// Retrieve byte
	byteVal = cache->cacheData[dataIndex];

	// Return work value
	return byteVal;
}

/*
 * This function writes the word to the correct address.
 */
void InstructionCache_writeWord(InstructionCache *cache, Cop0 *sccp,
		int32_t address, int32_t value)
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
}

/*
 * This function writes the byte to the correct address, and invalidates the
 * correct cache line.
 */
void InstructionCache_writeByte(InstructionCache *cache, Cop0 *sccp,
		int32_t address, int8_t value)
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
}

/*
 * This function refills a cache line using an address.
 */
void InstructionCache_refillLine(InstructionCache *cache, Cop0 *sccp,
		SystemInterlink *system, int32_t address)
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
				SystemInterlink_readByte(system,
				(int32_t)startingAddress);
		++startingAddress;
	}
}