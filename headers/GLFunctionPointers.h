/*
 * This header file provides the means for storing and referring to the
 * various OpenGL function pointers that we will require to send commands
 * to the (actual) GPU.
 * 
 * GLFunctionPointers.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_GLFUNCTIONPOINTERS_HEADER
#define PHILPSX_GLFUNCTIONPOINTERS_HEADER

// System includes
#include <stdbool.h>
#include <GL/gl.h>

// Typedefs
typedef struct GLFunctionPointers GLFunctionPointers;
typedef void (APIENTRY *glActiveTexture_type)(GLenum texture);
typedef void (APIENTRY *glAttachShader_type)(GLuint program, GLuint shader);
typedef void (APIENTRY *glBindBufferBase_type)(GLenum target, GLuint index,
		GLuint buffer);
typedef void (APIENTRY *glBindFramebuffer_type)(GLenum target,
		GLuint framebuffer);
typedef void (APIENTRY *glBindImageTexture_type)(GLuint unit, GLuint texture,
		GLint level, GLboolean layered, GLint layer, GLenum access,
		GLenum format);
typedef void (APIENTRY *glBindTexture_type)(GLenum target, GLuint texture);
typedef void (APIENTRY *glBindVertexArray_type)(GLuint array);
typedef void (APIENTRY *glBufferStorage_type)(GLenum target, GLsizeiptr size,
		const GLvoid *data, GLbitfield flags);
typedef void (APIENTRY *glCompileShader_type)(GLuint shader);
typedef void (APIENTRY *glCreateBuffers_type)(GLsizei n, GLuint *buffers);
typedef void (APIENTRY *glCreateFramebuffers_type)(GLsizei n, GLuint *ids);
typedef GLuint (APIENTRY *glCreateProgram_type)(void);
typedef GLuint (APIENTRY *glCreateShader_type)(GLenum shaderType);
typedef void (APIENTRY *glCreateTextures_type)(GLenum target, GLsizei n,
		GLuint *textures);
typedef void (APIENTRY *glCreateVertexArrays_type)(GLsizei n, GLuint *arrays);
typedef void (APIENTRY *glDeleteBuffers_type)(GLsizei n,
		const GLuint *buffers);
typedef void (APIENTRY *glDeleteFramebuffers_type)(GLsizei n,
		const GLuint *framebuffers);
typedef void (APIENTRY *glDeleteProgram_type)(GLuint program);
typedef void (APIENTRY *glDeleteShader_type)(GLuint shader);
typedef void (APIENTRY *glDeleteTextures_type)(GLsizei n,
		const GLuint *textures);
typedef void (APIENTRY *glDeleteVertexArrays_type)(GLsizei n,
		const GLuint *arrays);
typedef void (APIENTRY *glDetachShader_type)(GLuint program, GLuint shader);
typedef void (APIENTRY *glDisable_type)(GLenum cap);
typedef void (APIENTRY *glDrawArrays_type)(GLenum mode, GLint first,
		GLsizei count);
typedef void (APIENTRY *glDrawBuffers_type)(GLsizei n, const GLenum *bufs);
typedef void (APIENTRY *glFramebufferParameteri_type)(GLenum target,
		GLenum pname, GLint param);
typedef void (APIENTRY *glFramebufferTexture2D_type)(GLenum target,
		GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY *glGetError_type)(void);
typedef void (APIENTRY *glGetShaderiv_type)(GLuint shader, GLenum pname,
		GLuint *params);
typedef void (APIENTRY *glLinkProgram_type)(GLuint program);
typedef void (APIENTRY *glMemoryBarrier_type)(GLbitfield barriers);
typedef void (APIENTRY *glReadPixels_type)(GLint x, GLint y, GLsizei width,
		GLsizei height, GLenum format, GLenum type, GLvoid *data);
typedef void (APIENTRY *glShaderSource_type)(GLuint shader, GLsizei count,
		const GLchar **string, const GLint *length);
typedef void (APIENTRY *glTexParameteri_type)(GLenum target, GLenum pname,
		GLint param);
typedef void (APIENTRY *glTexStorage2D_type)(GLenum target, GLsizei levels,
		GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glTexSubImage2D_type)(GLenum target, GLint level,
		GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
		GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *glUniform1i_type)(GLint location, GLint v0);
typedef void (APIENTRY *glUniform3iv_type)(GLint location, GLsizei count,
		const GLint *value);
typedef void (APIENTRY *glUseProgram_type)(GLuint program);
typedef void (APIENTRY *glViewport_type)(GLint x, GLint y, GLsizei width,
		GLsizei height);

// Struct definition - all these member pointers should be initialised
// externally with the correct function address before use. Above each
// pointer declaration is the version of OpenGL + extensions required to
// support it.
struct GLFunctionPointers {
	// >= 2.0
	glActiveTexture_type glActiveTexture;
	// >= 2.0
	glAttachShader_type glAttachShader;
	// >= 4.3 with GL_SHADER_STORAGE_BUFFER,
	// >= 4.2 with GL_ATOMIC_COUNTER_BUFFER,
	// >= 3.0 otherwise
	glBindBufferBase_type glBindBufferBase;
	// >= 3.0
	glBindFramebuffer_type glBindFramebuffer;
	// >= 4.2
	glBindImageTexture_type glBindImageTexture;
	// >= 3.2 with GL_TEXTURE_2D_MULTISAMPLE and
	// GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
	// >= 2.0 otherwise
	glBindTexture_type glBindTexture;
	// >= 3.0
	glBindVertexArray_type glBindVertexArray;
	// >= 4.4
	glBufferStorage_type glBufferStorage;
	// >= 2.0
	glCompileShader_type glCompileShader;
	// >= 4.5
	glCreateBuffers_type glCreateBuffers;
	// >= 4.5
	glCreateFramebuffers_type glCreateFramebuffers;
	// >= 2.0
	glCreateProgram_type glCreateProgram;
	// >= 4.3 with GL_COMPUTE_SHADER,
	// >= 2.0 otherwise
	glCreateShader_type glCreateShader;
	// >= 4.5
	glCreateTextures_type glCreateTextures;
	// >= 4.5
	glCreateVertexArrays_type glCreateVertexArrays;
	// >= 2.0
	glDeleteBuffers_type glDeleteBuffers;
	// >= 3.0
	glDeleteFramebuffers_type glDeleteFramebuffers;
	// >= 2.0
	glDeleteProgram_type glDeleteProgram;
	// >= 2.0
	glDeleteShader_type glDeleteShader;
	// >= 2.0
	glDeleteTextures_type glDeleteTextures;
	// >= 3.0
	glDeleteVertexArrays_type glDeleteVertexArrays;
	// >= 2.0
	glDetachShader_type glDetachShader;
	// >= 4.3 with GL_PRIMITIVE_RESTART_FIXED_INDEX, GL_DEBUG_OUTPUT and
	// GL_DEBUG_OUTPUT_SYNCHRONOUS,
	// >= 3.2 with GL_TEXTURE_CUBE_MAP_SEAMLESS,
	// >= 3.1 with GL_PRIMITIVE_RESTART,
	// >= 2.0 otherwise
	glDisable_type glDisable;
	// >= 3.2 with GL_LINE_STRIP_ADJACENCY, GL_LINES_ADJACENCY,
	// GL_TRIANGLE_STRIP_ADJACENCY and GL_TRIANGLES_ADJACENCY
	// >= 2.0 otherwise
	glDrawArrays_type glDrawArrays;
	// >= 2.0
	glDrawBuffers_type glDrawBuffers;
	// >= 4.3
	glFramebufferParameteri_type glFramebufferParameteri;
	// >= 3.0
	glFramebufferTexture2D_type glFramebufferTexture2D;
	// >= 2.0
	glGetError_type glGetError;
	// >= 2.0
	glGetShaderiv_type glGetShaderiv;
	// >= 2.0
	glLinkProgram_type glLinkProgram;
	// >= 4.4 with GL_QUERY_BUFFER_BARRIER_BIT,
	// >= 4.3 with GL_SHADER_STORAGE_BARRIER_BIT,
	// >= 4.2 otherwise
	glMemoryBarrier_type glMemoryBarrier;
	// >= 2.0
	glReadPixels_type glReadPixels;
	// >= 2.0
	glShaderSource_type glShaderSource;
	// >= 4.4 with GL_MIRROR_CLAMP_TO_EDGE,
	// >= 4.3 with GL_DEPTH_STENCIL_TEXTURE_MODE,
	// >= 2.0 otherwise
	glTexParameteri_type glTexParameteri;
	// >= 4.4 with GL_STENCIL_INDEX8 as internalformat,
	// >= 4.2 otherwise
	glTexStorage2D_type glTexStorage2D;
	// >= 4.4 with GL_STENCIL_INDEX as format,
	// >= 2.0 otherwise
	glTexSubImage2D_type glTexSubImage2D;
	// >= 2.0
	glUniform1i_type glUniform1i;
	// >= 2.0
	glUniform3iv_type glUniform3iv;
	// >= 2.0
	glUseProgram_type glUseProgram;
	// >= 2.0
	glViewport_type glViewport;
};

// Public functions
void GLFunctionPointers_setFunctionPointers(GLFunctionPointers *gl);

#endif