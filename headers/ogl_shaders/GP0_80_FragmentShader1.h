/*
 * This header file provides the first OpenGL fragment shader for the GP0_80
 * routine.
 * 
 * GP0_80_FragmentShader1.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_GP0_80_FRAGMENTSHADER1
#define PHILPSX_GP0_80_FRAGMENTSHADER1

static const char *GPU_getGP0_80_FragmentShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Images corresponding to temp draw texture and vram texture\n"
	"layout (binding = 0, rgba8ui) uniform uimage2D tempDrawImage;\n"
	"layout (binding = 1, rgba8ui) uniform uimage2D vramImage;\n"
	"\n"
	"// Uniforms to control copy process\n"
	"layout (location = 0) uniform int xOffset;\n"
	"layout (location = 1) uniform int yOffset;\n"
	"\n"
	"// Dummy output value\n"
	"out vec4 colour;\n"
	"\n"
	"// Convert pixel format and store in vram texture\n"
	"void main(void) {\n"
	"	// Get coordinate from gl_FragCoord and apply offset to\n"
	"	// correctly reference temp draw texture\n"
	"	ivec2 tempDrawCoord = ivec2(gl_FragCoord.xy);\n"
	"	tempDrawCoord.x -= xOffset;\n"
	"	tempDrawCoord.y -= yOffset;\n"
	"\n"
	"	// Get coordinate from gl_FragCoord and correctly reference\n"
	"	// vram texture\n"
	"	ivec2 vramCoord = ivec2(gl_FragCoord.xy);\n"
	"\n"
	"	// Load vram pixel\n"
	"	uvec4 vramPixel = imageLoad(vramImage, vramCoord);\n"
	"\n"
	"	// Store vram pixel in temp draw texture\n"
	"	imageStore(tempDrawImage, tempDrawCoord, vramPixel);\n"
	"\n"
	"	// Set dummy output value\n"
	"	colour = vec4(0.0, 0.0, 0.0, 0.0);\n"
	"}\n";
}

#endif