/*
 * This header file provides the public API for the system interlink class.
 * 
 * Components.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_SYSTEM_INTERLINK_HEADER
#define PHILPSX_SYSTEM_INTERLINK_HEADER

// System includes
#include <stdint.h>
#include <stdbool.h>

// Typedefs
typedef struct SystemInterlink SystemInterlink;

// Includes
#include "CDROMDrive.h"
#include "ControllerIO.h"
#include "R3051.h"
#include "DMAArbiter.h"
#include "GPU.h"
#include "SPU.h"

// Public functions
SystemInterlink *construct_SystemInterlink(const char *biosPath);
void destruct_SystemInterlink(SystemInterlink *smi);
void SystemInterlink_appendSyncCycles(SystemInterlink *smi, int32_t cycles);
void SystemInterlink_executeGPUCycles(SystemInterlink *smi);
CDROMDrive *SystemInterlink_getCdrom(SystemInterlink *smi);
ControllerIO *SystemInterlink_getControllerIO(SystemInterlink *smi);
R3051 *SystemInterlink_getCpu(SystemInterlink *smi);
DMAArbiter *SystemInterlink_getDma(SystemInterlink *smi);
GPU *SystemInterlink_getGpu(SystemInterlink *smi);
int8_t *SystemInterlink_getRamArray(SystemInterlink *smi);
SPU *SystemInterlink_getSpu(SystemInterlink *smi);
int32_t SystemInterlink_howManyStallCycles(SystemInterlink *smi,
		int32_t address);
void SystemInterlink_incrementInterruptCounters(SystemInterlink *smi);
bool SystemInterlink_instructionCacheEnabled(SystemInterlink *smi);
bool SystemInterlink_okToIncrement(SystemInterlink *smi, int64_t address);
int8_t SystemInterlink_readByte(SystemInterlink *smi, int32_t address);
int32_t SystemInterlink_readInterruptStatus(SystemInterlink *smi);
int32_t SystemInterlink_readWord(SystemInterlink *smi, int32_t address);
void SystemInterlink_resyncAllTimers(SystemInterlink *smi);
bool SystemInterlink_scratchpadEnabled(SystemInterlink *smi);
void SystemInterlink_setCDROMInterruptDelay(SystemInterlink *smi,
		int32_t delay);
void SystemInterlink_setCDROMInterruptEnabled(SystemInterlink *smi,
		bool enabled);
void SystemInterlink_setCDROMInterruptNumber(SystemInterlink *smi,
		int32_t number);
void SystemInterlink_setCdrom(SystemInterlink *smi,
		CDROMDrive *cdrom);
void SystemInterlink_setControllerIO(SystemInterlink *smi,
		ControllerIO *cio);
void SystemInterlink_setCpu(SystemInterlink *smi, R3051 *cpu);
void SystemInterlink_setDMAInterruptDelay(SystemInterlink *smi, int32_t delay);
void SystemInterlink_setDma(SystemInterlink *smi, DMAArbiter *dma);
void SystemInterlink_setGPUInterruptDelay(SystemInterlink *smi, int32_t delay);
void SystemInterlink_setGpu(SystemInterlink *smi, GPU *gpu);
void SystemInterlink_setSpu(SystemInterlink *smi, SPU *spu);
bool SystemInterlink_tagTestEnabled(SystemInterlink *smi);
void SystemInterlink_writeByte(SystemInterlink *smi, int32_t address,
		int8_t value);
void SystemInterlink_writeInterruptStatus(SystemInterlink *smi,
		int32_t interruptStatus);
void SystemInterlink_writeWord(SystemInterlink *smi, int32_t address,
		int32_t word);

#endif