#pragma once 

#include "pk_internal.h"

// Returns the smallest power of two that is equal to or greater than [n].
// Source : https://github.com/wren-lang/wren/blob/main/src/vm/wren_utils.h#L119
int utilPowerOf2Ceil(int n);

// Returns true if `c` is [A-Za-z_].
bool utilIsName(char c);

// Returns true if `c` is [0-9].
bool utilIsDigit(char c);

// Return Reinterpreted bits of the double value.
uint64_t utilDoubleToBits(double value);

// Reinterpret and return double value from bits.
double utilDoubleFromBits(uint64_t value);

// Copied from wren-lang.
uint32_t utilHashBits(uint64_t hash);

// Generates a hash code for [num].
uint32_t utilHashNumber(double num);

// Generate a has code for [string].
uint32_t utilHashString(const char* string);

#ifndef UTF8_H
#define UTF8_H


#include <stdint.h>

int utf8_encodeBytesCount(int value);

int utf8_decodeBytesCount(uint8_t byte);

int utf8_encodeValue(int value, uint8_t* bytes);

int utf8_decodeBytes(uint8_t* bytes, int* value);

#endif