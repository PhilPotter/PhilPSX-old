/*
 * This C file models the MIPS R3051 processor of the PlayStation as a class.
 * 
 * R3051.c - Copyright Phillip Potter, 2018
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/R3051.h"
#include "../headers/Components.h"
#include "../headers/Cop0.h"
#include "../headers/Cop2.h"
#include "../headers/BusInterfaceUnit.h"
#include "../headers/SystemInterlink.h"
#include "../headers/math_utils.h"

// Data widths for read and write functions
#define PHILPSX_R3051_BYTE 8
#define PHILPSX_R3051_HALFWORD 16
#define PHILPSX_R3051_WORD 32

// List of exception reasons
#define PHILPSX_EXCEPTION_INT 0
#define PHILPSX_EXCEPTION_ADEL 4
#define PHILPSX_EXCEPTION_ADES 5
#define PHILPSX_EXCEPTION_IBE 6
#define PHILPSX_EXCEPTION_DBE 7
#define PHILPSX_EXCEPTION_SYS 8
#define PHILPSX_EXCEPTION_BP 9
#define PHILPSX_EXCEPTION_RI 10
#define PHILPSX_EXCEPTION_CPU 11
#define PHILPSX_EXCEPTION_OVF 12
#define PHILPSX_EXCEPTION_RESET 13
#define PHILPSX_EXCEPTION_NULL 14

// Cache types
#define PHILPSX_INSTRUCTION_CACHE 0

// Forward declarations for functions and subcomponents private to this class
// R3051-related stuff:
static void R3051_ADD(R3051 *cpu, int32_t instruction);
static void R3051_ADDI(R3051 *cpu, int32_t instruction);
static void R3051_ADDIU(R3051 *cpu, int32_t instruction);
static void R3051_ADDU(R3051 *cpu, int32_t instruction);
static void R3051_AND(R3051 *cpu, int32_t instruction);
static void R3051_ANDI(R3051 *cpu, int32_t instruction);
static void R3051_BC2F(R3051 *cpu, int32_t instruction);
static void R3051_BC2T(R3051 *cpu, int32_t instruction);
static void R3051_BEQ(R3051 *cpu, int32_t instruction);
static void R3051_BGEZ(R3051 *cpu, int32_t instruction);
static void R3051_BGEZAL(R3051 *cpu, int32_t instruction);
static void R3051_BGTZ(R3051 *cpu, int32_t instruction);
static void R3051_BLEZ(R3051 *cpu, int32_t instruction);
static void R3051_BLTZ(R3051 *cpu, int32_t instruction);
static void R3051_BLTZAL(R3051 *cpu, int32_t instruction);
static void R3051_BNE(R3051 *cpu, int32_t instruction);
static void R3051_BREAK(R3051 *cpu, int32_t instruction);
static void R3051_CF2(R3051 *cpu, int32_t instruction);
static void R3051_CT2(R3051 *cpu, int32_t instruction);
static void R3051_DIV(R3051 *cpu, int32_t instruction);
static void R3051_DIVU(R3051 *cpu, int32_t instruction);
static void R3051_J(R3051 *cpu, int32_t instruction);
static void R3051_JAL(R3051 *cpu, int32_t instruction);
static void R3051_JALR(R3051 *cpu, int32_t instruction);
static void R3051_JR(R3051 *cpu, int32_t instruction);
static void R3051_LB(R3051 *cpu, int32_t instruction);
static void R3051_LBU(R3051 *cpu, int32_t instruction);
static void R3051_LH(R3051 *cpu, int32_t instruction);
static void R3051_LHU(R3051 *cpu, int32_t instruction);
static void R3051_LUI(R3051 *cpu, int32_t instruction);
static void R3051_LW(R3051 *cpu, int32_t instruction);
static void R3051_LWC2(R3051 *cpu, int32_t instruction);
static void R3051_LWL(R3051 *cpu, int32_t instruction);
static void R3051_LWR(R3051 *cpu, int32_t instruction);
static void R3051_MF0(R3051 *cpu, int32_t instruction);
static void R3051_MF2(R3051 *cpu, int32_t instruction);
static void R3051_MFHI(R3051 *cpu, int32_t instruction);
static void R3051_MFLO(R3051 *cpu, int32_t instruction);
static void R3051_MT0(R3051 *cpu, int32_t instruction);
static void R3051_MT2(R3051 *cpu, int32_t instruction);
static void R3051_MTHI(R3051 *cpu, int32_t instruction);
static void R3051_MTLO(R3051 *cpu, int32_t instruction);
static void R3051_MULT(R3051 *cpu, int32_t instruction);
static void R3051_MULTU(R3051 *cpu, int32_t instruction);
static void R3051_NOR(R3051 *cpu, int32_t instruction);
static void R3051_OR(R3051 *cpu, int32_t instruction);
static void R3051_ORI(R3051 *cpu, int32_t instruction);
static void R3051_RFE(R3051 *cpu, int32_t instruction);
static void R3051_SB(R3051 *cpu, int32_t instruction);
static void R3051_SH(R3051 *cpu, int32_t instruction);
static void R3051_SLL(R3051 *cpu, int32_t instruction);
static void R3051_SLLV(R3051 *cpu, int32_t instruction);
static void R3051_SLT(R3051 *cpu, int32_t instruction);
static void R3051_SLTI(R3051 *cpu, int32_t instruction);
static void R3051_SLTIU(R3051 *cpu, int32_t instruction);
static void R3051_SLTU(R3051 *cpu, int32_t instruction);
static void R3051_SRA(R3051 *cpu, int32_t instruction);
static void R3051_SRAV(R3051 *cpu, int32_t instruction);
static void R3051_SRL(R3051 *cpu, int32_t instruction);
static void R3051_SRLV(R3051 *cpu, int32_t instruction);
static void R3051_SUB(R3051 *cpu, int32_t instruction);
static void R3051_SUBU(R3051 *cpu, int32_t instruction);
static void R3051_SW(R3051 *cpu, int32_t instruction);
static void R3051_SWC2(R3051 *cpu, int32_t instruction);
static void R3051_SWL(R3051 *cpu, int32_t instruction);
static void R3051_SWR(R3051 *cpu, int32_t instruction);
static void R3051_SYSCALL(R3051 *cpu, int32_t instruction);
static void R3051_XOR(R3051 *cpu, int32_t instruction);
static void R3051_XORI(R3051 *cpu, int32_t instruction);
static void R3051_executeOpcode(R3051 *cpu, int32_t instruction,
		int32_t tempBranchAddress);
static bool R3051_handleException(R3051 *cpu);
static bool R3051_handleInterrupts(R3051 *cpu);

// MIPSException-related stuff:
typedef struct MIPSException MIPSException;
static MIPSException *construct_MIPSException(void);
static void destruct_MIPSException(MIPSException *exception);
static void MIPSException_reset(MIPSException *exception);

// Cache-related stuff:
typedef struct Cache Cache;
static Cache *construct_Cache(int32_t cacheType);
static void destruct_Cache(Cache *cache);
static bool Cache_checkForHit(Cache *cache, int32_t address);
static int32_t Cache_readWord(Cache *cache, int32_t address);
static int8_t Cache_readByte(Cache *cache, int32_t address);
static void Cache_writeWord(Cache *cache, Cop0 *sccp, int32_t address,
		int32_t value);
static void Cache_writeByte(Cache *cache, Cop0 *sccp, int32_t address,
		int8_t value);
static void Cache_refillLine(Cache *cache, Cop0 *sccp, SystemInterlink *system,
		int32_t address);

/*
 * This struct contains registers, and pointers to subcomponents.
 */
struct R3051 {
	
	// Component ID
	int32_t componentId;

	// Register definitions
	int32_t *generalRegisters;
	int32_t programCounter;
	int32_t hiReg;
	int32_t loReg;

	// Jump address holder and boolean
	int32_t jumpAddress;
	bool jumpPending;

	// Co-processor definitions
	Cop0 *sccp;
	Cop2 *gte;

	// Bus Interface Unit definition
	BusInterfaceUnit *biu;

	// System link
	SystemInterlink *system;

	// This stores the current exception
	MIPSException *exception;

	// This stores the instruction cache
	Cache *instructionCache;

	// This tells us if the last instruction was a branch/jump instruction
	bool prevWasBranch;
	bool isBranch;

	// This counts the cycles of the current instruction
	int32_t cycles;
};

/*
 * This inner struct models a processor exception, which can occur during
 * any stage of the pipeline.
 */
struct MIPSException {
	
	// Exception variables
	int32_t exceptionReason;
	int32_t programCounterOrigin;
	int32_t badAddress;
	int32_t coProcessorNum;
	bool isInBranchDelaySlot;
};

/*
 * This inner struct allows the R3051 to use instruction caching.
 */
struct Cache {
	
	// Cache variables
	int8_t *cacheData;
	int32_t *cacheTag;
	bool *cacheValid;
	int32_t cacheType;
};

/*
 * This constructs a new R3051 object.
 */
