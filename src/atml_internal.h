#ifndef ATML_INTERNAL
#define ATML_INTERNAL

// includes
#include "include/pocketlang.h"
#include "atml_common.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#define DEBUG_DUMP_COMPILED_CODE 0

#define DEBUG_DUMP_CALL_STACK 0

#define VAR_NAN_TAGGING 1

#define MAX_ARGC 32

#define GROW_FACTOR 2

#define MIN_CAPACITY 8

#define ERROR_MESSAGE_SIZE 512

#define ALLOCATE(vm, type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type)))

#define ALLOCATE_DYNAMIC(vm, type, count, tail_type) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) + sizeof(tail_type) * (count)))

#define ALLOCATE_ARRAY(vm, type, count) \
    ((type*)vmRealloc(vm, NULL, 0, sizeof(type) * (count)))

#define DEALLOCATE(vm, pointer) \
    vmRealloc(vm, pointer, 0, 0)


#define CHECK_HASH(name, hash) hash

#endif
