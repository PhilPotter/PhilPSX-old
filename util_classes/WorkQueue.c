/*
 * This C file models a work queue implementation. GpuCommand objects are
 * stored and issued from an internal array in a thread-safe manner, for
 * submission by the emulator thread and execution by the rendering thread.
 * Doing it this way means no dynamic allocations after queue creation, but
 * could lead to lower performance - this is the initial design.
 * 
 * It is assumed due to use case that non-init pthread functions will not fail,
 * and therefore errors are not checked with these function calls, to simplify
 * the code and make it more readable.
 * 
 * WorkQueue.c - Copyright Phillip Potter, 2020
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "../headers/WorkQueue.h"
#include "../headers/GpuCommand.h"

// Queue size
#define PHILPSX_WORKQUEUE_SIZE 2048

/*
 * This struct contains a locking primitive that lets us decide when the
 * contained GPU command is in use.
 */
typedef struct GpuCommandContainer {

	// Synchronisation primitives and related variables
	pthread_mutex_t useLock;
	bool inUseByRenderingThread;
	bool completed;
	
	// Contained object
	GpuCommand command;
} GpuCommandContainer;

/*
 * This struct models the structure of the queue, and includes synchronisation
 * primitives.
 */
struct WorkQueue {
	
	// Backing store
	GpuCommandContainer backingStore[PHILPSX_WORKQUEUE_SIZE];

	// Synchronisation primitives and related variables
	pthread_mutex_t queueLock;
	pthread_cond_t waitForWorkCond;
	size_t retrievalMarker;
	size_t submissionMarker;
	bool endProcessingByRenderingThread;
};

/*
 * This constructs a WorkQueue object.
 */
WorkQueue *construct_WorkQueue(void)
{
	// Allocate memory for struct (import to use calloc here as inUse variables
	// in GpuCommandContainer array need to be zeroed)
	WorkQueue *wq = calloc(1, sizeof(WorkQueue));
	if (!wq) {
		fprintf(stderr, "PhilPSX: WorkQueue: Couldn't allocate memory for "
				"WorkQueue struct\n");
		goto end;
	}
	
	// Initialise mutex
	if (pthread_mutex_init(&wq->queueLock, NULL)) {
		fprintf(stderr, "PhilPSX: WorkQueue: Couldn't initialise queueLock "
				"mutex\n");
		goto cleanup_workqueue;
	}
	
	// Initialise condition variable
	if (pthread_cond_init(&wq->waitForWorkCond, NULL)) {
		fprintf(stderr, "PhilPSX: WorkQueue: Couldn't initialise "
				"waitForWorkCond condition variable\n");
		goto cleanup_mutex;
	}
	
	// Setup mutexes in GpuCommandContainer array
	for (size_t i = 0; i < PHILPSX_WORKQUEUE_SIZE; ++i) {
		if (pthread_mutex_init(&wq->backingStore[i].useLock, NULL)) {
			fprintf(stderr, "PhilPSX: WorkQueue: Couldn't initialise mutxes "
					"inside the backing store objects\n");
			for (size_t j = 0; j < i; ++j)
				pthread_mutex_destroy(&wq->backingStore[j].useLock);
			goto cleanup_mutex;
		}
	}
	
	// Set markers
	wq->retrievalMarker = 0;
	wq->submissionMarker = 0;
	
	// Normal path:
	return wq;

	// Cleanup path:
	cleanup_mutex:
	pthread_mutex_destroy(&wq->queueLock);

	cleanup_workqueue:
	free(wq);
	wq = NULL;

	end:
	return wq;
}

/*
 * This destructs a WorkQueue object.
 */
void destruct_WorkQueue(WorkQueue *wq)
{
	// Cleanup resources
	for (size_t i = 0; i < PHILPSX_WORKQUEUE_SIZE; ++i)
		pthread_mutex_destroy(&wq->backingStore[i].useLock);
	pthread_cond_destroy(&wq->waitForWorkCond);
	pthread_mutex_destroy(&wq->queueLock);
	free(wq);
}

/*
 * This returns a pointer to a GpuCommand object ready to be executed. It
 * should only be called from the rendering thread.
 */
