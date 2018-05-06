/*
 * This C file models a simple array list implementation as a class. It
 * also performs bounds checking where appropriate (when using indexes).
 * Thread safety is provided for all operations individually when enabled.
 * 
 * ArrayList.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../headers/ArrayList.h"

// Sizes for array list
#define PHILPSX_ARRAYLIST_INITIAL_SIZE 1
#define PHILPSX_ARRAYLIST_SCALE_FACTOR 2
#define PHILPSX_ARRAYLIST_NO_SHRINK_BELOW_SIZE 10

// Forward declarations for functions and subcomponents private to this class
// Expansion and shrinking functions:
static bool ArrayList_expandIfNeeded(ArrayList *al);
static bool ArrayList_shrinkIfNeeded(ArrayList *al);

/*
 * This struct models the list, as well as other state info to help with
 * common operations.
 */
struct ArrayList {
	
	// Mutex to allow thread-safe operations on this list
	pthread_mutex_t mutex;
	
	// Pointer to malloc'ed storage (the array stores void pointers)
	void **dataArray;
	
	// Function pointer for destructing objects stored in nodes
	void (*destructPtr)(void *object);
	
	// This flag can be used to disabled destruction of objects entirely
	bool dontDestructObjects;
	
	// This flag can be used to disabled synchronisation of objects entirely
	bool notThreadSafe;
	
	// Size of list (number of objects contained)
	size_t size;
	
	// Actual number of available array slots
	size_t actualSize;
};

/*
 * This constructs an ArrayList object.
 */
ArrayList *construct_ArrayList(void (*destructPtr)(void *object),
		bool dontDestructObjects, bool notThreadSafe)
{
	// Allocate list
	ArrayList *al = malloc(sizeof(ArrayList));
	if (!al) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't allocate memory for "
				"ArrayList struct\n");
		goto end;
	}
	
	// Allocate storage for array
	al->dataArray = calloc(PHILPSX_ARRAYLIST_INITIAL_SIZE, sizeof(void *));
	if (!al->dataArray) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't allocate memory for "
				"the initial array object\n");
		goto cleanup_arraylist;
	}
	
	// Wipe and initialise mutex
	if (memset(&al->mutex, 0, sizeof(pthread_mutex_t)) != &al->mutex) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't wipe memory for "
				"the pthread_mutex_t object\n");
		goto cleanup_arraydata;
	}
	al->notThreadSafe = notThreadSafe;
	if (!notThreadSafe) {
		if (pthread_mutex_init(&al->mutex, NULL) != 0) {
			fprintf(stderr, "PhilPSX: ArrayList: Couldn't initialise the "
					"pthread_mutex_t object\n");
			goto cleanup_arraydata;
		}
	}
	
	// Store object destruct function pointer
	al->destructPtr = destructPtr;
	
	// Store dont destruct flag
	al->dontDestructObjects = dontDestructObjects;
	
	// Set size to zero and actual size to initial size
	al->size = 0;
	al->actualSize = PHILPSX_ARRAYLIST_INITIAL_SIZE;
	
	// Normal return:
	return al;
	
	// Cleanup path:
	cleanup_arraydata:
	free(al->dataArray);
	
	cleanup_arraylist:
	free(al);
	al = NULL;
	
	end:
	return al;
}

/*
 * This destructs an ArrayList object. It will also destruct any objects
 * referenced by the array slots (if enabled) using the function pointer
 * supplied at construction time, or by calling free if this was NULL.
 */
void destruct_ArrayList(ArrayList *al)
{
	// Walk list, cleaning up as we go (if enabled)
	if (!al->dontDestructObjects) {
		for (size_t i = 0; i < al->size; ++i) {
			if (al->dataArray[i]) {

				// Destruct with custom function pointer if present,
				// else just free
				if (al->destructPtr)
					al->destructPtr(al->dataArray[i]);
				else
					free(al->dataArray[i]);
			}
		}
	}
	
	// Destroy mutex (if enabled)
	if (!al->notThreadSafe) {
		if (pthread_mutex_destroy(&al->mutex) != 0) {
			fprintf(stderr, "PhilPSX: ArrayList: Couldn't destroy the "
					"pthread_mutex_t object\n");
		}
	}
	
	// Free dataArray
	free(al->dataArray);

	// Free ArrayList struct itself
	free(al);
}

