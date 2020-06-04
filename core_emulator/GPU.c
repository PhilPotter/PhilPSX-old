/*
 * This C file models the GPU of the PlayStation as a class. Most pthread
 * function calls are unchecked for errors under the assumption that for this
 * use case, they cannot fail.
 * 
 * GPU.c - Copyright Phillip Potter, 2020, under GPLv3
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <GL/gl.h>
#include <SDL2/SDL.h>
#include "../headers/GPU.h"
#include "../headers/SystemInterlink.h"
#include "../headers/ArrayList.h"
#include "../headers/GLFunctionPointers.h"
#include "../headers/WorkQueue.h"
#include "../headers/math_utils.h"

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

// Values for easier reading when dealing with GPU cycle math
#define GPU_CYCLES_PER_FRAME 1069484
#define GPU_CYCLES_PER_SCANLINE 3406
#define GPU_CYCLES_VBLANK 817440

// Forward declarations for functions private to this class
// GPU-related stuff:
#ifdef PHILPSX_DEBUG_BUILD
#define GPU_checkOpenGLErrors(x, y) GPU_realCheckOpenGLErrors(x, y)
#else
#define GPU_checkOpenGLErrors(x, y) false
#endif
static void GPU_GP0_02(GPU *gpu, int32_t command, int32_t destination,
		int32_t dimensions);
static void GPU_GP0_02_implementation(GpuCommand *command);
static void GPU_GP0_1F(GPU *gpu);
static void GPU_GP0_80(GPU *gpu, int32_t command, int32_t sourceCoord,
		int32_t destinationCoord, int32_t widthAndHeight);
static void GPU_GP0_80_implementation(GpuCommand *command);
static void GPU_GP0_A0(GPU *gpu, int32_t command,
		int32_t destination, int32_t dimensions);
static void GPU_GP0_A0_implementation(GpuCommand *command);
static void GPU_GP0_C0(GPU *gpu, int32_t command,
		int32_t destination, int32_t dimensions);
static void GPU_GP0_C0_implementation(GpuCommand *command);
static void GPU_GP0_E1(GPU *gpu, int32_t command);
static void GPU_GP0_E2(GPU *gpu, int32_t command);
static void GPU_GP0_E3(GPU *gpu, int32_t command);
static void GPU_GP0_E4(GPU *gpu, int32_t command);
static void GPU_GP0_E5(GPU *gpu, int32_t command);
static void GPU_GP0_E6(GPU *gpu, int32_t command);
static void GPU_GP1_00(GPU *gpu, int32_t command);
static void GPU_GP1_01(GPU *gpu, int32_t command);
static void GPU_GP1_02(GPU *gpu, int32_t command);
static void GPU_GP1_03(GPU *gpu, int32_t command);
static void GPU_GP1_04(GPU *gpu, int32_t command);
static void GPU_GP1_05(GPU *gpu, int32_t command);
static void GPU_GP1_06(GPU *gpu, int32_t command);
static void GPU_GP1_07(GPU *gpu, int32_t command);
static void GPU_GP1_08(GPU *gpu, int32_t command);
static void GPU_GP1_09(GPU *gpu, int32_t command);
static void GPU_GP1_10(GPU *gpu, int32_t command);
static void GPU_anyLine(GPU *gpu, int32_t command, ArrayList *paramList);
static void GPU_anyLine_implementation(GpuCommand *command);
static GLuint GPU_createShaderProgram(GPU *gpu, const char *name,
		int shaderNumber);
static void GPU_displayScreen(GPU *gpu);
static void GPU_displayScreen_implementation(GpuCommand *command);
static void GPU_monochromePolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t vertex2, int32_t vertex3, int32_t vertex4);
static void GPU_monochromePolygon_implementation(GpuCommand *command);
static void GPU_monochromeRectangle(GPU *gpu, int32_t command, int32_t vertex,
		int32_t widthAndHeight);
static void GPU_monochromeRectangle_implementation(GpuCommand *command);
static int8_t GPU_readDMABuffer(GPU *gpu, int32_t index);
static bool GPU_realCheckOpenGLErrors(GPU *gpu, const char *message);
static void GPU_shadedPolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t colour2, int32_t vertex2, int32_t colour3, int32_t vertex3,
		int32_t colour4, int32_t vertex4);
static void GPU_shadedPolygon_implementation(GpuCommand *command);
static void GPU_shadedTexturedPolygon(GPU *gpu, int32_t command,
		int32_t vertex1, int32_t texCoord1AndPalette, int32_t colour2,
		int32_t vertex2, int32_t texCoord2AndTexPage, int32_t colour3,
		int32_t vertex3, int32_t texCoord3, int32_t colour4, int32_t vertex4,
		int32_t texCoord4);
static void GPU_shadedTexturedPolygon_implementation(GpuCommand *command);
static void GPU_texturedPolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t texCoord1AndPalette, int32_t vertex2,
		int32_t texCoord2AndTexPage, int32_t vertex3, int32_t texCoord3,
		int32_t vertex4, int32_t texCoord4);
static void GPU_texturedPolygon_implementation(GpuCommand *command);
static void GPU_texturedRectangle(GPU *gpu, int32_t command, int32_t vertex,
		int32_t texCoordAndPalette, int32_t widthAndHeight);
static void GPU_texturedRectangle_implementation(GpuCommand *command);
static void GPU_triggerVblankInterrupt(GPU *gpu);
static void GPU_writeDMABuffer(GPU *gpu, int32_t index, int8_t value);

/*
 * This struct contains registers and state that we need in order to model
 * the PlayStation GPU.
 */
struct GPU {

	// This list stores line parameters for the line rendering commands
	ArrayList *lineParameters;

	// GL variables/references are handled by this class, and so stored here,
	// as well as the SDL_Window reference
	WorkQueue *wq;
	GLFunctionPointers *gl;
	GLuint vertexArrayObject[1];
	GLuint vramTexture[1];
	GLuint tempDrawTexture[1];
	GLuint vramFramebuffer[1];
	GLuint tempDrawFramebuffer[1];
	GLuint emptyFramebuffer[1];
	GLuint clutBuffer[1];
	GLuint displayScreenProgram;
	GLuint gp0_a0Program;
	GLuint gp0_80Program1;
	GLuint gp0_80Program2;
	GLuint gp0_02Program;
	GLuint monochromeRectangleProgram1;
	GLuint texturedRectangleProgram1;
	GLuint texturedPolygonProgram1;
	GLuint shadedTexturedPolygonProgram1;
	GLuint shadedPolygonProgram1;
	GLuint monochromePolygonProgram1;
	GLuint anyLineProgram1;
	SDL_Window *window;

	// This lets us store values for DMA transfers, and synchronise access
	// to the associated buffer
	int32_t dmaBufferIndex;
	int32_t dmaNeededBytes;
	int8_t *dmaBuffer;
	int32_t dmaReadInProgress;
	int32_t dmaWriteInProgress;
	int32_t dmaWidthInPixels;
	int32_t dmaHeightInPixels;
	pthread_mutex_t dmaBufferMutex;

	// Link to system
	SystemInterlink *system;

	// Command FIFO buffer for GP0 commands
	int32_t fifoBuffer[16];
	int32_t commandsInFifo;

	// Status register
	int32_t statusRegister;

	// X and Y display start variables
	int32_t xStart;
	int32_t yStart;

	// X1 and X2 horizontal display range variables
	int32_t x1;
	int32_t x2;

	// Y1 and Y2 vertical display range variables
	int32_t y1;
	int32_t y2;

	// Texture window variable
	int32_t textureWindow;

	// Drawing area top left and bottom right variables
	int32_t drawingAreaTopLeft;
	int32_t drawingAreaBottomRight;

	// Drawing offset variable
	int32_t drawingOffset;

	// GPUREAD latch value
	int32_t gpureadLatchValue;
	bool gpureadLatched;

	// CPU and GPU cycle stores
	int32_t cpuCycles;
	int32_t gpuCycles;

	// This allows us to dynamically create the odd/even
	// line flag (bit 31) in the status register
	int32_t oddOrEven;

	// Variable caches to prevent constant recalculation
	// and improve performance
	int32_t horizontalRes;
	int32_t verticalRes;
	int32_t dotFactor;
	bool interlaceEnabled;

	// Real resolution variables (for resolution of window)
	int32_t realHorizontalRes;
	int32_t realVerticalRes;

	// This lets us trigger only once per frame
	bool vblankTriggered;
};

/*
 * This constructs a new GPU object.
 */
GPU *construct_GPU(void)
{
	// Allocate GPU struct
	GPU *gpu = calloc(1, sizeof(GPU));
	if (!gpu) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for GPU "
				"struct\n");
		goto end;
	}
	
	// Initialise DMA buffer mutex
	if (pthread_mutex_init(&gpu->dmaBufferMutex, NULL) != 0) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't initialise the "
				"DMA buffer mutex\n");
		goto cleanup_gpu;
	}
	
	// Setup ArrayList to store line parameters in a thread-safe manner
	gpu->lineParameters = construct_ArrayList(NULL, true, false);
	if (!gpu->lineParameters) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't initialise lineParameters "
				"list\n");
		goto cleanup_mutex;
	}
	
	// Setup GLFunctionPointers struct
	gpu->gl = calloc(1, sizeof(GLFunctionPointers));
	if (!gpu->gl) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"GLFunctionPointers struct\n");
		goto cleanup_lineparameters;
	}
	
	// Anything set below is done for clarity - struct members not dealt
	// with here are 0/NULL by virtue of the calloc call above. With specific
	// regard to GL-related state - it is handled by the GL_initGL function.
	
	// Setup real resolution (of window)
	gpu->realHorizontalRes = 640;
	gpu->realVerticalRes = 480;

	// Setup odd/even line variable
	gpu->oddOrEven = 0;

	// Setup DMA buffer (actual buffer allocation handled by
	// GPU_initGL function)
	gpu->dmaBufferIndex = -1;
	gpu->dmaNeededBytes = -1;
	gpu->dmaReadInProgress = -1;
	gpu->dmaWriteInProgress = -1;
	gpu->dmaWidthInPixels = -1;
	gpu->dmaHeightInPixels = -1;

	// Set FIFO buffer parameters
	gpu->commandsInFifo = 0;

	// Setup status register
	gpu->statusRegister = 0x14902000;

	// Setup resolution and dot factor caches
	gpu->horizontalRes = 256;
	gpu->verticalRes = 240;
	gpu->dotFactor = 10;
	gpu->interlaceEnabled = false;

	// Setup  X and Y display start variables
	gpu->xStart = 0;
	gpu->yStart = 0;

	// Setup display range variables
	gpu->x1 = 0x260;
	gpu->x2 = 0xC60;
	gpu->y1 = 0x1F;
	gpu->y2 = 0x127;

	// Setup texture window variable
	gpu->textureWindow = 0;

	// Setup drawing area top left and bottom right variables
	gpu->drawingAreaTopLeft = 0;
	gpu->drawingAreaBottomRight = 0;

	// Setup drawing offset
	gpu->drawingOffset = 0;

	// Setup GPUREAD latch
	gpu->gpureadLatchValue = 0;
	gpu->gpureadLatched = false;

	// Cpu cycles and gpu cycles
	gpu->cpuCycles = 0;
	gpu->gpuCycles = 0;

	// Setup VBLANK triggered flag
	gpu->vblankTriggered = false;
	
	// Normal return:
	return gpu;
	
	// Cleanup path:
	cleanup_lineparameters:
	destruct_ArrayList(gpu->lineParameters);
	
	cleanup_mutex:
	pthread_mutex_destroy(&gpu->dmaBufferMutex);

	cleanup_gpu:
	free(gpu);
	gpu = NULL;

	end:
	return gpu;
}

/*
 * This destructs a GPU object - make sure GPU_cleanupGL() has been called
 * from the relevant thread before running this, to prevent resource
 * leaks and related bugs.
 */
void destruct_GPU(GPU *gpu)
{
	free(gpu->gl);
	destruct_ArrayList(gpu->lineParameters);
	pthread_mutex_destroy(&gpu->dmaBufferMutex);
	free(gpu);
}

/*
 * This function updates the number of cycles the GPU needs to sync up by.
 */
void GPU_appendSyncCycles(GPU *gpu, int32_t cycles)
{
	gpu->cpuCycles += cycles;
}

/*
 * This function cleans up OpenGL-related resources that are referenced by
 * the GPU. It is intended to be called from the GL context thread.
 */
void GPU_cleanupGL(GPU *gpu)
{
	// Get GLFunctionPointers reference
	GLFunctionPointers *gl = gpu->gl;
	
	// Free OpenGL resources - we don't track these GL calls as if they fail
	// there is nothing we can do anyway
	gl->glDeleteBuffers(1, gpu->clutBuffer);
	gl->glDeleteFramebuffers(1, gpu->tempDrawFramebuffer);
	gl->glDeleteTextures(1, gpu->tempDrawTexture);
	gl->glDeleteFramebuffers(1, gpu->emptyFramebuffer);
	gl->glDeleteFramebuffers(1, gpu->vramFramebuffer);
	gl->glDeleteTextures(1, gpu->vramTexture);
	gl->glDeleteVertexArrays(1, gpu->vertexArrayObject);
	gl->glDeleteProgram(gpu->displayScreenProgram);
	gl->glDeleteProgram(gpu->gp0_a0Program);
	gl->glDeleteProgram(gpu->gp0_80Program1);
	gl->glDeleteProgram(gpu->gp0_80Program2);
	gl->glDeleteProgram(gpu->monochromeRectangleProgram1);
	gl->glDeleteProgram(gpu->texturedRectangleProgram1);
	gl->glDeleteProgram(gpu->texturedPolygonProgram1);
	gl->glDeleteProgram(gpu->shadedTexturedPolygonProgram1);
	gl->glDeleteProgram(gpu->shadedPolygonProgram1);
	gl->glDeleteProgram(gpu->monochromePolygonProgram1);
	gl->glDeleteProgram(gpu->anyLineProgram1);
	gl->glDeleteProgram(gpu->gp0_02Program);
	
	// Free allocated memory
	free(gpu->dmaBuffer);
}

/*
 * This function deals with counters and such like.
 */
void GPU_executeGPUCycles(GPU *gpu)
{
	// Convert CPU cycles to GPU cycles
	int32_t newGpuCycles = gpu->gpuCycles + gpu->cpuCycles * 11 / 7;

	// Test if we need to trigger a vblank interrupt
	if (newGpuCycles > GPU_CYCLES_VBLANK && !gpu->vblankTriggered) {
		gpu->vblankTriggered = true;
		GPU_triggerVblankInterrupt(gpu);
	}
	
	if (newGpuCycles > GPU_CYCLES_PER_FRAME) {
		// Reset vblank interrupt status
		gpu->vblankTriggered = false;

		// Check if we are on odd or even frame
		int32_t frameTraversals = newGpuCycles / GPU_CYCLES_PER_FRAME;
		gpu->oddOrEven = frameTraversals % 2 == 1 ?
				~gpu->oddOrEven & 0x1 : gpu->oddOrEven;
		
		// Modify newGpuCycles to reflect we are in a subsequent frame
		newGpuCycles %= GPU_CYCLES_PER_FRAME;
	}

	// Store state
	gpu->gpuCycles = newGpuCycles;
	gpu->cpuCycles = 0;
}

/*
 * This function tells the caller how many gpu cycles should
 * be left after a round of incrementation based on the dotclock timer.
 */
int32_t GPU_howManyDotclockGpuCyclesLeft(GPU *gpu, int32_t gpuCycles)
{
	return gpuCycles % gpu->dotFactor;
}

/*
 * This function tells the caller how many times to increment
 * the dotclock timer if needed.
 */
int32_t GPU_howManyDotclockIncrements(GPU *gpu, int32_t gpuCycles)
{
	return gpuCycles / gpu->dotFactor;
}

/*
 * This function tells the caller how many gpu cycles should
 * be left after a round of incrementation based on the hblank timer.
 */
int32_t GPU_howManyHblankGpuCyclesLeft(GPU *gpu, int32_t gpuCycles)
{
	return gpuCycles % GPU_CYCLES_PER_SCANLINE;
}

/*
 * This function tells the caller how many times to increment
 * the hblank timer if needed.
 */
int32_t GPU_howManyHblankIncrements(GPU *gpu, int32_t gpuCycles)
{
	return gpuCycles / GPU_CYCLES_PER_SCANLINE;
}

/*
 * This function initialises the OpenGL properties we need in the GPU object.
 * It is intended to be called from the GL context thread.
 */
