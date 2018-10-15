/*
 * This file starts the emulator off at present. Only pthread init functions
 * are checked for errors, as it is assumed all others can't fail given the
 * use cases of this code.
 * 
 * PhilPSX.c - Copyright Phillip Potter, 2018
 */

/* Included for profiling purposes*/
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <gperftools/profiler.h>

#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "headers/WorkQueue.h"
#include "headers/GpuCommand.h"
#include "headers/R3051.h"
#include "headers/GPU.h"
#include "headers/SPU.h"
#include "headers/CDROMDrive.h"
#include "headers/DMAArbiter.h"
#include "headers/ControllerIO.h"
#include "headers/SystemInterlink.h"

/*
 * This struct stores references to all emulated components.
 */
typedef struct {
	R3051 *cpu;
	GPU *gpu;
	SPU *spu;
	CDROMDrive *cdrom;
	DMAArbiter *dma;
	ControllerIO *cio;
	SystemInterlink *smi;
} Console;

/*
 * This struct stores refences to SDL objects.
 */
typedef struct {
	SDL_Window *window;
	SDL_GLContext context;
} SDLState;

typedef struct {
	Console *console;
	SDLState *sdl;
	WorkQueue *wq;
	pthread_mutex_t quitMutex;
	bool quitBool;
	pthread_t renderingThread;
	pthread_t emulatorThread;
} EmulatorState;

// Forward declarations for functions related to setup/cleanup of emulator:
static bool setupEmu(Console *console, int numOfArgs, char **args,
		WorkQueue *wq, SDL_Window *window);
static void cleanupEmu(Console *console);
static void *renderingFunction(void *arg);
static void *emulatorFunction(void *arg);
static bool setupSDL(void);

