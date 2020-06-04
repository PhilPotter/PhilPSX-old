/*
 * This header file contains math utility functions for things like logical
 * right shift that aren't available out-of-the-box in C.
 * 
 * math_utils.h - Copyright Phillip Potter, 2020, under GPLv3
 */
#ifndef PHILPSX_MATH_UTILS_HEADER
#define PHILPSX_MATH_UTILS_HEADER

// System includes
#include <stdint.h>

// Utility functions
static int32_t logical_rshift_32(int32_t value, int32_t shiftBy)
{
	return ((uint32_t)value >> shiftBy);
}

static int64_t logical_rshift_64(int64_t value, int32_t shiftBy)
{
	return ((uint64_t)value >> shiftBy);
}

static int32_t unsigned_logical_rshift_32(uint32_t value, int32_t shiftBy)
{
	return (int32_t)(value >> shiftBy);
}

static int64_t unsigned_logical_rshift_64(uint64_t value, int32_t shiftBy)
{
	return (int64_t)(value >> shiftBy);
}

#define logical_rshift(x, y) _Generic((x), \
		int32_t:logical_rshift_32, \
		int64_t:logical_rshift_64, \
		uint32_t:unsigned_logical_rshift_32, \
		uint64_t:unsigned_logical_rshift_64 \
		)(x, y)

static int32_t min_value_32(int32_t left, int32_t right)
{
	return left <= right ? left : right;
}

static int64_t min_value_64(int64_t left, int64_t right)
{
	return left <= right ? left : right;
}

#define min_value(x, y) _Generic((x)+(y), \
		int32_t:min_value_32, \
		int64_t:min_value_64 \
		)(x, y)

#endif