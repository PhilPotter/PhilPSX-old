/*
 * This header file provides the OpenGL fragment shader for the GP0_02
 * routine.
 * 
 * GP0_02_FragmentShader1.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_GP0_02_FRAGMENTSHADER1
#define PHILPSX_GP0_02_FRAGMENTSHADER1

static const char *GPU_getGP0_02_FragmentShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Uniforms to control copy process\n"
	"layout (location = 0) uniform int red;\n"
	"layout (location = 1) uniform int green;\n"
	"layout (location = 2) uniform int blue;\n"
	"\n"
	"// Output value\n"
	"out uvec4 colour;\n"
	"\n"
	"// Set rectangle pixel to right colour\n"
	"void main(void) {\n"
	"	colour = uvec4((red >> 3) & 0x1F,\n"
	"					(green >> 3) & 0x1F,\n"
	"					(blue >> 3) & 0x1F, 0);\n"
	"}\n";
}

#endif