R3051 *construct_R3051(void)
{
	// Allocate R3051 struct
	R3051 *cpu = malloc(sizeof(R3051));
	if (!cpu) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for R3051 "
				"struct\n");
		goto end;
	}
	
	// Set SystemInterlink reference to NULL
	cpu->system = NULL;

	// Set component ID
	cpu->componentId = PHILPSX_COMPONENTS_CPU;

	// Setup instruction cycle count
	cpu->cycles = 0;

	// Setup registers
	cpu->generalRegisters = calloc(32, sizeof(int32_t));
	if (!cpu->generalRegisters) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't allocate memory for general "
				"registers");
		goto cleanup_r3051;
	}

	cpu->generalRegisters[0] = 0;	// r1 is fixed at 0
									// (already 0, here for clarity)
	cpu->programCounter = 0;
	cpu->hiReg = 0;
	cpu->loReg = 0;

	// Setup jump variables
	cpu->jumpAddress = 0;
	cpu->jumpPending = false;

	// Setup co-processors
	cpu->sccp = construct_Cop0();
	if (!cpu->sccp) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct Cop0");
		goto cleanup_registers;
	}
	cpu->gte = construct_Cop2();
	if (!cpu->gte) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct Cop2");
		goto cleanup_cop0;
	}

	// Setup bus interface unit
	cpu->biu = construct_BusInterfaceUnit(cpu);
	if (!cpu->biu) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct BusInterfaceUnit");
		goto cleanup_cop2;
	}

	// Setup empty exception
	cpu->exception = construct_MIPSException();
	if (!cpu->exception) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct MIPSException");
		goto cleanup_biu;
	}

	// Setup instruction cache
	cpu->instructionCache = construct_Cache(PHILPSX_INSTRUCTION_CACHE);
	if (!cpu->instructionCache) {
		fprintf(stderr, "PhilPSX: R3051: Couldn't construct instruction cache");
		goto cleanup_exception;
	}

	// Setup the branch marker
	cpu->prevWasBranch = false;
	cpu->isBranch = false;

	// Reset main chip and Cop0
	R3051_reset(cpu);
	Cop0_reset(cpu->sccp);

	// Normal return:
	return cpu;

	// Cleanup path:
	cleanup_exception:
	destruct_MIPSException(cpu->exception);
	
	cleanup_biu:
	destruct_BusInterfaceUnit(cpu->biu);

	cleanup_cop2:
	destruct_Cop2(cpu->gte);

	cleanup_cop0:
	destruct_Cop0(cpu->sccp);

	cleanup_registers:
	free(cpu->generalRegisters);

	cleanup_r3051:
	free(cpu);
	cpu = NULL;

	end:
	return cpu;
}

/*
 * This destructs an R3051 object.
 */
void destruct_R3051(R3051 *cpu)
{
	destruct_Cache(cpu->instructionCache);
	destruct_MIPSException(cpu->exception);
	destruct_BusInterfaceUnit(cpu->biu);
	destruct_Cop2(cpu->gte);
	destruct_Cop0(cpu->sccp);
	free(cpu->generalRegisters);
	free(cpu);
}

/*
 * This function moves the whole processor on by one block of instructions.
 */
void R3051_executeInstructions(R3051 *cpu)
{
	// Enter loop
	do {
		// Setup cycle count and instruction
		cpu->cycles = 0;
		int32_t instruction = 0;

		// Check address is OK, throwing exception if not
		int32_t tempAddress =
				(int32_t)((cpu->programCounter & 0xFFFFFFFFL) - 4L);

		// Perform read of instruction
		int64_t tempInstruction = R3051_readInstructionWord(
				cpu,
				cpu->programCounter,
				tempAddress
				);
		if (tempInstruction == -1L) {
			cpu->cycles += 1;
			SystemInterlink_appendSyncCycles(cpu->system, cpu->cycles);
			continue;
		}

		// Cast long to int
		instruction = (int32_t)tempInstruction;

		// We now have instruction value. Swap bytes if we are in
		// little endian mode
		instruction = R3051_swapWordEndianness(cpu, instruction);

		// Execute
		R3051_executeOpcode(cpu, instruction, tempAddress);

		// Handle exception if there was one
		if (R3051_handleException(cpu)) {
			cpu->cycles += 1;
			SystemInterlink_appendSyncCycles(cpu->system, cpu->cycles);
			continue;
		}

		// Handle interrupt if there was one
		if (cpu->isBranch && R3051_handleInterrupts(cpu)) {
			cpu->cycles += 1;
			SystemInterlink_appendSyncCycles(cpu->system, cpu->cycles);
			continue;
		}

		// Jump if pending, else add four to program counter
		if (cpu->jumpPending && cpu->prevWasBranch) {
			cpu->programCounter = cpu->jumpAddress;
			cpu->jumpPending = false;
		} else {
			tempAddress = (int32_t)((cpu->programCounter & 0xFFFFFFFFL) + 4L);
			cpu->programCounter = tempAddress;
		}

		// Increment cycle count
		cpu->cycles += 1;

		// Setup whether the instruction just gone was a branch, and clear
		// current branch status
		cpu->prevWasBranch = cpu->isBranch;
		cpu->isBranch = false;

		// Return number of cycles instruction took
		SystemInterlink_appendSyncCycles(cpu->system, cpu->cycles);
	} while (!cpu->prevWasBranch);
}

/*
 * This returns a reference to the Bus Interface Unit.
 */
BusInterfaceUnit *R3051_getBiu(R3051 *cpu)
{
	return cpu->biu;
}

/*
 * This function returns Cop0.
 */
Cop0 *R3051_getCop0(R3051 *cpu)
{
	return cpu->sccp;
}

/*
 * This function returns Cop2.
 */
Cop2 *R3051_getCop2(R3051 *cpu)
{
	return cpu->gte;
}

/*
 * This function returns the hi register.
 */
int32_t R3051_getHiReg(R3051 *cpu)
{
	return cpu->hiReg;
}

/*
 * This function returns the lo register.
 */
int32_t R3051_getLoReg(R3051 *cpu)
{
	return cpu->loReg;
}

/*
 * This retrieves the program counter value.
 */
int32_t R3051_getProgramCounter(R3051 *cpu)
{
	return cpu->programCounter;
}

/*
 * This instruction reads a data value of the specified width, and abstracts
 * this functionality from the MEM stage.
 */
int32_t R3051_readDataValue(R3051 *cpu, int32_t width, int32_t address)
{
	// Define value
	int32_t value = 0;

	// Get cache control register and other related values
	bool dataCacheIsolated = Cop0_isDataCacheIsolated(cpu->sccp);

	// Get physical address
	int32_t physicalAddress = Cop0_virtualToPhysical(cpu->sccp, address);
	int64_t tempPhysicalAddress = physicalAddress & 0xFFFFFFFFL;

	// Is cache isolated? Although we don't have a data cache (it is used as
	// scratchpad instead) we should read from instruction cache if so
	if (dataCacheIsolated) { // Yes

		// Read from instruction cache no matter what
		switch (width) {
			case PHILPSX_R3051_BYTE:
				value = 0xFF & Cache_readByte(
						cpu->instructionCache,
						physicalAddress
						);
				break;
			case PHILPSX_R3051_HALFWORD:
				value = 0xFF & Cache_readByte(
						cpu->instructionCache,
						(int32_t)tempPhysicalAddress
						);
				++tempPhysicalAddress;
				value = (value << 8) | (0xFF & Cache_readByte(
						cpu->instructionCache,
						(int32_t)tempPhysicalAddress
						));
				break;
			case PHILPSX_R3051_WORD:
				value = Cache_readWord(cpu->instructionCache, physicalAddress);
				break;
		}
	} else { // No

		// Check if we are reading from scratchpad
		if (tempPhysicalAddress >= 0x1F800000L &&
				tempPhysicalAddress < 0x1F800400L &&
				SystemInterlink_scratchpadEnabled(cpu->system)) {

			// Although we are sending reads to system, this actually
			// accesses scratchpad
			switch (width) {
				case PHILPSX_R3051_BYTE:
					value = 0xFF & SystemInterlink_readByte(
							cpu->system,
							physicalAddress
							);
					break;
				case PHILPSX_R3051_HALFWORD:
					value = 0xFF & SystemInterlink_readByte(
							cpu->system,
							(int32_t)tempPhysicalAddress
							);
					++tempPhysicalAddress;
					value = (value << 8) | (0xFF & SystemInterlink_readByte(
							cpu->system,
							(int32_t)tempPhysicalAddress
							));
					break;
				case PHILPSX_R3051_WORD:
					value = SystemInterlink_readWord(
							cpu->system,
							physicalAddress
							);
					break;
			}

			return value;
		}

		// Read from system
		int32_t delayCycles = SystemInterlink_howManyStallCycles(
				cpu->system,
				physicalAddress
				);

		// Begin transaction
		BusInterfaceUnit_startTransaction(cpu->biu);

		// Read value straight away
		switch (width) {
			case PHILPSX_R3051_BYTE:
				value = 0xFF & SystemInterlink_readByte(
						cpu->system,
						physicalAddress
						);
				break;
			case PHILPSX_R3051_HALFWORD:
				value = 0xFF & SystemInterlink_readByte(
						cpu->system,
						(int32_t)tempPhysicalAddress
						);
				tempPhysicalAddress =
						SystemInterlink_okToIncrement(
						cpu->system,
						tempPhysicalAddress) ?
							tempPhysicalAddress + 1 : tempPhysicalAddress;
				value = (value << 8) | (0xFF & SystemInterlink_readByte(
						cpu->system,
						(int32_t)tempPhysicalAddress
						));
				break;
			case PHILPSX_R3051_WORD:
				value = SystemInterlink_readWord(cpu->system, physicalAddress);
				break;
		}
		cpu->cycles += delayCycles;

		// End transaction
		BusInterfaceUnit_stopTransaction(cpu->biu);

	}

	// Return data value
	return value;
}

/*
 * This function reads an instruction word. It allows abstraction of this
 * functionality from the IF pipeline stage.
 */