// PhilPSX entry point
int main(int argc, char **argv)
{
	// List software name
	fprintf(stdout, "PhilPSX - a Sony PlayStation 1 Emulator\n");

	// Variables
	int retval = 0;
	EmulatorState es;
	
	// Setup SDL
	if (!setupSDL()) {
		fprintf(stderr, "PhilPSX: Setting up SDL failed\n");
		retval = 1;
		goto end;
	}
	
	// Define SDLState object
	SDLState sdl;
	
	// Create window
	sdl.window = SDL_CreateWindow(
			"PhilPSX",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			640,
			480,
			SDL_WINDOW_OPENGL);
	if (!sdl.window) {
		fprintf(stderr, "PhilPSX: Couldn't create window: %s\n",
				SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	
	// Setup OpenGL context
	sdl.context = SDL_GL_CreateContext(sdl.window);
	if (!sdl.context) {
		fprintf(stderr, "PhilPSX: Couldn't create OpenGL context: %s\n",
				SDL_GetError());
		retval = 1;
		goto cleanup_window;
	}
	
	// Setup OpenGL context to synchronise screen updates with the vertical
	// retrace
	if (SDL_GL_SetSwapInterval(1)) {
		fprintf(stderr, "PhilPSX: Couldn't set OpenGL context to synchronise "
				"screen updates with the vertical retrace: %s\n",
				SDL_GetError());
		goto cleanup_context;
	}
	
	// Setup WorkQueue and set its reference in emulator state holder if
	// successful
	WorkQueue *wq = construct_WorkQueue();
	if (!wq) {
		fprintf(stderr, "PhilPSX: Couldn't initialise work queue\n");
		goto cleanup_context;
	}
	es.wq = wq;
	
	// Setup console itself
	Console console;
	if (!setupEmu(&console, argc - 1, argv + 1, wq, sdl.window)) {
		fprintf(stderr, "PhilPSX: Couldn't create console\n");
		retval = 1;
		goto cleanup_workqueue;
	}
	
	// Set emulator state holder to reference multiple objects from the
	// same struct - this makes passing state to threads without using global
	// variables easier
	es.console = &console;
	es.sdl = &sdl;
	es.quitBool = false;
	if (pthread_mutex_init(&es.quitMutex, NULL)) {
		fprintf(stderr, "PhilPSX: Couldn't initialise quitMutex\n");
		goto cleanup_console;
	}
	
	// Create threads, preparing a quit event to push in case they fail
	SDL_Event quitEvent;
	quitEvent.type = SDL_QUIT;
	if (pthread_create(&es.renderingThread, NULL, &renderingFunction, &es)) {
		fprintf(stderr, "PhilPSX: Couldn't start rendering thread\n");
		if (SDL_PushEvent(&quitEvent) != 1) {
			fprintf(stderr, "PhilPSX: Couldn't push quit event to event "
							"loop: %s\n", SDL_GetError());
		}
	}
	if (pthread_create(&es.emulatorThread, NULL, &emulatorFunction, &es)) {
		fprintf(stderr, "PhilPSX: Couldn't start emulator thread\n");
		if (SDL_PushEvent(&quitEvent) != 1) {
			fprintf(stderr, "PhilPSX: Couldn't push quit event to event "
							"loop: %s\n", SDL_GetError());
		}
	}
	
	// Enter event loop just to keep window open
	SDL_Event myEvent;
	int waitStatus;
	while (waitStatus = SDL_WaitEvent(&myEvent)) {
		switch (myEvent.type) {
		case SDL_QUIT:
			// End threads
			pthread_mutex_lock(&es.quitMutex);
			es.quitBool = true;
			pthread_mutex_unlock(&es.quitMutex);
			goto end_waitevent;
			break;
		}
	}
	end_waitevent:
	// This marker is here to jump out of the event loop and prevent any
	// other events after SDL_QUIT from being processed - they are not
	// relevant as we want to end the program

	// Check wait didn't end on an error condition
	if (waitStatus == 0) {
		fprintf(stderr, "PhilPSX: Couldn't wait on event properly: %s\n",
				SDL_GetError());
		retval = 1;
	}
	
	// Wait on both threads to end
	pthread_join(es.renderingThread, NULL);
	pthread_join(es.emulatorThread, NULL);
	
	// Regain OpenGL context
	if (SDL_GL_MakeCurrent(sdl.window, sdl.context)) {
		fprintf(stderr, "PhilPSX: Switching OpenGL context back to main "
						"thread failed: %s\n", SDL_GetError());
		// We don't do anything else here as we are ending anyway,
		// so not much we can do
	}
	
	// Destroy quit mutex
	pthread_mutex_destroy(&es.quitMutex);
	
	// Cleanup console
	cleanup_console:
	cleanupEmu(&console);
	
	// Cleanup work queue
	cleanup_workqueue:
	destruct_WorkQueue(es.wq);
	
	// Cleanup OpenGL context
	cleanup_context:
	SDL_GL_DeleteContext(sdl.context);
	
	// Destroy window
	cleanup_window:
	SDL_DestroyWindow(sdl.window);

	// Shutdown SDL
	cleanup_sdl:
	SDL_Quit();
	
	// End program
	end:
	fprintf(stdout, "PhilPSX: Terminating main thread\n");
	return retval;
}

/*
 * This sets up all the components of the virtual PlayStation and links them
 * together properly.
 */
static bool setupEmu(Console *console, int numOfArgs, char **args,
		WorkQueue *wq, SDL_Window *window)
{
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
	// CPU
	console->cpu = construct_R3051();
	if (!console->cpu) {
		fprintf(stderr, "PhilPSX: R3051 setup failed\n");
		goto end;
	}
	
	// SystemInterlink
	console->smi = construct_SystemInterlink(args[biosPathIndex]);
	if (!console->smi) {
		fprintf(stderr, "PhilPSX: System Interlink setup failed\n");
		goto cleanup_cpu;
	}
	
	// GPU
	console->gpu = construct_GPU();
	if (!console->gpu) {
		fprintf(stderr, "PhilPSX: GPU setup failed\n");
		goto cleanup_smi;
	}
	
	// Set OpenGL state
	GPU_setGLFunctionPointers(console->gpu);
	if (!GPU_initGL(console->gpu)) {
		fprintf(stderr, "PhilPSX: GPU GL setup failed\n");
		goto cleanup_gpu;
	}
	
	// SPU
	console->spu = construct_SPU();
	if (!console->spu) {
		fprintf(stderr, "PhilPSX: SPU setup failed\n");
		goto cleanup_gl;
	}
	
	// CDROMDrive
	console->cdrom = construct_CDROMDrive();
	if (!console->cdrom) {
		fprintf(stderr, "PhilPSX: CD-ROM drive setup failed\n");
		goto cleanup_spu;
	}
		
	// DMAArbiter
	console->dma = construct_DMAArbiter();
	if (!console->dma) {
		fprintf(stderr, "PhilPSX: DMA Arbiter setup failed\n");
		goto cleanup_cdrom;
	}
	
	// ControllerIO
	console->cio = construct_ControllerIO();
	if (!console->cio) {
		fprintf(stderr, "PhilPSX: Controller I/O setup failed\n");
		goto cleanup_dma;
	}
	
	// Load CD image if one was specified
	if (cdSpecified) {
		if (!CDROMDrive_loadCD(console->cdrom, args[cdPathIndex])) {
			fprintf(stderr, "PhilPSX: Loading of CD-ROM image failed\n");
			goto cleanup_cio;
		}
	}
	
	// Link components together and set their parameters
	
	// Link necessary components to the interlink
	SystemInterlink_setCpu(console->smi, console->cpu);
	SystemInterlink_setGpu(console->smi, console->gpu);
	SystemInterlink_setSpu(console->smi, console->spu);
	SystemInterlink_setCdrom(console->smi, console->cdrom);
	SystemInterlink_setDma(console->smi, console->dma);
	SystemInterlink_setControllerIO(console->smi, console->cio);
	
	// Link interlink back to those components where needed
	R3051_setMemoryInterface(console->cpu, console->smi);
	GPU_setMemoryInterface(console->gpu, console->smi);
	SPU_setMemoryInterface(console->spu, console->smi);
	CDROMDrive_setMemoryInterface(console->cdrom, console->smi);
	DMAArbiter_setMemoryInterface(console->dma, console->smi);
	ControllerIO_setMemoryInterface(console->cio, console->smi);
	
	// Now link necessary components to the DMA arbiter
	DMAArbiter_setCpu(console->dma, console->cpu);
	DMAArbiter_setGpu(console->dma, console->gpu);
	DMAArbiter_setCdrom(console->dma, console->cdrom);
	
	// Set work queue reference in GPU
	GPU_setWorkQueue(console->gpu, wq);
	
	// Set SDL_Window reference in GPU
	GPU_setSDLWindowReference(console->gpu, window);
	
	// Normal return:
	return true;
	
	// Cleanup path:
	cleanup_cio:
	destruct_ControllerIO(console->cio);
	
	cleanup_dma:
	destruct_DMAArbiter(console->dma);
	
	cleanup_cdrom:
	destruct_CDROMDrive(console->cdrom);
	
	cleanup_spu:
	destruct_SPU(console->spu);
	
	cleanup_gl:
	GPU_cleanupGL(console->gpu);
	
	cleanup_gpu:
	destruct_GPU(console->gpu);
	
	cleanup_smi:
	destruct_SystemInterlink(console->smi);
	
	cleanup_cpu:
	destruct_R3051(console->cpu);
	
	end:
	return false;
}

/*
 * This cleans up all the resources associated with the virtual PlayStation.
 */
static void cleanupEmu(Console *console)
{
	// Cleanup resources - the CD image is cleaned up automatically
	// by its destructor if present
	destruct_ControllerIO(console->cio);
	destruct_DMAArbiter(console->dma);
	destruct_CDROMDrive(console->cdrom);
	destruct_SPU(console->spu);
	GPU_cleanupGL(console->gpu);
	destruct_GPU(console->gpu);
	destruct_SystemInterlink(console->smi);
	destruct_R3051(console->cpu);
}

/*
 * This function is intended to be called in a dedicated thread, and handles
 * the execution of work items from the emulator thread.
 */
static void *renderingFunction(void *arg)
{
	// Announce entry
	fprintf(stdout, "PhilPSX: Started rendering thread\n");

	// Cast void argument back to objects
	EmulatorState *es = arg;
	Console *console = es->console;
	SDLState *sdl = es->sdl;
	WorkQueue *wq = es->wq;
	
	// Declare local bool for quitting rendering loop
	bool renderQuit;
		 
	// Make the OpenGL context current on this thread
	if (SDL_GL_MakeCurrent(sdl->window, sdl->context)) {
		fprintf(stderr, "PhilPSX: Switching OpenGL context to rendering "
						"thread failed: %s\n", SDL_GetError());
		
		// Send quit event to main loop
		SDL_Event quitEvent;
		quitEvent.type = SDL_QUIT;
		if (SDL_PushEvent(&quitEvent) != 1) {
			fprintf(stderr, "PhilPSX: Couldn't push quit event to event "
							"loop: %s\n", SDL_GetError());
		}
		fprintf(stderr, "PhilPSX: Ended rendering thread due to errors\n");
		return NULL;
	}
	
	// Now listen on work queue and keep executing items
	while (true) {
		
		// Check if we need to quit
		pthread_mutex_lock(&es->quitMutex);
		renderQuit = es->quitBool;
		pthread_mutex_unlock(&es->quitMutex);
		if (renderQuit)
			goto end;
		
		// Check for items in work queue
		GpuCommand *command = WorkQueue_waitForItem(wq);
		if (command)
			command->functionPointer(command);
		WorkQueue_returnItem(wq, command);
	}
	
	end:
	fprintf(stdout, "PhilPSX: Ended rendering thread\n");
	return NULL;
}

/*
 * This function is intended to be called in a dedicated thread, to initialise
 * the emulator's components and begin executing the PlayStation software.
 */
static void *emulatorFunction(void *arg)
{
	// Announce entry
	fprintf(stdout, "PhilPSX: Started emulator thread\n");

	// Cast void argument back to objects
	EmulatorState *es = arg;
	Console *console = es->console;
	WorkQueue *wq = es->wq;

	// Declare local bool for quitting rendering loop
	bool emulatorQuit;
	
	// Enter emulation loop
	struct timespec t1, t2;
	int64_t cycles = 0;
	clock_gettime(CLOCK_REALTIME, &t1);
	
	// Start gperftools log
	ProfilerStart("philpsx_emulator_thread.log");
	
	while (true) {
		
		// Check if we need to quit
		pthread_mutex_lock(&es->quitMutex);
		emulatorQuit = es->quitBool;
		pthread_mutex_unlock(&es->quitMutex);
		if (emulatorQuit)
			goto end;
		
		// Move the emulator on by one block of R3051 instructions
		cycles += R3051_executeInstructions(console->cpu);
		if (cycles >= 33868800) {
			clock_gettime(CLOCK_REALTIME, &t2);
			int64_t t1_ms = t1.tv_sec * 1000 + t1.tv_nsec / 1000000;
			int64_t t2_ms = t2.tv_sec * 1000 + t2.tv_nsec / 1000000;
			int64_t ms = t2_ms - t1_ms;
			printf("Time to emulate one second of R3051 time: %ld\n", ms);
			clock_gettime(CLOCK_REALTIME, &t1);
			cycles -= 33868800;
		}
	}
	ProfilerStop();

	end:
	fprintf(stdout, "PhilPSX: Ended emulator thread\n");
	
	// Set work queue to stop processing and notify
	WorkQueue_endProcessingByRenderingThread(wq);
	
	return NULL;
}

/*
 * This function initialises SDL and sets the properties for the OpenGL
 * context we are going to create.
 */
static bool setupSDL(void)
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "PhilPSX: Error starting SDL: %s\n", SDL_GetError());
		goto end;
	}
	
	// Set OpenGL context parameters (OpenGL 4.5 core profile with
	// 8 bits per colour channel
	if (SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_RED_SIZE: "
				"%s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_GREEN_SIZE: "
				"%s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_BLUE_SIZE: "
				"%s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_ALPHA_SIZE: "
				"%s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_DOUBLE_BUFFER: "
				"%s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_CORE) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute "
				"SDL_GL_CONTEXT_PROFILE_MASK: %s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute "
				"SDL_GL_CONTEXT_MAJOR_VERSION: %s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute "
				"SDL_GL_CONTEXT_MINOR_VERSION: %s\n", SDL_GetError());
		goto cleanup_sdl;
	}
	
	// Normal return:
	return true;
	
	// Cleanup path:
	cleanup_sdl:
	SDL_Quit();
	
	end:
	return false;
}