bool GPU_initGL(GPU *gpu)
{
	// Get GLFunctionPointers reference
	GLFunctionPointers *gl = gpu->gl;
	
	// Allocate required memory areas, including buffer to store DMA data
	// for transfer to/from the GPU
	int8_t *initialImage = calloc(1024 * 512 * 4, sizeof(int8_t));
	if (!initialImage) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"initialImage\n");
		goto cleanup_memory;
	}
	gpu->dmaBuffer = calloc(1024 * 512 * 4, sizeof(int8_t));
	if (!gpu->dmaBuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"dmaBuffer\n");
		goto cleanup_memory;
	}

	// Setup vertex array object and bind it
	gl->glCreateVertexArrays(1, gpu->vertexArrayObject);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateVertexArrays called"))
		goto cleanup_memory;
	gl->glBindVertexArray(gpu->vertexArrayObject[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindVertexArray called"))
		goto cleanup_delete_vao;

	// Explicitly disable depth testing and face culling (for clarity)
	gl->glDisable(GL_DEPTH_TEST);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glDisable called"))
		goto cleanup_delete_vao;
	gl->glDisable(GL_CULL_FACE);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glDisable called"))
		goto cleanup_delete_vao;

	// Create and bind vram texture - this will be what we draw to
	gl->glCreateTextures(GL_TEXTURE_2D, 1, gpu->vramTexture);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateTextures called"))
		goto cleanup_delete_vao;
	gl->glActiveTexture(GL_TEXTURE0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glActiveTexture called"))
		goto cleanup_delete_vram_texture;
	gl->glBindTexture(GL_TEXTURE_2D, gpu->vramTexture[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindTexture called"))
		goto cleanup_delete_vram_texture;

	// Allocate storage for vram texture and fill it with initial image
	gl->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 1024, 512);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexStorage2D called"))
		goto cleanup_delete_vram_texture;
	gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
			GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, initialImage);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexSubImage2D called"))
		goto cleanup_delete_vram_texture;

	// Set parameters for texture
	gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexParameteri called"))
		goto cleanup_delete_vram_texture;
	gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexParameteri called"))
		goto cleanup_delete_vram_texture;

	// Attach vram texture to new framebuffer object, then unbind framebuffer
	gl->glCreateFramebuffers(1, gpu->vramFramebuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateFramebuffers called"))
		goto cleanup_delete_vram_texture;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_vram_framebuffer;
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glFramebufferTexture2D called"))
		goto cleanup_delete_vram_framebuffer;
	int tempBuffer[1] = { GL_COLOR_ATTACHMENT0 };
	gl->glDrawBuffers(1, tempBuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glDrawBuffers called"))
		goto cleanup_delete_vram_framebuffer;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_vram_framebuffer;

	// Setup empty framebuffer for arbitrary copying between textures
	gl->glCreateFramebuffers(1, gpu->emptyFramebuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateFramebuffers called"))
		goto cleanup_delete_vram_framebuffer;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_empty_framebuffer;
	gl->glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
			1024);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glFramebufferParameteri called"))
		goto cleanup_delete_empty_framebuffer;
	gl->glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
			512);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glFramebufferParameteri called"))
		goto cleanup_delete_empty_framebuffer;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_empty_framebuffer;

	// Create and bind temp draw texture
	gl->glCreateTextures(GL_TEXTURE_2D, 1, gpu->tempDrawTexture);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateTextures called"))
		goto cleanup_delete_empty_framebuffer;
	gl->glActiveTexture(GL_TEXTURE1);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glActiveTexture called"))
		goto cleanup_delete_tempdraw_texture;
	gl->glBindTexture(GL_TEXTURE_2D, gpu->tempDrawTexture[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindTexture called"))
		goto cleanup_delete_tempdraw_texture;

	// Allocate storage for temp draw texture and fill it with initial image
	gl->glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8UI, 1024, 512);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexStorage2D called"))
		goto cleanup_delete_tempdraw_texture;
	gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
			GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, initialImage);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexSubImage2D called"))
		goto cleanup_delete_tempdraw_texture;

	// Set parameters for texture
	gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexParameteri called"))
		goto cleanup_delete_tempdraw_texture;
	gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glTexParameteri called"))
		goto cleanup_delete_tempdraw_texture;

	// Attach temp draw texture to new framebuffer object, then
	// unbind framebuffer
	gl->glCreateFramebuffers(1, gpu->tempDrawFramebuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateFramebuffers called"))
		goto cleanup_delete_tempdraw_texture;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->tempDrawFramebuffer[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_tempdraw_framebuffer;
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->tempDrawTexture[0], 0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glFramebufferTexture2D called"))
		goto cleanup_delete_tempdraw_framebuffer;
	tempBuffer[0] = GL_COLOR_ATTACHMENT0;
	gl->glDrawBuffers(1, tempBuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glDrawBuffers called"))
		goto cleanup_delete_tempdraw_framebuffer;
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindFramebuffer called"))
		goto cleanup_delete_tempdraw_framebuffer;

	// Create the CLUT buffer
	gl->glCreateBuffers(1, gpu->clutBuffer);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glCreateBuffers called"))
		goto cleanup_delete_tempdraw_framebuffer;
	gl->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpu->clutBuffer[0]);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBindBufferBase called"))
		goto cleanup_delete_clut_buffer;
	gl->glBufferStorage(GL_SHADER_STORAGE_BUFFER, 256 * 4 * 4, NULL,
			GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
	if (GPU_checkOpenGLErrors(gpu, "GPU_initGL function, "
								"glBufferStorage called"))
		goto cleanup_delete_clut_buffer;

	// Create shader programs
	if ((gpu->displayScreenProgram =
			GPU_createShaderProgram(gpu, "DisplayScreen", 1)) == 0)
		goto cleanup_delete_clut_buffer;
	if ((gpu->gp0_a0Program =
			GPU_createShaderProgram(gpu, "GP0_A0", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->gp0_80Program1 =
			GPU_createShaderProgram(gpu, "GP0_80", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->gp0_80Program2 =
			GPU_createShaderProgram(gpu, "GP0_80", 2)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->monochromeRectangleProgram1 =
			GPU_createShaderProgram(gpu, "MonochromeRectangle", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->texturedRectangleProgram1 =
			GPU_createShaderProgram(gpu, "TexturedRectangle", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->texturedPolygonProgram1 =
			GPU_createShaderProgram(gpu, "TexturedPolygon", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->shadedTexturedPolygonProgram1 =
			GPU_createShaderProgram(gpu, "ShadedTexturedPolygon", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->shadedPolygonProgram1 =
			GPU_createShaderProgram(gpu, "ShadedPolygon", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->monochromePolygonProgram1 =
			GPU_createShaderProgram(gpu, "MonochromePolygon", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->anyLineProgram1 =
			GPU_createShaderProgram(gpu, "AnyLine", 1)) == 0)
		goto cleanup_shader_programs;
	if ((gpu->gp0_02Program =
			GPU_createShaderProgram(gpu, "GP0_02", 1)) == 0)
		goto cleanup_shader_programs;
	
	// Normal path:
	free(initialImage);
	return true;
	
	// Cleanup path (we don't bother with logging these GL calls,
	// as at this point there is nothing we can do to rollback anyway):
	cleanup_delete_clut_buffer:
	gl->glDeleteBuffers(1, gpu->clutBuffer);
	
	cleanup_delete_tempdraw_framebuffer:
	gl->glDeleteFramebuffers(1, gpu->tempDrawFramebuffer);
	
	cleanup_delete_tempdraw_texture:
	gl->glDeleteTextures(1, gpu->tempDrawTexture);
	
	cleanup_delete_empty_framebuffer:
	gl->glDeleteFramebuffers(1, gpu->emptyFramebuffer);
	
	cleanup_delete_vram_framebuffer:
	gl->glDeleteFramebuffers(1, gpu->vramFramebuffer);
	
	cleanup_delete_vram_texture:
	gl->glDeleteTextures(1, gpu->vramTexture);
	
	cleanup_delete_vao:
	gl->glDeleteVertexArrays(1, gpu->vertexArrayObject);
	
	cleanup_shader_programs:
	if (gpu->displayScreenProgram != 0)
		gl->glDeleteProgram(gpu->displayScreenProgram);
	if (gpu->gp0_a0Program != 0)
		gl->glDeleteProgram(gpu->gp0_a0Program);
	if (gpu->gp0_80Program1 != 0)
		gl->glDeleteProgram(gpu->gp0_80Program1);
	if (gpu->gp0_80Program2 != 0)
		gl->glDeleteProgram(gpu->gp0_80Program2);
	if (gpu->monochromeRectangleProgram1 != 0)
		gl->glDeleteProgram(gpu->monochromeRectangleProgram1);
	if (gpu->texturedRectangleProgram1 != 0)
		gl->glDeleteProgram(gpu->texturedRectangleProgram1);
	if (gpu->texturedPolygonProgram1 != 0)
		gl->glDeleteProgram(gpu->texturedPolygonProgram1);
	if (gpu->shadedTexturedPolygonProgram1 != 0)
		gl->glDeleteProgram(gpu->shadedTexturedPolygonProgram1);
	if (gpu->shadedPolygonProgram1 != 0)
		gl->glDeleteProgram(gpu->shadedPolygonProgram1);
	if (gpu->monochromePolygonProgram1 != 0)
		gl->glDeleteProgram(gpu->monochromePolygonProgram1);
	if (gpu->anyLineProgram1 != 0)
		gl->glDeleteProgram(gpu->anyLineProgram1);
	if (gpu->gp0_02Program != 0)
		gl->glDeleteProgram(gpu->gp0_02Program);
	
	cleanup_memory:
	if (initialImage)
		free(initialImage);
	if (gpu->dmaBuffer)
		free(gpu->dmaBuffer);
	
	return false;
}

/*
 * This function tells us whether the GPU is in hblank phase of scanline.
 */
bool GPU_isInHblank(GPU *gpu)
{
	// Get scanline
	int32_t scanline = gpu->gpuCycles / GPU_CYCLES_PER_SCANLINE;

	// Get position in scanline
	float positionInScanline = (float)gpu->gpuCycles -
			(float)scanline * (float)GPU_CYCLES_PER_SCANLINE;

	// Get dot value we are on
	positionInScanline /= gpu->dotFactor;

	return positionInScanline > gpu->horizontalRes;
}

/*
 * This function tells us whether the GPU is in vblank phase of screen.
 */
bool GPU_isInVblank(GPU *gpu)
{
	// If we are over GPU_CYCLES_VBLANK we must be in vblank area
	return gpu->gpuCycles > GPU_CYCLES_VBLANK;
}

/*
 * This function retrieves command responses.
 */
int32_t GPU_readResponse(GPU *gpu)
{
	// Sync up to CPU
	GPU_executeGPUCycles(gpu);

	// Declare return value
	int32_t retVal = 0;

	switch (gpu->dmaReadInProgress) {
		default: // Read in progress, handle appropriately
			if (gpu->dmaBufferIndex == 0) {
				switch (gpu->dmaReadInProgress) {
					case 0xC0: // GP0(0xC0): copy rectangle (VRAM to CPU)
						GPU_GP0_C0(gpu, gpu->fifoBuffer[0],
								gpu->fifoBuffer[1], gpu->fifoBuffer[2]);
						GPU_GP1_01(gpu, 0);
						break;
				}
			}

			// Read first pixel into return value

			// Get pixel index first
			int32_t tempRowIndex =
					gpu->dmaBufferIndex / (gpu->dmaWidthInPixels * 4);
			tempRowIndex = gpu->dmaHeightInPixels - 1 - tempRowIndex;
			int32_t tempRowPixelOffset =
					gpu->dmaBufferIndex % (gpu->dmaWidthInPixels * 4);
			int32_t pixelIndex =
					(tempRowIndex * gpu->dmaWidthInPixels * 4)
					+ tempRowPixelOffset;

			// Organise bytes into original structure
			retVal = (int32_t)((int64_t)(GPU_readDMABuffer(gpu, pixelIndex + 3)
						& 0x1) << 31);
			retVal |= (GPU_readDMABuffer(gpu, pixelIndex + 2) & 0x1F) << 26;
			retVal |= (GPU_readDMABuffer(gpu, pixelIndex + 1) & 0x1F) << 21;
			retVal |= (GPU_readDMABuffer(gpu, pixelIndex) & 0x1F) << 16;
			retVal = logical_rshift((retVal & 0xFF000000), 8) |
					((retVal & 0xFF0000) << 8);

			// Increment dmaBufferIndex by 4
			gpu->dmaBufferIndex += 4;

			// Read second pixel if there is one to read
			if (gpu->dmaBufferIndex != gpu->dmaNeededBytes) {
				// Get second pixel
				tempRowIndex =
						gpu->dmaBufferIndex / (gpu->dmaWidthInPixels * 4);
				tempRowIndex = gpu->dmaHeightInPixels - 1 - tempRowIndex;
				tempRowPixelOffset =
						gpu->dmaBufferIndex % (gpu->dmaWidthInPixels * 4);
				pixelIndex = (tempRowIndex * gpu->dmaWidthInPixels * 4)
							 + tempRowPixelOffset;

				// Organise bytes into original structure
				retVal |= (GPU_readDMABuffer(gpu, pixelIndex + 3) & 0x1) << 15;
				retVal |= (GPU_readDMABuffer(gpu, pixelIndex + 2) & 0x1F) << 10;
				retVal |= (GPU_readDMABuffer(gpu, pixelIndex + 1) & 0x1F) << 5;
				retVal |= GPU_readDMABuffer(gpu, pixelIndex) & 0x1F;
				retVal = (retVal & 0xFFFF0000) |
						 logical_rshift((retVal & 0xFF00), 8) |
						 ((retVal & 0xFF) << 8);
				gpu->dmaBufferIndex += 4;
			}

			if (gpu->dmaBufferIndex == gpu->dmaNeededBytes) {
				gpu->dmaReadInProgress = -1;

				// Set bit 26 (ready to receive command word) of status register
				gpu->statusRegister |= 0x04000000;

				// Clear bit 27 (VRAM to CPU ready) of status register
				gpu->statusRegister &= 0xF7FFFFFF;

				// Set bit 28 (DMA ready) of status register
				gpu->statusRegister |= 0x10000000;
			}
			break;
		case -1: // No read in progress, deal with normally
			if (gpu->gpureadLatched) {
				retVal = gpu->gpureadLatchValue;
			}

			// Reverse endianness as we are dealing with a register value
			retVal = ((retVal << 24) & 0xFF000000) |
					 ((retVal << 8) & 0xFF0000) |
					 (logical_rshift(retVal, 8) & 0xFF00) |
					 (logical_rshift(retVal, 24) & 0xFF);
			break;
	}

	return retVal;
}

/*
 * This function retrieves the status register of the GPU.
 */
int32_t GPU_readStatus(GPU *gpu)
{
	// Sync up to CPU
	GPU_executeGPUCycles(gpu);

	// Store status register to temp variable, then correctly set bit 25
	int32_t tempStatus = gpu->statusRegister & 0x7DFFFFFF;
	int32_t mergeVal = 0;

	// Check DMA direction setting
	switch (logical_rshift(tempStatus, 29) & 0x3) {
		case 0:
			mergeVal = 0;
			break;
		case 1:
			mergeVal = ((gpu->commandsInFifo == 16) ? 0 : 1) << 25;
			break;
		case 2:
			mergeVal = logical_rshift(tempStatus, 3) & 0x2000000;
			break;
		case 3:
			mergeVal = logical_rshift(tempStatus, 2) & 0x2000000;
			break;
	}

	// Return correct interlace flag in bit 31
	switch (tempStatus & 0x400000) {
		case 0x400000:
			// If in vblank we leave bit 31 as 0
			if (!GPU_isInVblank(gpu)) {
				switch (tempStatus & 0x80000) {
					case 0x80000: // 480-line mode,
								  // bit only changes once per frame
						mergeVal |= (int32_t)((int64_t)gpu->oddOrEven << 31);
						break;
					default: // 240-line mode, bit changes once per scanline
						mergeVal |= (int32_t)((int64_t)((gpu->gpuCycles /
									GPU_CYCLES_PER_SCANLINE) % 2) << 31);
						break;
				}
			}
			break;
		default: // Do nothing
			break;
	}

	tempStatus |= mergeVal;

	return ((tempStatus << 24) & 0xFF000000) |
			((tempStatus << 8) & 0xFF0000) |
			(logical_rshift(tempStatus, 8) & 0xFF00) |
			(logical_rshift(tempStatus, 24) & 0xFF);
}

/*
 * This function allows us to set the GPU's OpenGL function pointers, needed
 * for making OpenGL calls to emulate the PlayStation GPU.
 */
void GPU_setGLFunctionPointers(GPU *gpu)
{
	GLFunctionPointers_setFunctionPointers(gpu->gl);
}

/*
 * This function sets the system reference to that of the supplied argument.
 */
void GPU_setMemoryInterface(GPU *gpu, SystemInterlink *smi)
{
	gpu->system = smi;
}

/*
 * This method sets the resolution of the real window on the screen - it is
 * intended to be called from the UI thread.
 */
void GPU_setResolution(GPU *gpu, int32_t horizontal, int32_t vertical)
{
	gpu->realHorizontalRes = horizontal;
	gpu->realVerticalRes = vertical;
}

/*
 * This function sets the SDL_Window reference inside the GPU object.
 */
void GPU_setSDLWindowReference(GPU *gpu, SDL_Window *window)
{
	gpu->window = window;
}

/*
 * This function allows us to set the GPU's reference to the work queue,
 * needed for making submissions to the rendering thread.
 */
void GPU_setWorkQueue(GPU *gpu, WorkQueue *wq)
{
	gpu->wq = wq;
}

/*
 * This function submits GP0 commands/packets.
 */
void GPU_submitToGP0(GPU *gpu, int32_t word)
{
	// Sync up to CPU
	GPU_executeGPUCycles(gpu);

	// Exit if DMA read is in progress
	switch (gpu->dmaReadInProgress) {
		case -1: // No read
			break;
		default: // Read happening, return from method
			return;
	}

	// Switch endianness
	int32_t commandByte = word & 0xFF;
	word = ((word << 24) & 0xFF000000) |
			((word << 8) & 0xFF0000) |
			(logical_rshift(word, 8) & 0xFF00) |
			(logical_rshift(word, 24) & 0xFF);

	switch (gpu->dmaWriteInProgress) {
		default: // Write in progress, handle appropriately
			// Swap word order back as we are dumping this data straight to VRAM
			word = ((word << 24) & 0xFF000000) |
					((word << 8) & 0xFF0000) |
					(logical_rshift(word, 8) & 0xFF00) |
					(logical_rshift(word, 24) & 0xFF);

			// Split first pixel into buffer
			GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++,
					(int8_t)(logical_rshift(word, 24) & 0xFF));
			GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++,
					(int8_t)(logical_rshift(word, 16) & 0xFF));
			GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++, (int8_t)0);
			GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++, (int8_t)0);

			// Test for second pixel and split as well if needed
			if (gpu->dmaBufferIndex != gpu->dmaNeededBytes) {
				GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++,
						(int8_t)(logical_rshift(word, 8) & 0xFF));
				GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++,
						(int8_t)(word & 0xFF));
				GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++, (int8_t)0);
				GPU_writeDMABuffer(gpu, gpu->dmaBufferIndex++, (int8_t)0);
			}

			if (gpu->dmaBufferIndex == gpu->dmaNeededBytes) {
				switch (gpu->dmaWriteInProgress) {
					case 0xA0: // GP0(0xA0): copy rectangle (CPU to VRAM)
						GPU_GP0_A0(gpu, gpu->fifoBuffer[0],
								gpu->fifoBuffer[1], gpu->fifoBuffer[2]);
						GPU_GP1_01(gpu, 0);
						break;
				}
				gpu->dmaWriteInProgress = -1;

				// Set bit 26 (ready to receive command word) of status register
				gpu->statusRegister |= 0x04000000;

				// Clear bit 27 (VRAM to CPU ready) of status register
				gpu->statusRegister &= 0xF7FFFFFF;

				// Set bit 28 (DMA ready) of status register
				gpu->statusRegister |= 0x10000000;
			}
			break;
		case -1: // No write in progress, deal with normally
			switch (gpu->commandsInFifo) {
				case 0: // Deal with command
					switch (commandByte) {
						case 0x1F: // Trigger interrupt
							GPU_GP0_1F(gpu);
							break;
						case 0x00:
						case 0x03:
						case 0x04:
						case 0x05:
						case 0x06:
						case 0x07:
						case 0x08:
						case 0x09:
						case 0x0A:
						case 0x0B:
						case 0x0C:
						case 0x0D:
						case 0x0E:
						case 0x0F:
						case 0x10:
						case 0x11:
						case 0x12:
						case 0x13:
						case 0x14:
						case 0x15:
						case 0x16:
						case 0x17:
						case 0x18:
						case 0x19:
						case 0x1A:
						case 0x1B:
						case 0x1C:
						case 0x1D:
						case 0x1E:
						case 0xE0:
						case 0xE7:
						case 0xE8:
						case 0xE9:
						case 0xEA:
						case 0xEB:
						case 0xEC:
						case 0xED:
						case 0xEE:
						case 0xEF: // NOP (do nothing)
							break;
						case 0x01: // Clear cache, but not clear
								   // (it actually does nothing)
							break;
						case 0x02: // GP0(0x02): fill rectangle in VRAM
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x24: // GP0(0x24): textured three-point polygon,
								   // opaque, texture-blending
						case 0x25: // GP0(0x25): textured three-point polygon,
								   // opaque, raw-texture
						case 0x26: // GP0(0x26): textured three-point polygon,
								   // semi-transparent, texture-blending
						case 0x27: // GP0(0x27): textured three-point polygon,
								   // semi-transparent, raw-texture
						case 0x2C: // GP0(0x2C): textured four-point polygon,
								   // opaque, texture-blending
						case 0x2D: // GP0(0x2D): textured four-point polygon,
								   // opaque, raw-texture
						case 0x2E: // GP0(0x2E): textured four-point polygon,
								   // semi-transparent, texture-blending
						case 0x2F: // GP0(0x2F): textured four-point polygon,
								   // semi-transparent, raw-texture
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x34: // GP0(0x34): shaded textured three-point
								   // polygon, opaque, texture-blending
						case 0x35: // GP0(0x35): undocumented, textured
								   // three-point polygon, opaque, no blending
						case 0x36: // GP0(0x36): shaded textured three-point
								   // polygon, semi-transparent,
								   // texture-blending
						case 0x37: // GP0(0x37): undocumented, textured
								   // three-point polygon, semi-transparent,
								   // no blending
						case 0x3C: // GP0(0x3C): shaded textured four-point
								   // polygon, opaque, texture-blending
						case 0x3D: // GP0(0x3D): undocumented, textured
								   // four-point polygon, opaque, no blending
						case 0x3E: // GP0(0x3E): shaded textured four-point
								   // polygon, semi-transparent,
								   // texture-blending
						case 0x3F: // GP0(0x3F): undocumented, textured
								   // four-point polygon, semi-transparent,
								   // no blending
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x20: // GP0(0x20): monochrome three-point polygon,
								   // opaque
						case 0x21: // GP0(0x21): undocumented command, same as
								   // GP0(0x20)
						case 0x22: // GP0(0x22): monochrome three-point polygon,
								   // semi-transparent
						case 0x23: // GP0(0x23): undocumented command, same as
								   // GP0(0x22)
						case 0x28: // GP0(0x28): monochrome four-point polygon,
								   // opaque
						case 0x29: // GP0(0x29): undocumented command, same as
								   // GP0(0x28)
						case 0x2A: // GP0(0x2A): monochrome four-point polygon,
								   // semi-transparent
						case 0x2B: // GP0(0x2B): undocumented command, same as
								   // GP0(0x2A)
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x30: // GP0(0x30): shaded three-point polygon,
								   // opaque
						case 0x31: // GP0(0x31): undocumented command, same as
								   // GP0(0x30)
						case 0x32: // GP0(0x32): shaded three-point polygon,
								   // semi-transparent
						case 0x33: // GP0(0x33): undocumented command, same as
								   // GP0(0x32)
						case 0x38: // GP0(0x38): shaded four-point polygon,
								   // opaque
						case 0x39: // GP0(0x39): undocumented command, same as
								   // GP0(0x38)
						case 0x3A: // GP0(0x3A): shaded four-point polygon,
								   // semi-transparent
						case 0x3B: // GP0(0x3B): undocumented command, same as
								   // GP0(0x3A)
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x40: // GP0(0x40): monochrome line, opaque
						case 0x42: // GP0(0x42): monochrome line,
								   // semi-transparent
						case 0x48: // GP0(0x48): monochrome poly-line, opaque
						case 0x4A: // GP0(0x4A): monochrome poly-line,
								   // semi-transparent
						case 0x50: // GP0(0x50): shaded line, opaque
						case 0x52: // GP0(0x52): shaded line, semi-transparent
						case 0x58: // GP0(0x58): shaded poly-line, opaque
						case 0x5A: // GP0(0x5A): shaded poly-line,
								   // semi-transparent
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x60: // GP0(0x60): monochrome rectangle, variable
								   // size, opaque
						case 0x62: // GP0(0x62): monochrome rectangle, variable
								   // size, semi-transparent
						case 0x68: // GP0(0x68): monochrome rectangle, 1x1,
								   // opaque
						case 0x6A: // GP0(0x6A): monochrome rectangle, 1x1,
								   // semi-transparent
						case 0x70: // GP0(0x70): monochrome rectangle, 8x8,
								   // opaque
						case 0x72: // GP0(0x72): monochrome rectangle, 8x8,
								   // semi-transparent
						case 0x78: // GP0(0x78): monochrome rectangle, 16x16,
								   // opaque
						case 0x7A: // GP0(0x7A): monochrome rectangle, 16x16,
								   // semi-transparent
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x64: // GP0(0x64): textured rectangle, variable
								   // size, opaque, texture-blending
						case 0x65: // GP0(0x65): textured rectangle, variable
								   // size, opaque, raw-texture
						case 0x66: // GP0(0x66): textured rectangle, variable
								   // size, semi-transparent, texture-blending
						case 0x67: // GP0(0x67): textured rectangle, variable
								   // size, semi-transparent, raw-texture
						case 0x6C: // GP0(0x6C): textured rectangle, 1x1
							       // (nonsense), opaque, texture-blending
						case 0x6D: // GP0(0x6D): textured rectangle, 1x1
								   // (nonsense), opaque, raw-texture
						case 0x6E: // GP0(0x6E): textured rectangle, 1x1
								   // (nonsense), semi-transparent,
								   // texture-blending
						case 0x6F: // GP0(0x6F): textured rectangle,
								   // 1x1 (nonsense), semi-transparent,
								   // raw-texture
						case 0x74: // GP0(0x74): textured rectangle, 8x8,
								   // opaque, texture-blending
						case 0x75: // GP0(0x75): textured rectangle, 8x8,
								   // opaque, raw-texture
						case 0x76: // GP0(0x76): textured rectangle, 8x8,
								   // semi-transparent, texture-blending
						case 0x77: // GP0(0x77): textured rectangle, 8x8,
								   // semi-transparent, raw-texture
						case 0x7C: // GP0(0x7C): textured rectangle, 16x16,
								   // opaque, texture-blending
						case 0x7D: // GP0(0x7D): textured rectangle, 16x16,
								   // opaque, raw-texture
						case 0x7E: // GP0(0x7E): textured rectangle, 16x16,
								   // semi-transparent, texture-blending
						case 0x7F: // GP0(0x7F): textured rectangle, 16x16,
								   // semi-transparent, raw-texture
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0x80:
						case 0x81:
						case 0x82:
						case 0x83:
						case 0x84:
						case 0x85:
						case 0x86:
						case 0x87:
						case 0x88:
						case 0x89:
						case 0x8A:
						case 0x8B:
						case 0x8C:
						case 0x8D:
						case 0x8E:
						case 0x8F:
						case 0x90:
						case 0x91:
						case 0x92:
						case 0x93:
						case 0x94:
						case 0x95:
						case 0x96:
						case 0x97:
						case 0x98:
						case 0x99:
						case 0x9A:
						case 0x9B:
						case 0x9C:
						case 0x9D:
						case 0x9E:
						case 0x9F: // GP0(0x80): copy rectangle (VRAM to VRAM)
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0xA0:
						case 0xA1:
						case 0xA2:
						case 0xA3:
						case 0xA4:
						case 0xA5:
						case 0xA6:
						case 0xA7:
						case 0xA8:
						case 0xA9:
						case 0xAA:
						case 0xAB:
						case 0xAC:
						case 0xAD:
						case 0xAE:
						case 0xAF:
						case 0xB0:
						case 0xB1:
						case 0xB2:
						case 0xB3:
						case 0xB4:
						case 0xB5:
						case 0xB6:
						case 0xB7:
						case 0xB8:
						case 0xB9:
						case 0xBA:
						case 0xBB:
						case 0xBC:
						case 0xBD:
						case 0xBE:
						case 0xBF: // GP0(0xA0): copy rectangle (CPU to VRAM)
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0xC0:
						case 0xC1:
						case 0xC2:
						case 0xC3:
						case 0xC4:
						case 0xC5:
						case 0xC6:
						case 0xC7:
						case 0xC8:
						case 0xC9:
						case 0xCA:
						case 0xCB:
						case 0xCC:
						case 0xCD:
						case 0xCE:
						case 0xCF:
						case 0xD0:
						case 0xD1:
						case 0xD2:
						case 0xD3:
						case 0xD4:
						case 0xD5:
						case 0xD6:
						case 0xD7:
						case 0xD8:
						case 0xD9:
						case 0xDA:
						case 0xDB:
						case 0xDC:
						case 0xDD:
						case 0xDE:
						case 0xDF: // GP0(0xC0): copy rectangle (VRAM to CPU)
							gpu->fifoBuffer[gpu->commandsInFifo++] = word;
							break;
						case 0xE1: // GP0(0xE1): draw mode ("texpage") setting
							GPU_GP0_E1(gpu, word);
							break;
						case 0xE2: // GP0(0xE2): texture window setting
							GPU_GP0_E2(gpu, word);
							break;
						case 0xE3: // GP0(0xE3): set drawing area (top-left)
							GPU_GP0_E3(gpu, word);
							break;
						case 0xE4: // GP0(0xE4): set drawing area (bottom-right)
							GPU_GP0_E4(gpu, word);
							break;
						case 0xE5: // GP0(0xE5): set drawing offset
							GPU_GP0_E5(gpu, word);
							break;
						case 0xE6: // GP0(0xE6): set mask setting bits
							GPU_GP0_E6(gpu, word);
							break;
						default:
							fprintf(stderr, "PhilPSX: GPU: GP0 SUBMIT: %08X\n",
									word);
							break;
					}
					break;
				default: // Command at index 0 in FIFO requires more parameters
					switch (logical_rshift(gpu->fifoBuffer[0], 24) & 0xFF) {
						case 0x02: // GP0(0x02): fill rectangle in VRAM
							if (gpu->commandsInFifo < 3) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 3) {
									GPU_GP0_02(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x24: // GP0(0x24): textured three-point polygon,
								   // opaque, texture-blending
						case 0x25: // GP0(0x25): textured three-point polygon,
								   // opaque, raw-texture
						case 0x26: // GP0(0x26): textured three-point polygon,
								   // semi-transparent, texture-blending
						case 0x27: // GP0(0x27): textured three-point polygon,
								   // semi-transparent, raw-texture
							if (gpu->commandsInFifo < 7) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 7) {
									GPU_texturedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											gpu->fifoBuffer[6],
											0, 0);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x2C: // GP0(0x2C): textured four-point polygon,
								   // opaque, texture-blending
						case 0x2D: // GP0(0x2D): textured four-point polygon,
								   // opaque, raw-texture
						case 0x2E: // GP0(0x2E): textured four-point polygon,
								   // semi-transparent, texture-blending
						case 0x2F: // GP0(0x2F): textured four-point polygon,
								   // semi-transparent, raw-texture
							if (gpu->commandsInFifo < 9) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 9) {
									GPU_texturedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											gpu->fifoBuffer[6],
											gpu->fifoBuffer[7],
											gpu->fifoBuffer[8]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x34: // Shaded textured three-point polygon,
								   // opaque, texture-blending
						case 0x35: // Undocumented, textured three-point
								   // polygon, opaque, no blending
						case 0x36: // Shaded textured three-point polygon,
								   // semi-transparent, texture-blending
						case 0x37: // Undocumented, textured three-point
								   // polygon, semi-transparent, no blending
							if (gpu->commandsInFifo < 9) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 9) {
									GPU_shadedTexturedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											gpu->fifoBuffer[6],
											gpu->fifoBuffer[7],
											gpu->fifoBuffer[8],
											0, 0, 0);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x3C: // Shaded textured four-point polygon,
								   // opaque, texture-blending
						case 0x3D: // Undocumented, textured four-point polygon,
								   // opaque, no blending
						case 0x3E: // Shaded textured four-point polygon,
								   // semi-transparent, texture-blending
						case 0x3F: // Undocumented, textured four-point polygon,
								   // semi-transparent, no blending
							if (gpu->commandsInFifo < 12) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 12) {
									GPU_shadedTexturedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											gpu->fifoBuffer[6],
											gpu->fifoBuffer[7],
											gpu->fifoBuffer[8],
											gpu->fifoBuffer[9],
											gpu->fifoBuffer[10],
											gpu->fifoBuffer[11]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x20: // GP0(0x20): monochrome three-point polygon,
								   // opaque
						case 0x21: // GP0(0x21): undocumented command, same as
								   // GP0(0x20)
						case 0x22: // GP0(0x22): monochrome three-point polygon,
								   // semi-transparent
						case 0x23: // GP0(0x23): undocumented command, same as
								   // GP0(0x22)
							if (gpu->commandsInFifo < 4) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 4) {
									GPU_monochromePolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											0);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x28: // GP0(0x28): monochrome four-point polygon,
								   // opaque
						case 0x29: // GP0(0x29): undocumented command, same as
								   // GP0(0x28)
						case 0x2A: // GP0(0x2A): monochrome four-point polygon,
								   // semi-transparent
						case 0x2B: // GP0(0x2B): undocumented command, same as
								   // GP0(0x2B)
							if (gpu->commandsInFifo < 5) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 5) {
									GPU_monochromePolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x30: // GP0(0x30): shaded three-point polygon,
								   // opaque
						case 0x31: // GP0(0x31): undocumented command, same as
								   // GP0(0x30)
						case 0x32: // GP0(0x32): shaded three-point polygon,
								   // semi-transparent
						case 0x33: // GP0(0x33): undocumented command, same as
								   // GP0(0x32)
							if (gpu->commandsInFifo < 6) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 6) {
									GPU_shadedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											0, 0);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x38: // GP0(0x38): shaded four-point polygon,
								   // opaque
						case 0x39: // GP0(0x39): undocumented command, same as
								   // GP0(0x38)
						case 0x3A: // GP0(0x3A): shaded four-point polygon,
								   // semi-transparent
						case 0x3B: // GP0(0x3B): undocumented command, same as
								   // GP0(0x3A)
							if (gpu->commandsInFifo < 8) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 8) {
									GPU_shadedPolygon(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3],
											gpu->fifoBuffer[4],
											gpu->fifoBuffer[5],
											gpu->fifoBuffer[6],
											gpu->fifoBuffer[7]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x40: // GP0(0x40): monochrome line, opaque
						case 0x42: // GP0(0x42): monochrome line,
								   // semi-transparent
							if (ArrayList_getSize(gpu->lineParameters) < 4) {
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)(gpu->fifoBuffer[0]
										& 0xFFFFFF));
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)word);
								if (ArrayList_getSize(gpu->lineParameters)
										== 4) {
									GPU_anyLine(gpu,
											gpu->fifoBuffer[0],
											gpu->lineParameters);
									ArrayList_wipeAllObjects(
											gpu->lineParameters);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x50: // GP0(0x50): shaded line, opaque
						case 0x52: // GP0(0x52): shaded line, semi-transparent
							if (ArrayList_getSize(gpu->lineParameters) < 4) {
								if (ArrayList_getSize(gpu->lineParameters)
										== 0) {
									ArrayList_addObject(gpu->lineParameters,
											(void *)(intptr_t)(
											gpu->fifoBuffer[0] & 0xFFFFFF));
								}
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)word);
								if (ArrayList_getSize(gpu->lineParameters)
										== 4) {
									GPU_anyLine(gpu,
											gpu->fifoBuffer[0],
											gpu->lineParameters);
									ArrayList_wipeAllObjects(
											gpu->lineParameters);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x48: // GP0(0x48): monochrome poly-line, opaque
						case 0x4A: // GP0(0x4A): monochrome poly-line,
								   // semi-transparent
							// Variable number of components, so stop at
							// termination code
							if (word != 0x55555555 && word != 0x50005000) {
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)(gpu->fifoBuffer[0]
										& 0xFFFFFF));
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)word);
							} else {
								GPU_anyLine(gpu,
										gpu->fifoBuffer[0],
										gpu->lineParameters);
								ArrayList_wipeAllObjects(gpu->lineParameters);
								GPU_GP1_01(gpu, 0);
							}
							break;
						case 0x58: // GP0(0x58): shaded poly-line, opaque
						case 0x5A: // GP0(0x5A): shaded poly-line,
								   // semi-transparent
							// Variable number of components, so stop at
							// termination code
							if (word != 0x55555555 && word != 0x50005000) {
								if (ArrayList_getSize(gpu->lineParameters)
										== 0) {
									ArrayList_addObject(gpu->lineParameters,
											(void *)(intptr_t)(
											gpu->fifoBuffer[0] & 0xFFFFFF));
								}
								ArrayList_addObject(gpu->lineParameters,
										(void *)(intptr_t)word);
							} else {
								GPU_anyLine(gpu,
										gpu->fifoBuffer[0],
										gpu->lineParameters);
								ArrayList_wipeAllObjects(gpu->lineParameters);
								GPU_GP1_01(gpu, 0);
							}
							break;
						case 0x60: // GP0(0x60): monochrome rectangle, variable
								   // size, opaque
						case 0x62: // GP0(0x62): monochrome rectangle, variable
								   // size, semi-transparent
							if (gpu->commandsInFifo < 3) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 3) {
									GPU_monochromeRectangle(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x68: // GP0(0x68): monochrome rectangle, 1x1,
								   // opaque
						case 0x6A: // GP0(0x6A): monochrome rectangle, 1x1,
								   // semi-transparent
						case 0x70: // GP0(0x70): monochrome rectangle, 8x8,
								   // opaque
						case 0x72: // GP0(0x72): monochrome rectangle, 8x8,
								   // semi-transparent
						case 0x78: // GP0(0x78): monochrome rectangle, 16x16,
								   // opaque
						case 0x7A: // GP0(0x7A): monochrome rectangle, 16x16,
								   // semi-transparent
							if (gpu->commandsInFifo < 2) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 2) {
									int32_t rectangleSize = 0;
									switch (logical_rshift(
											gpu->fifoBuffer[0], 24) & 0xFF) {
										case 0x68:
										case 0x6A: // 1x1 size
											rectangleSize = 0x00010001;
											break;
										case 0x70:
										case 0x72: // 8x8 size
											rectangleSize = 0x00080008;
											break;
										case 0x78:
										case 0x7A: // 16x16 size
											rectangleSize = 0x00100010;
											break;
									}
									GPU_monochromeRectangle(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											rectangleSize);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x64: // GP0(0x64): textured rectangle, variable
								   // size, opaque, texture-blending
						case 0x65: // GP0(0x65): textured rectangle, variable
								   // size, opaque, raw-texture
						case 0x66: // GP0(0x66): textured rectangle, variable
								   // size, semi-transparent, texture-blending
						case 0x67: // GP0(0x67): textured rectangle, variable
								   // size, semi-transparent, raw-texture
							if (gpu->commandsInFifo < 4) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 4) {
									GPU_texturedRectangle(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x6C: // GP0(0x6C): textured rectangle, 1x1
								   // (nonsense), opaque, texture-blending
						case 0x6D: // GP0(0x6D): textured rectangle, 1x1
								   // (nonsense), opaque, raw-texture
						case 0x6E: // GP0(0x6E): textured rectangle, 1x1
								   // (nonsense), semi-transparent, texture-blending
						case 0x6F: // GP0(0x6F): textured rectangle, 1x1
								   // (nonsense), semi-transparent, raw-texture
						case 0x74: // GP0(0x74): textured rectangle, 8x8,
								   // opaque, texture-blending
						case 0x75: // GP0(0x75): textured rectangle, 8x8,
								   // opaque, raw-texture
						case 0x76: // GP0(0x76): textured rectangle, 8x8,
								   // semi-transparent, texture-blending
						case 0x77: // GP0(0x77): textured rectangle, 8x8,
								   // semi-transparent, raw-texture
						case 0x7C: // GP0(0x7C): textured rectangle, 16x16,
								   // opaque, texture-blending
						case 0x7D: // GP0(0x7D): textured rectangle, 16x16,
								   // opaque, raw-texture
						case 0x7E: // GP0(0x7E): textured rectangle, 16x16,
								   // semi-transparent, texture-blending
						case 0x7F: // GP0(0x7F): textured rectangle, 16x16,
								   // semi-transparent, raw-texture
							if (gpu->commandsInFifo < 3) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 3) {
									int32_t rectangleSize = 0;
									switch (logical_rshift(
											gpu->fifoBuffer[0], 24) & 0xFF) {
										case 0x6C:
										case 0x6D:
										case 0x6E:
										case 0x6F: // 1x1 size
											rectangleSize = 0x00010001;
											break;
										case 0x74:
										case 0x75:
										case 0x76:
										case 0x77: // 8x8 size
											rectangleSize = 0x00080008;
											break;
										case 0x7C:
										case 0x7D:
										case 0x7E:
										case 0x7F: // 16x16 size
											rectangleSize = 0x00100010;
											break;
									}
									GPU_texturedRectangle(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											rectangleSize);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0x80:
						case 0x81:
						case 0x82:
						case 0x83:
						case 0x84:
						case 0x85:
						case 0x86:
						case 0x87:
						case 0x88:
						case 0x89:
						case 0x8A:
						case 0x8B:
						case 0x8C:
						case 0x8D:
						case 0x8E:
						case 0x8F:
						case 0x90:
						case 0x91:
						case 0x92:
						case 0x93:
						case 0x94:
						case 0x95:
						case 0x96:
						case 0x97:
						case 0x98:
						case 0x99:
						case 0x9A:
						case 0x9B:
						case 0x9C:
						case 0x9D:
						case 0x9E:
						case 0x9F: // GP0(0x80): copy rectangle (VRAM to VRAM)
							if (gpu->commandsInFifo < 4) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 4) {
									GPU_GP0_80(gpu,
											gpu->fifoBuffer[0],
											gpu->fifoBuffer[1],
											gpu->fifoBuffer[2],
											gpu->fifoBuffer[3]);
									GPU_GP1_01(gpu, 0);
								}
							}
							break;
						case 0xA0:
						case 0xA1:
						case 0xA2:
						case 0xA3:
						case 0xA4:
						case 0xA5:
						case 0xA6:
						case 0xA7:
						case 0xA8:
						case 0xA9:
						case 0xAA:
						case 0xAB:
						case 0xAC:
						case 0xAD:
						case 0xAE:
						case 0xAF:
						case 0xB0:
						case 0xB1:
						case 0xB2:
						case 0xB3:
						case 0xB4:
						case 0xB5:
						case 0xB6:
						case 0xB7:
						case 0xB8:
						case 0xB9:
						case 0xBA:
						case 0xBB:
						case 0xBC:
						case 0xBD:
						case 0xBE:
						case 0xBF: // GP0(0xA0): copy rectangle (CPU to VRAM)
							if (gpu->commandsInFifo < 3) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 3) {
									// Clear bit 28 (DMA ready) of status
									// register
									gpu->statusRegister &= 0xEFFFFFFF;

									// Clear bit 26 (ready to receive command
									// word) of status register
									gpu->statusRegister &= 0xFBFFFFFF;

									// Trigger DMA input
									gpu->dmaWriteInProgress = 0xA0;
									gpu->dmaBufferIndex = 0;
									gpu->dmaWidthInPixels =
											gpu->fifoBuffer[2] & 0xFFFF;
									gpu->dmaWidthInPixels =
											((gpu->dmaWidthInPixels - 1)
											& 0x3FF) + 1;
									gpu->dmaHeightInPixels = logical_rshift(
											gpu->fifoBuffer[2],
											16) & 0xFFFF;
									gpu->dmaHeightInPixels =
											((gpu->dmaHeightInPixels - 1)
											& 0x1FF) + 1;
									gpu->dmaWidthInPixels =
											(gpu->dmaWidthInPixels == 0) ?
											0x400 : gpu->dmaWidthInPixels;
									gpu->dmaHeightInPixels =
											(gpu->dmaHeightInPixels == 0) ?
											0x200 : gpu->dmaHeightInPixels;
									gpu->dmaNeededBytes =
											gpu->dmaWidthInPixels *
											gpu->dmaHeightInPixels;
									gpu->dmaNeededBytes *= 4;
								}
							}
							break;
						case 0xC0:
						case 0xC1:
						case 0xC2:
						case 0xC3:
						case 0xC4:
						case 0xC5:
						case 0xC6:
						case 0xC7:
						case 0xC8:
						case 0xC9:
						case 0xCA:
						case 0xCB:
						case 0xCC:
						case 0xCD:
						case 0xCE:
						case 0xCF:
						case 0xD0:
						case 0xD1:
						case 0xD2:
						case 0xD3:
						case 0xD4:
						case 0xD5:
						case 0xD6:
						case 0xD7:
						case 0xD8:
						case 0xD9:
						case 0xDA:
						case 0xDB:
						case 0xDC:
						case 0xDD:
						case 0xDE:
						case 0xDF: // GP0(0xC0): copy rectangle (VRAM to CPU)
							if (gpu->commandsInFifo < 3) {
								gpu->fifoBuffer[gpu->commandsInFifo++] = word;
								if (gpu->commandsInFifo == 3) {
									// Clear bit 28 (DMA ready) of status
									// register
									gpu->statusRegister &= 0xEFFFFFFF;

									// Set bit 27 (ready to send VRAM to CPU)
									gpu->statusRegister |= 0x08000000;

									// Clear bit 26 (ready to receive command
									// word) of status register
									gpu->statusRegister &= 0xFBFFFFFF;

									// Trigger DMA output
									gpu->dmaReadInProgress = 0xC0;
									gpu->dmaBufferIndex = 0;
									gpu->dmaWidthInPixels =
											gpu->fifoBuffer[2] & 0xFFFF;
									gpu->dmaWidthInPixels =
											((gpu->dmaWidthInPixels - 1)
											& 0x3FF) + 1;
									gpu->dmaHeightInPixels = logical_rshift(
											gpu->fifoBuffer[2], 16) & 0xFFFF;
									gpu->dmaHeightInPixels =
											((gpu->dmaHeightInPixels - 1)
											& 0x1FF) + 1;
									gpu->dmaWidthInPixels =
											(gpu->dmaWidthInPixels == 0) ?
											0x400 : gpu->dmaWidthInPixels;
									gpu->dmaHeightInPixels =
											(gpu->dmaHeightInPixels == 0) ?
											0x200 : gpu->dmaHeightInPixels;
									gpu->dmaNeededBytes =
											gpu->dmaWidthInPixels *
											gpu->dmaHeightInPixels;
									gpu->dmaNeededBytes *= 4;
								}
							}
							break;
					}
					break;
			}
			break;
	}
}

