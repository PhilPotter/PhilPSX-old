/*
 * This header file provides the struct definition for a GPU command -
 * essentially this allows us to encapsulate a function pointer with all the
 * state required for it to execute, and then submit it to a queue where it
 * can be executed on the rendering thread.
 * 
 * PhilPSXCommand.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_GPUCOMMAND
#define PHILPSX_GPUCOMMAND

// System includes
#include <stdint.h>

// Typedefs
typedef struct GpuCommand GpuCommand;

// Struct definition
struct GpuCommand {
	
	// Function pointer
	void (*functionPointer)(GpuCommand *cmd);
	
	// Parameters - these are used for all PlayStation GPU commands
	// in one way or another
	int32_t parameter1;
	int32_t parameter2;
	int32_t parameter3;
	int32_t parameter4;
	int32_t parameter5;
	int32_t parameter6;
	int32_t parameter7;
	int32_t parameter8;
	int32_t parameter9;
	int32_t parameter10;
	int32_t parameter11;
	int32_t parameter12;
	int32_t statusRegister;
	int32_t drawingAreaTopLeft;
	int32_t drawingAreaBottomRight;
	int32_t drawingOffset;
	int32_t textureWindow;
	int32_t xStart;
	int32_t yStart;
	int32_t x1;
	int32_t x2;
	int32_t y1;
	int32_t y2;
	int32_t realHorizontalRes;
	int32_t realVerticalRes;
};

#endif