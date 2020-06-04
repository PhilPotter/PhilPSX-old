/*
 * This header file provides the public API for the GPU (graphics chip) of the
 * PlayStation.
 * 
 * GPU.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_GPU_HEADER
#define PHILPSX_GPU_HEADER

// System includes
#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

// Typedefs
typedef struct GPU GPU;

// Includes
#include "SystemInterlink.h"
#include "WorkQueue.h"

// Public functions
GPU *construct_GPU(void);
void destruct_GPU(GPU *gpu);
void GPU_appendSyncCycles(GPU *gpu, int32_t cycles);
void GPU_cleanupGL(GPU *gpu);
void GPU_executeGPUCycles(GPU *gpu);
int32_t GPU_howManyDotclockGpuCyclesLeft(GPU *gpu, int32_t gpuCycles);
int32_t GPU_howManyDotclockIncrements(GPU *gpu, int32_t gpuCycles);
int32_t GPU_howManyHblankGpuCyclesLeft(GPU *gpu, int32_t gpuCycles);
int32_t GPU_howManyHblankIncrements(GPU *gpu, int32_t gpuCycles);
bool GPU_initGL(GPU *gpu);
bool GPU_isInHblank(GPU *gpu);
bool GPU_isInVblank(GPU *gpu);
int32_t GPU_readResponse(GPU *gpu);
int32_t GPU_readStatus(GPU *gpu);
void GPU_setGLFunctionPointers(GPU *gpu);
void GPU_setMemoryInterface(GPU *gpu, SystemInterlink *smi);
void GPU_setResolution(GPU *gpu, int32_t horizontal, int32_t vertical);
void GPU_setSDLWindowReference(GPU *gpu, SDL_Window *window);
void GPU_setWorkQueue(GPU *gpu, WorkQueue *wq);
void GPU_submitToGP0(GPU *gpu, int32_t word);
void GPU_submitToGP1(GPU *gpu, int32_t word);

#endif