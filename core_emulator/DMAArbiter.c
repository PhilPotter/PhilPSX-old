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

DMAArbiter *construct_DMAArbiter(void)
{
	return NULL;
}

void destruct_DMAArbiter(DMAArbiter *dma)
{
	
}

int32_t DMAArbiter_handleCDROM(DMAArbiter *dma)
{
	return 0;
}

void DMAArbiter_handleDMATransactions(DMAArbiter *dma)
{
	
}

int32_t DMAArbiter_handleGPU(DMAArbiter *dma)
{
	return 0;
}

int32_t DMAArbiter_handleOTC(DMAArbiter *dma)
{
	return 0;
}

int8_t DMAArbiter_readByte(DMAArbiter *dma, int32_t address)
{
	return 0;
}

int32_t DMAArbiter_readWord(DMAArbiter *dma, int32_t address)
{
	return 0;
}

void DMAArbiter_setBiu(DMAArbiter *dma, BusInterfaceUnit *biu)
{
	
}

void DMAArbiter_setCdrom(DMAArbiter *dma, CDROMDrive *cdrom)
{
	
}

void DMAArbiter_setCpu(DMAArbiter *dma, R3051 *cpu)
{
	
}

void DMAArbiter_setGpu(DMAArbiter *dma, GPU *gpu)
{
	
}

void DMAArbiter_setMemoryInterface(DMAArbiter *dma, SystemInterlink *smi)
{
	
}

void DMAArbiter_writeByte(DMAArbiter *dma, int32_t address, int8_t value)
{
	
}

void DMAArbiter_writeWord(DMAArbiter *dma, int32_t address, int32_t word)
{
	
}