/*
 * This function returns the current size (number of elements) of the array list.
 */
size_t ArrayList_getSize(ArrayList *al)
{
	// Declare return value
	size_t size = 0;
	
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_getSize\n");
	}
	
	// Store value
	size = al->size;
	
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_getSize\n");
	}
	
	return size;
}

/*
 * This function adds the object to the end of the array list.
 */
void *ArrayList_addObject(ArrayList *al, void *object)
{
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_addObject\n");
	}
	
	// Expand list if needed
	if (!ArrayList_expandIfNeeded(al)) {
		fprintf(stderr, "PhilPSX: ArrayList: Expansion routine failed in "
				"ArrayList_addObject, object not added\n");
		object = NULL;
		goto end;
	}
	
	// Add object to end of currently used slots
	al->dataArray[al->size] = object;

	// Increment size (number of objects stored)
	++al->size;
	
	end:
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_addObject\n");
	}
	return object;
}

/*
 * This function adds the object to the specified index in the array list.
 */
void *ArrayList_addObjectAt(ArrayList *al, size_t index, void *object)
{
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_addObjectAt\n");
	}
	
	// Check size of list (index == size will mean we are targetting last
	// position in the array list)
	if (index <= al->size) {
		
		// Expand list if needed
		if (!ArrayList_expandIfNeeded(al)) {
			fprintf(stderr, "PhilPSX: ArrayList: Expansion routine failed in "
					"ArrayList_addObjectAt, object not added\n");
			object = NULL;
			goto end;
		}
		
		// Move every object from dataArray[index] to
		// dataArray[num of elements] right by one
		void **destination = al->dataArray + index + 1;
		void **source = al->dataArray + index;
		size_t numOfBytes = (al->size - index) * sizeof(void *);
		memmove(destination, source, numOfBytes);
		
		// Add object to correct index
		al->dataArray[index] = object;

		// Increment size (number of objects stored)
		++al->size;
	}
	else {
		fprintf(stderr, "PhilPSX: ArrayList: Invalid index specified to "
				"ArrayList_addObjectAt, action not performed\n");
	}
	
	end:
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_addObjectAt\n");
	}
	return object;
}

/*
 * This function removes an object from the array list and also destructs it
 * (if enabled) using either the list's deletion function pointer, or by just
 * calling free.
 */
void ArrayList_removeObject(ArrayList *al, size_t index)
{
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_removeObject\n");
	}
	
    // Check index and continue if valid
    if (index < al->size)
    {
		// Destruct object if enabled
        if (!al->dontDestructObjects) {
			if (al->dataArray[index]) {

				// Destruct with custom function pointer if present,
				// else just free
				if (al->destructPtr)
					al->destructPtr(al->dataArray[index]);
				else
					free(al->dataArray[index]);
			}
		}
		
		// Move every object from dataArray[index + 1] to
		// dataArray[num of elements] left by one
		void **destination = al->dataArray + index;
		void **source = al->dataArray + index + 1;
		size_t numOfBytes = (al->size - (index + 1)) * sizeof(void *);
		memmove(destination, source, numOfBytes);

        // Decrement size
        --al->size;
		
		// Shrink list if we are oversized
		if (!ArrayList_shrinkIfNeeded(al)) {
			fprintf(stderr, "PhilPSX: ArrayList: Contraction routine failed in "
					"ArrayList_removeObject, actual size not changed\n");
		}
    }
	else {
		fprintf(stderr, "PhilPArrayList: Invalid index specified to "
				"ArrayList_removeObject, action not performed\n");
	}
	
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_removeObject\n");
	}
}