int64_t R3051_readInstructionWord(R3051 *cpu, int32_t address,
		int32_t tempBranchAddress)
{
	// Check for dodgy address
	if (!Cop0_isAddressAllowed(cpu->sccp, address) ||
			cpu->programCounter % 4 != 0) {

		// Trigger exception
		cpu->exception->badAddress = address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					tempBranchAddress : address;
		R3051_handleException(cpu);
		return -1L;
	}

	// Define word variable
	int32_t wordVal = 0;

	// Check if instruction cache enabled
	bool instructionCacheEnabled =
			SystemInterlink_instructionCacheEnabled(cpu->system);

	// Get physical address
	int32_t physicalAddress = Cop0_virtualToPhysical(cpu->sccp, address);

	// Check if address is cacheable or not
	if (Cop0_isCacheable(cpu->sccp, address) && instructionCacheEnabled) {

		// Check cache for hit
		if (Cache_checkForHit(cpu->instructionCache, physicalAddress)) {
			wordVal = Cache_readWord(cpu->instructionCache, physicalAddress);
		} else {

			// Refill cache then set wordVal
			if (BusInterfaceUnit_getHolder(cpu->biu) !=
					PHILPSX_COMPONENTS_CPU) {

				// Stall for one cycle as BIU is being used by
				// another component
				return -1L;
			} else {

				// Begin transaction
				BusInterfaceUnit_startTransaction(cpu->biu);

				cpu->cycles += SystemInterlink_howManyStallCycles(
						cpu->system,
						physicalAddress
						);
				Cache_refillLine(
						cpu->instructionCache,
						cpu->sccp,
						cpu->system,
						physicalAddress
						);
				wordVal = Cache_readWord(
						cpu->instructionCache,
						physicalAddress
						);

				// End transaction
				BusInterfaceUnit_stopTransaction(cpu->biu);
			}
		}
	} else {

		// Read word straight from system, stalling if being used
		if (BusInterfaceUnit_getHolder(cpu->biu) != PHILPSX_COMPONENTS_CPU) {

			// Stall for one cycle as BIU is being used by another component
			return -1L;
		} else {

			// Begin transaction
			BusInterfaceUnit_startTransaction(cpu->biu);

			cpu->cycles += SystemInterlink_howManyStallCycles(
					cpu->system,
					physicalAddress
					);
			wordVal = SystemInterlink_readWord(cpu->system, physicalAddress);

			// End transaction
			BusInterfaceUnit_stopTransaction(cpu->biu);
		}
	}

	// Return word variable
	return 0xFFFFFFFFL & wordVal;
}

/*
 * This function reads from a register.
 */
int32_t R3051_readReg(R3051 *cpu, int32_t reg)
{
	// Declare holding variable
	int32_t retVal = 0;

	// Determine which register we are after
	switch (reg) {
		case 0:
			break;
		default: 
			retVal = cpu->generalRegisters[reg];
			break;
	}

	return retVal;
}

/*
 * This function resets the main processor.
 */
void R3051_reset(R3051 *cpu)
{
	cpu->programCounter = Cop0_getResetExceptionVector(cpu->sccp);
}

/*
 * This function sets the system interlink reference.
 */
void R3051_setMemoryInterface(R3051 *cpu, SystemInterlink *system)
{
	cpu->system = system;
}

/*
 * This is a utility function to swap the endianness of a word. It can be
 * used to allow the processor to operate in both endian modes by transparently
 * swapping the byte order before writing or after writing.
 */
int32_t R3051_swapWordEndianness(R3051 *cpu, int32_t word)
{
	return (word << 24) |
			((word << 8) & 0xFF0000) |
			(logical_rshift(word, 8) & 0xFF00) |
			(logical_rshift(word, 24) & 0xFF);
}

/*
 * This instruction writes a data value of the specified width, and abstracts
 * this functionality from the MEM stage.
 */
void R3051_writeDataValue(R3051 *cpu, int32_t width, int32_t address,
		int32_t value)
{
	// Get cache control register and other related values
	bool dataCacheIsolated = Cop0_isDataCacheIsolated(cpu->sccp);

	// Get physical address
	int32_t physicalAddress = Cop0_virtualToPhysical(cpu->sccp, address);
	int64_t tempPhysicalAddress = physicalAddress & 0xFFFFFFFFL;

	// Is cache isolated?
	if (dataCacheIsolated) { // Yes

		// Write to instruction cache no matter what
		switch (width) {
			case PHILPSX_R3051_BYTE:
				Cache_writeByte(
						cpu->instructionCache,
						cpu->sccp,
						physicalAddress,
						(int8_t)value
						);
				break;
			case PHILPSX_R3051_HALFWORD:
				Cache_writeByte(cpu->instructionCache,
						cpu->sccp,
						(int32_t)tempPhysicalAddress,
						(int8_t)logical_rshift(value, 8)
						);
				++tempPhysicalAddress;
				Cache_writeByte(
						cpu->instructionCache,
						cpu->sccp,
						(int32_t)tempPhysicalAddress,
						(int8_t)value
						);
				break;
			case PHILPSX_R3051_WORD:
				Cache_writeWord(
						cpu->instructionCache,
						cpu->sccp,
						physicalAddress,
						value
						);
				break;
		}
	} else { // No

		// Check if we are writing to scratchpad
		if (tempPhysicalAddress >= 0x1F800000L &&
				tempPhysicalAddress < 0x1F800400L &&
				SystemInterlink_scratchpadEnabled(cpu->system)) {

			// Although we are sending writes to the system, they actually
			// go to the scratchpad
			switch (width) {
				case PHILPSX_R3051_BYTE:
					SystemInterlink_writeByte(
							cpu->system,
							physicalAddress,
							(int8_t)value);
					break;
				case PHILPSX_R3051_HALFWORD:
					SystemInterlink_writeByte(
							cpu->system,
							(int32_t)tempPhysicalAddress,
							(int8_t)logical_rshift(value, 8)
							);
					++tempPhysicalAddress;
					SystemInterlink_writeByte(
							cpu->system,
							(int32_t)tempPhysicalAddress,
							(int8_t)value);
					break;
				case PHILPSX_R3051_WORD:
					SystemInterlink_writeWord(
							cpu->system,
							physicalAddress,
							value
							);
					break;
			}

			return;
		}

		// Write directly to system
		int32_t delayCycles = SystemInterlink_howManyStallCycles(
				cpu->system,
				physicalAddress
				);

		// Begin transaction
		BusInterfaceUnit_startTransaction(cpu->biu);

		// Write value
		switch (width) {
			case PHILPSX_R3051_BYTE:
				SystemInterlink_writeByte(
						cpu->system,
						physicalAddress,
						(int8_t)value
						);
				break;
			case PHILPSX_R3051_HALFWORD:
				SystemInterlink_writeByte(
						cpu->system,
						(int32_t)tempPhysicalAddress,
						(int8_t)logical_rshift(value, 8)
						);
				tempPhysicalAddress =
						SystemInterlink_okToIncrement(
							cpu->system,
							tempPhysicalAddress
							) ?
							tempPhysicalAddress + 1 : tempPhysicalAddress;
				SystemInterlink_writeByte(
						cpu->system,
						(int32_t)tempPhysicalAddress,
						(int8_t)value
						);
				break;
			case PHILPSX_R3051_WORD:
				SystemInterlink_writeWord(cpu->system, physicalAddress, value);
				break;
		}
		cpu->cycles += delayCycles;

		// End transaction
		BusInterfaceUnit_stopTransaction(cpu->biu);
	}
}

/*
 * This function writes to a register. It allows override of write protection
 * on certain bits.
 */
void R3051_writeReg(R3051 *cpu, int32_t reg, int32_t value, bool override)
{
	// Determine which register we are writing
	if (override) {
		cpu->generalRegisters[reg] = value;
	} else {
		switch (reg) {
			case 0: break;
			default:
				cpu->generalRegisters[reg] = value;
				break;
		}
	}
}

/*
 * This function handles the ADD R3051 instruction.
 */
static void R3051_ADD(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Add rsVal to rtVal
	int32_t rsVal = R3051_readReg(cpu, rs);
	int32_t rtVal = R3051_readReg(cpu, rt);
	int32_t result = rsVal + rtVal;

	// Check for two's complement overflow
	if ((rsVal & 0x80000000) == (rtVal & 0x80000000)) {
		if ((rsVal & 0x80000000) != (result & 0x80000000)) {

			// Trigger exception
			int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
			tempAddress -= 4;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_OVF;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ? 
						(int32_t)tempAddress : cpu->programCounter;
			return;
		}
	}

	// Store result
	R3051_writeReg(cpu, rd, result, false);
}

/*
 * This function handles the ADDI R3051 instruction.
 */
static void R3051_ADDI(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Sign extend immediate
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}

	// Add to rsVal
	int32_t rsVal = R3051_readReg(cpu, rs);
	int32_t result = rsVal + immediate;

	// Check for two's complement overflow
	if ((rsVal & 0x80000000) == (immediate & 0x80000000)) {
		if ((rsVal & 0x80000000) != (result & 0x80000000)) {

			// Trigger exception
			int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
			tempAddress -= 4;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_OVF;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						(int32_t)tempAddress : cpu->programCounter;
			return;
		}
	}

	// Store result
	R3051_writeReg(cpu, rt, result, false);
}

/*
 * This function handles the ADDIU R3051 instruction.
 */
static void R3051_ADDIU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Sign extend immediate
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000L;
	}

	// Add to rsVal
	int32_t result =
			(int32_t)((R3051_readReg(cpu, rs) & 0xFFFFFFFFL) + immediate);

	// Store result
	R3051_writeReg(cpu, rt, result, false);
}

/*
 * This function handles the ADDU R3051 instruction.
 */
static void R3051_ADDU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Add rsVal to rtVal
	int32_t result = (int32_t)(
			(R3051_readReg(cpu, rs) & 0xFFFFFFFFL) +
			(R3051_readReg(cpu, rt) & 0xFFFFFFFFL)
			);

	// Store result
	R3051_writeReg(cpu, rd, result, false);
}

/*
 * This function handles the AND R3051 instruction.
 */
static void R3051_AND(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Bitwise AND rsVal and rtVal, storing result
	R3051_writeReg(cpu, rd,
			(R3051_readReg(cpu, rs) & R3051_readReg(cpu, rt)), false);
}

/*
 * This function handles the ANDI R3051 instruction.
 */
static void R3051_ANDI(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Zero extending immediate is already done for us
	// so just AND with rsVal and store result
	R3051_writeReg(cpu, rt, (immediate & R3051_readReg(cpu, rs)), false);
}

/*
 * This function handles the BC2F R3051 instruction.
 */
