/*
 * This C file models a simple singly-linked list implementation as a class. It
 * also performs bounds checking where appropriate (when using indexes).
 * Thread safety is provided for all operations individually when enabled.
 * Most pthread errors (other than those related to mutex initialisation) are
 * not checked for, as it is assumed they cannot occur under the use case for
 * which this linked list was written.
 * 
 * LinkedList.c - Copyright Phillip Potter, 2020
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../headers/LinkedList.h"

// Forward declarations for functions and subcomponents private to this class
// ListNode-related stuff:
typedef struct ListNode ListNode;
static ListNode *construct_ListNode(void);
static void destruct_ListNode(ListNode *ln);

/*
 * This struct models the list, as well as other state info to help with
 * common operations.
 */
struct LinkedList {
	
	// Mutex to allow thread-safe operations on this list
	pthread_mutex_t mutex;
	
	// Pointer to first node of the list
	ListNode *head;
	
	// Pointer to the last node of the list
	ListNode *end;
	
	// Function pointer for destructing objects stored in nodes
	void (*destructPtr)(void *object);
	
	// This flag can be used to disabled destruction of objects entirely
	bool dontDestructObjects;
	
	// This flag can be used to disabled synchronisation of objects entirely
	bool notThreadSafe;
	
	// Size of list
	size_t size;
};

/*
 * This struct models the list node, which contains the pointer to the object
 * itself as well as to the next node.
 */
struct ListNode {
	
	// Object we are pointing to
	void *object;
	
	// Next node
	ListNode *next;
};

/*
 * This constructs a LinkedList object.
 */
LinkedList *construct_LinkedList(void (*destructPtr)(void *object),
		bool dontDestructObjects, bool notThreadSafe)
{
	// Allocate list
	LinkedList *ll = malloc(sizeof(LinkedList));
	if (!ll) {
		fprintf(stderr, "PhilPSX: LinkedList: Couldn't allocate memory for "
				"LinkedList struct\n");
		goto end;
	}
	
	// Wipe and initialise mutex
	if (memset(&ll->mutex, 0, sizeof(pthread_mutex_t)) != &ll->mutex) {
		fprintf(stderr, "PhilPSX: LinkedList: Couldn't wipe memory for "
				"the pthread_mutex_t object\n");
		goto cleanup_linkedlist;
	}
	ll->notThreadSafe = notThreadSafe;
	if (!notThreadSafe) {
		if (pthread_mutex_init(&ll->mutex, NULL) != 0) {
			fprintf(stderr, "PhilPSX: LinkedList: Couldn't initialise the "
					"pthread_mutex_t object\n");
			goto cleanup_linkedlist;
		}
	}
	
	// Set head and end to NULL as we are currently empty
	ll->head = NULL;
	ll->end = NULL;
	
	// Store object destruct function pointer
	ll->destructPtr = destructPtr;
	
	// Store dont destruct flag
	ll->dontDestructObjects = dontDestructObjects;
	
	// Set size to zero
	ll->size = 0;
	
	// Normal return:
	return ll;
	
	// Cleanup path:
	cleanup_linkedlist:
	free(ll);
	ll = NULL;
	
	end:
	return ll;
}
/*
 * This destructs a LinkedList object. It will also destruct any objects
 * referenced by the contained nodes (if enabled) using the function pointer
 * supplied at construction time, or by calling free if this was NULL.
 */
void destruct_LinkedList(LinkedList *ll)
{
	// Get pointer to head
	ListNode *current = ll->head;

	// Walk list, cleaning up as we go
	while (current) {
		ListNode *toDestruct = current;
		current = toDestruct->next;

		// Destruct this node if enabled
		if (!ll->dontDestructObjects) {
			if (toDestruct->object) {

				// Destruct with custom function pointer if present,
				// else just free
				if (ll->destructPtr)
					ll->destructPtr(toDestruct->object);
				else
					free(toDestruct->object);
			}
		}
		destruct_ListNode(toDestruct);
	}
	
	// Destroy mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_destroy(&ll->mutex);

	// Free LinkedList struct itself
	free(ll);
}

/*
 * This function returns the current size of the linked list.
 */
size_t LinkedList_getSize(LinkedList *ll)
{
	// Declare return value
	size_t size = 0;
	
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
	// Store value
	size = ll->size;
	
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);
	
	return size;
}

/*
 * This function adds the object to the end of the linked list.
 */
