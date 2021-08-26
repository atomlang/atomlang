#include <inttypes.h>
#include "atomlang_hash.h"
#include "atomlang_macros.h"

#if ATOMLANGHASH_ENABLE_STATS
#define INC_COLLISION(tbl)      ++tbl->ncollision
#define INC_RESIZE(tbl)         ++tbl->nresize
#else
#define INC_COLLISION(tbl)
#define INC_RESIZE(tbl)
#endif

typedef struct hash_node_s {
    uint32_t                hash;
    atomlang_value_t         key;
    atomlang_value_t         value;
    struct hash_node_s      *next;
} hash_node_t;

struct atomlang_hash_t {
    uint32_t                size;
    uint32_t                count;
    hash_node_t             **nodes;
    atomlang_hash_compute_fn compute_fn;
    atomlang_hash_isequal_fn isequal_fn;
    atomlang_hash_iterate_fn free_fn;
    void                    *data;

    #if ATOMLANGHASH_ENABLE_STATS
    uint32_t                ncollision;
    uint32_t                nresize;
    #endif
};

#define HASH_SEED_VALUE                     5381
#define ROT32(x, y)                         ((x << y) | (x >> (32 - y)))
#define COMPUTE_HASH(tbl,key,hash)          register uint32_t hash = murmur3_32(key, len, HASH_SEED_VALUE); hash = hash % tbl->size
#define COMPUTE_HASH_NOMODULO(key,hash)     register uint32_t hash = murmur3_32(key, len, HASH_SEED_VALUE)
#define RECOMPUTE_HASH(tbl,key,hash)        hash = murmur3_32(key, len, HASH_SEED_VALUE); hash = hash % tbl->size

static inline uint32_t murmur3_32 (const char *key, uint32_t len, uint32_t seed) {
    static const uint32_t c1 = 0xcc9e2d51;
    static const uint32_t c2 = 0x1b873593;
    static const uint32_t r1 = 15;
    static const uint32_t r2 = 13;
    static const uint32_t m = 5;
    static const uint32_t n = 0xe6546b64;

    uint32_t hash = seed;

    const int nblocks = len / 4;
    const uint32_t *blocks = (const uint32_t *) key;
    for (int i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = ROT32(k, r1);
        k *= c2;

        hash ^= k;
        hash = ROT32(hash, r2) * m + n;
    }

    const uint8_t *tail = (const uint8_t *) (key + nblocks * 4);
    uint32_t k1 = 0;

    switch (len & 3) {
        case 3:
            k1 ^= tail[2] << 16;
        case 2:
            k1 ^= tail[1] << 8;
        case 1:
            k1 ^= tail[0];

            k1 *= c1;
            k1 = ROT32(k1, r1);
            k1 *= c2;
            hash ^= k1;
    }

    hash ^= len;
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    hash *= 0xc2b2ae35;
    hash ^= (hash >> 16);

    return hash;
}

static void table_dump (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused (hashtable, data)
    const char *k = ((atomlang_string_t *)key.p)->s;
    printf("%-20s=>\t",k);
    atomlang_value_dump(NULL, value, NULL, 0);
}

atomlang_hash_t *atomlang_hash_create (uint32_t size, atomlang_hash_compute_fn compute, atomlang_hash_isequal_fn isequal, atomlang_hash_iterate_fn free_fn, void *data) {
    if ((!compute) || (!isequal)) return NULL;
    if (size < ATOMLANGHASH_DEFAULT_SIZE) size = ATOMLANGHASH_DEFAULT_SIZE;

    atomlang_hash_t *hashtable = (atomlang_hash_t *)mem_alloc(NULL, sizeof(atomlang_hash_t));
    if (!hashtable) return NULL;
    if (!(hashtable->nodes = mem_calloc(NULL, size, sizeof(hash_node_t*)))) {mem_free(hashtable); return NULL;}

    hashtable->compute_fn = compute;
    hashtable->isequal_fn = isequal;
    hashtable->free_fn = free_fn;
    hashtable->data = data;
    hashtable->size = size;
    return hashtable;
}

void atomlang_hash_free (atomlang_hash_t *hashtable) {
    if (!hashtable) return;
    atomlang_hash_iterate_fn free_fn = hashtable->free_fn;

    for (uint32_t n = 0; n < hashtable->size; ++n) {
        hash_node_t *node = hashtable->nodes[n];
        hashtable->nodes[n] = NULL;
        while (node) {
            if (free_fn) free_fn(hashtable, node->key, node->value, hashtable->data);
            hash_node_t *old_node = node;
            node = node->next;
            mem_free(old_node);
        }
    }
    mem_free(hashtable->nodes);
    mem_free(hashtable);
    hashtable = NULL;
}

uint32_t atomlang_hash_memsize (atomlang_hash_t *hashtable) {
    uint32_t size = sizeof(atomlang_hash_t);
    size += hashtable->size * sizeof(hash_node_t);
    return size;
}

