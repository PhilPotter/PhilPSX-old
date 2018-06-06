/*
 * This C file models the GPU of the PlayStation as a class.
 * 
 * GPU.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/GPU.h"
#include "../headers/SystemInterlink.h"

GPU *construct_GPU(void)
{
	return NULL;
}

void destruct_GPU(GPU *gpu)
{
	
}

void GPU_appendSyncCycles(GPU *gpu, int32_t cycles)
{
	
}

void GPU_cleanupGL(GPU *gpu)
{
	
}

void GPU_executeGPUCycles(GPU *gpu)
{
	
}

int32_t GPU_howManyDotclockGpuCyclesLeft(GPU *gpu, int32_t gpuCycles)
{
	return 0;
}

int32_t GPU_howManyDotclockIncrements(GPU *gpu, int32_t gpuCycles)
{
	return 0;
}

int32_t GPU_howManyHblankGpuCyclesLeft(GPU *gpu, int32_t gpuCycles)
{
	return 0;
}

int32_t GPU_howManyHblankIncrements(GPU *gpu, int32_t gpuCycles)
{
	return 0;
}

void GPU_initGL(GPU *gpu)
{
	
}

bool GPU_isInHblank(GPU *gpu)
{
	return false;
}

bool GPU_isInVblank(GPU *gpu)
{
	return false;
}

int32_t GPU_readResponse(GPU *gpu)
{
	return 0;
}

int32_t GPU_readStatus(GPU *gpu)
{
	return 0;
}

void GPU_setGLInfo(GPU *gpu) // expand args later, just here as placeholder
{
	
}

void GPU_setMemoryInterface(GPU *gpu, SystemInterlink *smi)
{
	
}

void GPU_setResolution(GPU *gpu, int32_t horizontal, int32_t vertical)
{
	
}

void GPU_submitToGP0(GPU *gpu, int32_t word)
{
	
}

void GPU_submitToGP1(GPU *gpu, int32_t word)
{
	
}