GpuCommand *WorkQueue_waitForItem(WorkQueue *wq)
{
	// First lock the queue mutex
	pthread_mutex_lock(&wq->queueLock);

	// Now we check the next item can be fetched
	while (wq->retrievalMarker >= wq->submissionMarker) {
		pthread_cond_wait(&wq->waitForWorkCond, &wq->queueLock);
		if (wq->endProcessingByRenderingThread)
			goto end;
	}
	
	// At this point, there is work for us in the queue, fetch it
	GpuCommandContainer *container = &wq->backingStore[wq->retrievalMarker++];

	// Lock the container (this remains locked until the associated GpuCommand
	// is returned) and set as in us
	pthread_mutex_lock(&container->useLock);
	container->inUseByRenderingThread = true;
	
	end:
	// Unlock the queue mutex
	pthread_mutex_unlock(&wq->queueLock);
	
	// Return the object to the caller
	return wq->endProcessingByRenderingThread ? NULL : &container->command;
}

/*
 * This lets us return a GpuCommand object so that its associated container
 * can be unlocked and set as no longer in use. This should only be used
 * from the rendering thread, with objects on which the lock is already held
 * by the rendering thread.
 */
void WorkQueue_returnItem(WorkQueue *wq, GpuCommand *object)
{
	// Return immediately if object is NULL
	if (!object)
		return;
	
	// Calculate container object pointer using offset
	size_t offset = offsetof(GpuCommandContainer, command);
	GpuCommandContainer *container =
			(GpuCommandContainer *)((uint8_t *)object - offset);
	
	// Set as unused and completed, and release lock
	container->inUseByRenderingThread = false;
	container->completed = true;
	pthread_mutex_unlock(&container->useLock);
}

/*
 * This copies the object referenced by source to the next available slot in
 * the backing store and makes it available for processing. This should only
 * be called from the emulator thread.
 */
void WorkQueue_addItem(WorkQueue *wq, GpuCommand *source,
		bool waitForCompletion)
{
	// First lock the queue mutex
	recheck_end_of_queue:
	pthread_mutex_lock(&wq->queueLock);
	
	// Check the submission marker isn't at the end of the list
	if (wq->submissionMarker == PHILPSX_WORKQUEUE_SIZE) {
		// We are at the end of the list, check rendering thread has caught up
		if (wq->retrievalMarker != wq->submissionMarker) {
			// Release the mutex and wakeup rendering thread
			pthread_mutex_unlock(&wq->queueLock);
			pthread_cond_broadcast(&wq->waitForWorkCond);

			goto recheck_end_of_queue;
		}

		// Set both markers to beginning of backing store
		wq->retrievalMarker = wq->submissionMarker = 0;
	}
	
	// Obtain lock on current container
	GpuCommandContainer *container = &wq->backingStore[wq->submissionMarker++];
	pthread_mutex_lock(&container->useLock);
	
	// Copy GpuCommand object from source to destination
	memcpy(&container->command, source, sizeof(GpuCommand));
	
	// Set completion status to false
	container->completed = false;
	
	// Release lock on current container
	pthread_mutex_unlock(&container->useLock);
	
	// Unlock the queue mutex and wake rendering thread (if needed)
	pthread_mutex_unlock(&wq->queueLock);
	pthread_cond_broadcast(&wq->waitForWorkCond);
	
	// If it is required to wait for the completion of this GpuCommand object,
	// do so here
	while (waitForCompletion) {
		begin_check:
		pthread_mutex_lock(&container->useLock);
		
		// Check if object has been processed yet
		if (!container->completed) {
			// Object not processed yet, unlock mutex and try again
			pthread_mutex_unlock(&container->useLock);
			goto begin_check;
		} else {
			// Object processed, unlock mutex and end loop
			pthread_mutex_unlock(&container->useLock);
			break;
		}
	}
}

/*
 * This marks the work queue to say no further processing should be done,
 * and wakes the rendering thread, allowing the program to end cleanly. This
 * should only be called from the emulator thread.
 */
void WorkQueue_endProcessingByRenderingThread(WorkQueue *wq)
{
	// First lock the queue mutex
	pthread_mutex_lock(&wq->queueLock);
	
	// Set work queue mode
	wq->endProcessingByRenderingThread = true;
	
	// Unlock the queue mutex
	pthread_mutex_unlock(&wq->queueLock);
	
	// Wake rendering thread (if needed)
	pthread_cond_broadcast(&wq->waitForWorkCond);
}
