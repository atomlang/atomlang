#ifndef BUFFERS_TEMPLATE_H
#define BUFFERS_TEMPLATE_H

// includes
#include "atml_internal.h"

#define DECLARE_BUFFER(m_name, m_type)                                        \
  typedef struct {                                                            \
    m_type* data;                                                             \
    uint32_t count;                                                           \
    uint32_t capacity;                                                        \
  } pk##m_name##Buffer;                                                       \
                                                                              \
                                                                              \
  void pk##m_name##BufferInit(pk##m_name##Buffer* self);                      \
                                                                              \
                                                                              \
  void pk##m_name##BufferClear(pk##m_name##Buffer* self, PKVM* vm);           \
                                                                              \
                                                                              \
  void pk##m_name##BufferReserve(pk##m_name##Buffer* self, PKVM* vm,          \
                                 size_t size);                                \
                                                                              \
                                                                              \
                                                                              \
  void pk##m_name##BufferFill(pk##m_name##Buffer* self, PKVM* vm,             \
                              m_type data, int count);                        \
                                                                              \
                                                                              \
  void pk##m_name##BufferWrite(pk##m_name##Buffer* self,                      \
                               PKVM* vm, m_type data);                        \
                                                                              \
                                                                              \
  void pk##m_name##BufferConcat(pk##m_name##Buffer* self, PKVM* vm,           \
                                pk##m_name##Buffer* other);                   \

#define DEFINE_BUFFER(m_name, m_type)                                         \
  void pk##m_name##BufferInit(pk##m_name##Buffer* self) {                     \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferClear(pk##m_name##Buffer* self,                      \
              PKVM* vm) {                                                     \
    vmRealloc(vm, self->data, self->capacity * sizeof(m_type), 0);            \
    self->data = NULL;                                                        \
    self->count = 0;                                                          \
    self->capacity = 0;                                                       \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferReserve(pk##m_name##Buffer* self, PKVM* vm,          \
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
  void pk##m_name##BufferFill(pk##m_name##Buffer* self, PKVM* vm,             \
                              m_type data, int count) {                       \
                                                                              \
    pk##m_name##BufferReserve(self, vm, self->count + count);                 \
                                                                              \
    for (int i = 0; i < count; i++) {                                         \
      self->data[self->count++] = data;                                       \
    }                                                                         \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferWrite(pk##m_name##Buffer* self,                      \
                               PKVM* vm, m_type data) {                       \
    pk##m_name##BufferFill(self, vm, data, 1);                                \
  }                                                                           \
                                                                              \
  void pk##m_name##BufferConcat(pk##m_name##Buffer* self, PKVM* vm,           \
                                pk##m_name##Buffer* other) {                  \
    pk##m_name##BufferReserve(self, vm, self->count + other->count);          \
                                                                              \
    memcpy(self->data + self->count,                                          \
           other->data,                                                       \
           other->count * sizeof(m_type));                                    \
    self->count += other->count;                                              \
  }

#endif
