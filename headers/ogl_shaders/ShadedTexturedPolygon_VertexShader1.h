/*
 * This header file provides the OpenGL vertex shader for the
 * ShadedTexturedPolygon routine.
 * 
 * ShadedTexturedPolygon_VertexShader1.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_SHADEDTEXTUREDPOLYGON_VERTEXSHADER1
#define PHILPSX_SHADEDTEXTUREDPOLYGON_VERTEXSHADER1

static const char *GPU_getShadedTexturedPolygon_VertexShader1Source(void)
{
	return
	"#version 450 core\n"
	"\n"
	"layout (location = 0) uniform ivec3 vertex_x;\n"
	"layout (location = 1) uniform ivec3 vertex_y;\n"
	"layout (location = 2) uniform ivec3 texture_x;\n"
	"layout (location = 3) uniform ivec3 texture_y;\n"
	"layout (location = 11) uniform ivec3 redArray;\n"
	"layout (location = 12) uniform ivec3 greenArray;\n"
	"layout (location = 13) uniform ivec3 blueArray;\n"
	"\n"
	"out vec2 interpolated_tex_coord;\n"
	"out vec3 interpolated_colour;\n"
	"\n"
	"void main(void) {\n"
	"\n"
	"	// Convert coordinates into floating point and normalise them, also\n"
	"	// Dealing with texture coordinates\n"
	"	float x = 0.0;\n"
	"	float y = 0.0;\n"
	"	float tex_x = 0.0;\n"
	"	float tex_y = 0.0;\n"
	"	float red = 0.0;\n"
	"	float green = 0.0;\n"
	"	float blue = 0.0;\n"
	"\n"
	"	switch (gl_VertexID) {\n"
	"		case 0:\n"
	"			x = float(vertex_x.x);\n"
	"			y = float(vertex_y.x);\n"
	"			tex_x = float(texture_x.x);\n"
	"			tex_y = float(texture_y.x);\n"
	"			red = float(redArray.x);\n"
	"			green = float(greenArray.x);\n"
	"			blue = float(blueArray.x);\n"
	"			break;\n"
	"		case 1:\n"
	"			x = float(vertex_x.y);\n"
	"			y = float(vertex_y.y);\n"
	"			tex_x = float(texture_x.y);\n"
	"			tex_y = float(texture_y.y);\n"
	"			red = float(redArray.y);\n"
	"			green = float(greenArray.y);\n"
	"			blue = float(blueArray.y);\n"
	"			break;\n"
	"		case 2:\n"
	"			x = float(vertex_x.z);\n"
	"			y = float(vertex_y.z);\n"
	"			tex_x = float(texture_x.z);\n"
	"			tex_y = float(texture_y.z);\n"
	"			red = float(redArray.z);\n"
	"			green = float(greenArray.z);\n"
	"			blue = float(blueArray.z);\n"
	"			break;\n"
	"	}\n"
	"	x = (x / 512) - 1.0;\n"
	"	y = (y / 256) - 1.0;\n"
	"\n"
	"	// Output vertex coordinate\n"
	"	gl_Position = vec4(x, y, 0.0, 1.0);\n"
	"\n"
	"	// Output texture coordinate\n"
	"	interpolated_tex_coord = vec2(tex_x, tex_y);\n"
	"\n"
	"	// Output colour\n"
	"	interpolated_colour = vec3(red, green, blue);\n"
	"}\n";
}

#endif
