/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#ifndef VM_H
#define VM_H

// includes
#include "atml_compiler.h"
#include "atml_internal.h"
#include "atml_var.h"

#define MAX_TEMP_REFERENCE 16

#define BUILTIN_FN_CAPACITY 50

// Initially allocated call frame capacity. Will grow dynamically.
#define INITIAL_CALL_FRAMES 4

// The minimum size of the stack that will be initialized for a fiber before
// running one.
#define MIN_STACK_SIZE 128

// The allocated size that will trigger the first GC. (~10MB).
#define INITIAL_GC_SIZE (1024 * 1024 * 10)

// The heap size might shrink if the remaining allocated bytes after a GC
// is less than the one before the last GC. So we need a minimum size.
#define MIN_HEAP_SIZE (1024 * 1024)

// The heap size for the next GC will be calculated as the bytes we have
// allocated so far plus the fill factor of it.
#define HEAP_FILL_PERCENT 75

// Evaluated to "true" if a runtime error set on the current fiber.
#define VM_HAS_ERROR(vm) (vm->fiber->error != NULL)

// Set the error message [err] to the [vm]'s current fiber.
#define VM_SET_ERROR(vm, err)        \
  do {                               \
    ASSERT(!VM_HAS_ERROR(vm), OOPS); \
    (vm->fiber->error = err);        \
  } while (false)

// Builtin functions are stored in an array in the VM (unlike script functions
// they're member of function buffer of the script) and this struct is a single
// entry of the array.
typedef struct {
  const char* name; //< Name of the function.
  uint32_t length;  //< Length of the name.
  Function* fn;     //< Native function pointer.
} BuiltinFn;

// A doubly link list of vars that have reference in the host application.
// Handles are wrapper around Var that lives on the host application.
struct ATMLHandle {
  Var value;

  ATMLHandle* prev;
  ATMLHandle* next;
};

struct ATMLVM {

  // The first object in the link list of all heap allocated objects.
  Object* first;

  // The number of bytes allocated by the vm and not (yet) garbage collected.
  size_t bytes_allocated;

  // The number of bytes that'll trigger the next GC.
  size_t next_gc;

  // Minimum size the heap could get.
  size_t min_heap_size;

  int heap_fill_percent;

  Object** working_set;
  int working_set_count;
  int working_set_capacity;

  // A stack of temporary object references to ensure that the object
  // doesn't garbage collected.
  Object* temp_reference[MAX_TEMP_REFERENCE];
  int temp_reference_count;

  ATMLHandle* handles;

  // VM's configurations.
  ATMLConfiguration config;

  Compiler* compiler;

  // A cache of the compiled scripts with their path as key and the Scrpit
  // object as the value.
  Map* scripts;

  // A map of core libraries with their name as the key and the Script object
  // as the value.
  Map* core_libs;

  // Array of all builtin functions.
  BuiltinFn builtins[BUILTIN_FN_CAPACITY];
  uint32_t builtins_count;

  // Current fiber.
  Fiber* fiber;
};

void* vmRealloc(ATMLVM* vm, void* memory, size_t old_size, size_t new_size);

// Create and return a new handle for the [value].
ATMLHandle* vmNewHandle(ATMLVM* vm, Var value);

void vmCollectGarbage(ATMLVM* vm);

void vmPushTempRef(ATMLVM* vm, Object* obj);

void vmPopTempRef(ATMLVM* vm);

Script* vmGetScript(ATMLVM* vm, String* path);

bool vmPrepareFiber(ATMLVM* vm, Fiber* fiber, int argc, Var** argv);

bool vmSwitchFiber(ATMLVM* vm, Fiber* fiber, Var* value);

void vmYieldFiber(ATMLVM* vm, Var* value);

#endif 