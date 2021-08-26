#ifndef __ATOMLANG_HASH__
#define __ATOMLANG_HASH__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "atomlang_value.h"

#define ATOMLANGHASH_ENABLE_STATS    1               
#define ATOMLANGHASH_DEFAULT_SIZE    32              
#define ATOMLANGHASH_THRESHOLD       0.75            
#define ATOMLANGHASH_MAXENTRIES      1073741824      

#ifndef ATOMLANG_HASH_DEFINED
#define ATOMLANG_HASH_DEFINED
typedef struct         atomlang_hash_t    atomlang_hash_t;    
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef bool        (*atomlang_hash_compare_fn) (atomlang_value_t value1, atomlang_value_t value2, void *data);
typedef uint32_t    (*atomlang_hash_compute_fn) (atomlang_value_t key);
typedef bool        (*atomlang_hash_isequal_fn) (atomlang_value_t v1, atomlang_value_t v2);
typedef void        (*atomlang_hash_iterate_fn) (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data);
typedef void        (*atomlang_hash_iterate2_fn) (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data1, void *data2);
typedef void        (*atomlang_hash_iterate3_fn) (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data1, void *data2, void *data3);
typedef void        (*atomlang_hash_transform_fn) (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t *value, void *data);

ATOMLANG_API atomlang_hash_t  *atomlang_hash_create (uint32_t size, atomlang_hash_compute_fn compute, atomlang_hash_isequal_fn isequal, atomlang_hash_iterate_fn free, void *data);
ATOMLANG_API void             atomlang_hash_free (atomlang_hash_t *hashtable);
ATOMLANG_API bool             atomlang_hash_insert (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value);
ATOMLANG_API bool             atomlang_hash_isempty (atomlang_hash_t *hashtable);
ATOMLANG_API atomlang_value_t *atomlang_hash_lookup (atomlang_hash_t *hashtable, atomlang_value_t key);
ATOMLANG_API atomlang_value_t *atomlang_hash_lookup_cstring (atomlang_hash_t *hashtable, const char *key);
ATOMLANG_API bool             atomlang_hash_remove  (atomlang_hash_t *hashtable, atomlang_value_t key);

ATOMLANG_API void             atomlang_hash_append (atomlang_hash_t *hashtable1, atomlang_hash_t *hashtable2);
ATOMLANG_API uint32_t         atomlang_hash_compute_buffer (const char *key, uint32_t len);
ATOMLANG_API uint32_t         atomlang_hash_compute_float (atomlang_float_t f);
ATOMLANG_API uint32_t         atomlang_hash_compute_int (atomlang_int_t n);
ATOMLANG_API uint32_t         atomlang_hash_count (atomlang_hash_t *hashtable);
ATOMLANG_API void             atomlang_hash_dump (atomlang_hash_t *hashtable);
ATOMLANG_API void             atomlang_hash_iterate (atomlang_hash_t *hashtable, atomlang_hash_iterate_fn iterate, void *data);
ATOMLANG_API void             atomlang_hash_iterate2 (atomlang_hash_t *hashtable, atomlang_hash_iterate2_fn iterate, void *data1, void *data2);
ATOMLANG_API void             atomlang_hash_iterate3 (atomlang_hash_t *hashtable, atomlang_hash_iterate3_fn iterate, void *data1, void *data2, void *data3);
ATOMLANG_API uint32_t         atomlang_hash_memsize (atomlang_hash_t *hashtable);
ATOMLANG_API void             atomlang_hash_resetfree (atomlang_hash_t *hashtable);
ATOMLANG_API void             atomlang_hash_stat (atomlang_hash_t *hashtable);
ATOMLANG_API void             atomlang_hash_transform (atomlang_hash_t *hashtable, atomlang_hash_transform_fn iterate, void *data);

ATOMLANG_API bool             atomlang_hash_compare (atomlang_hash_t *hashtable1, atomlang_hash_t *hashtable2, atomlang_hash_compare_fn compare, void *data);

ATOMLANG_API void                atomlang_hash_interalfree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data);
ATOMLANG_API void                atomlang_hash_keyfree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data);
ATOMLANG_API void                atomlang_hash_keyvaluefree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data);
ATOMLANG_API void                atomlang_hash_valuefree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data);

#ifdef __cplusplus
}
#endif

#endif