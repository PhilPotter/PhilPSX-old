/*
 * This header file provides the public API for the MIPS R3051 bus interface
 * unit of the PlayStation.
 * 
 * BusInterfaceUnit.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_BUS_INTERFACE_UNIT_HEADER
#define PHILPSX_BUS_INTERFACE_UNIT_HEADER

// System includes
#include <stdint.h>

// Typedefs
typedef struct BusInterfaceUnit BusInterfaceUnit;

// Includes
#include "R3051.h"

// Public functions
BusInterfaceUnit *construct_BusInterfaceUnit(R3051 *cpu);
void destruct_BusInterfaceUnit(BusInterfaceUnit *biu);
void BusInterfaceUnit_setHolder(BusInterfaceUnit *biu, int32_t holder);
int32_t BusInterfaceUnit_getHolder(BusInterfaceUnit *biu);
void BusInterfaceUnit_startTransaction(BusInterfaceUnit *biu);
void BusInterfaceUnit_stopTransaction(BusInterfaceUnit *biu);
bool BusInterfaceUnit_isTransactionInProgress(BusInterfaceUnit *biu);

#endif