/*
 * This function submits GP1 commands/packets.
 */
void GPU_submitToGP1(GPU *gpu, int32_t word)
{
	// Sync up to CPU
	GPU_executeGPUCycles(gpu);

	// Switch endianness
	word = ((word << 24) & 0xFF000000) |
			((word << 8) & 0xFF0000) |
			(logical_rshift(word, 8) & 0xFF00) |
			(logical_rshift(word, 24) & 0xFF);

	// Submit to correct method
	switch (logical_rshift(word, 24) & 0xFF) {
		case 0:
		case 0x40:
		case 0x80:
		case 0xC0:
			GPU_GP1_00(gpu, word);
			break;
		case 0x1:
		case 0x41:
		case 0x81:
		case 0xC1:
			GPU_GP1_01(gpu, word);
			break;
		case 0x2:
		case 0x42:
		case 0x82:
		case 0xC2:
			GPU_GP1_02(gpu, word);
			break;
		case 0x3:
		case 0x43:
		case 0x83:
		case 0xC3:
			GPU_GP1_03(gpu, word);
			break;
		case 0x4:
		case 0x44:
		case 0x84:
		case 0xC4:
			GPU_GP1_04(gpu, word);
			break;
		case 0x5:
		case 0x45:
		case 0x85:
		case 0xC5:
			GPU_GP1_05(gpu, word);
			break;
		case 0x6:
		case 0x46:
		case 0x86:
		case 0xC6:
			GPU_GP1_06(gpu, word);
			break;
		case 0x7:
		case 0x47:
		case 0x87:
		case 0xC7:
			GPU_GP1_07(gpu, word);
			break;
		case 0x8:
		case 0x48:
		case 0x88:
		case 0xC8:
			GPU_GP1_08(gpu, word);
			break;
		case 0x9:
		case 0x49:
		case 0x89:
		case 0xC9:
			GPU_GP1_09(gpu, word);
			break;
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
		case 0x1E:
		case 0x1F:
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5A:
		case 0x5B:
		case 0x5C:
		case 0x5D:
		case 0x5E:
		case 0x5F:
		case 0x90:
		case 0x91:
		case 0x92:
		case 0x93:
		case 0x94:
		case 0x95:
		case 0x96:
		case 0x97:
		case 0x98:
		case 0x99:
		case 0x9A:
		case 0x9B:
		case 0x9C:
		case 0x9D:
		case 0x9E:
		case 0x9F:
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
		case 0xD4:
		case 0xD5:
		case 0xD6:
		case 0xD7:
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF:
			GPU_GP1_10(gpu, word);
			break;
		default:
			fprintf(stderr, "PhilPSX: GPU: GP1 SUBMIT: %08X\n",
					word);
			break;
	}
}

