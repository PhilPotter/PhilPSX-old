/*
 * This header file provides the OpenGL vertex shader for the AnyLine
 * routine.
 * 
 * AnyLine_VertexShader1.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_ANYLINE_VERTEXSHADER1
#define PHILPSX_ANYLINE_VERTEXSHADER1

static const char *GPU_getAnyLine_VertexShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Uniforms to specify parameters of line drawing\n"
	"layout (location = 0) uniform int first_x;\n"
	"layout (location = 1) uniform int first_y;\n"
	"layout (location = 2) uniform int second_x;\n"
	"layout (location = 3) uniform int second_y;\n"
	"layout (location = 4) uniform int firstRed;\n"
	"layout (location = 5) uniform int firstGreen;\n"
	"layout (location = 6) uniform int firstBlue;\n"
	"layout (location = 7) uniform int secondRed;\n"
	"layout (location = 8) uniform int secondGreen;\n"
	"layout (location = 9) uniform int secondBlue;\n"
	"\n"
	"// Output value to allow colour interpolation\n"
	"out vec3 vertexColour;\n"
	"\n"
	"// Draw line\n"
	"void main(void) {\n"
	"\n"
	"	// Convert coordinates of line to floating point and normalise\n"
	"	float x = 0.0;\n"
	"	float y = 0.0;\n"
	"	switch (gl_VertexID) {\n"
	"		case 0:\n"
	"			x = float(first_x);\n"
	"			y = float(first_y);\n"
	"			break;\n"
	"		case 1:\n"
	"			x = float(second_x);\n"
	"			y = float(second_y);\n"
	"			break;\n"
	"	}\n"
	"	x = (x / 512) - 1.0;\n"
	"	y = (y / 256) - 1.0;\n"
	"\n"
	"	// Store for fragment shader\n"
	"	gl_Position = vec4(x, y, 0.0, 1.0);\n"
	"\n"
	"	// Store output colour\n"
	"	vec3 tempColour = vec3(0.0, 0.0, 0.0);\n"
	"	switch (gl_VertexID) {\n"
	"		case 0:\n"
	"			tempColour.r = float(firstRed);\n"
	"			tempColour.g = float(firstGreen);\n"
	"			tempColour.b = float(firstBlue);\n"
	"			break;\n"
	"		case 1:\n"
	"			tempColour.r = float(secondRed);\n"
	"			tempColour.g = float(secondGreen);\n"
	"			tempColour.b = float(secondBlue);\n"
	"			break;\n"
	"	}\n"
	"	vertexColour = tempColour;\n"
	"}\n";
}

#endif