bool atomlang_hash_isempty (atomlang_hash_t *hashtable) {
    return (hashtable->count == 0);
}

static inline int atomlang_hash_resize (atomlang_hash_t *hashtable) {
    uint32_t size = (hashtable->size * 2);
    atomlang_hash_t newtbl = {
        .size = size,
        .count = 0,
        .isequal_fn = hashtable->isequal_fn,
        .compute_fn = hashtable->compute_fn
    };
    if (!(newtbl.nodes = mem_calloc(NULL, size, sizeof(hash_node_t*)))) return -1;

    hash_node_t *node, *next;
    for (uint32_t n = 0; n < hashtable->size; ++n) {
        for (node = hashtable->nodes[n]; node; node = next) {
            next = node->next;
            atomlang_hash_insert(&newtbl, node->key, node->value);
            atomlang_hash_iterate_fn free_fn = hashtable->free_fn;
            hashtable->free_fn = NULL;
            atomlang_hash_remove(hashtable, node->key);
            hashtable->free_fn = free_fn;
        }
    }

    mem_free(hashtable->nodes);
    hashtable->size = newtbl.size;
    hashtable->count = newtbl.count;
    hashtable->nodes = newtbl.nodes;
    INC_RESIZE(hashtable);

    return 0;
}

bool atomlang_hash_remove (atomlang_hash_t *hashtable, atomlang_value_t key) {
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    atomlang_hash_iterate_fn free_fn = hashtable->free_fn;
    hash_node_t *node = hashtable->nodes[position];
    hash_node_t *prevnode = NULL;
    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) {
            if (free_fn) free_fn(hashtable, node->key, node->value, hashtable->data);
            if (prevnode) prevnode->next = node->next;
            else hashtable->nodes[position] = node->next;
            mem_free(node);
            hashtable->count--;
            return true;
        }

        prevnode = node;
        node = node->next;
    }

    return false;
}

bool atomlang_hash_insert (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value) {
    if (hashtable->count >= ATOMLANGHASH_MAXENTRIES) return false;
    
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    hash_node_t *node = hashtable->nodes[position];
    if (node) INC_COLLISION(hashtable);

    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) {
            node->value = value;
            return false;
        }
        node = node->next;
    }

    if (hashtable->count >= hashtable->size * ATOMLANGHASH_THRESHOLD) {
        if (atomlang_hash_resize(hashtable) == -1) return false;
        position = hash % hashtable->size;
    }

    if (!(node = mem_alloc(NULL, sizeof(hash_node_t)))) return false;
    node->key = key;
    node->hash = hash;
    node->value = value;
    node->next = hashtable->nodes[position];
    hashtable->nodes[position] = node;
    ++hashtable->count;

    return true;
}

atomlang_value_t *atomlang_hash_lookup (atomlang_hash_t *hashtable, atomlang_value_t key) {
    register uint32_t hash = hashtable->compute_fn(key);
    register uint32_t position = hash % hashtable->size;

    hash_node_t *node = hashtable->nodes[position];
    while (node) {
        if ((node->hash == hash) && (hashtable->isequal_fn(key, node->key))) return &node->value;
        node = node->next;
    }

    return NULL;
}

atomlang_value_t *atomlang_hash_lookup_cstring (atomlang_hash_t *hashtable, const char *ckey) {
    STATICVALUE_FROM_STRING(key, ckey, strlen(ckey));
    return atomlang_hash_lookup(hashtable, key);
}

uint32_t atomlang_hash_count (atomlang_hash_t *hashtable) {
    return hashtable->count;
}

uint32_t atomlang_hash_compute_buffer (const char *key, uint32_t len) {
    return murmur3_32(key, len, HASH_SEED_VALUE);
}

uint32_t atomlang_hash_compute_int (atomlang_int_t n) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%" PRId64, n);
    return murmur3_32(buffer, (uint32_t)strlen(buffer), HASH_SEED_VALUE);
}

uint32_t atomlang_hash_compute_float (atomlang_float_t f) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%f", f);
    return murmur3_32(buffer, (uint32_t)strlen(buffer), HASH_SEED_VALUE);
}

void atomlang_hash_stat (atomlang_hash_t *hashtable) {
    #if ATOMLANGHASH_ENABLE_STATS
    printf("==============\n");
    printf("Collision: %d\n", hashtable->ncollision);
    printf("Resize: %d\n", hashtable->nresize);
    printf("Size: %d\n", hashtable->size);
    printf("Count: %d\n", hashtable->count);
    printf("==============\n");
    #endif
}

void atomlang_hash_transform (atomlang_hash_t *hashtable, atomlang_hash_transform_fn transform, void *data) {
    if ((!hashtable) || (!transform)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            transform(hashtable, node->key, &node->value, data);
            node = node->next;
        }
    }
}

