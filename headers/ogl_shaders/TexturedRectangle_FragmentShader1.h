/*
 * This header file provides the OpenGL fragment shader for the
 * TexturedRectangle routine.
 * 
 * TexturedRectangle_FragmentShader1.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_TEXTUREDRECTANGLE_FRAGMENTSHADER1
#define PHILPSX_TEXTUREDRECTANGLE_FRAGMENTSHADER1

static const char *GPU_getTexturedRectangle_FragmentShader1Source(void)
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
	"layout (location = 3) uniform int texBaseX;\n"
	"layout (location = 4) uniform int texBaseY;\n"
	"layout (location = 5) uniform int tex_x;\n"
	"layout (location = 6) uniform int tex_y;\n"
	"layout (location = 7) uniform int texColourMode;\n"
	"layout (location = 8) uniform int texWidthMask;\n"
	"layout (location = 9) uniform int texHeightMask;\n"
	"layout (location = 10) uniform int texWinOffsetX;\n"
	"layout (location = 11) uniform int texWinOffsetY;\n"
	"layout (location = 12) uniform int red;\n"
	"layout (location = 13) uniform int green;\n"
	"layout (location = 14) uniform int blue;\n"
	"layout (location = 15) uniform int rawTextureEnabled;\n"
	"layout (location = 16) uniform int clut_x;\n"
	"layout (location = 17) uniform int clut_y;\n"
	"layout (location = 18) uniform int setMask;\n"
	"layout (location = 19) uniform int checkMask;\n"
	"layout (location = 20) uniform int semiTransparencyEnabled;\n"
	"layout (location = 21) uniform int semiTransparencyMode;\n"
	"layout (location = 22) uniform int drawTopLeftX;\n"
	"layout (location = 23) uniform int drawTopLeftY;\n"
	"layout (location = 24) uniform int drawBottomRightX;\n"
	"layout (location = 25) uniform int drawBottomRightY;\n"
	"\n"
	"// Dummy output value\n"
	"out vec4 colour;\n"
	"\n"
	"// Function declarations\n"
	"bool inDrawingArea(ivec2 pixelCoord);\n"
	"\n"
	"// Convert pixel format and store in vram texture\n"
	"void main(void) {\n"
	"	// Get coordinate from gl_FragCoord and apply offset to\n"
	"	// make calculations easier\n"
	"	ivec2 tempDrawCoord = ivec2(gl_FragCoord.xy);\n"
	"	tempDrawCoord.x -= xOffset;\n"
	"	tempDrawCoord.y -= yOffset;\n"
	"\n"
	"	// Declare texture pixel variable and make 0 for now\n"
	"	uvec4 texPixel = uvec4(0, 0, 0, 0);\n"
	"\n"
	"	// Handle differently depending on colour mode\n"
	"	if (texColourMode == 0) { // 4-bit colour mode\n"
	"		\n"
	"		// Calculate texture pixel coordinates using texture\n"
	"		// window settings\n"
	"		int new_tex_x =\n"
	"			((tex_x + (tempDrawCoord.x % 256)) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			((tex_y + (tempDrawCoord.y % 256)) & ~(texHeightMask * 8)) |\n"
	"			((texWinOffsetY & texHeightMask) * 8);\n"
	"\n"
	"		// Convert texture pixel x coordinate to 4-bit form, and\n"
	"		// get final pixel coordinates\n"
	"		new_tex_x = texBaseX + (new_tex_x / 4);\n"
	"		new_tex_y = texBaseY - new_tex_y;\n"
	"\n"
	"		// Get texture pixel\n"
	"		texPixel = imageLoad(vramImage, ivec2(new_tex_x, new_tex_y));\n"
	"		int texPixeli = ((int(texPixel.a) & 0x1) << 15) |\n"
	"							((int(texPixel.b) & 0x1F) << 10) |\n"
	"							((int(texPixel.g) & 0x1F) << 5) |\n"
	"							(int(texPixel.r) & 0x1F);\n"
	"		int clutIndex =\n"
	"			(texPixeli >> ((tempDrawCoord.x % 4) * 4)) & 0xF;\n"
	"\n"
	"		// Read correct pixel from CLUT\n"
	"		uvec4 clutPixel =\n"
	"			imageLoad(vramImage, ivec2(clut_x + clutIndex, clut_y));\n"
	"		texPixel.r = clutPixel.r;\n"
	"		texPixel.g = clutPixel.g;\n"
	"		texPixel.b = clutPixel.b;\n"
	"		texPixel.a = clutPixel.a;\n"
	"	}\n"
	"	else if (texColourMode == 1) { // 8-bit colour mode\n"
	"\n"
	"		// Calculate texture pixel coordinates using texture\n"
	"		// window settings\n"
	"		int new_tex_x =\n"
	"			((tex_x + (tempDrawCoord.x % 256)) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			((tex_y + (tempDrawCoord.y % 256)) & ~(texHeightMask * 8)) |\n"
	"			((texWinOffsetY & texHeightMask) * 8);\n"
	"\n"
	"		// Convert texture pixel x coordinate to 4-bit form, and\n"
	"		// get final pixel coordinates\n"
	"		new_tex_x = texBaseX + (new_tex_x / 2);\n"
	"		new_tex_y = texBaseY - new_tex_y;\n"
	"\n"
	"		// Get texture pixel\n"
	"		texPixel = imageLoad(vramImage, ivec2(new_tex_x, new_tex_y));\n"
	"		int texPixeli = ((int(texPixel.a) & 0x1) << 15) |\n"
	"							((int(texPixel.b) & 0x1F) << 10) |\n"
	"							((int(texPixel.g) & 0x1F) << 5) |\n"
	"							(int(texPixel.r) & 0x1F);\n"
	"		int clutIndex =\n"
	"			(texPixeli >> ((tempDrawCoord.x % 2) * 8)) & 0xFF;\n"
	"\n"
	"		// Read correct pixel from CLUT\n"
	"		uvec4 clutPixel =\n"
	"			imageLoad(vramImage, ivec2(clut_x + clutIndex, clut_y));\n"
	"		texPixel.r = clutPixel.r;\n"
	"		texPixel.g = clutPixel.g;\n"
	"		texPixel.b = clutPixel.b;\n"
	"		texPixel.a = clutPixel.a;\n"
	"	}\n"
	"	else { // 15-bit direct pixel mode\n"
	"\n"
	"		// Calculate texture pixel coordinates using texture\n"
	"		// window settings\n"
	"		int new_tex_x =\n"
	"			((tex_x + (tempDrawCoord.x % 256)) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			((tex_y + (tempDrawCoord.y % 256)) & ~(texHeightMask * 8)) |\n"
	"			((texWinOffsetY & texHeightMask) * 8);\n"
	"\n"
	"		// Convert texture pixel x coordinate to 4-bit form, and\n"
	"		// get final pixel coordinates\n"
	"		new_tex_x = texBaseX + new_tex_x;\n"
	"		new_tex_y = texBaseY - new_tex_y;\n"
	"\n"
	"		// Get texture pixel\n"
	"		texPixel = imageLoad(vramImage, ivec2(new_tex_x, new_tex_y));\n"
	"	}\n"
	"\n"
	"	// Swap y coordinate round so we flip texture back to\n"
	"	// the right way up\n"
	"	tempDrawCoord.y = height - 1 - tempDrawCoord.y;\n"
	"\n"
	"	// Get coordinate from gl_FragCoord and correctly reference\n"
	"	// vram texture\n"
	"	ivec2 vramCoord = ivec2(gl_FragCoord.xy);\n"
	"	vramCoord.y = tempDrawCoord.y + yOffset;\n"
	"\n"
	"	// Load vram pixel\n"
	"	uvec4 vramPixel = imageLoad(vramImage, vramCoord);\n"
	"\n"
	"	// Only proceed if pixel isn't fully transparent\n"
	"	if (texPixel.a == 0 && texPixel.r == 0 &&\n"
	"		texPixel.g == 0 && texPixel.b == 0) {\n"
	"		texPixel.r = vramPixel.r;\n"
	"		texPixel.g = vramPixel.g;\n"
	"		texPixel.b = vramPixel.b;\n"
	"	} else {\n"
	"\n"
	"		// Deal with blending\n"
	"		if (rawTextureEnabled == 0) {\n"
	"			// Merge pixel with blend colour\n"
	"			texPixel.r = ((texPixel.r << 3) * red) >> 7;\n"
	"			texPixel.r = texPixel.r >> 3;\n"
	"			if (texPixel.r > 0x1F) {\n"
	"				texPixel.r = 0x1F;\n"
	"			}\n"
	"			texPixel.g = ((texPixel.g << 3) * green) >> 7;\n"
	"			texPixel.g = texPixel.g >> 3;\n"
	"			if (texPixel.g > 0x1F) {\n"
	"				texPixel.g = 0x1F;\n"
	"			}\n"
	"			texPixel.b = ((texPixel.b << 3) * blue) >> 7;\n"
	"			texPixel.b = texPixel.b >> 3;\n"
	"			if (texPixel.b > 0x1F) {\n"
	"				texPixel.b = 0x1F;\n"
	"			}\n"
	"		}\n"
	"\n"
	"		// Handle semi-transparency here if enabled\n"
	"		if (semiTransparencyEnabled == 1) {\n"
	"			\n"
	"			if (texPixel.a == 1) {\n"
	"				int oldRed = int(vramPixel.r);\n"
	"				int oldGreen = int(vramPixel.g);\n"
	"				int oldBlue = int(vramPixel.b);\n"
	"			\n"
	"				int newRed = int(texPixel.r);\n"
	"				int newGreen = int(texPixel.g);\n"
	"				int newBlue = int(texPixel.b);\n"
	"\n"
	"				// Do calculation\n"
	"				switch (semiTransparencyMode) {\n"
	"					case 0: // B/2 + F/2\n"
	"						newRed = oldRed / 2 + newRed / 2;\n"
	"						newGreen = oldGreen / 2 + newGreen / 2;\n"
	"						newBlue = oldBlue / 2 + newBlue / 2;\n"
	"						break;\n"
	"					case 1: // B + F\n"
	"						newRed = oldRed + newRed;\n"
	"						newGreen = oldGreen + newGreen;\n"
	"						newBlue = oldBlue + newBlue;\n"
	"						break;\n"
	"					case 2: // B - F\n"
	"						newRed = oldRed - newRed;\n"
	"						newGreen = oldGreen - newGreen;\n"
	"						newBlue = oldBlue - newBlue;\n"
	"						break;\n"
	"					case 3: // B + F/4\n"
	"						newRed = oldRed + newRed / 4;\n"
	"						newGreen = oldGreen + newGreen / 4;\n"
	"						newBlue = oldBlue + newBlue / 4;\n"
	"						break;\n"
	"				}\n"
	"\n"
	"				// Saturate pixel\n"
	"				if (newRed < 0) {\n"
	"					newRed = 0;\n"
	"				}\n"
	"				else if (newRed > 31) {\n"
	"					newRed = 31;\n"
	"				}\n"
	"\n"
	"				if (newGreen < 0) {\n"
	"					newGreen = 0;\n"
	"				}\n"
	"				else if (newGreen > 31) {\n"
	"					newGreen = 31;\n"
	"				}\n"
	"\n"
	"				if (newBlue < 0) {\n"
	"					newBlue = 0;\n"
	"				}\n"
	"				else if (newBlue > 31) {\n"
	"					newBlue = 31;\n"
	"				}\n"
	"\n"
	"				// Store new pixel values\n"
	"				texPixel.r = newRed;\n"
	"				texPixel.g = newGreen;\n"
	"				texPixel.b = newBlue;\n"
	"			}\n"
	"		}\n"
	"	}\n"
	"\n"
	"	// Set mask bit if enabled\n"
	"	if (setMask == 1) {\n"
	"		texPixel.a = 0x1;\n"
	"	} else {\n"
	"		texPixel.a = 0;\n"
	"	}\n"
	"\n"
	"	// Check vram pixel mask if enabled, else just merge,\n"
	"	// also checking it is in draw area\n"
	"	bool inArea = inDrawingArea(vramCoord);\n"
	"	if (checkMask == 1) {\n"
	"		if (vramPixel.a != 1 && inArea) {\n"
	"			imageStore(vramImage, vramCoord, texPixel);\n"
	"		}\n"
	"	}\n"
	"	else if (inArea) {\n"
	"		imageStore(vramImage, vramCoord, texPixel);\n"
	"	}\n"
	"\n"
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