void *LinkedList_addObject(LinkedList *ll, void *object)
{
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
	// If end pointer is not NULL then go straight there, else walk the list
	// with a double pointer - end should only be NULL when we have no elements
	// anyway, so the while loop never executes in theory
	ListNode **listWalker;
	if (ll->end) {
		// Set double pointer to address of last 'next' pointer in the list
		listWalker = &ll->end->next;
	}
	else {
		// Set double pointer to head
		listWalker = &ll->head;

		// Walk list until we get to end
		while (*listWalker)
			listWalker = &(*listWalker)->next;
	}

	// Add new item to end of list
	*listWalker = construct_ListNode();
	if (!*listWalker) {
		fprintf(stderr, "PhilPSX: LinkedList: Couldn't allocate memory for "
				"ListNode struct...\n");
		object = NULL;
		goto end;
	}

	// Set object and increment size
	(*listWalker)->object = object;
	++ll->size;
	
	// Set end pointer
	ll->end = *listWalker;
	
	end:
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);

	return object;
}

/*
 * This function adds the object to the specified index in the linked list.
 */
void *LinkedList_addObjectAt(LinkedList *ll, size_t index, void *object)
{
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
	// Check size of list (index == size will mean we are targetting last
	// position in the list)
	if (index <= ll->size) {
		
		// Get double pointer to head
		ListNode **toHead = &ll->head;

		// Walk list until we get to correct position
		for (int i = 0; i < index; ++i)
			toHead = &(*toHead)->next;

		// Add new item to this index in the list - store separately until
		// we know construct function has succeeded, thus preventing change of
		// the list until the last moment
		ListNode *current = *toHead;
		ListNode *new = construct_ListNode();
		if (!new) {
			fprintf(stderr, "PhilPSX: LinkedList: Couldn't allocate memory for "
					"ListNode struct...\n");
			object = NULL;
			goto end;
		}
		
		// Set pointers properly
		*toHead = new;
		new->next = current;

		// Set object and increment size
		new->object = object;
		++ll->size;
		
		// Set end pointer if needed
		if (!new->next)
			ll->end = new;
	}
	else {
		fprintf(stderr, "PhilPSX: LinkedList: Invalid index specified to "
				"LinkedList_addObjectAt, action not performed\n");
	}
	
	end:
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);

	return object;
}

/*
 * This function removes an object from the linked list and also destructs it
 * (if enabled) using either the list's deletion function pointer, or by just
 * calling free.
 */
void LinkedList_removeObject(LinkedList *ll, size_t index)
{
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
    // Check index and continue if valid
    if (index < ll->size)
    {
        // Get double pointer to head
        ListNode **toHead = &ll->head;

        // Move head forward to right point
        for (size_t i = 0; i < index; ++i)
            toHead = &(*toHead)->next;

        // Destruct object (if enabled)
        ListNode *toDestruct = *toHead;
        *toHead = (*toHead)->next;
		if (!ll->dontDestructObjects) {
			if (ll->destructPtr)
				ll->destructPtr(toDestruct->object);
			else
				free(toDestruct->object);
		}
		
		// Reset end pointer (if we removed the last node) and
		// also destruct the node
		if (!*toHead) {
			if (ll->head)
				// In this case toHead points to a next pointer inside the
				// last struct in the list, so we can calculate the struct
				// address with offsetof
				ll->end = (ListNode *)((uintptr_t)toHead -
						offsetof(ListNode, next));
			else
				// In this case toHead points to the head pointer in the
				// LinkedList struct itself, so offset calculation above would
				// produce garbage - list is empty so set end to NULL
				ll->end = NULL;				
		}
		destruct_ListNode(toDestruct);

        // Decrement size
        --ll->size;
    }
	else {
		fprintf(stderr, "PhilPSX: LinkedList: Invalid index specified to "
				"LinkedList_removeObject, action not performed\n");
	}
	
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);
}

/*
 * This function replaces an object in the linked list.
 */
void LinkedList_replaceObject(LinkedList *ll, size_t index, void *object)
{
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
    // Check index and continue if valid
    if (index < ll->size)
    {
		if (index == ll->size - 1) {
			// Use end pointer
			ll->end->object = object;
		}
		else {
			// Iterate from the start
			ListNode *nodePtr = ll->head;
			for (size_t i = 0; i < index; ++i)
				nodePtr = nodePtr->next;
			
			nodePtr->object = object;
		}
    }
	else {
		fprintf(stderr, "PhilPSX: LinkedList: Invalid index specified to "
				"LinkedList_replaceObject, action not performed\n");
	}
	
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);
}

/*
 * This function returns an object from the list at the specified index.
 */