void atomlang_hash_iterate (atomlang_hash_t *hashtable, atomlang_hash_iterate_fn iterate, void *data) {
    if ((!hashtable) || (!iterate)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            iterate(hashtable, node->key, node->value, data);
            node = node->next;
        }
    }
}

void atomlang_hash_iterate2 (atomlang_hash_t *hashtable, atomlang_hash_iterate2_fn iterate, void *data1, void *data2) {
    if ((!hashtable) || (!iterate)) return;

    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;

        while (node) {
            iterate(hashtable, node->key, node->value, data1, data2);
            node = node->next;
        }
    }
}

void atomlang_hash_iterate3 (atomlang_hash_t *hashtable, atomlang_hash_iterate3_fn iterate, void *data1, void *data2, void *data3) {
    if ((!hashtable) || (!iterate)) return;
    
    for (uint32_t i=0; i<hashtable->size; ++i) {
        hash_node_t *node = hashtable->nodes[i];
        if (!node) continue;
        
        while (node) {
            iterate(hashtable, node->key, node->value, data1, data2, data3);
            node = node->next;
        }
    }
}

void atomlang_hash_dump (atomlang_hash_t *hashtable) {
    atomlang_hash_iterate(hashtable, table_dump, NULL);
}

void atomlang_hash_append (atomlang_hash_t *hashtable1, atomlang_hash_t *hashtable2) {
    for (uint32_t i=0; i<hashtable2->size; ++i) {
        hash_node_t *node = hashtable2->nodes[i];
        if (!node) continue;

        while (node) {
            atomlang_hash_insert(hashtable1, node->key, node->value);
            node = node->next;
        }
    }
}

void atomlang_hash_resetfree (atomlang_hash_t *hashtable) {
    hashtable->free_fn = NULL;
}

bool atomlang_hash_compare (atomlang_hash_t *hashtable1, atomlang_hash_t *hashtable2, atomlang_hash_compare_fn compare, void *data) {
    if (hashtable1->count != hashtable2->count) return false;
    if (!compare) return false;

    atomlang_value_r keys1; atomlang_value_r values1;
    atomlang_value_r keys2; atomlang_value_r values2;
    marray_init(keys1); marray_init(values1);
    marray_init(keys2); marray_init(values2);
    marray_resize(atomlang_value_t, keys1, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(atomlang_value_t, keys2, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(atomlang_value_t, values1, hashtable1->count + MARRAY_DEFAULT_SIZE);
    marray_resize(atomlang_value_t, values2, hashtable1->count + MARRAY_DEFAULT_SIZE);

    for (uint32_t i=0; i<hashtable1->size; ++i) {
        hash_node_t *node = hashtable1->nodes[i];
        if (!node) continue;
        while (node) {
            marray_push(atomlang_value_t, keys1, node->key);
            marray_push(atomlang_value_t, values1, node->value);
            node = node->next;
        }
    }

    for (uint32_t i=0; i<hashtable2->size; ++i) {
        hash_node_t *node = hashtable2->nodes[i];
        if (!node) continue;
        while (node) {
            marray_push(atomlang_value_t, keys2, node->key);
            marray_push(atomlang_value_t, values2, node->value);
            node = node->next;
        }
    }

    bool result = false;
    uint32_t count = (uint32_t)marray_size(keys1);
    if (count != (uint32_t)marray_size(keys2)) goto cleanup;

    for (uint32_t i=0; i<count; ++i) {
        if (!compare(marray_get(keys1, i), marray_get(keys2, i), data)) goto cleanup;
        if (!compare(marray_get(values1, i), marray_get(values2, i), data)) goto cleanup;
    }

    result = true;

cleanup:
    marray_destroy(keys1);
    marray_destroy(keys2);
    marray_destroy(values1);
    marray_destroy(values2);

    return result;
}

void atomlang_hash_interalfree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(table, key, data)
    if (atomlang_value_isobject(value)) {
        atomlang_object_t *obj = VALUE_AS_OBJECT(value);
        if (OBJECT_ISA_CLOSURE(obj)) {
            atomlang_closure_t *closure = (atomlang_closure_t *)obj;
            if (closure->f && closure->f->tag == EXEC_TYPE_INTERNAL) atomlang_function_free(NULL, closure->f);
        }
    }
}

void atomlang_hash_keyfree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(table, value)
    atomlang_vm *vm = (atomlang_vm *)data;
    atomlang_value_free(vm, key);
}

void atomlang_hash_keyvaluefree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(table)
    atomlang_vm *vm = (atomlang_vm *)data;
    atomlang_value_free(vm, key);
    atomlang_value_free(vm, value);
}

void atomlang_hash_valuefree (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(table, key)
    atomlang_vm *vm = (atomlang_vm *)data;
    atomlang_value_free(vm, value);
}