/*
 * This function replaces an object in the array list.
 */
void ArrayList_replaceObject(ArrayList *al, size_t index, void *object)
{
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_replaceObject\n");
	}
	
    // Check index and continue if valid
    if (index < al->size)
    {
		al->dataArray[index] = object;
    }
	else {
		fprintf(stderr, "PhilPArrayList: Invalid index specified to "
				"ArrayList_replaceObject, action not performed\n");
	}
	
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_replaceObject\n");
	}
}

/*
 * This function returns an object from the list at the specified index.
 */
void *ArrayList_getObject(ArrayList *al, size_t index)
{
	// Define return value
	void *object = NULL;
	
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_getObject\n");
	}
	
	// Check index and continue if valid
	if (index < al->size)
	{
		// Set return value
		object = al->dataArray[index];		
	}
	else
	{
		fprintf(stderr, "PhilPSX: ArrayList: Invalid index specified to "
				"ArrayList_getObject, NULL returned\n");
	}
	
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_getObject\n");
	}

	// return
	return object;
}

/*
 * This destructs an ArrayList object. It will also destruct any objects
 * referenced by the slots of the array (if enabled) using the function pointer
 * supplied at construction time, or by calling free if this was NULL.
 */
void ArrayList_wipeAllObjects(ArrayList *al)
{
	// Lock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_lock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the mutex in "
				"ArrayList_wipeAllObjects\n");
	}

	// Walk list, cleaning up as we go
	for (size_t i = 0; i < al->size; ++i) {
		
		// Destruct this node if enabled
		if (!al->dontDestructObjects) {
			if (al->dataArray[i]) {

				// Destruct with custom function pointer if present,
				// else just free
				if (al->destructPtr)
					al->destructPtr(al->dataArray[i]);
				else
					free(al->dataArray[i]);
			}
		}
	}
	
	// Set size (number of elements) to zero
	al->size = 0;
	
	// Shrink array back to original size
	void **tempPtr = realloc(al->dataArray, sizeof(void *) * PHILPSX_ARRAYLIST_INITIAL_SIZE);
	if (!tempPtr) {
		// Contraction failed
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't shrink array back to "
				"original size in ArrayList_wipeAllObjects\n");
	} else {
		// Contraction succeeded, set dataArray to the new pointer and
		// set actualSize
		al->dataArray = tempPtr;
		al->actualSize = PHILPSX_ARRAYLIST_INITIAL_SIZE;
	}
	
	// Unlock mutex (if enabled)
	if (!al->notThreadSafe && pthread_mutex_unlock(&al->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the mutex in "
				"ArrayList_wipeAllObjects\n");
	} 
}

/*
 * This clones an ArrayList into another ArrayList, replicating its properties
 * such as thread safety and destructor choice. It does NOT clone the objects
 * referenced by each array slot, but merely copies their pointers. For this
 * reason, it is best to use this with dontDestructObjects enabled to avoid
 * double destruct operations/free operations.
 */