static void R3051_BC2F(R3051 *cpu, int32_t instruction)
{
	// Get immediate
	int32_t immediate = instruction & 0xFFFF;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// Jump if COP2 condition line is false
	if (!Cop2_getConditionLineStatus(cpu->gte)) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BC2T R3051 instruction.
 */
static void R3051_BC2T(R3051 *cpu, int32_t instruction)
{
	// Get immediate
	int32_t immediate = instruction & 0xFFFF;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// Jump if COP2 condition line is true
	if (Cop2_getConditionLineStatus(cpu->gte)) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BEQ R3051 instruction.
 */
static void R3051_BEQ(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// Tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if condition holds true
	if (R3051_readReg(cpu, rs) == R3051_readReg(cpu, rt)) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BGEZ R3051 instruction.
 */
static void R3051_BGEZ(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs greater than or equal to 0
	if (R3051_readReg(cpu, rs) >= 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BGEZAL R3051 instruction.
 */
static void R3051_BGEZAL(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Define target address and return address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 8L;
	int64_t returnAddress = targetAddress;

	// Create target address
	targetAddress -= 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs greater than or equal to 0, and save return address in r31
	if (R3051_readReg(cpu, rs) >= 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
		R3051_writeReg(cpu, 31, (int32_t)returnAddress, false);
	}
}

/*
 * This function handles the BGTZ R3051 instruction.
 */
static void R3051_BGTZ(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs greater than 0
	if (R3051_readReg(cpu, rs) > 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BLEZ R3051 instruction.
 */
static void R3051_BLEZ(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs less than or equal to 0
	if (R3051_readReg(cpu, rs) <= 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BLTZ R3051 instruction.
 */
static void R3051_BLTZ(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs less than 0
	if (R3051_readReg(cpu, rs) < 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BLTZAL R3051 instruction.
 */
static void R3051_BLTZAL(R3051 *cpu, int32_t instruction)
{
	// Get rs and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Define target address and return address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 8L;
	int64_t returnAddress = targetAddress;

	// Create target address
	targetAddress -= 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// This tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if rs less than 0, and save return address in r31
	if (R3051_readReg(cpu, rs) < 0) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
		R3051_writeReg(cpu, 31, (int32_t)returnAddress, false);
	}
}

/*
 * This function handles the BNE R3051 instruction.
 */
static void R3051_BNE(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Create target address
	int64_t targetAddress = (cpu->programCounter & 0xFFFFFFFFL) + 4L;
	int32_t offset = immediate << 2;
	if ((offset & 0x20000) == 0x20000) {
		offset |= 0xFFFC0000;
	}
	targetAddress += offset;

	// Tells us this is a branch-type instruction
	cpu->isBranch = true;

	// Jump if condition holds false
	if (R3051_readReg(cpu, rs) != R3051_readReg(cpu, rt)) {
		cpu->jumpAddress = (int32_t)targetAddress;
		cpu->jumpPending = true;
	}
}

/*
 * This function handles the BREAK R3051 instruction.
 */
static void R3051_BREAK(R3051 *cpu, int32_t instruction)
{
	// Trigger Breakpoint Exception
	int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
	tempAddress -= 4;
	cpu->exception->exceptionReason = PHILPSX_EXCEPTION_BP;
	cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
	cpu->exception->programCounterOrigin
			= cpu->exception->isInBranchDelaySlot ?
				(int32_t)tempAddress : cpu->programCounter;
}

/*
 * This function handles the CF2 R3051 instruction.
 */
static void R3051_CF2(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move from COP2 control reg rd to CPU reg rt
	R3051_writeReg(cpu, rt, Cop2_readControlReg(cpu->gte, rd), false);
}

/*
 * This function handles the CT2 R3051 instruction.
 */
static void R3051_CT2(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move from CPU reg rt to COP2 control reg rd
	Cop2_writeControlReg(cpu->gte, rd, R3051_readReg(cpu, rt), false);
}

/*
 * This function handles the DIV R3051 instruction.
 */
static void R3051_DIV(R3051 *cpu, int32_t instruction)
{
	// Get rs and rt
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Divide rs by rt as signed values
	int64_t rsVal = R3051_readReg(cpu, rs);
	int64_t rtVal = R3051_readReg(cpu, rt);
	int64_t quotient = 0;
	int64_t remainder = 0;

	if (rtVal != 0) {
		quotient = rsVal / rtVal;
		remainder = rsVal % rtVal;
	} else {
		//fprintf(stdout, "PhilPSX: R3051: R3051_DIV: divided by 0\n");
		quotient = 0xFFFFFFFFL;
		remainder = rsVal;
	}

	// Store result
	cpu->hiReg = (int32_t)remainder;
	cpu->loReg = (int32_t)quotient;
}

/*
 * This function handles the DIVU R3051 instruction.
 */
static void R3051_DIVU(R3051 *cpu, int32_t instruction)
{
	// Get rs and rt
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Divide rs by rt as unsigned values
	int64_t rsVal = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	int64_t rtVal = R3051_readReg(cpu, rt) & 0xFFFFFFFFL;
	int64_t quotient = 0;
	int64_t remainder = 0;

	if (rtVal != 0) {
		quotient = rsVal / rtVal;
		remainder = rsVal % rtVal;
	} else {
		//fprintf(stdout, "PhilPSX: R3051: R3051_DIVU: divided by 0\n");
		quotient = 0xFFFFFFFFL;
		remainder = rsVal;
	}

	// Store result
	cpu->hiReg = (int32_t)remainder;
	cpu->loReg = (int32_t)quotient;
}

/*
 * This function handles the J R3051 instruction.
 */
static void R3051_J(R3051 *cpu, int32_t instruction)
{
	// Get target
	int32_t target = instruction & 0x3FFFFFF;

	// Create address to jump to
	cpu->jumpAddress = (target << 2) | (cpu->programCounter & 0xF0000000);
	cpu->jumpPending = true;
	cpu->isBranch = true;
}

/*
 * This function handles the JAL R3051 instruction.
 */
static void R3051_JAL(R3051 *cpu, int32_t instruction)
{
	// Get target
	int32_t target = instruction & 0x3FFFFFF;

	// Create address to jump to, and place address of instruction
	// after delay slot in r31
	cpu->jumpAddress = (target << 2) | (cpu->programCounter & 0xF0000000);
	cpu->jumpPending = true;
	cpu->isBranch = true;
	int64_t newAddress = (cpu->programCounter & 0xFFFFFFFFL) + 8L;
	R3051_writeReg(cpu, 31, (int32_t)newAddress, false);
}

/*
 * This function handles the JALR R3051 instruction.
 */
static void R3051_JALR(R3051 *cpu, int32_t instruction)
{
	// Get rs and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Jump to rs value and place address of instruction
	// after delay slot into rd
	cpu->jumpAddress = R3051_readReg(cpu, rs);
	cpu->jumpPending = true;
	cpu->isBranch = true;
	int64_t newAddress = (cpu->programCounter & 0xFFFFFFFFL) + 8L;
	R3051_writeReg(cpu, rd, (int32_t)newAddress, false);
}

/*
 * This function handles the JR R3051 instruction.
 */
static void R3051_JR(R3051 *cpu, int32_t instruction)
{
	// Get rs
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Jump to rs value
	cpu->jumpAddress = R3051_readReg(cpu, rs);
	cpu->jumpPending = true;
	cpu->isBranch = true;
}

/*
 * This function handles the LB R3051 instruction.
 */
static void R3051_LB(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load byte and sign extend
	int32_t tempByte = 0xFF & R3051_readDataValue(
			cpu,
			PHILPSX_R3051_BYTE,
			(int32_t)address
			);
	if ((tempByte & 0x80) == 0x80) {
		tempByte |= 0xFFFFFF00;
	}

	// Write byte to correct register
	R3051_writeReg(cpu, rt, tempByte, false);
}

/*
 * This function handles the LBU R3051 instruction.
 */
static void R3051_LBU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load byte and zero extend
	int32_t tempByte = 0xFF & R3051_readDataValue(
			cpu,
			PHILPSX_R3051_BYTE,
			(int32_t)address
			);

	// Write byte to correct register
	R3051_writeReg(cpu, rt, tempByte, false);
}

/*
 * This function handles the LH R3051 instruction.
 */
static void R3051_LH(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and half-word aligned, trigger
	// exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 2 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load half-word and swap endianness, sign extend
	int32_t tempHalfWord = 0xFFFF & R3051_readDataValue(
			cpu,
			PHILPSX_R3051_HALFWORD,
			(int32_t)address
			);
	tempHalfWord = ((tempHalfWord << 8) & 0xFF00) |
			logical_rshift(tempHalfWord, 8);
	if ((tempHalfWord & 0x8000) == 0x8000) {
		tempHalfWord |= 0xFFFF0000;
	}

	// Write half-word to correct register
	R3051_writeReg(cpu, rt, tempHalfWord, false);
}

/*
 * This function handles the LHU R3051 instruction.
 */
static void R3051_LHU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and half-word aligned, trigger
	// exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 2 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load half-word and swap endianness, zero extend
	int32_t tempHalfWord = 0xFFFF & R3051_readDataValue(
			cpu,
			PHILPSX_R3051_HALFWORD,
			(int32_t)address
			);

	// Swap byte order
	tempHalfWord = ((tempHalfWord << 8) & 0xFF00) |
			logical_rshift(tempHalfWord, 8);

	// Write half-word to correct register
	R3051_writeReg(cpu, rt, tempHalfWord, false);
}

/*
 * This function handles the LUI R3051 instruction.
 */
static void R3051_LUI(R3051 *cpu, int32_t instruction)
{
	// Get rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Shift immediate left by 16 bits (leaving least significant
	// 16 bits as zeroes) and store result
	R3051_writeReg(cpu, rt, (immediate << 16), false);
}

/*
 * This function handles the LW R3051 instruction.
 */
static void R3051_LW(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and word aligned, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 4 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load word
	int32_t tempWord = R3051_readDataValue(
			cpu,
			PHILPSX_R3051_WORD,
			(int32_t)address
			);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Write word to correct register
	R3051_writeReg(cpu, rt, tempWord, false);
}

/*
 * This function handles the LWC2 R3051 instruction.
 */
static void R3051_LWC2(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and word aligned, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 4 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load word
	int32_t tempWord = R3051_readDataValue(
			cpu,
			PHILPSX_R3051_WORD,
			(int32_t)address
			);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Write word to correct COP2 data register
	Cop2_writeDataReg(cpu->gte, rt, tempWord, false);
}

/*
 * This function handles the LWL R3051 instruction.
 */
static void R3051_LWL(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Align address, fetch word, and store shift index
	int32_t tempAddress = (int32_t)(address & 0xFFFFFFFC);
	int32_t byteShiftIndex = (int32_t)(~address & 0x3);
	int32_t tempWord = R3051_readDataValue(
			cpu,
			PHILPSX_R3051_WORD,
			tempAddress
			);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Shift word value left by required amount
	tempWord = tempWord << (byteShiftIndex * 8);

	// Fetch rt contents, and calculate mask
	int32_t tempRtVal = R3051_readReg(cpu, rt);
	int32_t mask = ~(0xFFFFFFFF << (byteShiftIndex * 8));
	tempRtVal &= mask;

	// Merge contents
	tempWord |= tempRtVal;

	// Write word to correct register
	R3051_writeReg(cpu, rt, tempWord, false);
}

/*
 * This function handles the LWR R3051 instruction.
 */
static void R3051_LWR(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADEL;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ? (int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Align address, fetch word, and store shift index
	int32_t tempAddress = (int32_t)(address & 0xFFFFFFFC);
	int32_t byteShiftIndex = (int32_t)(address & 0x3);
	int32_t tempWord = R3051_readDataValue(cpu, PHILPSX_R3051_WORD, tempAddress);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Shift word value left by required amount
	tempWord = logical_rshift(tempWord, (byteShiftIndex * 8));

	// Fetch rt contents, and calculate mask
	int32_t tempRtVal = R3051_readReg(cpu, rt);
	int32_t mask = ~(logical_rshift(0xFFFFFFFF, (byteShiftIndex * 8)));
	tempRtVal &= mask;

	// Merge contents
	tempWord |= tempRtVal;

	// Write word to correct register
	R3051_writeReg(cpu, rt, tempWord, false);
}

/*
 * This function handles the MF0 R3051 instruction.
 */
static void R3051_MF0(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Check if rd is any of the following and trigger exception if so
	switch (rd) {
		case 0:
		case 1:
		case 2:
		case 4:
		case 10:
		{
			// Trigger exception
			int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
			tempAddress -= 4;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_RI;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						(int32_t)tempAddress : cpu->programCounter;
			return;
		}
	}

	// Move COP0 reg rd to CPU reg rt
	R3051_writeReg(cpu, rt, Cop0_readReg(cpu->sccp, rd), false);
}

/*
 * This function handles the MF2 R3051 instruction.
 */
static void R3051_MF2(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move from COP2 data reg rd to CPU reg rt
	R3051_writeReg(cpu, rt, Cop2_readDataReg(cpu->gte, rd), false);
}

/*
 * This function handles the MFHI R3051 instruction.
 */
static void R3051_MFHI(R3051 *cpu, int32_t instruction)
{
	// Get rd
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move Hi to rd
	R3051_writeReg(cpu, rd, cpu->hiReg, false);
}

/*
 * This function handles the MFLO R3051 instruction.
 */
static void R3051_MFLO(R3051 *cpu, int32_t instruction)
{
	// Get rd
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move Lo to rd
	R3051_writeReg(cpu, rd, cpu->loReg, false);
}

/*
 * This function handles the MT0 R3051 instruction.
 */
static void R3051_MT0(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move CPU reg rt to COP0 reg rd
	Cop0_writeReg(cpu->sccp, rd, R3051_readReg(cpu, rt), false);
}

/*
 * This function handles the MT2 R3051 instruction.
 */
static void R3051_MT2(R3051 *cpu, int32_t instruction)
{
	// Get rt and rd
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Move from CPU reg rt to COP2 data reg rd
	Cop2_writeDataReg(cpu->gte, rd, R3051_readReg(cpu, rt), false);
}

/*
 * This function handles the MTHI R3051 instruction.
 */
static void R3051_MTHI(R3051 *cpu, int32_t instruction)
{
	// Get rs
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Move rs to Hi
	cpu->hiReg = R3051_readReg(cpu, rs);
}

/*
 * This function handles the MTLO R3051 instruction.
 */
static void R3051_MTLO(R3051 *cpu, int32_t instruction)
{
	// Get rs
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;

	// Move rs to Lo
	cpu->loReg = R3051_readReg(cpu, rs);
}

/*
 * This function handles the MULT R3051 instruction.
 */
static void R3051_MULT(R3051 *cpu, int32_t instruction)
{
	// Get rs and rt
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Multiply rs and rt as signed values
	int64_t rsVal = R3051_readReg(cpu, rs);
	int64_t rtVal = R3051_readReg(cpu, rt);
	int64_t result = rsVal * rtVal;

	// Store result
	cpu->hiReg = (int32_t)logical_rshift(result, 32);
	cpu->loReg = (int32_t)result;
}

/*
 * This function handles the MULTU R3051 instruction.
 */
static void R3051_MULTU(R3051 *cpu, int32_t instruction)
{
	// Get rs and rt
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Multiply rs and rt as unsigned values
	int64_t rsVal = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	int64_t rtVal = R3051_readReg(cpu, rt) & 0xFFFFFFFFL;
	int64_t result = rsVal * rtVal;

	// Store result
	cpu->hiReg = (int32_t)logical_rshift(result, 32);
	cpu->loReg = (int32_t)result;
}

/*
 * This function handles the NOR R3051 instruction.
 */
static void R3051_NOR(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Bitwise NOR rsVal and rtVal, storing result
	R3051_writeReg(
			cpu,
			rd,
			~(R3051_readReg(cpu, rs) | R3051_readReg(cpu, rt)),
			false
			);
}

/*
 * This function handles the OR R3051 instruction.
 */
static void R3051_OR(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Bitwise OR rsVal and rtVal, storing result
	R3051_writeReg(
			cpu,
			rd,
			(R3051_readReg(cpu, rs) | R3051_readReg(cpu, rt)),
			false
			);
}

/*
 * This function handles the ORI R3051 instruction.
 */
static void R3051_ORI(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Zero extending immediate is already done for us
	// so just OR with rsVal and store result
	R3051_writeReg(cpu, rt, (immediate | R3051_readReg(cpu, rs)), false);
}

/*
 * This function handles the RFE R3051 instruction.
 */
static void R3051_RFE(R3051 *cpu, int32_t instruction)
{
	Cop0_rfe(cpu->sccp);
}

/*
 * This function handles the SB R3051 instruction.
 */
static void R3051_SB(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load byte from register and write to memory
	int32_t tempByte = 0xFF & R3051_readReg(cpu, rt);
	R3051_writeDataValue(cpu, PHILPSX_R3051_BYTE, (int32_t)address, tempByte);
}

/*
 * This function handles the SH R3051 instruction.
 */
static void R3051_SH(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and half-word aligned, trigger
	// exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 2 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load half-word from register and swap endianness, then write to memory
	// (checking for exceptions and stalls)
	int32_t tempHalfWord = 0xFFFF & R3051_readReg(cpu, rt);

	// Swap byte order
	tempHalfWord = ((tempHalfWord << 8) & 0xFF00) |
			logical_rshift(tempHalfWord, 8);

	R3051_writeDataValue(
			cpu,
			PHILPSX_R3051_HALFWORD,
			(int32_t)address,
			tempHalfWord
			);
}

/*
 * This function handles the SLL R3051 instruction.
 */
static void R3051_SLL(R3051 *cpu, int32_t instruction)
{
	// Get rt, rd and shamt
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;
	int32_t shamt = logical_rshift(instruction, 6) & 0x1F;

	// Shift rt value left by shamt bits, inserting zeroes
	// into low order bits, then store result
	R3051_writeReg(cpu, rd, (R3051_readReg(cpu, rt) << shamt), false);
}

/*
 * This function handles the SLLV R3051 instruction.
 */
static void R3051_SLLV(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Shift rt value left by (lowest 5 bits of rs value), 
	// inserting zeroes into low order bits, then
	// store result
	R3051_writeReg(
			cpu,
			rd,
			(R3051_readReg(cpu, rt) << (R3051_readReg(cpu, rs) & 0x1F)),
			false
			);
}

/*
 * This function handles the SLT R3051 instruction.
 */
static void R3051_SLT(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Compare rsVal and rtVal, storing result
	if (R3051_readReg(cpu, rs) < R3051_readReg(cpu, rt)) {
		R3051_writeReg(cpu, rd, 1, false);
	} else {
		R3051_writeReg(cpu, rd, 0, false);
	}
}

/*
 * This function handles the SLTI R3051 instruction.
 */
static void R3051_SLTI(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Sign extend immediate
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}

	// Store result
	if (R3051_readReg(cpu, rs) < immediate) {
		R3051_writeReg(cpu, rt, 1, false);
	} else {
		R3051_writeReg(cpu, rt, 0, false);
	}
}

/*
 * This function handles the SLTIU R3051 instruction.
 */
static void R3051_SLTIU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Sign extend immediate
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000L;
	}

	// Treat rsVal as unsigned
	int64_t tempRsVal = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;

	// Store result
	if (tempRsVal < immediate) {
		R3051_writeReg(cpu, rt, 1, false);
	} else {
		R3051_writeReg(cpu, rt, 0, false);
	}
}

/*
 * This function handles the SLTU R3051 instruction.
 */
static void R3051_SLTU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Compare rsVal and rtVal as unsigned values, storing result
	if ((R3051_readReg(cpu, rs) & 0xFFFFFFFFL) <
			(R3051_readReg(cpu, rt) & 0xFFFFFFFFL)) {
		R3051_writeReg(cpu, rd, 1, false);
	} else {
		R3051_writeReg(cpu, rd, 0, false);
	}
}

/*
 * This function handles the SRA R3051 instruction.
 */
static void R3051_SRA(R3051 *cpu, int32_t instruction)
{
	// Get rt, rd and shamt
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;
	int32_t shamt = logical_rshift(instruction, 6) & 0x1F;

	// Shift rt value right by shamt bits, sign extending
	// high order bits, then store result
	R3051_writeReg(cpu, rd, (R3051_readReg(cpu, rt) >> shamt), false);
}

/*
 * This function handles the SRAV R3051 instruction.
 */
static void R3051_SRAV(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Shift rt value right by (lowest 5 bits of rs value), 
	// sign extending high order bits, then
	// store result
	R3051_writeReg(
			cpu,
			rd,
			(R3051_readReg(cpu, rt) >> (R3051_readReg(cpu, rs) & 0x1F)),
			false
			);
}

/*
 * This function handles the SRL R3051 instruction.
 */
static void R3051_SRL(R3051 *cpu, int32_t instruction)
{
	// Get rt, rd and shamt
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;
	int32_t shamt = logical_rshift(instruction, 6) & 0x1F;

	// Shift rt value right by shamt bits, inserting zeroes
	// into high order bits, then store result
	R3051_writeReg(
			cpu,
			rd,
			logical_rshift(R3051_readReg(cpu, rt), shamt),
			false
			);
}

/*
 * This function handles the SRLV R3051 instruction.
 */
static void R3051_SRLV(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Shift rt value right by (lowest 5 bits of rs value), 
	// inserting zeroes into high order bits, then
	// store result
	R3051_writeReg(
			cpu,
			rd,
			logical_rshift(
			R3051_readReg(cpu, rt),
			(R3051_readReg(cpu, rs) & 0x1F)
			),
			false
			);
}

/*
 * This function handles the SUB R3051 instruction.
 */
static void R3051_SUB(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Subtract rtVal from rsVal
	int32_t rsVal = R3051_readReg(cpu, rs);
	int32_t rtVal = R3051_readReg(cpu, rt);
	int32_t result = rsVal - rtVal;

	// Check for two's complement overflow
	if ((rsVal & 0x80000000) != (rtVal & 0x80000000)) {
		if ((rsVal & 0x80000000) != (result & 0x80000000)) {

			// Trigger exception
			int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
			tempAddress -= 4;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_OVF;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						(int32_t)tempAddress : cpu->programCounter;
			return;
		}
	}

	// Store result
	R3051_writeReg(cpu, rd, result, false);
}

/*
 * This function handles the SUBU R3051 instruction.
 */
static void R3051_SUBU(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Subtract rtVal from rsVal
	int32_t result = (int32_t)((R3051_readReg(cpu, rs) & 0xFFFFFFFFL) -
			(R3051_readReg(cpu, rt) & 0xFFFFFFFFL));

	// Store result
	R3051_writeReg(cpu, rd, result, false);
}

/*
 * This function handles the SW R3051 instruction.
 */
static void R3051_SW(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and word aligned, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 4 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load word from register, set byte order and write to memory
	// (checking for exceptions and stalls)
	int32_t tempWord = R3051_readReg(cpu, rt);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	R3051_writeDataValue(cpu, PHILPSX_R3051_WORD, (int32_t)address, tempWord);
}

/*
 * This function handles the SWC2 R3051 instruction.
 */
static void R3051_SWC2(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed and word aligned, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address) ||
			address % 4 != 0) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Load word from register, set byte order and write to memory
	int32_t tempWord = Cop2_readDataReg(cpu->gte, rt);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	R3051_writeDataValue(cpu, PHILPSX_R3051_WORD, (int32_t)address, tempWord);
}

/*
 * This function handles the SWL R3051 instruction.
 */
static void R3051_SWL(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Align address, fetch word, and store shift index - sort byte order too
	int32_t tempAddress = (int32_t)(address & 0xFFFFFFFC);
	int32_t byteShiftIndex = (int32_t)(~address & 0x3);
	int32_t tempWord = R3051_readReg(cpu, rt);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Shift word value left by required amount
	tempWord = tempWord << (byteShiftIndex * 8);

	// Fetch memory contents, and calculate mask
	int32_t tempVal = SystemInterlink_readWord(cpu->system, tempAddress);
	int32_t mask = ~(0xFFFFFFFF << (byteShiftIndex * 8));
	tempVal &= mask;

	// Merge contents
	tempWord |= tempVal;

	// Write word to memory
	R3051_writeDataValue(cpu, PHILPSX_R3051_WORD, tempAddress, tempWord);
}

/*
 * This function handles the SWR R3051 instruction.
 */
static void R3051_SWR(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Calculate address
	int64_t address = R3051_readReg(cpu, rs) & 0xFFFFFFFFL;
	if ((immediate & 0x8000) == 0x8000) {
		immediate |= 0xFFFF0000;
	}
	address += immediate;

	// Check if address is allowed, trigger exception if not
	if (!Cop0_isAddressAllowed(cpu->sccp, (int32_t)address)) {
		int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
		tempAddress -= 4;
		cpu->exception->badAddress = (int32_t)address;
		cpu->exception->exceptionReason = PHILPSX_EXCEPTION_ADES;
		cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
		cpu->exception->programCounterOrigin
				= cpu->exception->isInBranchDelaySlot ?
					(int32_t)tempAddress : cpu->programCounter;
		return;
	}

	// Align address, fetch word, and store shift index - sort byte order too
	int32_t tempAddress = (int32_t)(address & 0xFFFFFFFC);
	int32_t byteShiftIndex = (int32_t)(address & 0x3);
	int32_t tempWord = R3051_readReg(cpu, rt);

	// Swap byte order
	tempWord = R3051_swapWordEndianness(cpu, tempWord);

	// Shift word value right by required amount
	tempWord = logical_rshift(tempWord, (byteShiftIndex * 8));

	// Fetch rt contents, and calculate mask
	int32_t tempVal = SystemInterlink_readWord(cpu->system, tempAddress);
	int32_t mask = ~logical_rshift(0xFFFFFFFF, (byteShiftIndex * 8));
	tempVal &= mask;

	// Merge contents
	tempWord |= tempVal;

	// Write word to main memory
	R3051_writeDataValue(cpu, PHILPSX_R3051_WORD, tempAddress, tempWord);
}

/*
 * This function handles the SYSCALL R3051 instruction.
 */
static void R3051_SYSCALL(R3051 *cpu, int32_t instruction)
{
	// Trigger System Call Exception
	int64_t tempAddress = cpu->programCounter & 0xFFFFFFFFL;
	tempAddress -= 4;
	cpu->exception->exceptionReason = PHILPSX_EXCEPTION_SYS;
	cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
	cpu->exception->programCounterOrigin
			= cpu->exception->isInBranchDelaySlot ?
				(int32_t)tempAddress : cpu->programCounter;
}

/*
 * This function handles the XOR R3051 instruction.
 */
static void R3051_XOR(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and rd
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;
	int32_t rd = logical_rshift(instruction, 11) & 0x1F;

	// Bitwise XOR rsVal and rtVal, storing result
	R3051_writeReg(
			cpu,
			rd,
			(R3051_readReg(cpu, rs) ^ R3051_readReg(cpu, rt)),
			false
			);
}

/*
 * This function handles the XORI R3051 instruction.
 */
static void R3051_XORI(R3051 *cpu, int32_t instruction)
{
	// Get rs, rt and immediate
	int32_t immediate = instruction & 0xFFFF;
	int32_t rs = logical_rshift(instruction, 21) & 0x1F;
	int32_t rt = logical_rshift(instruction, 16) & 0x1F;

	// Zero extending immediate is already done for us
	// so just XOR with rsVal and store result
	R3051_writeReg(cpu, rt, (immediate ^ R3051_readReg(cpu, rs)), false);
}

/*
 * This function executes an opcode in interpretive mode.
 */
static void R3051_executeOpcode(R3051 *cpu, int32_t instruction,
		int32_t tempBranchAddress)
{
	// Deal with opcode
	int32_t opcode = logical_rshift(instruction, 26);
	switch (opcode) {
		case 0: // SPECIAL
		{
			int32_t specialVal = instruction & 0x3F;
			switch (specialVal) {
				case 0:
					// SLL
					R3051_SLL(cpu, instruction);
					break;
				case 2:
					// SRL
					R3051_SRL(cpu, instruction);
					break;
				case 3:
					// SRA
					R3051_SRA(cpu, instruction);
					break;
				case 4:
					// SLLV
					R3051_SLLV(cpu, instruction);
					break;
				case 6:
					// SRLV
					R3051_SRLV(cpu, instruction);
					break;
				case 7:
					// SRAV
					R3051_SRAV(cpu, instruction);
					break;
				case 8:
					// JR
					R3051_JR(cpu, instruction);
					break;
				case 9:
					// JALR
					R3051_JALR(cpu, instruction);
					break;
				case 12:
					// SYSCALL
					R3051_SYSCALL(cpu, instruction);
					break;
				case 13:
					// BREAK
					R3051_BREAK(cpu, instruction);
					break;
				case 16:
					// MFHI
					R3051_MFHI(cpu, instruction);
					break;
				case 17:
					// MTHI
					R3051_MTHI(cpu, instruction);
					break;
				case 18:
					// MFLO
					R3051_MFLO(cpu, instruction);
					break;
				case 19:
					// MTLO
					R3051_MTLO(cpu, instruction);
					break;
				case 24:
					// MULT
					R3051_MULT(cpu, instruction);
					break;
				case 25:
					// MULTU
					R3051_MULTU(cpu, instruction);
					break;
				case 26:
					// DIV
					R3051_DIV(cpu, instruction);
					break;
				case 27:
					// DIVU
					R3051_DIVU(cpu, instruction);
					break;
				case 32:
					// ADD
					R3051_ADD(cpu, instruction);
					break;
				case 33:
					// ADDU
					R3051_ADDU(cpu, instruction);
					break;
				case 34:
					// SUB
					R3051_SUB(cpu, instruction);
					break;
				case 35:
					// SUBU
					R3051_SUBU(cpu, instruction);
					break;
				case 36:
					// AND
					R3051_AND(cpu, instruction);
					break;
				case 37:
					// OR
					R3051_OR(cpu, instruction);
					break;
				case 38:
					// XOR
					R3051_XOR(cpu, instruction);
					break;
				case 39:
					// NOR
					R3051_NOR(cpu, instruction);
					break;
				case 42:
					// SLT
					R3051_SLT(cpu, instruction);
					break;
				case 43:
					// SLTU
					R3051_SLTU(cpu, instruction);
					break;
				default:
					// Unrecognised - trigger Reserved Instruction Exception
					cpu->exception->exceptionReason = PHILPSX_EXCEPTION_RI;
					cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
					cpu->exception->programCounterOrigin
							= cpu->exception->isInBranchDelaySlot ?
								tempBranchAddress : cpu->programCounter;
					break;
			}
		}
		break;
		case 1: // BCOND
		{
			int32_t bcondVal = logical_rshift(instruction, 16) & 0x1F;
			switch (bcondVal) {
				case 0:
					// BLTZ
					R3051_BLTZ(cpu, instruction);
					break;
				case 1:
					// BGEZ
					R3051_BGEZ(cpu, instruction);
					break;
				case 16:
					// BLTZAL
					R3051_BLTZAL(cpu, instruction);
					break;
				case 17:
					// BGEZAL
					R3051_BGEZAL(cpu, instruction);
					break;
			}
		}
		break;
		case 2:
			// J
			R3051_J(cpu, instruction);
			break;
		case 3:
			// JAL
			R3051_JAL(cpu, instruction);
			break;
		case 4:
			// BEQ
			R3051_BEQ(cpu, instruction);
			break;
		case 5:
			// BNE
			R3051_BNE(cpu, instruction);
			break;
		case 6:
			// BLEZ
			R3051_BLEZ(cpu, instruction);
			break;
		case 7:
			// BGTZ
			R3051_BGTZ(cpu, instruction);
			break;
		case 8:
			// ADDI
			R3051_ADDI(cpu, instruction);
			break;
		case 9:
			// ADDIU
			R3051_ADDIU(cpu, instruction);
			break;
		case 10:
			// SLTI
			R3051_SLTI(cpu, instruction);
			break;
		case 11:
			// SLTIU
			R3051_SLTIU(cpu, instruction);
			break;
		case 12:
			// ANDI
			R3051_ANDI(cpu, instruction);
			break;
		case 13:
			// ORI
			R3051_ORI(cpu, instruction);
			break;
		case 14:
			// XORI
			R3051_XORI(cpu, instruction);
			break;
		case 15:
			// LUI
			R3051_LUI(cpu, instruction);
			break;
		case 16: // COP0
			if (!Cop0_isCoProcessorUsable(cpu->sccp, 0) &&
					!Cop0_areWeInKernelMode(cpu->sccp)) {
				// COP0 unusable and we are not in kernel mode -
				// throw exception
				cpu->exception->coProcessorNum = 0;
				cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
				cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
				cpu->exception->programCounterOrigin
						= cpu->exception->isInBranchDelaySlot ?
							tempBranchAddress : cpu->programCounter;
				break;
			}
		{
			int32_t rfe = instruction & 0x1F;
			switch (rfe) {
				case 16:
					// RFE
					R3051_RFE(cpu, instruction);
					break;
				default:
				{

					int32_t cop0Val = logical_rshift(instruction, 21) & 0x1F;
					switch (cop0Val) {
						case 0:
							// MF
							R3051_MF0(cpu, instruction);
							break;
						case 4:
							// MT
							R3051_MT0(cpu, instruction);
							break;
						default:
							// Throw reserved instruction exception
							cpu->exception->exceptionReason =
									PHILPSX_EXCEPTION_RI;
							cpu->exception->isInBranchDelaySlot =
									cpu->prevWasBranch;
							cpu->exception->programCounterOrigin
									= cpu->exception->isInBranchDelaySlot ?
									tempBranchAddress : cpu->programCounter;
							break;
					}
				}
				break;
			}
		}
		break;
		case 17: // COP1
			// COP1 unusable - throw exception
			cpu->exception->coProcessorNum = 1;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 18: // COP2
			if (!Cop0_isCoProcessorUsable(cpu->sccp, 2)) {
				// COP2 unusable - throw exception
				cpu->exception->coProcessorNum = 2;
				cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
				cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
				cpu->exception->programCounterOrigin
						= cpu->exception->isInBranchDelaySlot ?
							tempBranchAddress : cpu->programCounter;
				break;
			}
		{
			int32_t cop2Val = logical_rshift(instruction, 21) & 0x1F;
			switch (cop2Val) {
				case 0:
					// MF
					R3051_MF2(cpu, instruction);
					break;
				case 2:
					// CF
					R3051_CF2(cpu, instruction);
					break;
				case 4:
					// MT
					R3051_MT2(cpu, instruction);
					break;
				case 6:
					// CT
					R3051_CT2(cpu, instruction);
					break;
				case 8: // BC
				{
					int32_t cop2Val_extra =
							logical_rshift(instruction, 16) & 0x1F;
					switch (cop2Val_extra) {
						case 0:
							// BC2F
							R3051_BC2F(cpu, instruction);
							break;
						case 1:
							// BC2T
							R3051_BC2T(cpu, instruction);
							break;
					}
				}
				break;
				case 16:
				case 17:
				case 18:
				case 19:
				case 20:
				case 21:
				case 22:
				case 23:
				case 24:
				case 25:
				case 26:
				case 27:
				case 28:
				case 29:
				case 30:
				case 31:
					// Co-processor specific
					Cop2_gteFunction(cpu->gte, instruction);
					break;
			}
		}
		break;
		case 19: // COP3
			// COP3 unusable - throw exception
			cpu->exception->coProcessorNum = 3;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 32:
			// LB
			R3051_LB(cpu, instruction);
			break;
		case 33:
			// LH
			R3051_LH(cpu, instruction);
			break;
		case 34:
			// LWL
			R3051_LWL(cpu, instruction);
			break;
		case 35:
			// LW
			R3051_LW(cpu, instruction);
			break;
		case 36:
			// LBU
			R3051_LBU(cpu, instruction);
			break;
		case 37:
			// LHU
			R3051_LHU(cpu, instruction);
			break;
		case 38:
			// LWR
			R3051_LWR(cpu, instruction);
			break;
		case 40:
			// SB
			R3051_SB(cpu, instruction);
			break;
		case 41:
			// SH
			R3051_SH(cpu, instruction);
			break;
		case 42:
			// SWL
			R3051_SWL(cpu, instruction);
			break;
		case 43:
			// SW
			R3051_SW(cpu, instruction);
			break;
		case 46:
			// SWR
			R3051_SWR(cpu, instruction);
			break;
		case 49:
			// LWC1 (COP1 doesn't exist so throw exception)
			cpu->exception->coProcessorNum = 1;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 50:
			// LWC2
			R3051_LWC2(cpu, instruction);
			break;
		case 51:
			// LWC3 (COP3 doesn't exist so throw exception)
			cpu->exception->coProcessorNum = 3;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 48:
		case 56:
			// LWC0 and SWC0
			cpu->exception->coProcessorNum = 0;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 57:
			// SWC1 (COP1 doesn't exist so throw exception)
			cpu->exception->coProcessorNum = 1;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		case 58:
			// SWC2
			R3051_SWC2(cpu, instruction);
			break;
		case 59:
			// SWC3 (COP3 doesn't exist so throw exception)
			cpu->exception->coProcessorNum = 3;
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_CPU;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
		default:
			// Unrecognised - trigger Reserved Instruction Exception
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_RI;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			cpu->exception->programCounterOrigin
					= cpu->exception->isInBranchDelaySlot ?
						tempBranchAddress : cpu->programCounter;
			break;
	}
}

/*
 * This function can deal with an exception, making sure the right things
 * are done.
 */
static bool R3051_handleException(R3051 *cpu)
{
	// Exit if exception is NULL
	if (cpu->exception->exceptionReason == PHILPSX_EXCEPTION_NULL)
		return false;

	// Wipe out pending jump and branch delay info
	cpu->jumpPending = false;
	cpu->prevWasBranch = false;

	// Define temp variable
	int32_t temp = 0;

	// Fetch cause register and status register from Cop0
	int32_t tempCause = Cop0_readReg(cpu->sccp, 13);
	int32_t tempStatus = Cop0_readReg(cpu->sccp, 12);

	// Sheck the exception code and act accordingly
	switch (cpu->exception->exceptionReason) {
		// Handle address error exception (load operation)
		case PHILPSX_EXCEPTION_ADEL:
		// Handle address error exception (store operation)
		case PHILPSX_EXCEPTION_ADES:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Set bad virtual address register
			Cop0_writeReg(cpu->sccp, 8, cpu->exception->badAddress, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_BP:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_DBE:
		case PHILPSX_EXCEPTION_IBE:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_CPU:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set relevant bit of CE field in cause register
			tempCause = (tempCause & 0xCFFFFFFF) |
					(cpu->exception->coProcessorNum << 28);

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_INT:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_OVF:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_RI:

			// Mask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_SYS:

			// Mmmmask out ExcCode and replace
			tempCause = (tempCause & 0xFFFFFF83) |
					(cpu->exception->exceptionReason << 2);

			// Set BD bit of cause register if necessary
			if (cpu->exception->isInBranchDelaySlot)
				tempCause |= 0x80000000;
			else
				tempCause &= 0x7FFFFFFF;

			// Set EPC register
			Cop0_writeReg(cpu->sccp, 14,
					cpu->exception->programCounterOrigin, true);

			// Save KUp and IEp to KUo and IEo, KUc and IEc to KUp and IEp,
			// and reset KUc and IEc to 0
			temp = (tempStatus & 0x0000000F) << 2;
			tempStatus &= 0xFFFFFFC0;
			tempStatus |= temp;

			// Set PC to general exception vector and finish processing
			cpu->programCounter = Cop0_getGeneralExceptionVector(cpu->sccp);
			break;

		case PHILPSX_EXCEPTION_RESET:

			// Reset and exit method early
			R3051_reset(cpu);
			Cop0_reset(cpu->sccp);
			return true;

	}

	// Write cause and status registers back to Cop0
	Cop0_writeReg(cpu->sccp, 13, tempCause, true);
	Cop0_writeReg(cpu->sccp, 12, tempStatus, true);

	// Reset exception object
	MIPSException_reset(cpu->exception);

	// Signal that an exception has been processed
	return true;
}

/*
 * This function is for handling interrupts from within the execution loop.
 */
static bool R3051_handleInterrupts(R3051 *cpu)
{
	// Resync system timers
	SystemInterlink_resyncAllTimers(cpu->system);

	// Resync GPU
	SystemInterlink_executeGPUCycles(cpu->system);

	// Increment all interrupt counters
	SystemInterlink_incrementInterruptCounters(cpu->system);

	// Get interrupt status and mask registers
	int32_t interruptStatus = R3051_swapWordEndianness(
			cpu,
			SystemInterlink_readWord(cpu->system, 0x1F801070)
			) & 0x7FF;
	int32_t interruptMask = R3051_swapWordEndianness(
			cpu,
			SystemInterlink_readWord(cpu->system, 0x1F801074)
			) & 0x7FF;

	// Mask the interrupt status register
	interruptStatus &= interruptMask;

	// Set bit 10 of COP0 cause register if needed
	if (interruptStatus != 0) {
		int32_t causeRegister = Cop0_readReg(cpu->sccp, 13);
		causeRegister |= 0x400;
		Cop0_writeReg(cpu->sccp, 13, causeRegister, true);
	} else {
		int32_t causeRegister = Cop0_readReg(cpu->sccp, 13);
		causeRegister &= 0xFFFFFBFF;
		Cop0_writeReg(cpu->sccp, 13, causeRegister, true);
	}

	// Get status reg and cause reg from COP0
	int32_t statusRegister = Cop0_readReg(cpu->sccp, 12);
	int32_t causeRegister = Cop0_readReg(cpu->sccp, 13);

	// Check if interrupts are enabled, if not then do nothing
	if ((statusRegister & 0x1) == 0x1) {

		// Use IM mask from status register to mask interrupt bits
		// in cause register
		statusRegister &= 0x0000FF00;
		causeRegister &= 0x0000FF00;
		causeRegister &= statusRegister;

		// If resulting value is non-zero, trigger interrupt
		if (causeRegister != 0) {
			cpu->exception->exceptionReason = PHILPSX_EXCEPTION_INT;
			cpu->exception->programCounterOrigin = cpu->programCounter;
			cpu->exception->isInBranchDelaySlot = cpu->prevWasBranch;
			R3051_handleException(cpu);

			// Signal that interrupt occurred and was handled
			// by exception routine
			return true;
		}
	}

	// Signal that no interrupt occurred
	return false;
}

/*
 * This constructs a MIPSException object.
 */
static MIPSException *construct_MIPSException(void)
{
	// Allocate MIPSException struct
	MIPSException *exception = calloc(1, sizeof(MIPSException));
	if (!exception) {
		fprintf(stderr, "PhilPSX: R3051: MIPSException: Couldn't allocate "
				"memory for R3051 struct\n");
		goto end;
	}

	// Set default exception reason
	exception->exceptionReason = PHILPSX_EXCEPTION_NULL;

	end:
	return exception;
}

/*
 * This destructs a MIPSException object.
 */
static void destruct_MIPSException(MIPSException *exception)
{
	free(exception);
}

/*
 * This function resets an exception to its initial empty state.
 */
static void MIPSException_reset(MIPSException *exception)
{
	exception->exceptionReason = PHILPSX_EXCEPTION_NULL;
	exception->programCounterOrigin = 0;
	exception->badAddress = 0;
	exception->coProcessorNum = 0;
	exception->isInBranchDelaySlot = false;
}

/*
 * This constructs an instruction cache object.
 */
static Cache *construct_Cache(int32_t cacheType)
{
	// Allocate Cache struct
	Cache *cache = malloc(sizeof(Cache));
	if (!cache) {
		fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
				"memory for Cache struct\n");
		goto end;
	}

	// Setup appropriate cache type
	switch (cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			cache->cacheData = calloc(4096, sizeof(int8_t));
			if (!cache->cacheData) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheData array\n");
				goto cleanup_cache;
			}
			
			cache->cacheTag = calloc(256, sizeof(int32_t));
			if (!cache->cacheTag) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheTag array\n");
				goto cleanup_cachedata;
			}
			
			cache->cacheValid = calloc(256, sizeof(bool));
			if (!cache->cacheValid) {
				fprintf(stderr, "PhilPSX: R3051: Cache: Couldn't allocate "
						"memory for cacheValid array\n");
				goto cleanup_cachetag;
			}
			
			cache->cacheType = cacheType;
		}
		break;
	}

	// Normal return:
	return cache;

	// Cleanup path:
	cleanup_cachetag:
	free(cache->cacheTag);
	
	cleanup_cachedata:
	free(cache->cacheData);

	cleanup_cache:
	free(cache);
	cache = NULL;
	
	end:
	return cache;
}

