/*
 * This header file provides the public API for the controllers and memory cards
 * of the PlayStation. At the moment, most functionality is stubbed out/not
 * implemented.
 * 
 * ControllerIO.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_CONTROLLERIO_HEADER
#define PHILPSX_CONTROLLERIO_HEADER

// System includes
#include <stdint.h>

// Typedefs
typedef struct ControllerIO ControllerIO;

// Includes
#include "SystemInterlink.h"

// Public functions
ControllerIO *construct_ControllerIO(void);
void destruct_ControllerIO(ControllerIO *cio);
void ControllerIO_appendSyncCycles(ControllerIO *cio, int32_t cycles);
int8_t ControllerIO_readByte(ControllerIO *cio, int32_t address);
void ControllerIO_writeByte(ControllerIO *cio, int32_t address, int8_t value);
void ControllerIO_setMemoryInterface(ControllerIO *cio, SystemInterlink *smi);

#endif