/*
 * This function fills a rectangle in the VRAM by queuing it on the rendering
 * thread.
 */
static void GPU_GP0_02(GPU *gpu, int32_t command, int32_t destination,
		int32_t dimensions)
{
	// Perform copying on GL thread, making sure to set pointers
	GpuCommand gp0_02;
	gp0_02.functionPointer = &GPU_GP0_02_implementation;
	gp0_02.gpu = gpu;

	// Store parameters in object then submit it to work queue
	gp0_02.parameter1 = command;
	gp0_02.parameter2 = destination;
	gp0_02.parameter3 = dimensions;
	WorkQueue_addItem(gpu->wq, &gp0_02, false);
}

/*
 * This function contains the implementation of GP0_02.
 */
static void GPU_GP0_02_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Determine needed dimensions
	int32_t width = command->parameter3 & 0xFFFF;
	width = ((width & 0x3FF) + 0xF) & ~(0xF);
	int32_t height = logical_rshift(command->parameter3, 16) & 0xFFFF;
	height &= 0x1FF;
	if (width == 0 || height == 0)
		return;

	// Determine starting position
	int32_t x = command->parameter2 & 0x3F0;
	int32_t y = logical_rshift(command->parameter2, 16) & 0x1FF;

	// Adjust y to account for OpenGL coordinate system
	y = 511 - y;
	y -= height - 1;

	// Split colours out
	int32_t red = command->parameter1 & 0xFF;
	int32_t green = logical_rshift(command->parameter1, 8) & 0xFF;
	int32_t blue = logical_rshift(command->parameter1, 16) & 0xFF;

	// Bind vram FBO to framebuffer, setting viewport and uniforms
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glBindFramebuffer called");
	gl->glViewport(x, y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glViewport called");
	gl->glUseProgram(gpu->gp0_02Program);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glUseProgram called");
	gl->glUniform1i(0, red);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(1, green);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(2, blue);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glDrawArrays called");
	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_02_implementation function, "
			"glMemoryBarrier called");
}

/*
 * This function triggers a GPU interrupt.
 */
static void GPU_GP0_1F(GPU *gpu)
{
	// Set IRQ flag in status register
	gpu->statusRegister |= 0x01000000;

	// Set flag in interrupt status register to trigger IRQ if enabled
	SystemInterlink_setGPUInterruptDelay(gpu->system, 0);
}

/*
 * This method copies a rectangle within VRAM by queuing it on the rendering
 * thread.
 */
static void GPU_GP0_80(GPU *gpu, int32_t command, int32_t sourceCoord,
		int32_t destinationCoord, int32_t widthAndHeight)
{
	// Perform copying on GL thread, making sure to set pointers
	GpuCommand gp0_80;
	gp0_80.functionPointer = &GPU_GP0_80_implementation;
	gp0_80.gpu = gpu;
	
	// Store parameters in object then submit it to work queue
	gp0_80.parameter1 = command;
	gp0_80.parameter2 = sourceCoord;
	gp0_80.parameter3 = destinationCoord;
	gp0_80.parameter4 = widthAndHeight;
	gp0_80.statusRegister = gpu->statusRegister;
	WorkQueue_addItem(gpu->wq, &gp0_80, false);
}

/*
 * This function contains the implementation of GP0_80.
 */