/*
 * This destructs an instruction cache object.
 */
static void destruct_Cache(Cache *cache)
{
	free(cache->cacheValid);
	free(cache->cacheTag);
	free(cache->cacheData);
	free(cache);
}

/*
 * This function checks for a cache hit. The address provided must be
 * physical and not virtual.
 */
static bool Cache_checkForHit(Cache *cache, int32_t address)
{
	// Define return value
	bool retVal = false;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
			int32_t expectedTag = logical_rshift(address, 12) & 0xFFFFF;
			if (cache->cacheTag[tagIndex] ==
					expectedTag && cache->cacheValid[tagIndex])
				retVal = true;
		} break;
	}

	// Return hit status
	return retVal;
}

/*
 * This function retrieves the word from the correct address.
 */
static int32_t Cache_readWord(Cache *cache, int32_t address)
{
	// Define word value
	int32_t wordVal = 0;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			int32_t dataIndex = address & 0xFFC;
			wordVal = (cache->cacheData[dataIndex] & 0xFF) << 24;
			++dataIndex;
			wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 16;
			++dataIndex;
			wordVal |= (cache->cacheData[dataIndex] & 0xFF) << 8;
			++dataIndex;
			wordVal |= cache->cacheData[dataIndex] & 0xFF;
		} break;
	}

	// Return word value
	return wordVal;
}

