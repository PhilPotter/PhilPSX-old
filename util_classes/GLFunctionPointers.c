/*
 * This C file takes care of setting a struct full of all the OpenGL function
 * pointers the emulator uses to make GL calls.
 * 
 * GLFunctionPointers.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <GL/gl.h>
#include <SDL2/SDL.h>
#include "../headers/GLFunctionPointers.h"

/*
 * This function allows us to supply a GLFunctionPointers struct and have it
 * populated with the correct function pointers, thus helpfully keeping this
 * functionality all in one place. It is assumed that the GL context has been
 * initialised before calling this function.
 */
void GLFunctionPointers_setFunctionPointers(GLFunctionPointers *gl)
{
	// We assume that none of these calls can fail, as we ask for a GL 4.5
	// core context on startup, meaning if that fails then this will never
	// execute (in theory)
	gl->glActiveTexture =
		(glActiveTexture_type)SDL_GL_GetProcAddress("glActiveTexture");
	gl->glAttachShader =
		(glAttachShader_type)SDL_GL_GetProcAddress("glAttachShader");
	gl->glBindBufferBase =
		(glBindBufferBase_type)SDL_GL_GetProcAddress("glBindBufferBase");
	gl->glBindFramebuffer =
		(glBindFramebuffer_type)SDL_GL_GetProcAddress("glBindFramebuffer");
	gl->glBindImageTexture =
		(glBindImageTexture_type)SDL_GL_GetProcAddress("glBindImageTexture");
	gl->glBindTexture =
		(glBindTexture_type)SDL_GL_GetProcAddress("glBindTexture");
	gl->glBindVertexArray =
		(glBindVertexArray_type)SDL_GL_GetProcAddress("glBindVertexArray");
	gl->glBufferStorage =
		(glBufferStorage_type)SDL_GL_GetProcAddress("glBufferStorage");
	gl->glCompileShader =
		(glCompileShader_type)SDL_GL_GetProcAddress("glCompileShader");
	gl->glCreateBuffers =
		(glCreateBuffers_type)SDL_GL_GetProcAddress("glCreateBuffers");
	gl->glCreateFramebuffers =
		(glCreateFramebuffers_type)SDL_GL_GetProcAddress(
			"glCreateFramebuffers");
	gl->glCreateProgram =
		(glCreateProgram_type)SDL_GL_GetProcAddress("glCreateProgram");
	gl->glCreateShader =
		(glCreateShader_type)SDL_GL_GetProcAddress("glCreateShader");
	gl->glCreateTextures =
		(glCreateTextures_type)SDL_GL_GetProcAddress("glCreateTextures");
	gl->glCreateVertexArrays =
		(glCreateVertexArrays_type)SDL_GL_GetProcAddress(
			"glCreateVertexArrays");
	gl->glDeleteBuffers =
		(glDeleteBuffers_type)SDL_GL_GetProcAddress("glDeleteBuffers");
	gl->glDeleteFramebuffers =
		(glDeleteFramebuffers_type)SDL_GL_GetProcAddress(
			"glDeleteFramebuffers");
	gl->glDeleteProgram =
		(glDeleteProgram_type)SDL_GL_GetProcAddress("glDeleteProgram");
	gl->glDeleteShader =
		(glDeleteShader_type)SDL_GL_GetProcAddress("glDeleteShader");
	gl->glDeleteTextures =
		(glDeleteTextures_type)SDL_GL_GetProcAddress("glDeleteTextures");
	gl->glDeleteVertexArrays =
		(glDeleteVertexArrays_type)SDL_GL_GetProcAddress(
			"glDeleteVertexArrays");
	gl->glDetachShader =
			(glDetachShader_type)SDL_GL_GetProcAddress("glDetachShader");
	gl->glDisable =
		(glDisable_type)SDL_GL_GetProcAddress("glDisable");
	gl->glDrawArrays =
		(glDrawArrays_type)SDL_GL_GetProcAddress("glDrawArrays");
	gl->glDrawBuffers =
		(glDrawBuffers_type)SDL_GL_GetProcAddress("glDrawBuffers");
	gl->glFramebufferParameteri =
		(glFramebufferParameteri_type)SDL_GL_GetProcAddress(
			"glFramebufferParameteri");
	gl->glFramebufferTexture2D =
		(glFramebufferTexture2D_type)SDL_GL_GetProcAddress(
			"glFramebufferTexture2D");
	gl->glGetError =
		(glGetError_type)SDL_GL_GetProcAddress("glGetError");
	gl->glGetShaderiv =
		(glGetShaderiv_type)SDL_GL_GetProcAddress("glGetShaderiv");
	gl->glLinkProgram =
		(glLinkProgram_type)SDL_GL_GetProcAddress("glLinkProgram");
	gl->glMemoryBarrier =
		(glMemoryBarrier_type)SDL_GL_GetProcAddress("glMemoryBarrier");
	gl->glReadPixels =
		(glReadPixels_type)SDL_GL_GetProcAddress("glReadPixels");
	gl->glShaderSource =
		(glShaderSource_type)SDL_GL_GetProcAddress("glShaderSource");
	gl->glTexParameteri =
		(glTexParameteri_type)SDL_GL_GetProcAddress("glTexParameteri");
	gl->glTexStorage2D =
		(glTexStorage2D_type)SDL_GL_GetProcAddress("glTexStorage2D");
	gl->glTexSubImage2D =
		(glTexSubImage2D_type)SDL_GL_GetProcAddress("glTexSubImage2D");
	gl->glUniform1i =
		(glUniform1i_type)SDL_GL_GetProcAddress("glUniform1i");
	gl->glUniform3iv =
		(glUniform3iv_type)SDL_GL_GetProcAddress("glUniform3iv");
	gl->glUseProgram =
		(glUseProgram_type)SDL_GL_GetProcAddress("glUseProgram");
	gl->glViewport =
		(glViewport_type)SDL_GL_GetProcAddress("glViewport");
}