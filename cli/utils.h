#pragma once

#include "internal.h"

typedef struct {
  uint8_t* data;
  uint32_t count;
  uint32_t capacity;
} ByteBuffer;

void byteBufferInit(ByteBuffer* buffer);

void byteBufferClear(ByteBuffer* buffer);

void byteBufferReserve(ByteBuffer* buffer, size_t size);

void byteBufferFill(ByteBuffer* buffer, uint8_t data, int count);

void byteBufferWrite(ByteBuffer* buffer, uint8_t data);

void byteBufferAddString(ByteBuffer* buffer, const char* str, uint32_t length);
