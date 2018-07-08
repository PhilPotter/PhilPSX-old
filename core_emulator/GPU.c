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

// OpenGL shader-specific include directives
#include "../headers/ogl_shaders/AnyLine_VertexShader1.h"
#include "../headers/ogl_shaders/AnyLine_FragmentShader1.h"
#include "../headers/ogl_shaders/DisplayScreen_VertexShader1.h"
#include "../headers/ogl_shaders/DisplayScreen_FragmentShader1.h"
#include "../headers/ogl_shaders/GP0_02_VertexShader1.h"
#include "../headers/ogl_shaders/GP0_02_FragmentShader1.h"
#include "../headers/ogl_shaders/GP0_80_VertexShader1.h"
#include "../headers/ogl_shaders/GP0_80_FragmentShader1.h"
#include "../headers/ogl_shaders/GP0_80_VertexShader2.h"
#include "../headers/ogl_shaders/GP0_80_FragmentShader2.h"
#include "../headers/ogl_shaders/GP0_A0_VertexShader1.h"
#include "../headers/ogl_shaders/GP0_A0_FragmentShader1.h"
#include "../headers/ogl_shaders/MonochromePolygon_VertexShader1.h"
#include "../headers/ogl_shaders/MonochromePolygon_FragmentShader1.h"
#include "../headers/ogl_shaders/MonochromeRectangle_VertexShader1.h"
#include "../headers/ogl_shaders/MonochromeRectangle_FragmentShader1.h"
#include "../headers/ogl_shaders/ShadedPolygon_VertexShader1.h"
#include "../headers/ogl_shaders/ShadedPolygon_FragmentShader1.h"
#include "../headers/ogl_shaders/ShadedTexturedPolygon_VertexShader1.h"
#include "../headers/ogl_shaders/ShadedTexturedPolygon_FragmentShader1.h"
#include "../headers/ogl_shaders/TexturedPolygon_VertexShader1.h"
#include "../headers/ogl_shaders/TexturedPolygon_FragmentShader1.h"
#include "../headers/ogl_shaders/TexturedRectangle_VertexShader1.h"
#include "../headers/ogl_shaders/TexturedRectangle_FragmentShader1.h"

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