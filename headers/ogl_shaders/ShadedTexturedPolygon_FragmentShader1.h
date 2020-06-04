/*
 * This header file provides the OpenGL fragment shader for the
 * ShadedTexturedPolygon routine.
 * 
 * ShadedTexturedPolygon_FragmentShader1.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_SHADEDTEXTUREDPOLYGON_FRAGMENTSHADER1
#define PHILPSX_SHADEDTEXTUREDPOLYGON_FRAGMENTSHADER1

static const char *GPU_getShadedTexturedPolygon_FragmentShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Image corresponding to vram texture\n"
	"layout (binding = 1, rgba8ui) uniform uimage2D vramImage;\n"
	"\n"
	"// Uniforms to control draw process\n"
	"layout (location = 2) uniform ivec3 texture_x;\n"
	"layout (location = 3) uniform ivec3 texture_y;\n"
	"layout (location = 4) uniform int texBaseX;\n"
	"layout (location = 5) uniform int texBaseY;\n"
	"layout (location = 6) uniform int texColourMode;\n"
	"layout (location = 7) uniform int texWidthMask;\n"
	"layout (location = 8) uniform int texHeightMask;\n"
	"layout (location = 9) uniform int texWinOffsetX;\n"
	"layout (location = 10) uniform int texWinOffsetY;\n"
	"layout (location = 14) uniform int dither;\n"
	"layout (location = 15) uniform int disableBlending;\n"
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
	"// Texture coordinate input value\n"
	"in vec2 interpolated_tex_coord;\n"
	"\n"
	"// Colour input value\n"
	"in vec3 interpolated_colour;\n"
	"\n"
	"// Function declarations\n"
	"bool inDrawingArea(ivec2 pixelCoord);\n"
	"\n"
	"// Dummy output value\n"
	"out vec4 colour;\n"
	"\n"
	"// Draw pixel to vram texture, correctly applying textures etc.\n"
	"void main(void) {\n"
	"	// Get coordinate from gl_FragCoord\n"
	"	ivec2 tempDrawCoord = ivec2(gl_FragCoord.xy);\n"
	"\n"
	"	// Declare texture pixel variable and make 0 for now\n"
	"	uvec4 texPixel = uvec4(0, 0, 0, 0);\n"
	"\n"
	"	// Handle differently depending on colour mode\n"
	"	if (texColourMode == 0) { // 4-bit colour mode\n"
	"\n"
	"		// Calculate texture pixel coordinates using texture\n"
	"		// window settings\n"
	"		int new_tex_x =\n"
	"			(int(interpolated_tex_coord.x) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			(int(interpolated_tex_coord.y) & ~(texHeightMask * 8)) |\n"
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
	"						((int(texPixel.b) & 0x1F) << 10) |\n"
	"						((int(texPixel.g) & 0x1F) << 5) |\n"
	"						(int(texPixel.r) & 0x1F);\n"
	"		int clutIndex =\n"
	"			(texPixeli >>\n"
	"			((int(interpolated_tex_coord.x) % 4) * 4)) & 0xF;\n"
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
	"		// Calculate texture pixel coordinates using\n"
	"		// texture window settings\n"
	"		int new_tex_x =\n"
	"			(int(interpolated_tex_coord.x) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			(int(interpolated_tex_coord.y) & ~(texHeightMask * 8)) |\n"
	"			((texWinOffsetY & texHeightMask) * 8);\n"
	"\n"
	"		// Convert texture pixel x coordinate to 4-bit form,\n"
	"		// and get final pixel coordinates\n"
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
	"			(texPixeli >>\n"
	"			((int(interpolated_tex_coord.x) % 2) * 8)) & 0xFF;\n"
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
	"			(int(interpolated_tex_coord.x) & ~(texWidthMask * 8)) |\n"
	"			((texWinOffsetX & texWidthMask) * 8);\n"
	"		int new_tex_y =\n"
	"			(int(interpolated_tex_coord.y) & ~(texHeightMask * 8)) |\n"
	"			((texWinOffsetY & texHeightMask) * 8);\n"
	"\n"
	"		// Convert texture pixel x coordinate to 4-bit form,\n"
	"		// and get final pixel coordinates\n"
	"		new_tex_x = texBaseX + new_tex_x;\n"
	"		new_tex_y = texBaseY - new_tex_y;\n"
	"\n"
	"		// Get texture pixel\n"
	"		texPixel = imageLoad(vramImage, ivec2(new_tex_x, new_tex_y));\n"
	"	}\n"
	"\n"
	"	// Load vram pixel\n"
	"	uvec4 vramPixel = imageLoad(vramImage, tempDrawCoord);\n"
	"\n"
	"	// Deal with blending and dithering if pixel isn't fully transparent\n"
	"	if (texPixel.a == 0 && texPixel.r == 0 &&\n"
	"		texPixel.g == 0 && texPixel.b == 0) {\n"
	"		texPixel.r = vramPixel.r;\n"
	"		texPixel.g = vramPixel.g;\n"
	"		texPixel.b = vramPixel.b;\n"
	"	} else {\n"
	"\n"
	"		// Merge pixel with blend colour\n"
	"		if (disableBlending == 0) {\n"
	"			texPixel.r =\n"
	"				((texPixel.r << 3) * int(interpolated_colour.r)) >> 7;\n"
	"			texPixel.g =\n"
	"				((texPixel.g << 3) * int(interpolated_colour.g)) >> 7;\n"
	"			texPixel.b =\n"
	"				((texPixel.b << 3) * int(interpolated_colour.b)) >> 7;\n"
	"		}\n"
	"\n"
	"		// Check for dither bit\n"
	"		if (dither == 1 && disableBlending == 0) {\n"
	"\n"
	"			// Declare dither pixel as signed int vector as\n"
	"			// otherwise calculations will be off\n"
	"			ivec3 ditherPixel =\n"
	"				ivec3(int(texPixel.r),\n"
	"						int(texPixel.g),\n"
	"						int(texPixel.b));\n"
	"\n"
	"			// Define dither offset array\n"
	"			int ditherArray[4][4];\n"
	"			ditherArray[0][0] = -4;\n"
	"			ditherArray[0][1] = 2;\n"
	"			ditherArray[0][2] = -3;\n"
	"			ditherArray[0][3] = +3;\n"
	"			ditherArray[1][0] = 0;\n"
	"			ditherArray[1][1] = -2;\n"
	"			ditherArray[1][2] = 1;\n"
	"			ditherArray[1][3] = -1;\n"
	"			ditherArray[2][0] = -3;\n"
	"			ditherArray[2][1] = 3;\n"
	"			ditherArray[2][2] = -4;\n"
	"			ditherArray[2][3] = 2;\n"
	"			ditherArray[3][0] = 1;\n"
	"			ditherArray[3][1] = -1;\n"
	"			ditherArray[3][2] = 0;\n"
	"			ditherArray[3][3] = -2;\n"
	"\n"
	"			// Calculate dither column and row\n"
	"			int ditherColumn = tempDrawCoord.x % 4;\n"
	"			int ditherRow = (511 - tempDrawCoord.y) % 4;\n"
	"\n"
	"			// Modify pixel\n"
	"			ditherPixel.r += ditherArray[ditherColumn][ditherRow];\n"
	"			ditherPixel.g += ditherArray[ditherColumn][ditherRow];\n"
	"			ditherPixel.b += ditherArray[ditherColumn][ditherRow];\n"
	"\n"
	"			if (ditherPixel.r < 0) {\n"
	"				ditherPixel.r = 0;\n"
	"			}\n"
	"			else if (ditherPixel.r > 0xFF) {\n"
	"				ditherPixel.r = 0xFF;\n"
	"			}\n"
	"\n"
	"			if (ditherPixel.g < 0) {\n"
	"				ditherPixel.g = 0;\n"
	"			}\n"
	"			else if (ditherPixel.g > 0xFF) {\n"
	"				ditherPixel.g = 0xFF;\n"
	"			}\n"
	"\n"
	"			if (ditherPixel.b < 0) {\n"
	"				ditherPixel.b = 0;\n"
	"			}\n"
	"			else if (ditherPixel.b > 0xFF) {\n"
	"				ditherPixel.b = 0xFF;\n"
	"			}\n"
	"\n"
	"			texPixel.r = uint(ditherPixel.r);\n"
	"			texPixel.g = uint(ditherPixel.g);\n"
	"			texPixel.b = uint(ditherPixel.b);\n"
	"		}\n"
	"\n"
	"		// Restore colours to original 15-bit format\n"
	"		if (disableBlending == 0) {\n"
	"			texPixel.r = texPixel.r >> 3;\n"
	"			if (texPixel.r > 0x1F) {\n"
	"				texPixel.r = 0x1F;\n"
	"			}\n"
	"			texPixel.g = texPixel.g >> 3;\n"
	"			if (texPixel.g > 0x1F) {\n"
	"				texPixel.g = 0x1F;\n"
	"			}\n"
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
	"	// Check vram pixel if enabled, else just merge, also\n"
	"	// checking tex pixel is in draw area\n"
	"	bool inArea = inDrawingArea(tempDrawCoord);\n"
	"	if (checkMask == 1) {\n"
	"		if (vramPixel.a != 1 && inArea) {\n"
	"			imageStore(vramImage, tempDrawCoord, texPixel);\n"
	"		}\n"
	"	}\n"
	"	else if (inArea) {\n"
	"		imageStore(vramImage, tempDrawCoord, texPixel);\n"
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