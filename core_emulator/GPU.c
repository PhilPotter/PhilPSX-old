/*
 * This C file models the GPU of the PlayStation as a class. Most pthread
 * function calls are unchecked for errors under the assumption that for this
 * use case, they cannot fail.
 * 
 * GPU.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <GL/gl.h>
#include "../headers/GPU.h"
#include "../headers/SystemInterlink.h"
#include "../headers/ArrayList.h"
#include "../headers/GLFunctionPointers.h"

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
static bool GPU_realCheckOpenGLErrors(GPU *gpu, const char *message);
static GLuint GPU_createShaderProgram(GPU *gpu, const char *name,
		int shaderNumber);

/*
 * This struct contains registers and state that we need in order to model
 * the PlayStation GPU.
 */
struct GPU {
	
	// Mutex to allow thread-safe operations on the GPU struct itself
	pthread_mutex_t mutex;
	
	// This list stores line parameters for the line rendering commands
	ArrayList *lineParameters;

	// GL variables/references are handled by this class, and so stored here
	GLFunctionPointers *gl;
	GLuint *vertexArrayObject;
	GLuint *vramTexture;
	GLuint *tempDrawTexture;
	GLuint *vramFramebuffer;
	GLuint *tempDrawFramebuffer;
	GLuint *emptyFramebuffer;
	GLuint *clutBuffer;
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

	// This lets us store values for DMA transfers
	int32_t dmaBufferIndex;
	int32_t dmaNeededBytes;
	int8_t *dmaBuffer;
	int32_t dmaReadInProgress;
	int32_t dmaWriteInProgress;
	int32_t dmaWidthInPixels;
	int32_t dmaHeightInPixels;

	// Link to system
	SystemInterlink *system;

	// Command FIFO buffer for GP0 commands
	int32_t *fifoBuffer;
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
	
	// Initialise mutex
	if (pthread_mutex_init(&gpu->mutex, NULL) != 0) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't initialise the "
				"pthread_mutex_t object\n");
		goto cleanup_gpu;
	}
	
	// Setup ArrayList to store line parameters in a thread-safe manner
	gpu->lineParameters = construct_ArrayList(NULL, true, false);
	if (!gpu->lineParameters) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't initialise lineParameters "
				"list\n");
		goto cleanup_mutex;
	}
	
	// Setup FIFO buffer
	gpu->fifoBuffer = calloc(16, sizeof(int32_t));
	if (!gpu->fifoBuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"fifoBuffer array\n");
		goto cleanup_lineparameters;
	}
	
	// Setup GLFunctionPointers struct
	gpu->gl = calloc(1, sizeof(GLFunctionPointers));
	if (!gpu->gl) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"GLFunctionPointers struct\n");
		goto cleanup_fifobuffer;
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
	cleanup_fifobuffer:
	free(gpu->fifoBuffer);
	
	cleanup_lineparameters:
	destruct_ArrayList(gpu->lineParameters);
	
	cleanup_mutex:
	pthread_mutex_destroy(&gpu->mutex);

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
	free(gpu->fifoBuffer);
	destruct_ArrayList(gpu->lineParameters);
	pthread_mutex_destroy(&gpu->mutex);
	free(gpu);
}

void GPU_appendSyncCycles(GPU *gpu, int32_t cycles)
{
	
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
	free(gpu->clutBuffer);
	free(gpu->dmaBuffer);
	free(gpu->emptyFramebuffer);
	free(gpu->tempDrawFramebuffer);
	free(gpu->tempDrawTexture);
	free(gpu->vertexArrayObject);
	free(gpu->vramFramebuffer);
	free(gpu->vramTexture);
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
	gpu->clutBuffer = calloc(1, sizeof(GLuint));
	if (!gpu->clutBuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"clutBuffer\n");
		goto cleanup_memory;
	}
	gpu->dmaBuffer = calloc(1024 * 512 * 4, sizeof(int8_t));
	if (!gpu->dmaBuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"dmaBuffer\n");
		goto cleanup_memory;
	}
	gpu->emptyFramebuffer = calloc(1, sizeof(GLuint));
	if (!gpu->emptyFramebuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"emptyFramebuffer\n");
		goto cleanup_memory;
	}
	gpu->tempDrawFramebuffer = calloc(1, sizeof(GLuint));
	if (!gpu->tempDrawFramebuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"tempDrawFramebuffer\n");
		goto cleanup_memory;
	}
	gpu->tempDrawTexture = calloc(1, sizeof(GLuint));
	if (!gpu->tempDrawTexture) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"tempDrawTexture\n");
		goto cleanup_memory;
	}
	gpu->vertexArrayObject = calloc(1, sizeof(GLuint));
	if (!gpu->vertexArrayObject) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"vertexArrayObject\n");
		goto cleanup_memory;
	}
	gpu->vramFramebuffer = calloc(1, sizeof(GLuint));
	if (!gpu->vramFramebuffer) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"vramFramebuffer\n");
		goto cleanup_memory;
	}
	gpu->vramTexture = calloc(1, sizeof(GLuint));
	if (!gpu->vramTexture) {
		fprintf(stderr, "PhilPSX: GPU: Couldn't allocate memory for "
				"vramTexture\n");
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
	if (gpu->clutBuffer)
		free(gpu->clutBuffer);
	if (gpu->dmaBuffer)
		free(gpu->dmaBuffer);
	if (gpu->emptyFramebuffer)
		free(gpu->emptyFramebuffer);
	if (gpu->tempDrawFramebuffer)
		free(gpu->tempDrawFramebuffer);
	if (gpu->tempDrawTexture)
		free(gpu->tempDrawTexture);
	if (gpu->vertexArrayObject)
		free(gpu->vertexArrayObject);
	if (gpu->vramFramebuffer)
		free(gpu->vramFramebuffer);
	if (gpu->vramTexture)
		free(gpu->vramTexture);
	
	return false;
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

/*
 * This function allows us to set the GPU's OpenGL function pointers, needed
 * for making OpenGL calls to emulate the PlayStation GPU.
 */
void GPU_setGLFunctionPointers(GPU *gpu)
{
	GLFunctionPointers_setFunctionPointers(gpu->gl);
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

/*
 * This method creates a shader program using the specified name. It is
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