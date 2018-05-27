/*
 * This C file models the MIPS R3051 bus interface unit of the PlayStation as
 * a class.
 * 
 * BusInterfaceUnit.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/BusInterfaceUnit.h"
#include "../headers/R3051.h"
#include "../headers/Components.h"

/*
 * The R3051 bus interface unit struct models the state of an bus interface
 * unit. The BIU's purpose is for interfacing the CPU with main memory and
 * other components via a read and write interface. It can also be taken
 * control of by other components for DMA read/write requests.
 */
struct BusInterfaceUnit {

	// CPU reference
	R3051 *cpu;

	// Current holder of BIU
	int32_t holder;

	// currently processing transaction
	bool transactionInProgress;
};

/*
 * This constructs and returns a new BusInterfaceUnit object.
 */
BusInterfaceUnit *construct_BusInterfaceUnit(R3051 *cpu)
{
	// Allocate BusInterfaceUnit struct
	BusInterfaceUnit *biu = malloc(sizeof(BusInterfaceUnit));
	if (!biu) {
		fprintf(stderr, "PhilPSX: BusInterfaceUnit: Couldn't allocate memory "
				"for BusInterfaceUnit struct\n");
		goto end;
	}
	
	// Set CPU reference
	biu->cpu = cpu;

	// Set current bus interface unit holder
	biu->holder = PHILPSX_COMPONENTS_CPU;

	// Set transaction status
	biu->transactionInProgress = false;
	
	end:
	return biu;
}

/*
 * This destructs a BusInterfaceUnit object.
 */
void destruct_BusInterfaceUnit(BusInterfaceUnit *biu)
{
	free(biu);
}

/*
 * This function sets the current holder of the bus interface unit.
 */
void BusInterfaceUnit_setHolder(BusInterfaceUnit *biu, int32_t holder)
{
	biu->holder = holder;
}

/*
 * This function gets the current holder of the bus interface unit.
 */
int32_t BusInterfaceUnit_getHolder(BusInterfaceUnit *biu)
{
	return biu->holder;
}

/*
 * This function marks a transaction as in progress.
 */
void BusInterfaceUnit_startTransaction(BusInterfaceUnit *biu)
{
	biu->transactionInProgress = true;
}

/*
 * This function ends a transaction.
 */
void BusInterfaceUnit_stopTransaction(BusInterfaceUnit *biu)
{
	biu->transactionInProgress = false;
}

/*
 * This function tells us whether a transaction is ongoing.
 */
bool BusInterfaceUnit_isTransactionInProgress(BusInterfaceUnit *biu)
{
	return biu->transactionInProgress;
}