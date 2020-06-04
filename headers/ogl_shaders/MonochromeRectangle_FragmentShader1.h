/*
 * This header file provides the OpenGL fragment shader for the
 * MonochromeRectangle routine.
 * 
 * MonochromeRectangle_FragmentShader1.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_MONOCHROMERECTANGLE_FRAGMENTSHADER1
#define PHILPSX_MONOCHROMERECTANGLE_FRAGMENTSHADER1

static const char *GPU_getMonochromeRectangle_FragmentShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Image corresponding to vram texture\n"
	"layout (binding = 1, rgba8ui) uniform uimage2D vramImage;\n"
	"\n"
	"// Uniforms to control copy process\n"
	"layout (location = 0) uniform int xOffset;\n"
	"layout (location = 1) uniform int yOffset;\n"
	"layout (location = 2) uniform int height;\n"
	"layout (location = 3) uniform int red;\n"
	"layout (location = 4) uniform int green;\n"
	"layout (location = 5) uniform int blue;\n"
	"layout (location = 6) uniform int semiTransparencyEnabled;\n"
	"layout (location = 7) uniform int semiTransparencyMode;\n"
	"layout (location = 8) uniform int setMask;\n"
	"layout (location = 9) uniform int checkMask;\n"
	"layout (location = 10) uniform int drawTopLeftX;\n"
	"layout (location = 11) uniform int drawTopLeftY;\n"
	"layout (location = 12) uniform int drawBottomRightX;\n"
	"layout (location = 13) uniform int drawBottomRightY;\n"
	"\n"
	"// Function declarations\n"
	"bool inDrawingArea(ivec2 pixelCoord);\n"
	"\n"
	"// Dummy output value\n"
	"out vec4 colour;\n"
	"\n"
	"// Convert pixel format and store in vram texture\n"
	"void main(void) {\n"
	"	// Define rectangle pixel variable\n"
	"	uvec4 rectPixel = uvec4((red >> 3) & uint(0x1F),\n"
	"							(green >> 3) & uint(0x1F),\n"
	"							(blue >> 3) & uint(0x1F), 0);\n"
	"\n"
	"	// Solid colour so no need to flip tempDrawCoord\n"
	"\n"
	"	// Get coordinate from gl_FragCoord and correctly reference\n"
	"	// vram texture\n"
	"	ivec2 vramCoord = ivec2(gl_FragCoord.xy);\n"
	"\n"
	"	// Load vram pixel\n"
	"	uvec4 vramPixel = imageLoad(vramImage, vramCoord);\n"
	"\n"
	"	// Handle semi-transparency here if enabled\n"
	"	if (semiTransparencyEnabled == 1) {\n"
	"\n"
	"		int oldRed = int(vramPixel.r);\n"
	"		int oldGreen = int(vramPixel.g);\n"
	"		int oldBlue = int(vramPixel.b);\n"
	"\n"
	"		int newRed = int(rectPixel.r);\n"
	"		int newGreen = int(rectPixel.g);\n"
	"		int newBlue = int(rectPixel.b);\n"
	"\n"
	"		// Do calculation\n"
	"		switch (semiTransparencyMode) {\n"
	"			case 0: // B/2 + F/2\n"
	"				newRed = oldRed / 2 + newRed / 2;\n"
	"				newGreen = oldGreen / 2 + newGreen / 2;\n"
	"				newBlue = oldBlue / 2 + newBlue / 2;\n"
	"				break;\n"
	"			case 1: // B + F\n"
	"				newRed = oldRed + newRed;\n"
	"				newGreen = oldGreen + newGreen;\n"
	"				newBlue = oldBlue + newBlue;\n"
	"				break;\n"
	"			case 2: // B - F\n"
	"				newRed = oldRed - newRed;\n"
	"				newGreen = oldGreen - newGreen;\n"
	"				newBlue = oldBlue - newBlue;\n"
	"				break;\n"
	"			case 3: // B + F/4\n"
	"				newRed = oldRed + newRed / 4;\n"
	"				newGreen = oldGreen + newGreen / 4;\n"
	"				newBlue = oldBlue + newBlue / 4;\n"
	"				break;\n"
	"		}\n"
	"\n"
	"		// Saturate pixel\n"
	"		if (newRed < 0) {\n"
	"			newRed = 0;\n"
	"		}\n"
	"		else if (newRed > 31) {\n"
	"			newRed = 31;\n"
	"		}\n"
	"\n"
	"		if (newGreen < 0) {\n"
	"			newGreen = 0;\n"
	"		}\n"
	"		else if (newGreen > 31) {\n"
	"			newGreen = 31;\n"
	"		}\n"
	"\n"
	"		if (newBlue < 0) {\n"
	"			newBlue = 0;\n"
	"		}\n"
	"		else if (newBlue > 31) {\n"
	"			newBlue = 31;\n"
	"		}\n"
	"\n"
	"		// Store new pixel values\n"
	"		rectPixel.r = newRed;\n"
	"		rectPixel.g = newGreen;\n"
	"		rectPixel.b = newBlue;\n"
	"	}\n"
	"\n"
	"	// Set mask bit if enabled\n"
	"	if (setMask == 1) {\n"
	"		rectPixel.a = 0x1;\n"
	"	}\n"
	"\n"
	"	// Check vram pixel if enabled, else just merge, also\n"
	"	// checking rect pixel is in draw area\n"
	"	bool inArea = inDrawingArea(vramCoord);\n"
	"	if (checkMask == 1) {\n"
	"		if (vramPixel.a != 1 && inArea) {\n"
	"			imageStore(vramImage, vramCoord, rectPixel);\n"
	"		}\n"
	"	}\n"
	"	else if (inArea) {\n"
	"		imageStore(vramImage, vramCoord, rectPixel);\n"
	"	}\n"
	"	\n"
	"	// Set dummy output value\n"
	"	colour = vec4(0.0, 0.0, 0.0, 0.0);\n"
	"}\n"
	"\n"
	"// Tells us if a pixel is in the drawing area\n"
	"bool inDrawingArea(ivec2 pixelCoord) {\n"
	"	bool retVal = false;\n"
	"	if (pixelCoord.x >= drawTopLeftX &&\n"
	"		pixelCoord.x <= drawBottomRightX &&\n"
	"		pixelCoord.y <= drawTopLeftY &&\n"
	"		pixelCoord.y >= drawBottomRightY) {\n"
	"		retVal = true;\n"
	"	}\n"
	"\n"
	"	return retVal;\n"
	"}\n";
}

#endif
