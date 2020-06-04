/*
 * This header file provides the public API for an array based list, which
 * can be used anywhere in the emulator where a list is required. This list
 * will destruct anything put inside it when it is destructed, using the
 * specified function pointer (or by calling free if NULL is supplied here).
 * This behaviour can be disabled by passing a parameter to the constructor.
 * Removing an object will also destruct if (if enabled). Bounds checking is
 * performed in functions that take an index. Thread safety is provided for all
 * operations individually when enabled.
 * 
 * The array list is backed by a malloc'ed array which is grown and shrunk
 * according to certain metrics.
 * 
 * ArrayList.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_ARRAYLIST_HEADER
#define PHILPSX_ARRAYLIST_HEADER

// System includes
#include <stdbool.h>
#include <stddef.h>

// Typedefs
typedef struct ArrayList ArrayList;

// Public functions
ArrayList *construct_ArrayList(void (*destructPtr)(void *object),
		bool dontDestructObjects, bool notThreadSafe);
void destruct_ArrayList(ArrayList *al);
size_t ArrayList_getSize(ArrayList *al);
void *ArrayList_addObject(ArrayList *al, void *object);
void *ArrayList_addObjectAt(ArrayList *al, size_t index, void *object);
void ArrayList_removeObject(ArrayList *al, size_t index);
void ArrayList_replaceObject(ArrayList *al, size_t index, void *object);
void *ArrayList_getObject(ArrayList *al, size_t index);
void ArrayList_wipeAllObjects(ArrayList *al);
ArrayList *ArrayList_clone(ArrayList *original);

#endif