/*
 * This function retrieves a byte from the correct address.
 */
static int8_t Cache_readByte(Cache *cache, int32_t address)
{
	// Define byte value
	int8_t byteVal = 0;

	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Get address of byte
			int32_t dataIndex = address & 0xFFF;

			// Retrieve byte
			byteVal = cache->cacheData[dataIndex];
		} break;
	}

	// Return work value
	return byteVal;
}

/*
 * This function writes the word to the correct address.
 */
static void Cache_writeWord(Cache *cache, Cop0 *sccp, int32_t address,
		int32_t value)
{
	// Act depending on type of cache
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Update correct word
			int32_t dataIndex = address & 0xFFC;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 24);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 16);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)logical_rshift(value, 8);
			++dataIndex;
			cache->cacheData[dataIndex] = (int8_t)value;

			// Invalidate line if cache is isolated
			if (Cop0_isDataCacheIsolated(sccp)) {
				int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
				int32_t tag = logical_rshift(address, 12) & 0xFFFFF;
				cache->cacheTag[tagIndex] = tag;
				cache->cacheValid[tagIndex] = false;
			}
		} break;
	}
}

/*
 * This function writes the byte to the correct address, and invalidates the
 * correct cache line.
 */
static void Cache_writeByte(Cache *cache, Cop0 *sccp, int32_t address,
		int8_t value)
{
	// Act depending on type of cache
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Update correct byte
			int32_t dataIndex = address & 0xFFF;

			// Write byte and invalidate cache line
			cache->cacheData[dataIndex] = value;

			// Invalidate line if cache is isolated
			if (Cop0_isDataCacheIsolated(sccp)) {
				int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
				int32_t tag = logical_rshift(address, 12) & 0xFFFFF;
				cache->cacheTag[tagIndex] = tag;
				cache->cacheValid[tagIndex] = false;
			}
		} break;
	}
}

/*
 * This function refills a cache line using an address.
 */
static void Cache_refillLine(Cache *cache, Cop0 *sccp, SystemInterlink *system,
		int32_t address)
{
	// Act according to cache type
	switch (cache->cacheType) {
		case PHILPSX_INSTRUCTION_CACHE:
		{
			// Refill a line - four words
			// Check if cache is isolated first
			if (Cop0_isDataCacheIsolated(sccp))
				return;

			// Calculate tag index and tag
			int32_t tagIndex = logical_rshift(address, 4) & 0xFF;
			int32_t tag = logical_rshift(address, 12) & 0xFFFFF;

			// Write tag and valid flag
			cache->cacheTag[tagIndex] = tag;
			cache->cacheValid[tagIndex] = true;

			// Refill cache line
			int64_t startingAddress = address & 0xFFFFFFF0L;
			for (int32_t i = 0; i < 16; ++i) {
				cache->cacheData[(int32_t)(startingAddress & 0xFFF)] =
						SystemInterlink_readByte(system,
										(int32_t)startingAddress);
				++startingAddress;
			}
		} break;
	}
}