ArrayList *ArrayList_clone(ArrayList *original)
{
	// Lock source mutex (if enabled), and store original notThreadSafe value
	bool notThreadSafe = original->notThreadSafe;
	if (!original->notThreadSafe && pthread_mutex_lock(&original->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't lock the source mutex "
				"in ArrayList_clone\n");
	}
	
	// Set original temporarily to not thread safe so following calls do not
	// try and lock the mutex and lock the thread forever (if thread safety is
	// on)
	original->notThreadSafe = true;
	
	// Allocate return value
	ArrayList *newList = construct_ArrayList(original->destructPtr,
			original->dontDestructObjects, notThreadSafe);
	if (!newList) { 
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't construct new ArrayList"
				" object in ArrayList_clone\n");
		goto end;
	}
	
	// Do a memcpy from the original list to the new one, after resizing it
	if (original->actualSize != newList->actualSize) {
		size_t numOfBytes = sizeof(void *) * original->actualSize;
		void **tempPtr = realloc(newList->dataArray, numOfBytes);
		if (!tempPtr) {
			// Resize failed
			fprintf(stderr, "PhilPSX: ArrayList: Couldn't resize array by %zu "
					"bytes in ArayList_clone\n", numOfBytes);
			goto cleanup_arraylist;
		} else {
			// Resize succeeded, set dataArray to the new pointer and
			// set actualSize
			newList->dataArray = tempPtr;
			newList->actualSize = original->actualSize;
			
			// Do the memcpy and set number of objects size
			memcpy(newList->dataArray, original->dataArray, numOfBytes);
			newList->size = original->size;
		}
	}
	
	// Normal return:
	// Unlock source mutex (if enabled)
	original->notThreadSafe = notThreadSafe;
	if (!original->notThreadSafe && pthread_mutex_unlock(&original->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the source mutex "
				"in ArrayList_clone\n");
	}
	
	return newList;
	
	// Cleanup path:
	cleanup_arraylist:
	destruct_ArrayList(newList);
	newList = NULL;
	
	end:
	// Unlock source mutex (if enabled)
	original->notThreadSafe = notThreadSafe;
	if (!original->notThreadSafe && pthread_mutex_unlock(&original->mutex) != 0) {
		fprintf(stderr, "PhilPSX: ArrayList: Couldn't unlock the source mutex "
				"in ArrayList_clone\n");
	}
	
	return newList;
}

/*
 * This expands the data array if necessary - returns true if succeeded and
 * false if failed, but a true value doesn't necessarily mean any changes
 * were made.
 */
static bool ArrayList_expandIfNeeded(ArrayList *al)
{
	// Define return value
	bool retVal = true;
	
	// Determine if we need to expand dataArray
	size_t proposedSize = al->actualSize * PHILPSX_ARRAYLIST_SCALE_FACTOR;
	if (al->size == al->actualSize) {

		// dataArray is full, expand by scale factor
		void **tempPtr = realloc(al->dataArray,
				sizeof(void *) * proposedSize);
		if (!tempPtr) {
			// Expansion failed
			fprintf(stderr, "PhilPSX: ArrayList: Couldn't expand array by "
					"factor of %zu in ArayList_expandIfNeeded\n",
					PHILPSX_ARRAYLIST_SCALE_FACTOR);
			retVal = false;
		}
		else {
			// Expansion succeeded, set dataArray to the new pointer and
			// set actualSize
			al->dataArray = tempPtr;
			al->actualSize = proposedSize;
		}
	}
	
	return retVal;
}

/*
 * This shrinks the data array if necessary - returns true if succeeded and
 * false if failed, but a true value doesn't necessarily mean any changes were
 * made.
 */
static bool ArrayList_shrinkIfNeeded(ArrayList *al)
{
	// Define return value
	bool retVal = true;
	
	// Determine if we need to shrink dataArray
	size_t proposedSize = al->actualSize / PHILPSX_ARRAYLIST_SCALE_FACTOR;
	if (al->size <= proposedSize && proposedSize >= PHILPSX_ARRAYLIST_NO_SHRINK_BELOW_SIZE) {

		// dataArray is at threshold, shrink by scale factor
		void **tempPtr = realloc(al->dataArray, sizeof(void *) * proposedSize);
		if (!tempPtr) {
			// Contraction failed
			fprintf(stderr, "PhilPSX: ArrayList: Couldn't shrink array by "
					"factor of %zu in ArrayList_shrinkIfNeeded\n",
					PHILPSX_ARRAYLIST_SCALE_FACTOR);
			retVal = false;
		}
		else {
			// Contraction succeeded, set dataArray to the new pointer and
			// set actualSize
			al->dataArray = tempPtr;
			al->actualSize = proposedSize;
		}
	}
	
	return retVal;
}