static void GPU_GP0_80_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Determine needed dimensions
	int32_t width = command->parameter4 & 0xFFFF;
	width = ((width - 1) & 0x3FF) + 1;
	int32_t height = logical_rshift(command->parameter4, 16) & 0xFFFF;
	height = ((height - 1) & 0x1FF) + 1;
	width = (width == 0) ? 0x400 : width;
	height = (height == 0) ? 0x200 : height;

	// Determine source coordinate
	int32_t source_x = command->parameter2 & 0x3FF;
	int32_t source_y = logical_rshift(command->parameter2, 16) & 0x1FF;

	// Determine destination coordinate
	int32_t destination_x = command->parameter3 & 0x3FF;
	int32_t destination_y = logical_rshift(command->parameter3, 16) & 0x1FF;

	// Adjust source and destination to account for OpenGL coordinate system
	source_y = 511 - source_y;
	source_y -= height - 1;
	destination_y = 511 - destination_y;
	destination_y -= height - 1;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;

	// Switch active texture unit to temp draw texture
	gl->glActiveTexture(GL_TEXTURE1);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glActiveTexture called");

	// Unbind temp draw texture from framebuffer object
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->tempDrawFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glFramebufferTexture2D called");

	// Bind temp draw texture to image unit
	gl->glBindImageTexture(0, gpu->tempDrawTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindImageTexture called");

	// Switch active texture unit back to 0
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glActiveTexture called");

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport and uniforms correctly
	// for each program run

	// Copy original rectangle to temp draw texture
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindFramebuffer called");
	gl->glViewport(source_x, source_y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glViewport called");
	gl->glUseProgram(gpu->gp0_80Program1);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUseProgram called");
	gl->glUniform1i(0, source_x);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(1, source_y);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glDrawArrays called");
	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glMemoryBarrier called");

	// Write back from temp draw texture to new location
	gl->glViewport(destination_x, destination_y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glViewport called");
	gl->glUseProgram(gpu->gp0_80Program2);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUseProgram called");
	gl->glUniform1i(0, destination_x);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(1, destination_y);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(2, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(3, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glDrawArrays called");
	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glMemoryBarrier called");

	// Unbind textures from image units
	gl->glBindImageTexture(0, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindImageTexture called");
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindImageTexture called");

	// Rebind temp draw texture to FBO
	gl->glActiveTexture(GL_TEXTURE1);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->tempDrawFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->tempDrawTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glFramebufferTexture2D called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_80_implementation function, "
			"glFramebufferTexture2D called");
}

/*
 * This function copys a rectangle into the VRAM by queuing it on the rendering
 * thread.
 */
static void GPU_GP0_A0(GPU *gpu, int32_t command,
		int32_t destination, int32_t dimensions)
{
	// Perform copying on GL thread, making sure to set pointers
	GpuCommand gp0_a0;
	gp0_a0.functionPointer = &GPU_GP0_A0_implementation;
	gp0_a0.gpu = gpu;
	
	// Store parameters in object then submit it to work queue, waiting for
	// it to finish executing
	gp0_a0.parameter1 = command;
	gp0_a0.parameter2 = destination;
	gp0_a0.parameter3 = dimensions;
	gp0_a0.statusRegister = gpu->statusRegister;
	WorkQueue_addItem(gpu->wq, &gp0_a0, true);
}

/*
 * This function contains the implementation of GP0_A0.
 */
static void GPU_GP0_A0_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Determine needed dimensions
	int32_t width = command->parameter3 & 0xFFFF;
	width = ((width - 1) & 0x3FF) + 1;
	int32_t height = logical_rshift(command->parameter3, 16) & 0xFFFF;
	height = ((height - 1) & 0x1FF) + 1;
	width = (width == 0) ? 0x400 : width;
	height = (height == 0) ? 0x200 : height;

	// Determine starting position
	int32_t x = command->parameter2 & 0x3FF;
	int32_t y = logical_rshift(command->parameter2, 16) & 0x1FF;

	// Adjust y to account for OpenGL coordinate system
	y = 511 - y;
	y -= height - 1;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;

	// Copy DMA buffer pixels to temp draw texture, utilising dmaBufferMutex
	// to ensure mutual exclusion
	gl->glActiveTexture(GL_TEXTURE1);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glActiveTexture called");
	pthread_mutex_lock(&gpu->dmaBufferMutex);
	gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, gpu->dmaBuffer);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glTexSubImage2D called");
	pthread_mutex_unlock(&gpu->dmaBufferMutex);

	// Unbind temp draw texture from framebuffer object
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->tempDrawFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glFramebufferTexture2D called");

	// Bind temp draw texture to image unit
	gl->glBindImageTexture(0, gpu->tempDrawTexture[0], 0, false, 0,
			GL_READ_ONLY, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindImageTexture called");

	// Switch active texture unit back to 0
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glActiveTexture called");

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport and uniforms correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindFramebuffer called");
	gl->glViewport(x, y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glViewport called");
	gl->glUseProgram(gpu->gp0_a0Program);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUseProgram called");
	gl->glUniform1i(0, x);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(1, y);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(2, height);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(3, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(4, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glDrawArrays called");
	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glMemoryBarrier called");

	// Unbind textures from image units
	gl->glBindImageTexture(0, 0, 0, false, 0,
			GL_READ_ONLY, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindImageTexture called");
	gl->glBindImageTexture(1, 0, 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindImageTexture called");

	// Rebind temp draw texture to FBO
	gl->glActiveTexture(GL_TEXTURE1);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->tempDrawFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->tempDrawTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glFramebufferTexture2D called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_A0_implementation function, "
			"glFramebufferTexture2D called");
}

/*
 * This method copys a rectangle from the VRAM by queuing it on the rendering
 * thread.
 */
static void GPU_GP0_C0(GPU *gpu, int32_t command,
		int32_t destination, int32_t dimensions)
{
	// Perform copying on GL thread, making sure to set pointers
	GpuCommand gp0_c0;
	gp0_c0.functionPointer = &GPU_GP0_C0_implementation;
	gp0_c0.gpu = gpu;
	
	// Store parameters in object then submit it to work queue
	gp0_c0.parameter1 = command;
	gp0_c0.parameter2 = destination;
	gp0_c0.parameter3 = dimensions;
	WorkQueue_addItem(gpu->wq, &gp0_c0, true);
}

/*
 * This function contains the implementation of GP0_C0.
 */
static void GPU_GP0_C0_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Determine needed dimensions
	int32_t width = command->parameter3 & 0xFFFF;
	width = ((width - 1) & 0x3FF) + 1;
	int32_t height = logical_rshift(command->parameter3, 16) & 0xFFFF;
	height = ((height - 1) & 0x1FF) + 1;
	width = (width == 0) ? 0x400 : width;
	height = (height == 0) ? 0x200 : height;

	// Determine starting position
	int32_t x = command->parameter2 & 0x3FF;
	int32_t y = logical_rshift(command->parameter2, 16) & 0x1FF;

	// Adjust y to account for OpenGL coordinate system
	y = 511 - y;
	y -= height - 1;

	// Bind VRAM FBO
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_C0_implementation function, "
			"glBindFramebuffer called");

	// Read pixels into byte buffer, utilising dmaBufferMutex to ensure mutual
	// exclusion
	pthread_mutex_lock(&gpu->dmaBufferMutex);
	gl->glReadPixels(x, y, width, height, GL_RGBA_INTEGER,
			GL_UNSIGNED_BYTE, gpu->dmaBuffer);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_C0_implementation function, "
			"glReadPixels called");
	pthread_mutex_unlock(&gpu->dmaBufferMutex);

	// Bind to 0 FBO
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_C0_implementation function, "
			"glBindFramebuffer called");

	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_GP0_C0_implementation function, "
			"glMemoryBarrier called");
}

/*
 * This function sets the draw mode ("texpage") setting.
 */
static void GPU_GP0_E1(GPU *gpu, int32_t command)
{
	// Set bits 0-10 of status register
	gpu->statusRegister &= 0xFFFFF800;
	gpu->statusRegister |= command & 0x7FF;

	// Set bit 15 of status register
	gpu->statusRegister &= 0xFFFF7FFF;
	gpu->statusRegister |= (command << 4) & 0x8000;
}

/*
 * This function sets the texture window setting.
 */
static void GPU_GP0_E2(GPU *gpu, int32_t command)
{
	// Store in texture window variable
	gpu->textureWindow = command & 0xFFFFF;
}

/*
 * This function sets the top-left drawing area.
 */
static void GPU_GP0_E3(GPU *gpu, int32_t command)
{
	// Store in top-left drawing area variable
	gpu->drawingAreaTopLeft = command & 0xFFFFF;
}

/*
 * This function sets the bottom-right drawing area.
 */
static void GPU_GP0_E4(GPU *gpu, int32_t command)
{
	// Store in bottom-right drawing area variable
	gpu->drawingAreaBottomRight = command & 0xFFFFF;
}

/*
 * This function sets the drawing offset.
 */
static void GPU_GP0_E5(GPU *gpu, int32_t command)
{
	// Store in drawing offset variable
	gpu->drawingOffset = command & 0x3FFFFF;
}

/*
 * This function affects the mask bit settings in the status register.
 */
static void GPU_GP0_E6(GPU *gpu, int32_t command)
{
	// Set the two bits (11 and 12) in the status register
	gpu->statusRegister &= 0xFFFFE7FF;
	gpu->statusRegister |= (command & 0x3) << 11;
}

/*
 * This function resets the GPU.
 */
static void GPU_GP1_00(GPU *gpu, int32_t command)
{
	// Clear fifo
	GPU_GP1_01(gpu, command);

	// Reset interrupt flag in status register
	GPU_GP1_02(gpu, command);

	// Turn display off
	GPU_GP1_03(gpu, 0x1);

	// Set DMA direction to off
	GPU_GP1_04(gpu, 0);

	// Set start of display area
	GPU_GP1_05(gpu, 0);

	// Set horizontal display range variables
	GPU_GP1_06(gpu, 0xC60260);

	// Set vertical display range variables
	GPU_GP1_07(gpu, 0x49C1F);

	// Set display mode to 256x240 PAL
	GPU_GP1_08(gpu, 0x8);

	// Set display attributes
	GPU_GP0_E1(gpu, 0);
	GPU_GP0_E2(gpu, 0);
	GPU_GP0_E3(gpu, 0);
	GPU_GP0_E4(gpu, 0);
	GPU_GP0_E5(gpu, 0);
	GPU_GP0_E6(gpu, 0);
}

/*
 * This function emulates GP1_01, which resets the command
 * buffer.
 */
static void GPU_GP1_01(GPU *gpu, int32_t command)
{
	// Empty buffer and set commandsInBuffer to 0
	memset(gpu->fifoBuffer, 0, sizeof(gpu->fifoBuffer));
	gpu->commandsInFifo = 0;
}

/*
 * This function acknowledges a GPU interrupt (not a VBlank interrupt,
 * but one actually requested by the GPU with GP0_1F).
 */
static void GPU_GP1_02(GPU *gpu, int32_t command)
{
	// Reset IRQ flag in status register
	gpu->statusRegister &= 0xFEFFFFFF;
}

/*
 * This function enables or disables the display.
 */
static void GPU_GP1_03(GPU *gpu, int32_t command)
{
	// Enable or disable display based on command word
	command = (command & 0x1) << 23;
	gpu->statusRegister &= 0xFF7FFFFF;
	gpu->statusRegister |= command;
}

/*
 * This function sets the DMA direction / data request.
 */
static void GPU_GP1_04(GPU *gpu, int32_t command)
{
	// Set direction in status register
	command = (command & 0x3) << 29;
	gpu->statusRegister &= 0x9FFFFFFF;
	gpu->statusRegister |= command;
}

/*
 * This function sets the start of the display area in VRAM.
 */
static void GPU_GP1_05(GPU *gpu, int32_t command)
{
	// Set X half-word address (0-1023)
	gpu->xStart = command & 0x3FF;
	gpu->yStart = logical_rshift(command, 10) & 0x1FF;
}

/*
 * This function sets the horizontal display range.
 */
static void GPU_GP1_06(GPU *gpu, int32_t command)
{
	// Set X1 and X2
	gpu->x1 = 0xFFF & command;
	gpu->x2 = 0xFFF & logical_rshift(command, 12);
}

/*
 * This function sets the vertical display range.
 */
static void GPU_GP1_07(GPU *gpu, int32_t command)
{
	// Set Y1 and Y2
	gpu->y1 = 0x3FF & command;
	gpu->y2 = 0x3FF & logical_rshift(command, 10);
}

/*
 * This function sets the display mode.
 */
static void GPU_GP1_08(GPU *gpu, int32_t command)
{
	// Mask out relevant bits in status register
	gpu->statusRegister &= 0xFF80BFFF;

	// Horizontal resolution 1
	int32_t tempHoriz1 = command & 0x3;
	gpu->statusRegister |= (command & 0x3) << 17;

	// Vertical resolution
	int32_t tempVert = logical_rshift((command & 0x4), 2);
	gpu->statusRegister |= (command & 0x4) << 17;

	// Video mode
	gpu->statusRegister |= (command & 0x8) << 17;

	// Display area colour depth
	gpu->statusRegister |= (command & 0x10) << 17;

	// Vertical interlace (also set cache value)
	gpu->interlaceEnabled = (command & 0x20) > 0;
	gpu->statusRegister |= (command & 0x20) << 17;

	// Horizontal resolution 2
	int32_t tempHoriz2 = logical_rshift((command & 0x40), 6);
	gpu->statusRegister |= (command & 0x40) << 10;

	// Reverse flag
	gpu->statusRegister |= (command & 0x80) << 7;

	// Now set cache values for horizontal resolution and dot factor
	switch (tempHoriz2) {
		case 1:
			gpu->horizontalRes = 368;
			gpu->dotFactor = 7;
			break;
		case 0:
			switch (tempHoriz1) {
				case 0:
					gpu->horizontalRes = 256;
					gpu->dotFactor = 10;
					break;
				case 1:
					gpu->horizontalRes = 320;
					gpu->dotFactor = 8;
					break;
				case 2:
					gpu->horizontalRes = 512;
					gpu->dotFactor = 5;
					break;
				case 3:
					gpu->horizontalRes = 640;
					gpu->dotFactor = 4;
					break;
			}
			break;
	}

	// Now set cache value for vertical resolution
	switch (tempVert) {
		case 0:
			gpu->verticalRes = 240;
			break;
		case 1:
			switch (gpu->interlaceEnabled ? 1 : 0) {
				case 0:
					gpu->verticalRes = 240;
					break;
				case 1:
					gpu->verticalRes = 480;
					break;
			}
			break;
	}
}

/*
 * This function allows disabling of texturing by other commands.
 */
static void GPU_GP1_09(GPU *gpu, int32_t command)
{
	// Mask bit 15 of status register
	gpu->statusRegister &= 0xFFFF7FFF;

	// Merge in command bit
	gpu->statusRegister |= (command & 0x1) << 15;
}

/*
 * This function allows GPU information to be read.
 */
static void GPU_GP1_10(GPU *gpu, int32_t command)
{
	// Determine what to put in latch value
	switch (command & 0xF) {
		case 0:
		case 0x1: // Old value
			gpu->gpureadLatched = true;
			break;
		case 0x2: // Texture window
			gpu->gpureadLatchValue = gpu->textureWindow & 0xFFFFF;
			gpu->gpureadLatched = true;
			break;
		case 0x3: // Drawing area top left
			gpu->gpureadLatchValue = gpu->drawingAreaTopLeft & 0xFFFFF;
			gpu->gpureadLatched = true;
			break;
		case 0x4: // Drawing area bottom right
			gpu->gpureadLatchValue = gpu->drawingAreaBottomRight & 0xFFFFF;
			gpu->gpureadLatched = true;
			break;
		case 0x5: // Drawing offset
			gpu->gpureadLatchValue = gpu->drawingOffset & 0x3FFFFF;
			gpu->gpureadLatched = true;
			break;
		case 0x6: // Old value
			gpu->gpureadLatched = true;
			break;
		case 0x7: // GPU type
			gpu->gpureadLatchValue = 2;
			gpu->gpureadLatched = true;
			break;
		case 0x8: // Unknown (0)
			gpu->gpureadLatchValue = 0;
			gpu->gpureadLatched = true;
			break;
		case 0x9:
		case 0xA:
		case 0xB:
		case 0xC:
		case 0xD:
		case 0xE:
		case 0xF: // Old value
			gpu->gpureadLatched = true;
			break;
	}
}

/*
 * This function renders all forms of line primitive supported by the
 * PlayStation hardware by queuing them on the rendering
 * thread.
 */
static void GPU_anyLine(GPU *gpu, int32_t command, ArrayList *paramList)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand anyLine;
	anyLine.functionPointer = &GPU_anyLine_implementation;
	anyLine.gpu = gpu;
	
	// Store parameters in object then submit it to work queue
	anyLine.parameter1 = command;
	anyLine.statusRegister = gpu->statusRegister;
	anyLine.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	anyLine.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	anyLine.drawingOffset = gpu->drawingOffset;
	WorkQueue_addItem(gpu->wq, &anyLine, false);
}

/*
 * This function contains the implementation of GPU_anyLine.
 */
static void GPU_anyLine_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;
	ArrayList *paramList = gpu->lineParameters;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	int32_t semiTransparencyMode = (command->statusRegister >> 5) & 0x3;
	switch (commandByte) {
		case 0x42:
		case 0x4A:
		case 0x52:
		case 0x5A: // Semi-transparent mode
			semiTransparencyEnabled = 1;
	}

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;
	int32_t dither = logical_rshift(command->statusRegister, 9) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL basis
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glBindFramebuffer called");
	gl->glViewport(0, 0, 1024, 512);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glViewport called");

	// Determine how many lines we are dealing with, then draw them
	// all in a loop
	size_t paramListSize = ArrayList_getSize(paramList);
	for (size_t i = 0; i < paramListSize / 2 - 1; ++i) {

		// Pull parameters from list
		int32_t firstColour = (int32_t)(intptr_t)ArrayList_getObject(paramList, i * 2);
		int32_t firstVertex =
				(int32_t)(intptr_t)ArrayList_getObject(paramList, i * 2 + 1);
		int32_t secondColour =
				(int32_t)(intptr_t)ArrayList_getObject(paramList, i * 2 + 2);
		int32_t secondVertex =
				(int32_t)(intptr_t)ArrayList_getObject(paramList, i * 2 + 3);

		// Get first colour and vertex, sign extending vertex if needed
		int32_t firstRed = firstColour & 0xFF;
		int32_t firstGreen = logical_rshift(firstColour, 8) & 0xFF;
		int32_t firstBlue = logical_rshift(firstColour, 16) & 0xFF;
		int32_t first_x = firstVertex & 0x7FF;
		int32_t first_y = logical_rshift(firstVertex, 16) & 0x7FF;
		if ((first_x & 0x400) == 0x400) {
			first_x |= 0xFFFFF800;
		}
		if ((first_y & 0x400) == 0x400) {
			first_y |= 0xFFFFF800;
		}

		// Get second colour and vertex, sign extending vertex if needed
		int32_t secondRed = secondColour & 0xFF;
		int32_t secondGreen = logical_rshift(secondColour, 8) & 0xFF;
		int32_t secondBlue = logical_rshift(secondColour, 16) & 0xFF;
		int32_t second_x = secondVertex & 0x7FF;
		int32_t second_y = logical_rshift(secondVertex, 16) & 0x7FF;
		if ((second_x & 0x400) == 0x400) {
			second_x |= 0xFFFFF800;
		}
		if ((second_y & 0x400) == 0x400) {
			second_y |= 0xFFFFF800;
		}

		// As we go from bottom-left corner in OpenGL viewport, adjust y
		first_y = 511 - first_y;
		second_y = 511 - second_y;

		// Adjust coordinates with offsets
		first_x += drawXOffset;
		first_y += drawYOffset;
		second_x += drawXOffset;
		second_y += drawYOffset;

		// Draw line to vram texture, setting uniforms correctly
		gl->glUseProgram(gpu->anyLineProgram1);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUseProgram called");
		gl->glUniform1i(0, first_x);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(1, first_y);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(2, second_x);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(3, second_y);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(4, firstRed);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(5, firstGreen);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(6, firstBlue);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(7, secondRed);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(8, secondGreen);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(9, secondBlue);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(10, semiTransparencyEnabled);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(11, semiTransparencyMode);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(12, setMask);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(13, checkMask);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(14, drawTopLeftX);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(15, drawTopLeftY);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(16, drawBottomRightX);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(17, drawBottomRightY);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glUniform1i(18, dither);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glUniform1i called");
		gl->glDrawArrays(GL_LINES, 0, 2);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glDrawArrays called");
		gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glMemoryBarrier called");
	}

	// Unbind texture from image unit
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_anyLine_implementation function, "
			"glFramebufferTexture2D called");
}

