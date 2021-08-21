/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

#ifndef BUFFERS_TEMPLATE_H
#define BUFFERS_TEMPLATE_H

// includes
#include "atml_internal.h"

#define DECLARE_BUFFER(m_name, m_type)                                        \
  typedef struct {                                                            \
    m_type* data;                                                             \
    uint32_t count;                                                           \
    uint32_t capacity;                                                        \
  } ATML##m_name##Buffer;                                                       \
                                                                              \
                                                                              \
  void ATML##m_name##BufferInit(ATML##m_name##Buffer* self);                      \
                                                                              \
                                                                              \
  void ATML##m_name##BufferClear(ATML##m_name##Buffer* self, ATMLVM* vm);           \
                                                                              \
                                                                              \
  void ATML##m_name##BufferReserve(ATML##m_name##Buffer* self, ATMLVM* vm,          \
                                 size_t size);                                \
                                                                              \
                                                                              \
                                                                              \
  void ATML##m_name##BufferFill(ATML##m_name##Buffer* self, ATMLVM* vm,             \
                              m_type data, int count);                        \
                                                                              \
                                                                              \
  void ATML##m_name##BufferWrite(ATML##m_name##Buffer* self,                      \
                               ATMLVM* vm, m_type data);                        \
                                                                              \
                                                                              \
  void ATML##m_name##BufferConcat(ATML##m_name##Buffer* self, ATMLVM* vm,           \
                                ATML##m_name##Buffer* other);                   \

#define DEFINE_BUFFER(m_name, m_type)                                         \
  void ATML##m_name##BufferInit(ATML##m_name##Buffer* self) {                     \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void ATML##m_name##BufferClear(ATML##m_name##Buffer* self,                      \
              ATMLVM* vm) {                                                     \
    vmRealloc(vm, self->data, self->capacity * sizeof(m_type), 0);            \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void ATML##m_name##BufferReserve(ATML##m_name##Buffer* self, ATMLVM* vm,          \
                                 size_t size) {                               \
    if (self->capacity < size) {                                              \
      int capacity = utilPowerOf2Ceil((int)size);                             \
      if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;                   \
      self->data = (m_type*)vmRealloc(vm, self->data,                         \
        self->capacity * sizeof(m_type), capacity * sizeof(m_type));          \
      self->capacity = capacity;                                              \
    }                                                                         \
  }                                                                           \
                                                                              \
  void ATML##m_name##BufferFill(ATML##m_name##Buffer* self, ATMLVM* vm,             \
                              m_type data, int count) {                       \
                                                                              \
    ATML##m_name##BufferReserve(self, vm, self->count + count);                 \
                                                                              \
    for (int i = 0; i < count; i++) {                                         \
      self->data[self->count++] = data;                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  void ATML##m_name##BufferWrite(ATML##m_name##Buffer* self,                      \
                               ATMLVM* vm, m_type data) {                       \
    ATML##m_name##BufferFill(self, vm, data, 1);                                \
  }                                                                           \
                                                                              \
  void ATML##m_name##BufferConcat(ATML##m_name##Buffer* self, ATMLVM* vm,           \
                                ATML##m_name##Buffer* other) {                  \
    ATML##m_name##BufferReserve(self, vm, self->count + other->count);          \
                                                                              \
    memcpy(self->data + self->count,                                          \
           other->data,                                                       \
           other->count * sizeof(m_type));                                    \
    self->count += other->count;                                              \
  }

#endif
