/*
 * This header file provides the public API for a work queue, which allows
 * submission of items from the emulator thread in a thread-safe manner, which
 * can then be executed by the rendering thread. 
 * 
 * WorkQueue.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_WORKQUEUE_HEADER
#define PHILPSX_WORKQUEUE_HEADER

// System includes
#include <stdbool.h>

// Typedefs
typedef struct WorkQueue WorkQueue;

// Includes
#include "GpuCommand.h"

// Public functions
WorkQueue *construct_WorkQueue(void);
void destruct_WorkQueue(WorkQueue *wq);
GpuCommand *WorkQueue_waitForItem(WorkQueue *wq);
void WorkQueue_returnItem(WorkQueue *wq, GpuCommand *object);
void WorkQueue_addItem(WorkQueue *wq, GpuCommand *source,
		bool waitForCompletion);
void WorkQueue_endProcessingByRenderingThread(WorkQueue *wq);

#endif