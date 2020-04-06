/*
 * This header file provides the first OpenGL vertex shader for the GP0_80
 * routine.
 * 
 * GP0_80_VertexShader1.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_GP0_80_VERTEXSHADER1
#define PHILPSX_GP0_80_VERTEXSHADER1

static const char *GPU_getGP0_80_VertexShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"// Fill whole viewport\n"
	"void main(void) {\n"
	"	const vec4 positions[4] = vec4[4](vec4(-1.0, -1.0, 0.0, 1.0),\n"
	"									  vec4(-1.0, 1.0, 0.0, 1.0),\n"
	"									  vec4(1.0, -1.0, 0.0, 1.0),\n"
	"									  vec4(1.0, 1.0, 0.0, 1.0));\n"
	"	gl_Position = positions[gl_VertexID];\n"
	"}\n";
}

#endif
