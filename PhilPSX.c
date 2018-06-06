/*
 * This file starts the emulator off at present.
 * 
 * PhilPSX.c - Copyright Phillip Potter, 2018
 */
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "headers/SystemInterlink.h"

// PhilPSX entry point
int main(int argc, char **argv)
{	
	// variables
	int retval = 0;
	
	// Setup SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "PhilPSX: Error starting SDL: %s\n", SDL_GetError());
		retval = 1;
		goto end;
	}
	
	// Set OpenGL context parameters (OpenGL 4.5 core profile with
	// 8 bits per colour channel
	if (SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_RED_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_GREEN_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_BLUE_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_ALPHA_SIZE: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_DOUBLE_BUFFER: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_PROFILE_MASK: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_MAJOR_VERSION: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) != 0) {
		fprintf(stderr, "PhilPSX: Couldn't set attribute SDL_GL_CONTEXT_MINOR_VERSION: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	
	// Create window
	SDL_Window *philpsxWindow = SDL_CreateWindow(
			"PhilPSX",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			640,
			480,
			SDL_WINDOW_OPENGL);
	if (!philpsxWindow) {
		fprintf(stderr, "PhilPSX: Couldn't create window: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_sdl;
	}
	
	// Create OpenGL context
	SDL_GLContext philpsxGlContext = SDL_GL_CreateContext(philpsxWindow);
	if (!philpsxGlContext) {
		fprintf(stderr, "PhilPSX: Couldn't create OpenGL context: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_window;
	}
	
	// Enter event loop just to keep window open
	SDL_Event myEvent;
	int waitStatus;
	bool exit = false;
	while ((waitStatus = SDL_WaitEvent(&myEvent)) && !exit) {
		switch (myEvent.type) {
		case SDL_QUIT:
			exit = true;
			break;
		}
	}
		
	if (waitStatus == 0) {
		fprintf(stderr, "PhilPSX: Couldn't wait on event properly: %s\n", SDL_GetError());
		retval = 1;
		goto cleanup_opengl;
	}
	
	// Destroy OpenGL context
	cleanup_opengl:
	SDL_GL_DeleteContext(philpsxGlContext);
	
	// Destroy window
	cleanup_window:
	SDL_DestroyWindow(philpsxWindow);

	// Shutdown SDL
	cleanup_sdl:
	SDL_Quit();
	
	// End program
	end:
	return retval;
}