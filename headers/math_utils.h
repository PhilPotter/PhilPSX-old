/*
 * This header file contains math utility functions for things like logical
 * right shift that aren't available out-of-the-box in C.
 * 
 * math_utils.h - Copyright Phillip Potter, 2018
 */
#ifndef PHILPSX_MATH_UTILS_HEADER
#define PHILPSX_MATH_UTILS_HEADER

// Includes
#include <stdint.h>

// Utility functions
static int32_t logical_rshift(int32_t value, int32_t shiftBy)
{
    return ((uint32_t)value >> shiftBy);
}

#endif