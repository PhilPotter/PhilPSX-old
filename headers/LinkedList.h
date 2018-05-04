/*
 * This header file provides the public API for a singly-linked list, which
 * can be used anywhere in the emulator where a list is required. This list
 * will destruct anything put inside it when it is destructed, using the
 * specified function pointer (or by calling free if NULL is supplied here).
 * This behaviour can be disabled by passing a parameter to the constructor.
 * Removing an object will also do the same. Bounds checking is performed
 * in functions that take an index. Thread safety is provided for all
 * operations individually.
 * 
 * LinkedList.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_LINKEDLIST_HEADER
#define PHILPSX_LINKEDLIST_HEADER

// Includes
#include <stdbool.h>
#include <stddef.h>

// Typedefs
typedef struct LinkedList LinkedList;

// Public functions
LinkedList *construct_LinkedList(void (*destructPtr)(void *object),
		bool dontDestructObjects, bool notThreadSafe);
void destruct_LinkedList(LinkedList *ll);
size_t LinkedList_getSize(LinkedList *ll);
void *LinkedList_addObject(LinkedList *ll, void *object);
void *LinkedList_addObjectAt(LinkedList *ll, size_t index, void *object);
void LinkedList_removeObject(LinkedList *ll, size_t index);
void *LinkedList_getObject(LinkedList *ll, size_t index);
void LinkedList_wipeAllObjects(LinkedList *ll);
LinkedList *LinkedList_clone(LinkedList *original);

#endif