/*
 * This header file provides the public API for the SPU (sound chip) of the
 * PlayStation. At the moment, most functionality is stubbed out/not implemented.
 * 
 * SPU.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_SPU_HEADER
#define PHILPSX_SPU_HEADER

// System includes
#include <stdint.h>

// Typedefs
typedef struct SPU SPU;

// Includes
#include "SystemInterlink.h"

// Public functions
SPU *construct_SPU(void);
void destruct_SPU(SPU *spu);
void SPU_writeByte(SPU *spu, int32_t address, int8_t value);
int8_t SPU_readByte(SPU *spu, int32_t address);
void SPU_setMemoryInterface(SPU *spu, SystemInterlink *smi);

#endif