/*
 * This function creates a shader program using the specified name. It is
 * intended to be called from the GL context thread.
 */
static GLuint GPU_createShaderProgram(GPU *gpu, const char *name,
		int shaderNumber)
{
	// Get GLFunctionPointers reference
	GLFunctionPointers *gl = gpu->gl;

	// Define source strings for shaders
	const char *vertexShaderSource;
	const char *fragmentShaderSource;
	if (strncmp(name, "AnyLine", strlen("AnyLine")) == 0) {
		vertexShaderSource = GPU_getAnyLine_VertexShader1Source();
		fragmentShaderSource = GPU_getAnyLine_FragmentShader1Source();
	}
	else if (strncmp(name, "DisplayScreen", strlen("DisplayScreen")) == 0) {
		vertexShaderSource = GPU_getDisplayScreen_VertexShader1Source();
		fragmentShaderSource = GPU_getDisplayScreen_FragmentShader1Source();
	}
	else if (strncmp(name, "GP0_02", strlen("GP0_02")) == 0) {
		vertexShaderSource = GPU_getGP0_02_VertexShader1Source();
		fragmentShaderSource = GPU_getGP0_02_FragmentShader1Source();
	}
	else if (strncmp(name, "GP0_80", strlen("GP0_80")) == 0) {
		switch (shaderNumber) {
			case 1:
				vertexShaderSource = GPU_getGP0_80_VertexShader1Source();
				fragmentShaderSource = GPU_getGP0_80_FragmentShader1Source();
				break;
			case 2:
				vertexShaderSource = GPU_getGP0_80_VertexShader2Source();
				fragmentShaderSource = GPU_getGP0_80_FragmentShader2Source();
				break;
		}
	}
	else if (strncmp(name, "GP0_A0", strlen("GP0_A0")) == 0) {
		vertexShaderSource = GPU_getGP0_A0_VertexShader1Source();
		fragmentShaderSource = GPU_getGP0_A0_FragmentShader1Source();
	}
	else if (strncmp(name, "MonochromePolygon",
			strlen("MonochromePolygon")) == 0) {
		vertexShaderSource =
				GPU_getMonochromePolygon_VertexShader1Source();
		fragmentShaderSource =
				GPU_getMonochromePolygon_FragmentShader1Source();
	}
	else if (strncmp(name, "MonochromeRectangle",
			strlen("MonochromeRectangle")) == 0) {
		vertexShaderSource =
				GPU_getMonochromeRectangle_VertexShader1Source();
		fragmentShaderSource =
				GPU_getMonochromeRectangle_FragmentShader1Source();
	}
	else if (strncmp(name, "ShadedPolygon", strlen("ShadedPolygon")) == 0) {
		vertexShaderSource = GPU_getShadedPolygon_VertexShader1Source();
		fragmentShaderSource = GPU_getShadedPolygon_FragmentShader1Source();
	}
	else if (strncmp(name, "ShadedTexturedPolygon",
			strlen("ShadedTexturedPolygon")) == 0) {
		vertexShaderSource =
				GPU_getShadedTexturedPolygon_VertexShader1Source();
		fragmentShaderSource =
				GPU_getShadedTexturedPolygon_FragmentShader1Source();
	}
	else if (strncmp(name, "TexturedPolygon", strlen("TexturedPolygon")) == 0) {
		vertexShaderSource = GPU_getTexturedPolygon_VertexShader1Source();
		fragmentShaderSource = GPU_getTexturedPolygon_FragmentShader1Source();
	}
	else if (strncmp(name, "TexturedRectangle",
			strlen("TexturedRectangle")) == 0) {
		vertexShaderSource =
				GPU_getTexturedRectangle_VertexShader1Source();
		fragmentShaderSource =
				GPU_getTexturedRectangle_FragmentShader1Source();
	}
	
	// Compile shaders now
	GLuint vs = gl->glCreateShader(GL_VERTEX_SHADER);
	GPU_checkOpenGLErrors(gpu, "GPU createShaderProgram function, "
								"glCreateShader called");
	GLint vsSourceLength[1] = { strlen(vertexShaderSource) };
	gl->glShaderSource(vs, 1, &vertexShaderSource, vsSourceLength);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glShaderSource called");
	gl->glCompileShader(vs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glCompileShader called");

	GLuint fs = gl->glCreateShader(GL_FRAGMENT_SHADER);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glCreateShader called");
	GLint fsSourceLength[1] = { strlen(fragmentShaderSource) };
	gl->glShaderSource(fs, 1, &fragmentShaderSource, fsSourceLength);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glShaderSource called");
	gl->glCompileShader(fs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glCompileShader called");

	// Check compilation status, cleaning up if it failed
	GLuint compilationStatus[1];
	gl->glGetShaderiv(vs, GL_COMPILE_STATUS, compilationStatus);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glGetShaderiv called");
	if (compilationStatus[0] == GL_FALSE) {
		fprintf(stderr, "PhilPSX: GPU: %s vertex shader compilation failed\n",
				name);
		goto cleanup_shaders;
	}
	gl->glGetShaderiv(fs, GL_COMPILE_STATUS, compilationStatus);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glGetShaderiv called");
	if (compilationStatus[0] == GL_FALSE) {
		fprintf(stderr, "PhilPSX: GPU: %s fragment shader compilation failed\n",
				name);
		goto cleanup_shaders;
	}

	// Create and link program
	GLuint program = gl->glCreateProgram();
	if (program == 0) {
		fprintf(stderr, "PhilPSX: GPU: %s OpenGL shader program creation "
						"failed\n", name);
		goto cleanup_shaders;
	}
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glCreateProgram called");
	gl->glAttachShader(program, vs);
	if (GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glAttachShader called")) {
		fprintf(stderr, "PhilPSX: GPU: Attaching %s OpenGL vertex shader "
						"to program object failed\n", name);
		goto cleanup_program;
	}
	gl->glAttachShader(program, fs);
	if (GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glAttachShader")) {
		fprintf(stderr, "PhilPSX: GPU: Attaching %s OpenGL fragment shader "
						"to program object failed\n", name);
		goto cleanup_program;
	}
	gl->glLinkProgram(program);
	if (GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glLinkProgram called")) {
		fprintf(stderr, "PhilPSX: GPU: Linking %s OpenGL shader program "
						"failed\n", name);
		goto cleanup_program;
	}

	// Shaders no longer needed as we have fully linked program now
	gl->glDetachShader(program, vs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glDetachShader called");
	gl->glDetachShader(program, fs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glDetachShader called");
	gl->glDeleteShader(vs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glDeleteShader called");
	gl->glDeleteShader(fs);
	GPU_checkOpenGLErrors(gpu, "GPU_createShaderProgram function, "
								"glDeleteShader called");

	// Normal return path:
	return program;
	
	// Cleanup path (we don't bother with logging these GL calls,
	// as at this point there is nothing we can do to rollback anyway):
	cleanup_program:
	gl->glDeleteProgram(program);
	
	cleanup_shaders:
	gl->glDeleteShader(vs);
	gl->glDeleteShader(fs);
	
	// 0 indicates that program creation failed
	return 0;
}

/*
 * This function displays the screen at the end of each frame, by queuing
 * this work on the rendering thread.
 */
static void GPU_displayScreen(GPU *gpu)
{	
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand displayScreen;
	displayScreen.functionPointer = &GPU_displayScreen_implementation;
	displayScreen.gpu = gpu;

	// Store parameters in object then submit it to work queue
	displayScreen.xStart = gpu->xStart;
	displayScreen.yStart = gpu->yStart;
	displayScreen.x1 = gpu->x1;
	displayScreen.x2 = gpu->x2;
	displayScreen.y1 = gpu->y1;
	displayScreen.y2 = gpu->y2;
	displayScreen.realHorizontalRes = gpu->realHorizontalRes;
	displayScreen.realVerticalRes = gpu->realVerticalRes;
	displayScreen.parameter1 = gpu->dotFactor;
	displayScreen.parameter2 = gpu->interlaceEnabled ? 1 : 0;
	WorkQueue_addItem(gpu->wq, &displayScreen, false);
}

/*
 * This function contains the implementation of GPU_displayScreen.
 */
static void GPU_displayScreen_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Bind to screen framebuffer
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
								"glBindFramebuffer called");

	// Determine resolution parameters
	int32_t topX = command->xStart;
	int32_t topY = command->yStart;

	int32_t pixelsPerLine = (((command->x2 - command->x1) /
			command->parameter1) + 2) & 0xFFFFFFFC;
	int32_t lines = command->y2 - command->y1;
	if (command->parameter2 == 1)
		lines *= 2;

	int32_t bottomX = topX + pixelsPerLine;
	int32_t bottomY = topY + lines;

	topY = 511 - topY;
	bottomY = 511 - bottomY;

	gl->glViewport(0, 0, command->realHorizontalRes, command->realVerticalRes);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glViewport called");
	gl->glUseProgram(gpu->displayScreenProgram);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUseProgram called");
	gl->glUniform1i(0, command->realHorizontalRes);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(1, command->realVerticalRes);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(2, pixelsPerLine);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(3, lines);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(4, topX);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(5, bottomY);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_displayScreen_implementation function, "
			"glDrawArrays called");

	// Swap buffers
	SDL_GL_SwapWindow(gpu->window);
}

/*
 * This function draws a monochrome three or four point polygon, by queuing
 * this work on the rendering thread.
 */
static void GPU_monochromePolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t vertex2, int32_t vertex3, int32_t vertex4)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand monochromePolygon;
	monochromePolygon.functionPointer = &GPU_monochromePolygon_implementation;
	monochromePolygon.gpu = gpu;
	
	// Store parameters in object then submit it to work queue
	monochromePolygon.parameter1 = command;
	monochromePolygon.parameter2 = vertex1;
	monochromePolygon.parameter3 = vertex2;
	monochromePolygon.parameter4 = vertex3;
	monochromePolygon.parameter5 = vertex4;
	monochromePolygon.statusRegister = gpu->statusRegister;
	monochromePolygon.drawingOffset = gpu->drawingOffset;
	monochromePolygon.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	monochromePolygon.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	WorkQueue_addItem(gpu->wq, &monochromePolygon, false);
}

/*
 * This function contains the implementation of GPU_monochromePolygon.
 */
