/*
 * This header file provides the public API for the CD-ROM module of the
 * PlayStation.
 * 
 * CDROMDrive.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_CDROMDRIVE_HEADER
#define PHILPSX_CDROMDRIVE_HEADER

// Includes
#include <stdbool.h>
#include <stdint.h>
#include "SystemInterlink.h"

// Typedefs
typedef struct CDROMDrive CDROMDrive;

// Public functions
CDROMDrive *construct_CDROMDrive(void);
void destruct_CDROMDrive(CDROMDrive *cdrom);
void CDROMDrive_chunkCopy(CDROMDrive *cdrom, int8_t *destination,
		int32_t startIndex, int32_t length);
bool CDROMDrive_loadCD(CDROMDrive *cdrom, const char *cdPath);
int8_t CDROMDrive_read1800(CDROMDrive *cdrom);
int8_t CDROMDrive_read1801(CDROMDrive *cdrom);
int8_t CDROMDrive_read1802(CDROMDrive *cdrom);
int8_t CDROMDrive_read1803(CDROMDrive *cdrom);
void CDROMDrive_setInterruptNumber(CDROMDrive *cdrom, int32_t interruptNum);
void CDROMDrive_setMemoryInterface(CDROMDrive *cdrom, SystemInterlink *smi);
void CDROMDrive_write1800(CDROMDrive *cdrom, int8_t value);
void CDROMDrive_write1801(CDROMDrive *cdrom, int8_t value);
void CDROMDrive_write1802(CDROMDrive *cdrom, int8_t value);
void CDROMDrive_write1803(CDROMDrive *cdrom, int8_t value);

#endif