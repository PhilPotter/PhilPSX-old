/*
 * This C file models the CD of the PlayStation as a class.
 * 
 * CD.c - Copyright Phillip Potter, 2018
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../headers/CD.h"

// List of track types
#define PHILPSX_TRACKTYPE_AUDIO 0
#define PHILPSX_TRACKTYPE_MODE2_2352 1

// Forward declarations for functions and subcomponents private to this class
// CDTrack-related stuff:
typedef struct CDTrack CDTrack;
static CDTrack *construct_CDTrack(void);
static void destruct_CDTrack(CDTrack *cdt);
static bool CDTrack_isWithin(int64_t position);

/*
 * This struct models a CD itself, and abstracts image type away from the
 * emulator, allowing different image types to be supported.
 */