static void GPU_monochromePolygon_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	int32_t semiTransparencyMode = (command->statusRegister >> 5) & 0x3;
	switch (commandByte) {
		case 0x22:
		case 0x23:
		case 0x2A:
		case 0x2B:// Semi-transparent mode
			semiTransparencyEnabled = 1;
			break;
	}

	// Detect if we are doing three or four points
	int32_t fourPoints = 0;
	switch (commandByte) {
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B: // Four pointed commands
			fourPoints = 1;
			break;
	}

	// Calculate vertices
	int32_t vertex_x[4];
	int32_t vertex_y[4];
	vertex_x[0] = command->parameter2 & 0x7FF;
	vertex_y[0] = logical_rshift(command->parameter2, 16) & 0x7FF;
	vertex_x[1] = command->parameter3 & 0x7FF;
	vertex_y[1] = logical_rshift(command->parameter3, 16) & 0x7FF;
	vertex_x[2] = command->parameter4 & 0x7FF;
	vertex_y[2] = logical_rshift(command->parameter4, 16) & 0x7FF;
	vertex_x[3] = command->parameter5 & 0x7FF;
	vertex_y[3] = logical_rshift(command->parameter5, 16) & 0x7FF;

	// Sign extend if necessary
	for (int32_t i = 0; i < 4; ++i) {
		if ((vertex_x[i] & 0x400) == 0x400)
			vertex_x[i] |= 0xFFFFF800;
		if ((vertex_y[i] & 0x400) == 0x400)
			vertex_y[i] |= 0xFFFFF800;
	}

	// Check if the polygon is legal
	bool legalPolygon = true;
	for (int32_t i = 0; i < (2 + fourPoints); ++i) {
		int32_t xDiff = vertex_x[i] - vertex_x[i + 1];
		int32_t yDiff = vertex_y[i] - vertex_y[i + 1];
		if (xDiff > 1023 || xDiff < -1023) {
			legalPolygon = false;
			break;
		}
		if (yDiff > 511 || yDiff < -511) {
			legalPolygon = false;
			break;
		}
	}

	// Do nothing if polygon is illegal
	if (!legalPolygon)
		return;

	// Convert y coordinates to OpenGL form
	for (int32_t i = 0; i < 4; ++i) {
		vertex_y[i] = 511 - vertex_y[i];
	}

	// Split colours out
	int32_t red = command->parameter1 & 0xFF;
	int32_t green = logical_rshift(command->parameter1, 8) & 0xFF;
	int32_t blue = logical_rshift(command->parameter1, 16) & 0xFF;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL form
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	for (int32_t i = 0; i < 4; ++i) {
		vertex_x[i] += drawXOffset;
		vertex_y[i] += drawYOffset;
	}

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindFramebuffer called");
	gl->glViewport(0, 0, 1024, 512);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glViewport called");

	// Set uniforms and render first triangle to vram texture
	gl->glUseProgram(gpu->monochromePolygonProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUseProgram called");
	gl->glUniform3iv(0, 1, vertex_x);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform3iv called");
	gl->glUniform3iv(1, 1, vertex_y);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform3iv called");
	gl->glUniform1i(2, red);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(3, green);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(4, blue);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(5, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(6, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(7, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(8, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(9, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(10, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(11, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glUniform1i(12, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLES, 0, 3);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glDrawArrays called");
	gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glMemoryBarrier called");

	// Check if we need a second triangle
	if (fourPoints == 1) {
		gl->glUniform3iv(0, 1, vertex_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation "
			"function, glUniform3iv called");
		gl->glUniform3iv(1, 1, vertex_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation "
			"function, glUniform3iv called");
		gl->glDrawArrays(GL_TRIANGLES, 0, 3);
		GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation "
			"function, glDrawArrays called");
		gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation "
			"function, glMemoryBarrier called");
	}

	// Unbind texture from image unit
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glFramebufferTexture2D called");

	// Bind to zero framebuffer
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromePolygon_implementation function, "
			"glBindFramebuffer called");
}

/*
 * This function draws a monochrome rectangle, with various options, by queuing
 * this work on the rendering thread.
 */
static void GPU_monochromeRectangle(GPU *gpu, int32_t command, int32_t vertex,
		int32_t widthAndHeight)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand monochromeRectangle;
	monochromeRectangle.functionPointer =
			&GPU_monochromeRectangle_implementation;
	monochromeRectangle.gpu = gpu;

	// Store parameters in object then submit it to work queue
	monochromeRectangle.parameter1 = command;
	monochromeRectangle.parameter2 = vertex;
	monochromeRectangle.parameter3 = widthAndHeight;
	monochromeRectangle.statusRegister = gpu->statusRegister;
	monochromeRectangle.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	monochromeRectangle.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	monochromeRectangle.drawingOffset = gpu->drawingOffset;
	WorkQueue_addItem(gpu->wq, &monochromeRectangle, false);
}

/*
 * This function contains the implementation of GPU_monochromeRectangle.
 */
static void GPU_monochromeRectangle_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	int32_t semiTransparencyMode = (command->statusRegister >> 5) & 0x3;
	switch (commandByte) {
		case 0x62:
		case 0x6A:
		case 0x72:
		case 0x7A: // Semi-transparent mode
			semiTransparencyEnabled = 1;
	}

	// Get colour
	int32_t red = command->parameter1 & 0xFF;
	int32_t green = logical_rshift(command->parameter1, 8) & 0xFF;
	int32_t blue = logical_rshift(command->parameter1, 16) & 0xFF;

	// Get width and height
	int32_t width = command->parameter3 & 0xFFFF;
	width = ((width - 1) & 0x3FF) + 1;
	int32_t height = logical_rshift(command->parameter3, 16) & 0xFFFF;
	height = ((height - 1) & 0x1FF) + 1;
	width = (width == 0) ? 0x400 : width;
	height = (height == 0) ? 0x200 : height;

	// Get vertex parameters and sign-extend if needed
	int32_t x = command->parameter2 & 0x7FF;
	int32_t y = logical_rshift(command->parameter2, 16) & 0x7FF;
	if ((x & 0x400) == 0x400)
		x |= 0xFFFFF800;
	if ((y & 0x400) == 0x400)
		y |= 0xFFFFF800;

	// As we go from bottom-left corner in OpenGL viewport, adjust y
	y = 511 - y;
	y -= height - 1;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL basis
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	x += drawXOffset;
	y += drawYOffset;

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glViewport(x, y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glViewport called");

	// Set rectangle colour pixels in correct manner to vram texture,
	// setting uniforms correctly
	gl->glUseProgram(gpu->monochromeRectangleProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUseProgram called");
	gl->glUniform1i(0, x);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(1, y);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(2, height);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(3, red);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(4, green);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(5, blue);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(6, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(7, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(8, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(9, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(10, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(11, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(12, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(13, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glDrawArrays called");
	gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glMemoryBarrier called");

	// Unbind texture from image unit
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_monochromeRectangle_implementation "
			"function, glFramebufferTexture2D called");
}

/*
 * This function lets us read the DMA buffer in a thread-safe way.
 */
static int8_t GPU_readDMABuffer(GPU *gpu, int32_t index)
{
	pthread_mutex_lock(&gpu->dmaBufferMutex);
	int8_t value = gpu->dmaBuffer[index];
	pthread_mutex_unlock(&gpu->dmaBufferMutex);

	return value;
}

/*
 * This function checks for and displays OpenGL errors.
 */
static bool GPU_realCheckOpenGLErrors(GPU *gpu, const char *message)
{
	int32_t errorVal = 0;
	bool errorDetected = false;
	while ((errorVal = gpu->gl->glGetError()) != GL_NO_ERROR) {
		switch (errorVal) {
			case GL_INVALID_ENUM:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_INVALID_ENUM encountered in %s\n", message);
				errorDetected = true;
				break;
			case GL_INVALID_VALUE:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_INVALID_VALUE encountered in %s\n", message);
				errorDetected = true;
				break;
			case GL_INVALID_OPERATION:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_INVALID_OPERATION encountered in %s\n", message);
				errorDetected = true;
				break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_INVALID_FRAMEBUFFER_OPERATION encountered in %s\n",
						message);
				errorDetected = true;
				break;
			case GL_OUT_OF_MEMORY:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_OUT_OF_MEMORY encountered in %s\n", message);
				errorDetected = true;
				break;
			case GL_STACK_UNDERFLOW:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_STACK_UNDERFLOW encountered in %s\n", message);
				errorDetected = true;
				break;
			case GL_STACK_OVERFLOW:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"GL_STACK_OVERFLOW encountered in %s\n", message);
				errorDetected = true;
				break;
			default:
				fprintf(stderr, "PhilPSX: GPU: OpenGL error: "
						"Unspecified OpenGL error encountered in %s\n",
						message);
				errorDetected = true;
				break;
		}
	}
	
	return errorDetected;
}

/**
 * This function draws a shaded three or four point polygon, by queuing
 * this work on the rendering thread.
 */
static void GPU_shadedPolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t colour2, int32_t vertex2, int32_t colour3, int32_t vertex3,
		int32_t colour4, int32_t vertex4)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand shadedPolygon;
	shadedPolygon.functionPointer = &GPU_shadedPolygon_implementation;
	shadedPolygon.gpu = gpu;

	// Store parameters in object then submit it to work queue
	shadedPolygon.parameter1 = command;
	shadedPolygon.parameter2 = vertex1;
	shadedPolygon.parameter3 = colour2;
	shadedPolygon.parameter4 = vertex2;
	shadedPolygon.parameter5 = colour3;
	shadedPolygon.parameter6 = vertex3;
	shadedPolygon.parameter7 = colour4;
	shadedPolygon.parameter8 = vertex4;
	shadedPolygon.statusRegister = gpu->statusRegister;
	shadedPolygon.drawingOffset = gpu->drawingOffset;
	shadedPolygon.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	shadedPolygon.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	WorkQueue_addItem(gpu->wq, &shadedPolygon, false);
}

/*
 * This function contains the implementation of GPU_shadedPolygon.
 */
static void GPU_shadedPolygon_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	int32_t semiTransparencyMode = (command->statusRegister >> 5) & 0x3;
	switch (commandByte) {
		case 0x32:
		case 0x33:
		case 0x3A:
		case 0x3B: // Semi-transparent mode
			semiTransparencyEnabled = 1;
			break;
	}

	// Detect if we are doing three or four points
	int32_t fourPoints = 0;
	switch (commandByte) {
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B: // Four pointed commands
			fourPoints = 1;
			break;
	}

	// Calculate vertices
	int32_t vertex_x[4];
	int32_t vertex_y[4];
	vertex_x[0] = command->parameter2 & 0x7FF;
	vertex_y[0] = logical_rshift(command->parameter2, 16) & 0x7FF;
	vertex_x[1] = command->parameter4 & 0x7FF;
	vertex_y[1] = logical_rshift(command->parameter4, 16) & 0x7FF;
	vertex_x[2] = command->parameter6 & 0x7FF;
	vertex_y[2] = logical_rshift(command->parameter6, 16) & 0x7FF;
	vertex_x[3] = command->parameter8 & 0x7FF;
	vertex_y[3] = logical_rshift(command->parameter8, 16) & 0x7FF;

	// Sign extend if necessary
	for (int32_t i = 0; i < 4; ++i) {
		if ((vertex_x[i] & 0x400) == 0x400)
			vertex_x[i] |= 0xFFFFF800;
		if ((vertex_y[i] & 0x400) == 0x400)
			vertex_y[i] |= 0xFFFFF800;
	}

	// Check if the polygon is legal
	bool legalPolygon = true;
	for (int32_t i = 0; i < (2 + fourPoints); ++i) {
		int32_t xDiff = vertex_x[i] - vertex_x[i + 1];
		int32_t yDiff = vertex_y[i] - vertex_y[i + 1];
		if (xDiff > 1023 || xDiff < -1023) {
			legalPolygon = false;
			break;
		}
		if (yDiff > 511 || yDiff < -511) {
			legalPolygon = false;
			break;
		}
	}

	// Do nothing if polygon is illegal
	if (!legalPolygon)
		return;

	// Convert y coordinates to OpenGL form
	for (int32_t i = 0; i < 4; ++i) {
		vertex_y[i] = 511 - vertex_y[i];
	}

	// Split colours out
	int32_t redArray[4];
	int32_t greenArray[4];
	int32_t blueArray[4];
	redArray[0] = command->parameter1 & 0xFF;
	greenArray[0] = logical_rshift(command->parameter1, 8) & 0xFF;
	blueArray[0] = logical_rshift(command->parameter1, 16) & 0xFF;
	redArray[1] = command->parameter3 & 0xFF;
	greenArray[1] = logical_rshift(command->parameter3, 8) & 0xFF;
	blueArray[1] = logical_rshift(command->parameter3, 16) & 0xFF;
	redArray[2] = command->parameter5 & 0xFF;
	greenArray[2] = logical_rshift(command->parameter5, 8) & 0xFF;
	blueArray[2] = logical_rshift(command->parameter5, 16) & 0xFF;
	redArray[3] = command->parameter7 & 0xFF;
	greenArray[3] = logical_rshift(command->parameter7, 8) & 0xFF;
	blueArray[3] = logical_rshift(command->parameter7, 16) & 0xFF;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;
	int32_t dither = logical_rshift(command->statusRegister, 9) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL form
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	for (int32_t i = 0; i < 4; ++i) {
		vertex_x[i] += drawXOffset;
		vertex_y[i] += drawYOffset;
	}

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glViewport(0, 0, 1024, 512);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glViewport called");

	// Set uniforms and render first triangle to vram texture
	gl->glUseProgram(gpu->shadedPolygonProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUseProgram called");
	gl->glUniform3iv(0, 1, vertex_x);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(1, 1, vertex_y);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(2, 1, redArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(3, 1, greenArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(4, 1, blueArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform1i(5, dither);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(6, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(7, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(8, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(9, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(10, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(11, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(12, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(13, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLES, 0, 3);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glDrawArrays called");
	gl->glMemoryBarrier(GL_ALL_BARRIER_BITS);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glMemoryBarrier called");

	// Check if we need a second triangle
	if (fourPoints == 1) {
		gl->glUniform3iv(0, 1, vertex_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(1, 1, vertex_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(2, 1, redArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(3, 1, greenArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(4, 1, blueArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glDrawArrays(GL_TRIANGLES, 0, 3);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glDrawArrays called");
		gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
				"function, glMemoryBarrier called");
	}

	// Unbind textures from image units
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glFramebufferTexture2D called");

	// Bind to zero framebuffer
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedPolygon_implementation "
			"function, glBindFramebuffer called");
}

/*
 * This method draws a shaded textured three or four point polygon, by queuing
 * this work on the rendering thread.
 */
static void GPU_shadedTexturedPolygon(GPU *gpu, int32_t command,
		int32_t vertex1, int32_t texCoord1AndPalette, int32_t colour2,
		int32_t vertex2, int32_t texCoord2AndTexPage, int32_t colour3,
		int32_t vertex3, int32_t texCoord3, int32_t colour4, int32_t vertex4,
		int32_t texCoord4)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand shadedTexturedPolygon;
	shadedTexturedPolygon.functionPointer =
			&GPU_shadedTexturedPolygon_implementation;
	shadedTexturedPolygon.gpu = gpu;

	// Store parameters in object then submit it to work queue
	shadedTexturedPolygon.parameter1 = command;
	shadedTexturedPolygon.parameter2 = vertex1;
	shadedTexturedPolygon.parameter3 = texCoord1AndPalette;
	shadedTexturedPolygon.parameter4 = colour2;
	shadedTexturedPolygon.parameter5 = vertex2;
	shadedTexturedPolygon.parameter6 = texCoord2AndTexPage;
	shadedTexturedPolygon.parameter7 = colour3;
	shadedTexturedPolygon.parameter8 = vertex3;
	shadedTexturedPolygon.parameter9 = texCoord3;
	shadedTexturedPolygon.parameter10 = colour4;
	shadedTexturedPolygon.parameter11 = vertex4;
	shadedTexturedPolygon.parameter12 = texCoord4;
	shadedTexturedPolygon.statusRegister = gpu->statusRegister;
	shadedTexturedPolygon.drawingOffset = gpu->drawingOffset;
	shadedTexturedPolygon.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	shadedTexturedPolygon.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	shadedTexturedPolygon.textureWindow = gpu->textureWindow;
	WorkQueue_addItem(gpu->wq, &shadedTexturedPolygon, false);
}

/*
 * This function contains the implementation of GPU_shadedTexturedPolygon.
 */
static void GPU_shadedTexturedPolygon_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	switch (commandByte) {
		case 0x36:
		case 0x37:
		case 0x3E:
		case 0x3F: // Semi-transparent mode
			semiTransparencyEnabled = 1;
			break;
	}

	// Detect if we are doing three or four points
	int32_t fourPoints = 0;
	switch (commandByte) {
		case 0x3C:
		case 0x3D:
		case 0x3E:
		case 0x3F: // Four pointed commands
			fourPoints = 1;
			break;
	}

	// Determine if we should disable blending/colouring for
	// undocumented commands
	int32_t disableBlending = 0;
	switch (commandByte) {
		case 0x35:
		case 0x37:
		case 0x3D:
		case 0x3F: // Undocumented commands, disable blending
			disableBlending = 1;
			break;
	}

	// Calculate vertices
	int32_t vertex_x[4];
	int32_t vertex_y[4];
	vertex_x[0] = command->parameter2 & 0x7FF;
	vertex_y[0] = logical_rshift(command->parameter2, 16) & 0x7FF;
	vertex_x[1] = command->parameter5 & 0x7FF;
	vertex_y[1] = logical_rshift(command->parameter5, 16) & 0x7FF;
	vertex_x[2] = command->parameter8 & 0x7FF;
	vertex_y[2] = logical_rshift(command->parameter8, 16) & 0x7FF;
	vertex_x[3] = command->parameter11 & 0x7FF;
	vertex_y[3] = logical_rshift(command->parameter11, 16) & 0x7FF;

	// Sign extend if necessary
	for (int32_t i = 0; i < 4; ++i) {
		if ((vertex_x[i] & 0x400) == 0x400)
			vertex_x[i] |= 0xFFFFF800;
		if ((vertex_y[i] & 0x400) == 0x400)
			vertex_y[i] |= 0xFFFFF800;
	}

	// Calculate texture coordinates
	int32_t texture_x[4];
	int32_t texture_y[4];
	texture_x[0] = command->parameter3 & 0xFF;
	texture_y[0] = logical_rshift(command->parameter3, 8) & 0xFF;
	texture_x[1] = command->parameter6 & 0xFF;
	texture_y[1] = logical_rshift(command->parameter6, 8) & 0xFF;
	texture_x[2] = command->parameter9 & 0xFF;
	texture_y[2] = logical_rshift(command->parameter9, 8) & 0xFF;
	texture_x[3] = command->parameter12 & 0xFF;
	texture_y[3] = logical_rshift(command->parameter12, 8) & 0xFF;

	// Check if the polygon is legal
	bool legalPolygon = true;
	for (int32_t i = 0; i < (2 + fourPoints); ++i) {
		int32_t xDiff = vertex_x[i] - vertex_x[i + 1];
		int32_t yDiff = vertex_y[i] - vertex_y[i + 1];
		if (xDiff > 1023 || xDiff < -1023) {
			legalPolygon = false;
			break;
		}
		if (yDiff > 511 || yDiff < -511) {
			legalPolygon = false;
			break;
		}
	}

	// Do nothing if polygon is illegal
	if (!legalPolygon)
		return;

	// Convert y coordinates to OpenGL form
	for (int32_t i = 0; i < 4; ++i) {
		vertex_y[i] = 511 - vertex_y[i];
	}

	// Split colours out
	int32_t redArray[4];
	int32_t greenArray[4];
	int32_t blueArray[4];
	redArray[0] = command->parameter1 & 0xFF;
	greenArray[0] = logical_rshift(command->parameter1, 8) & 0xFF;
	blueArray[0] = logical_rshift(command->parameter1, 16) & 0xFF;
	redArray[1] = command->parameter4 & 0xFF;
	greenArray[1] = logical_rshift(command->parameter4, 8) & 0xFF;
	blueArray[1] = logical_rshift(command->parameter4, 16) & 0xFF;
	redArray[2] = command->parameter7 & 0xFF;
	greenArray[2] = logical_rshift(command->parameter7, 8) & 0xFF;
	blueArray[2] = logical_rshift(command->parameter7, 16) & 0xFF;
	redArray[3] = command->parameter10 & 0xFF;
	greenArray[3] = logical_rshift(command->parameter10, 8) & 0xFF;
	blueArray[3] = logical_rshift(command->parameter10, 16) & 0xFF;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;
	int32_t dither = logical_rshift(command->statusRegister, 9) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL form
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	for (int32_t i = 0; i < 4; ++i) {
		vertex_x[i] += drawXOffset;
		vertex_y[i] += drawYOffset;
	}

	// Get clut coordinates
	int32_t clut_x = (logical_rshift(command->parameter3, 16) & 0x3F) * 16;
	int32_t clut_y = logical_rshift(command->parameter3, 22) & 0x1FF;
	clut_y = 511 - clut_y;

	// Get texture window parameters
	int32_t texWidthMask = command->textureWindow & 0x1F;
	int32_t texHeightMask = logical_rshift(command->textureWindow, 5) & 0x1F;
	int32_t texWinOffsetX = logical_rshift(command->textureWindow, 10) & 0x1F;
	int32_t texWinOffsetY = logical_rshift(command->textureWindow, 15) & 0x1F;

	// Get texture page base coordinates
	int32_t texPage = logical_rshift(command->parameter6, 16) & 0xFFFF;
	int32_t texBaseX = (texPage & 0xF) * 64;
	int32_t texBaseY = (logical_rshift(texPage, 4) & 0x1) * 256;
	texBaseY = 511 - texBaseY;

	// Get texture colour mode
	int32_t texColourMode = logical_rshift(texPage, 7) & 0x3;

	// Set transparency mode
	int32_t semiTransparencyMode = logical_rshift(texPage, 5) & 0x3;

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glViewport(0, 0, 1024, 512);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glViewport called");

	// Set uniforms and render first triangle to vram texture
	gl->glUseProgram(gpu->shadedTexturedPolygonProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUseProgram called");
	gl->glUniform3iv(0, 1, vertex_x);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(1, 1, vertex_y);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(2, 1, texture_x);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(3, 1, texture_y);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform1i(4, texBaseX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(5, texBaseY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(6, texColourMode);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(7, texWidthMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(8, texHeightMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(9, texWinOffsetX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(10, texWinOffsetY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform3iv(11, 1, redArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(12, 1, greenArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(13, 1, blueArray);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform1i(14, dither);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(15, disableBlending);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(16, clut_x);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(17, clut_y);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(18, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(19, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(20, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(21, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(22, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(23, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(24, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(25, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLES, 0, 3);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glDrawArrays called");
	gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glMemoryBarrier called");

	// Check if we need a second triangle
	if (fourPoints == 1) {
		gl->glUniform3iv(0, 1, vertex_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(1, 1, vertex_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(2, 1, texture_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(3, 1, texture_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(11, 1, redArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(12, 1, greenArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(13, 1, blueArray + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glDrawArrays(GL_TRIANGLES, 0, 3);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glDrawArrays called");
		gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
				"function, glMemoryBarrier called");
	}

	// Unbind texture from image unit
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glFramebufferTexture2D called");

	// Bind to zero framebuffer
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_shadedTexturedPolygon_implementation "
			"function, glBindFramebuffer called");
}

/*
 * This method draws a textured three or four point polygon, by queuing
 * this work on the rendering thread.
 */
static void GPU_texturedPolygon(GPU *gpu, int32_t command, int32_t vertex1,
		int32_t texCoord1AndPalette, int32_t vertex2,
		int32_t texCoord2AndTexPage, int32_t vertex3, int32_t texCoord3,
		int32_t vertex4, int32_t texCoord4)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand texturedPolygon;
	texturedPolygon.functionPointer = &GPU_texturedPolygon_implementation;
	texturedPolygon.gpu = gpu;

	// Store parameters in object then submit it to work queue
	texturedPolygon.parameter1 = command;
	texturedPolygon.parameter2 = vertex1;
	texturedPolygon.parameter3 = texCoord1AndPalette;
	texturedPolygon.parameter4 = vertex2;
	texturedPolygon.parameter5 = texCoord2AndTexPage;
	texturedPolygon.parameter6 = vertex3;
	texturedPolygon.parameter7 = texCoord3;
	texturedPolygon.parameter8 = vertex4;
	texturedPolygon.parameter9 = texCoord4;
	texturedPolygon.statusRegister = gpu->statusRegister;
	texturedPolygon.drawingOffset = gpu->drawingOffset;
	texturedPolygon.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	texturedPolygon.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	texturedPolygon.textureWindow = gpu->textureWindow;
	WorkQueue_addItem(gpu->wq, &texturedPolygon, false);
}

/*
 * This function contains the implementation of GPU_texturedPolygon.
 */
static void GPU_texturedPolygon_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in raw texture or blending mode
	// (blending by default)
	int32_t rawTextureEnabled = 0;
	switch (commandByte) {
		case 0x25:
		case 0x27:
		case 0x2D:
		case 0x2F: // Raw texture mode
			rawTextureEnabled = 1;
			break;
	}

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	switch (commandByte) {
		case 0x26:
		case 0x27:
		case 0x2E:
		case 0x2F: // Semi-transparent mode
			semiTransparencyEnabled = 1;
			break;
	}

	// Detect if we are doing three or four points
	int32_t fourPoints = 0;
	switch (commandByte) {
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F: // Four pointed commands
			fourPoints = 1;
			break;
	}

	// Calculate vertices
	int32_t vertex_x[4];
	int32_t vertex_y[4];
	vertex_x[0] = command->parameter2 & 0x7FF;
	vertex_y[0] = logical_rshift(command->parameter2, 16) & 0x7FF;
	vertex_x[1] = command->parameter4 & 0x7FF;
	vertex_y[1] = logical_rshift(command->parameter4, 16) & 0x7FF;
	vertex_x[2] = command->parameter6 & 0x7FF;
	vertex_y[2] = logical_rshift(command->parameter6, 16) & 0x7FF;
	vertex_x[3] = command->parameter8 & 0x7FF;
	vertex_y[3] = logical_rshift(command->parameter8, 16) & 0x7FF;

	// Sign extend if necessary
	for (int32_t i = 0; i < 4; ++i) {
		if ((vertex_x[i] & 0x400) == 0x400)
			vertex_x[i] |= 0xFFFFF800;
		if ((vertex_y[i] & 0x400) == 0x400)
			vertex_y[i] |= 0xFFFFF800;
	}

	// Calculate texture coordinates
	int32_t texture_x[4];
	int32_t texture_y[4];
	texture_x[0] = command->parameter3 & 0xFF;
	texture_y[0] = logical_rshift(command->parameter3, 8) & 0xFF;
	texture_x[1] = command->parameter5 & 0xFF;
	texture_y[1] = logical_rshift(command->parameter5, 8) & 0xFF;
	texture_x[2] = command->parameter7 & 0xFF;
	texture_y[2] = logical_rshift(command->parameter7, 8) & 0xFF;
	texture_x[3] = command->parameter9 & 0xFF;
	texture_y[3] = logical_rshift(command->parameter9, 8) & 0xFF;

	// Check if the polygon is legal
	bool legalPolygon = true;
	for (int32_t i = 0; i < (2 + fourPoints); ++i) {
		int32_t xDiff = vertex_x[i] - vertex_x[i + 1];
		int32_t yDiff = vertex_y[i] - vertex_y[i + 1];
		if (xDiff > 1023 || xDiff < -1023) {
			legalPolygon = false;
			break;
		}
		if (yDiff > 511 || yDiff < -511) {
			legalPolygon = false;
			break;
		}
	}

	// Do nothing if polygon is illegal
	if (!legalPolygon)
		return;

	// Convert y coordinates to OpenGL form
	for (int32_t i = 0; i < 4; ++i) {
		vertex_y[i] = 511 - vertex_y[i];
	}

	// Split colours out
	int32_t red = command->parameter1 & 0xFF;
	int32_t green = logical_rshift(command->parameter1, 8) & 0xFF;
	int32_t blue = logical_rshift(command->parameter1, 16) & 0xFF;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;
	int32_t dither = logical_rshift(command->statusRegister, 9) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL form
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	for (int32_t i = 0; i < 4; ++i) {
		vertex_x[i] += drawXOffset;
		vertex_y[i] += drawYOffset;
	}

	// Get clut coordinates
	int32_t clut_x = (logical_rshift(command->parameter3, 16) & 0x3F) * 16;
	int32_t clut_y = logical_rshift(command->parameter3, 22) & 0x1FF;
	clut_y = 511 - clut_y;

	// Get texture window parameters
	int32_t texWidthMask = command->textureWindow & 0x1F;
	int32_t texHeightMask = logical_rshift(command->textureWindow, 5) & 0x1F;
	int32_t texWinOffsetX = logical_rshift(command->textureWindow, 10) & 0x1F;
	int32_t texWinOffsetY = logical_rshift(command->textureWindow, 15) & 0x1F;

	// Get texture page base coordinates
	int32_t texPage = logical_rshift(command->parameter5, 16) & 0xFFFF;
	int32_t texBaseX = (texPage & 0xF) * 64;
	int32_t texBaseY = (logical_rshift(texPage, 4) & 0x1) * 256;
	texBaseY = 511 - texBaseY;

	// Get texture colour mode
	int32_t texColourMode = logical_rshift(texPage, 7) & 0x3;

	// Set transparency mode
	int32_t semiTransparencyMode = logical_rshift(texPage, 5) & 0x3;

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glViewport(0, 0, 1024, 512);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glViewport called");

	// Set uniforms and render first triangle to vram texture
	gl->glUseProgram(gpu->texturedPolygonProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUseProgram called");
	gl->glUniform3iv(0, 1, vertex_x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(1, 1, vertex_y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(2, 1, texture_x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform3iv(3, 1, texture_y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform3iv called");
	gl->glUniform1i(4, texBaseX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(5, texBaseY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(6, texColourMode);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(7, texWidthMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(8, texHeightMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(9, texWinOffsetX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(10, texWinOffsetY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(11, red);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(12, green);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(13, blue);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(14, rawTextureEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(15, dither);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(16, clut_x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(17, clut_y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(18, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(19, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(20, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(21, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(22, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(23, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(24, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(25, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLES, 0, 3);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glDrawArrays called");
	gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glMemoryBarrier called");

	// Check if we need a second triangle
	if (fourPoints == 1) {
		gl->glUniform3iv(0, 1, vertex_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(1, 1, vertex_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(2, 1, texture_x + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glUniform3iv(3, 1, texture_y + 1);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glUniform3iv called");
		gl->glDrawArrays(GL_TRIANGLES, 0, 3);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glDrawArrays called");
		gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
				"function, glMemoryBarrier called");
	}

	// Unbind texture from image unit
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glFramebufferTexture2D called");

	// Bind to zero framebuffer
	gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedPolygon_implementation "
			"function, glBindFramebuffer called");
}

/**
 * This method draws a textured rectangle, with various options, by queuing
 * this work on the rendering thread.
 */
static void GPU_texturedRectangle(GPU *gpu, int32_t command, int32_t vertex,
		int32_t texCoordAndPalette, int32_t widthAndHeight)
{
	// Perform draw on GL thread, making sure to set pointers
	GpuCommand texturedRectangle;
	texturedRectangle.functionPointer = &GPU_texturedRectangle_implementation;
	texturedRectangle.gpu = gpu;

	// Store parameters in object then submit it to work queue
	texturedRectangle.parameter1 = command;
	texturedRectangle.parameter2 = vertex;
	texturedRectangle.parameter3 = texCoordAndPalette;
	texturedRectangle.parameter4 = widthAndHeight;
	texturedRectangle.statusRegister = gpu->statusRegister;
	texturedRectangle.drawingAreaBottomRight = gpu->drawingAreaBottomRight;
	texturedRectangle.drawingAreaTopLeft = gpu->drawingAreaTopLeft;
	texturedRectangle.drawingOffset = gpu->drawingOffset;
	texturedRectangle.textureWindow = gpu->textureWindow;
	WorkQueue_addItem(gpu->wq, &texturedRectangle, false);
}

/*
 * This function contains the implementation of GPU_texturedRectangle.
 */
static void GPU_texturedRectangle_implementation(GpuCommand *command)
{
	// Get GL function pointers and GPU objects
	GPU *gpu = command->gpu;
	GLFunctionPointers *gl = gpu->gl;

	// Extract command byte
	int32_t commandByte = logical_rshift(command->parameter1, 24) & 0xFF;

	// Detect if we are in raw texture or blending mode
	// (blending by default)
	int32_t rawTextureEnabled = 0;
	switch (commandByte) {
		case 0x65:
		case 0x67:
		case 0x6D:
		case 0x6F:
		case 0x75:
		case 0x77:
		case 0x7D:
		case 0x7F: // Raw texture mode
			rawTextureEnabled = 1;
			break;
	}

	// Detect if we are in opaque or semi-transparent mode
	// (opaque by default)
	int32_t semiTransparencyEnabled = 0;
	int32_t semiTransparencyMode = (command->statusRegister >> 5) & 0x3;
	switch (commandByte) {
		case 0x66:
		case 0x67:
		case 0x6E:
		case 0x6F:
		case 0x76:
		case 0x77:
		case 0x7E:
		case 0x7F: // Semi-transparent mode
			semiTransparencyEnabled = 1;
	}

	// Get colour
	int32_t red = command->parameter1 & 0xFF;
	int32_t green = logical_rshift(command->parameter1, 8) & 0xFF;
	int32_t blue = logical_rshift(command->parameter1, 16) & 0xFF;

	// Get width and height
	int32_t width = command->parameter4 & 0xFFFF;
	width = ((width - 1) & 0x3FF) + 1;
	int32_t height = logical_rshift(command->parameter4, 16) & 0xFFFF;
	height = ((height - 1) & 0x1FF) + 1;
	width = (width == 0) ? 0x400 : width;
	height = (height == 0) ? 0x200 : height;

	// Get vertex parameters and sign-extend if needed
	int32_t x = command->parameter2 & 0x7FF;
	int32_t y = logical_rshift(command->parameter2, 16) & 0x7FF;
	if ((x & 0x400) == 0x400)
		x |= 0xFFFFF800;
	if ((y & 0x400) == 0x400)
		y |= 0xFFFFF800;

	// As we go from bottom-left corner in OpenGL viewport, adjust y
	y = 511 - y;
	y -= height - 1;

	// Split out masking bits from status register
	int32_t setMask = logical_rshift(command->statusRegister, 11) & 0x1;
	int32_t checkMask = logical_rshift(command->statusRegister, 12) & 0x1;

	// Setup drawing area variables
	int32_t drawXOffset = command->drawingOffset & 0x7FF;
	int32_t drawYOffset = logical_rshift(command->drawingOffset, 11) & 0x7FF;
	if ((drawXOffset & 0x400) == 0x400)
		drawXOffset |= 0xFFFFF800;
	if ((drawYOffset & 0x400) == 0x400)
		drawYOffset |= 0xFFFFF800;

	int32_t drawTopLeftX = command->drawingAreaTopLeft & 0x3FF;
	int32_t drawTopLeftY =
			logical_rshift(command->drawingAreaTopLeft, 10) & 0x1FF;
	int32_t drawBottomRightX = command->drawingAreaBottomRight & 0x3FF;
	int32_t drawBottomRightY =
			logical_rshift(command->drawingAreaBottomRight, 10) & 0x1FF;

	// Convert Y values to OpenGL basis
	drawYOffset = -drawYOffset;
	drawTopLeftY = 511 - drawTopLeftY;
	drawBottomRightY = 511 - drawBottomRightY;

	// Adjust coordinates with offsets
	x += drawXOffset;
	y += drawYOffset;

	// Get texture coordinates
	int32_t tex_x = command->parameter3 & 0xFF;
	int32_t tex_y = logical_rshift(command->parameter3, 8) & 0xFF;

	// Get clut coordinates
	int32_t clut_x = (logical_rshift(command->parameter3, 16) & 0x3F) * 16;
	int32_t clut_y = logical_rshift(command->parameter3, 22) & 0x1FF;
	clut_y = 511 - clut_y;

	// Get texture window parameters
	int32_t texWidthMask = command->textureWindow & 0x1F;
	int32_t texHeightMask = logical_rshift(command->textureWindow, 5) & 0x1F;
	int32_t texWinOffsetX = logical_rshift(command->textureWindow, 10) & 0x1F;
	int32_t texWinOffsetY = logical_rshift(command->textureWindow, 15) & 0x1F;

	// Get texture page base coordinates
	int32_t texBaseX = (command->statusRegister & 0xF) * 64;
	int32_t texBaseY = (logical_rshift(command->statusRegister, 4) & 0x1) * 256;
	texBaseY = 511 - texBaseY;

	// Get texture colour mode
	int32_t texColourMode = logical_rshift(command->statusRegister, 7) & 0x3;

	// Unbind vram texture from FBO so we can attach it to image unit
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, 0, 0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glFramebufferTexture2D called");
	gl->glBindImageTexture(1, gpu->vramTexture[0], 0, false, 0,
			GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glBindImageTexture called");

	// Bind to empty framebuffer, setting viewport correctly
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->emptyFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glViewport(x, y, width, height);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glViewport called");

	// Copy texture pixels in correct manner to vram texture,
	// setting uniforms correctly
	gl->glUseProgram(gpu->texturedRectangleProgram1);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUseProgram called");
	gl->glUniform1i(0, x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(1, y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(2, height);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(3, texBaseX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(4, texBaseY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(5, tex_x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(6, tex_y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(7, texColourMode);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(8, texWidthMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(9, texHeightMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(10, texWinOffsetX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(11, texWinOffsetY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(12, red);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(13, green);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(14, blue);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(15, rawTextureEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(16, clut_x);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(17, clut_y);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(18, setMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(19, checkMask);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(20, semiTransparencyEnabled);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(21, semiTransparencyMode);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(22, drawTopLeftX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(23, drawTopLeftY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(24, drawBottomRightX);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glUniform1i(25, drawBottomRightY);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glUniform1i called");
	gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glDrawArrays called");
	gl->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glMemoryBarrier called");

	// Unbind textures from image units
	gl->glBindImageTexture(1, 0, 0, false, 0, GL_READ_WRITE, GL_RGBA8UI);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glBindImageTexture called");

	// Rebind vram texture to FBO
	gl->glActiveTexture(GL_TEXTURE0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glActiveTexture called");
	gl->glBindFramebuffer(GL_FRAMEBUFFER, gpu->vramFramebuffer[0]);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glBindFramebuffer called");
	gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, gpu->vramTexture[0], 0);
	GPU_checkOpenGLErrors(gpu, "GPU_texturedRectangle_implementation "
			"function, glFramebufferTexture2D called");
}

/*
 * This function triggers a vblank interrupt, also updating the screen.
 */
static void GPU_triggerVblankInterrupt(GPU *gpu)
{
	// Trigger the interrupt
	SystemInterlink_setGPUInterruptDelay(gpu->system, 0);

	// Update screen
	GPU_displayScreen(gpu);
}

/*
 * This function lets us write to the DMA buffer in a thread-safe way.
 */
static void GPU_writeDMABuffer(GPU *gpu, int32_t index, int8_t value)
{
	pthread_mutex_lock(&gpu->dmaBufferMutex);
	gpu->dmaBuffer[index] = value;
	pthread_mutex_unlock(&gpu->dmaBufferMutex);
}