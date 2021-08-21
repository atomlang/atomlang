/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

#ifndef UTILS_H
#define UTILS_H

// includes
#include "atml_internal.h"

int utilPowerOf2Ceil(int n);

bool utilIsName(char c);

bool utilIsDigit(char c);

uint64_t utilDoubleToBits(double value);

double utilDoubleFromBits(uint64_t value);

uint32_t utilHashBits(uint64_t hash);

uint32_t utilHashNumber(double num);

uint32_t utilHashString(const char* string);

#endif 

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

int utf8_encodeBytesCount(int value);

int utf8_decodeBytesCount(uint8_t byte);

int utf8_encodeValue(int value, uint8_t* bytes);

int utf8_decodeBytes(uint8_t* bytes, int* value);

#endif 