void *LinkedList_getObject(LinkedList *ll, size_t index)
{
	// Define return value
	void *object = NULL;
	
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);
	
	// Check index and continue if valid
	if (index < ll->size)
	{
		// Check if end item is being requested and shortcut the list walk if so
		if (index == ll->size - 1) {
			object = ll->end->object;
		}
		else {
			// Get double pointer to head
			ListNode **toHead = &ll->head;
			for (size_t i = 0; i < index; ++i)
				toHead = &(*toHead)->next;

			// Set return value
			object = (*toHead)->object;
		}
	}
	else
	{
		fprintf(stderr, "PhilPSX: LinkedList: Invalid index specified to "
				"LinkedList_getObject, NULL returned\n");
	}
	
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);

	// return
	return object;
}

/*
 * This destructs a LinkedList object. It will also destruct any objects
 * referenced by the contained nodes (if enabled) using the function pointer
 * supplied at construction time, or by calling free if this was NULL.
 */
void LinkedList_wipeAllObjects(LinkedList *ll)
{
	// Lock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_lock(&ll->mutex);

	// Get pointer to head
	ListNode *current = ll->head;

	// Walk list, cleaning up as we go
	while (current) {
		ListNode *toDestruct = current;
		current = toDestruct->next;

		// Destruct this node if enabled
		if (!ll->dontDestructObjects) {
			if (toDestruct->object) {

				// Destruct with custom function pointer if present,
				// else just free
				if (ll->destructPtr)
					ll->destructPtr(toDestruct->object);
				else
					free(toDestruct->object);
			}
		}
		destruct_ListNode(toDestruct);
	}
	
	// Set head and end back to NULL
	ll->head = NULL;
	ll->end = NULL;
	
	// Set size to zero
	ll->size = 0;
	
	// Unlock mutex (if enabled)
	if (!ll->notThreadSafe)
		pthread_mutex_unlock(&ll->mutex);
}

/*
 * This clones a LinkedList into another LinkedList, replicating its properties
 * such as thread safety and destructor choice. It does NOT clone the objects
 * referenced by each ListNode, but merely copies their pointers. For this
 * reason, it is best to use this with dontDestructObjects enabled to avoid
 * double destruct operations/free operations.
 */
LinkedList *LinkedList_clone(LinkedList *original)
{
	// Lock source mutex (if enabled), and store original notThreadSafe value
	bool notThreadSafe = original->notThreadSafe;
	if (!original->notThreadSafe)
		pthread_mutex_lock(&original->mutex);
	
	// Set original temporarily to not thread safe so following calls do not
	// try and lock the mutex and lock the thread forever (if thread safety is
	// on)
	original->notThreadSafe = true;
	
	// Allocate return value
	LinkedList *newList = construct_LinkedList(original->destructPtr,
			original->dontDestructObjects, notThreadSafe);
	if (!newList) { 
		fprintf(stderr, "PhilPSX: LinkedList: Couldn't construct new LinkedList"
				" object in LinkedList_clone\n");
		goto end;
	}
	
	// Iterate through original list and copy each object ptr to a new ListNode
	// object in the new LinkedList object
	size_t originalSize = original->size;
	void *result;
	for (size_t i = 0; i < originalSize; ++i) {
		result =
			LinkedList_addObject(newList, LinkedList_getObject(original, i));
		if (!result) {
			fprintf(stderr, "PhilPSX: LinkedList: Copy routine failed in "
					"LinkedList_clone\n");
			goto cleanup_linkedlist;
		}
	}
	
	// Normal return:
	// Unlock source mutex (if enabled)
	original->notThreadSafe = notThreadSafe;
	if (!original->notThreadSafe)
		pthread_mutex_unlock(&original->mutex);
	
	return newList;
	
	// Cleanup path:
	cleanup_linkedlist:
	destruct_LinkedList(newList);
	newList = NULL;
	
	end:
	// Unlock source mutex (if enabled)
	original->notThreadSafe = notThreadSafe;
	if (!original->notThreadSafe)
		pthread_mutex_unlock(&original->mutex);
	
	return newList;
}

/*
 * This constructs a ListNode object.
 */
static ListNode *construct_ListNode(void)
{
	// Allocate node
	ListNode *ln = malloc(sizeof(ListNode));
	if (!ln) {
		fprintf(stderr, "PhilPSX: LinkedList: ListNode: Couldn't allocate "
				"memory for ListNode struct\n");
		goto end;
	}
	
	// Set properties
	ln->object = NULL;
	ln->next = NULL;
	
	end:
	return ln;
}

/*
 * This destructs a ListNode object.
 */
static void destruct_ListNode(ListNode *ln)
{
	free(ln);
}