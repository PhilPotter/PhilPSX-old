/*
 * This file starts the emulator off at present.
 * 
 * PhilPSX.c - Copyright Phillip Potter, 2018
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "headers/R3051.h"
#include "headers/GPU.h"
#include "headers/SPU.h"
#include "headers/CDROMDrive.h"
#include "headers/DMAArbiter.h"
#include "headers/ControllerIO.h"
#include "headers/SystemInterlink.h"

// Declarations of main components
static R3051 *cpu;
static GPU *gpu;
static SPU *spu;
static CDROMDrive *cdrom;
static DMAArbiter *dma;
static ControllerIO *cio;
static SystemInterlink *smi;

// Forward declarations for functions related to setup:
static bool setupEmu(int numOfArgs, char **args);

// PhilPSX entry point
int main(int argc, char **argv)
{	
	// variables
	int retval = 0;
	
	// Setup SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "PhilPSX: Error starting SDL: %s\n", SDL_GetError());
		retval = 1;
		goto end;
	}
	
	// Set OpenGL context parameters (OpenGL 4.5 core profile with
	// 8 bits per colour channel
	if (SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_RED_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_GREEN_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_BLUE_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_ALPHA_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_DOUBLE_BUFFER: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_PROFILE_MASK: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_MAJOR_VERSION: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_MINOR_VERSION: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	
	// Create window
	SDL_Window *philpsxWindow = SDL_CreateWindow(
			"PhilPSX",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			640,
			480,
			SDL_WINDOW_OPENGL);
	if (!philpsxWindow) {
		fprintf(stderr, "PhilPSX: Couldn't create window: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	
	// Create OpenGL context
	SDL_GLContext philpsxGlContext = SDL_GL_CreateContext(philpsxWindow);
	if (!philpsxGlContext) {
		fprintf(stderr, "PhilPSX: Couldn't create OpenGL context: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_window;
	}
	
	// Setup PlayStation console itself
	if (!setupEmu(argc - 1, argv + 1)) {
		fprintf(stderr, "PhilPSX: Couldn't setup virtual PlayStation "
				"components\n");
		retval = 1;
		goto cleanup_opengl;
	}
	
	// Enter event loop just to keep window open
	SDL_Event myEvent;
	int waitStatus;
	bool exit = false;
	while ((waitStatus = SDL_WaitEvent(&myEvent)) && !exit) {
		switch (myEvent.type) {
		case SDL_QUIT:
			exit = true;
			break;
		}
	}
		
	if (waitStatus == 0) {
		fprintf(stderr, "PhilPSX: Couldn't wait on event properly: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_opengl;
	}
	
	// Destroy OpenGL context
	cleanup_opengl:
	SDL_GL_DeleteContext(philpsxGlContext);
	
	// Destroy window
	cleanup_window:
	SDL_DestroyWindow(philpsxWindow);

	// Shutdown SDL
	cleanup_sdl:
	SDL_Quit();
	
	// End program
	end:
	return retval;
}

static bool setupEmu(int numOfArgs, char **args)
{
	// List software name
	fprintf(stdout, "PhilPSX - a Sony PlayStation 1 Emulator\n");

	// Parse BIOS path from command line arguments
	bool biosSpecified = false;
	int biosPathIndex = 0;
	for (int i = 0; i < numOfArgs; ++i) {
		if (strlen(args[i]) == 5 && strncmp(args[i], "-bios", 5) == 0) {
			if (i + 1 < numOfArgs) {
				biosSpecified = true;
				biosPathIndex = i + 1;
				break;
			}
		}		
	}
	if (!biosSpecified) {
		fprintf(stderr, "PhilPSX: BIOS file path not provided\n");
		goto end;
	}

	// Parse CD path from command line arguments
	bool cdSpecified = false;
	int cdPathIndex = 0;
	for (int i = 0; i < numOfArgs; ++i) {
		if (strlen(args[i]) == 3 && strncmp(args[i], "-cd", 3) == 0) {
			if (i + 1 < numOfArgs) {
				cdSpecified = true;
				cdPathIndex = i + 1;
				break;
			}
		}
	}

	// Initialise components
	cpu = construct_R3051();
	if (!cpu) {
		fprintf(stderr, "PhilPSX: R3051 setup failed\n");
		goto end;
	}
	
	smi = construct_SystemInterlink(args[biosPathIndex]);
	if (!smi) {
		fprintf(stderr, "PhilPSX: System Interlink setup failed\n");
		goto cleanup_cpu;
	}
	
	gpu = construct_GPU();
	if (!gpu) {
		fprintf(stderr, "PhilPSX: GPU setup failed\n");
		goto cleanup_smi;
	}
	
	spu = construct_SPU();
	if (!spu) {
		fprintf(stderr, "PhilPSX: SPU setup failed\n");
		goto cleanup_gpu;
	}
	
	cdrom = construct_CDROMDrive();
	if (!cdrom) {
		fprintf(stderr, "PhilPSX: CD-ROM drive setup failed\n");
		goto cleanup_spu;
	}
	
	dma = construct_DMAArbiter();
	if (!dma) {
		fprintf(stderr, "PhilPSX: DMA Arbiter setup failed\n");
		goto cleanup_cdrom;
	}
	
	cio = construct_ControllerIO();
	if (!cio) {
		fprintf(stderr, "PhilPSX: Controller I/O setup failed\n");
		goto cleanup_dma;
	}
	
	// Link components together and set their parameters
	
	// Link necessary components to the interlink
	SystemInterlink_setCpu(smi, cpu);
	SystemInterlink_setGpu(smi, gpu);
	SystemInterlink_setSpu(smi, spu);
	SystemInterlink_setCdrom(smi, cdrom);
	SystemInterlink_setDma(smi, dma);
	SystemInterlink_setControllerIO(smi, cio);
	
	// Link interlink back to those components where needed
	R3051_setMemoryInterface(cpu, smi);
	GPU_setMemoryInterface(gpu, smi);
	SPU_setMemoryInterface(spu, smi);
	CDROMDrive_setMemoryInterface(cdrom, smi);
	DMAArbiter_setMemoryInterface(dma, smi);
	ControllerIO_setMemoryInterface(cio, smi);
	
	// Now link necessary components to the DMA arbiter
	DMAArbiter_setCpu(dma, cpu);
	DMAArbiter_setGpu(dma, gpu);
	DMAArbiter_setCdrom(dma, cdrom);
	DMAArbiter_setBiu(dma, R3051_getBiu(cpu));
	
	// Set OpenGL info in GPU object (stub for now)
	GPU_setGLInfo(gpu);
	
	// Load CD image if one was specified
	if (cdSpecified) {
		if (!CDROMDrive_loadCD(cdrom, args[cdPathIndex])) {
			fprintf(stderr, "PhilPSX: Loading of CD-ROM image failed\n");
			goto cleanup_cio;
		}
	}
	
	// Normal return:
	return true;
	
	// Cleanup path:
	cleanup_cio:
	destruct_ControllerIO(cio);
	
	cleanup_dma:
	destruct_DMAArbiter(dma);
	
	cleanup_cdrom:
	destruct_CDROMDrive(cdrom);
	
	cleanup_spu:
	destruct_SPU(spu);
	
	cleanup_gpu:
	destruct_GPU(gpu);
	
	cleanup_smi:
	destruct_SystemInterlink(smi);
	
	cleanup_cpu:
	destruct_R3051(cpu);
	
	end:
	return false;
}