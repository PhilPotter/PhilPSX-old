/*
 * This header file provides the public API for the CD class, which models and
 * abstracts handling of Compact Discs for the emulator.
 * 
 * CD.h - Copyright Phillip Potter, 2020
 */
#ifndef PHILPSX_CD_HEADER
#define PHILPSX_CD_HEADER

// System includes
#include <stdbool.h>
#include <stdint.h>

// Typedefs
typedef struct CD CD;

// Public functions
CD *construct_CD(void);
void destruct_CD(CD *cd);
bool CD_isEmpty(CD *cd);
bool CD_loadCD(CD *cd, const char *cdPath);
int8_t CD_readByte(CD *cd, int64_t position);

#endif
