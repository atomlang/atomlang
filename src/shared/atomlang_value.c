#include <inttypes.h>
#include "atomlang_hash.h"
#include "atomlang_core.h"
#include "atomlang_value.h"
#include "atomlang_utils.h"
#include "atomlang_memory.h"
#include "atomlang_macros.h"
#include "atomlang_opcodes.h"
#include "atomlang_vmmacros.h"

                                               
#define SET_OBJECT_VISITED_FLAG(_obj, _flag)    (((atomlang_object_t *)_obj)->gc.visited = _flag)

static void atomlang_function_special_serialize (atomlang_function_t *f, const char *key, json_t *json);
static atomlang_map_t *atomlang_map_deserialize (atomlang_vm *vm, json_value *json);

static void atomlang_hash_serialize (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(table)
    json_t *json = (json_t *)data;
    
    if (VALUE_ISA_CLOSURE(value)) value = VALUE_FROM_OBJECT(VALUE_AS_CLOSURE(value)->f);

    if (VALUE_ISA_FUNCTION(value)) {
        atomlang_function_t *f = VALUE_AS_FUNCTION(value);
        if (f->tag == EXEC_TYPE_SPECIAL) atomlang_function_special_serialize(f, VALUE_AS_CSTRING(key), json);
        else {
            atomlang_string_t *s = VALUE_AS_STRING(key);
            bool is_super_function = ((s->len > 5) && (string_casencmp(s->s, CLASS_INTERNAL_INIT_NAME, 5) == 0));
            const char *saved = f->identifier;
            if (is_super_function) f->identifier = s->s;
            atomlang_function_serialize(f, json);
            if (is_super_function) f->identifier = saved;
        }
    }
    else if (VALUE_ISA_CLASS(value)) {
        atomlang_class_serialize(VALUE_AS_CLASS(value), json);
    }
    else
        assert(0);
}

static void atomlang_hash_internalsize (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data1, void *data2) {
    #pragma unused(table)
    uint32_t    *size = (uint32_t *)data1;
    atomlang_vm    *vm = (atomlang_vm *)data2;
    *size = atomlang_value_size(vm, key);
    *size += atomlang_value_size(vm, value);
}

static void atomlang_hash_gray (atomlang_hash_t *table, atomlang_value_t key, atomlang_value_t value, void *data1) {
    #pragma unused(table)
    atomlang_vm *vm = (atomlang_vm *)data1;
    atomlang_gray_value(vm, key);
    atomlang_gray_value(vm, value);
}

atomlang_module_t *atomlang_module_new (atomlang_vm *vm, const char *identifier) {
    atomlang_module_t *m = (atomlang_module_t *)mem_alloc(NULL, sizeof(atomlang_module_t));
    assert(m);

    m->isa = atomlang_class_module;
    m->identifier = string_dup(identifier);
    m->htable = atomlang_hash_create(0, atomlang_value_hash, atomlang_value_equals, atomlang_hash_keyvaluefree, (void*)vm);

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*)m);
    return m;
}

void atomlang_module_free (atomlang_vm *vm, atomlang_module_t *m) {
    #pragma unused(vm)

    if (m->identifier) mem_free(m->identifier);
    atomlang_hash_free(m->htable);
    mem_free(m);
}

uint32_t atomlang_module_size (atomlang_vm *vm, atomlang_module_t *m) {
    SET_OBJECT_VISITED_FLAG(m, true);
    
    uint32_t hash_size = 0;
    atomlang_hash_iterate2(m->htable, atomlang_hash_internalsize, (void*)&hash_size, (void*)vm);
    uint32_t module_size = (sizeof(atomlang_module_t)) + string_size(m->identifier) + hash_size + atomlang_hash_memsize(m->htable);
    
    SET_OBJECT_VISITED_FLAG(m, false);
    return module_size;
}

void atomlang_module_blacken (atomlang_vm *vm, atomlang_module_t *m) {
    atomlang_vm_memupdate(vm, atomlang_module_size(vm, m));
    atomlang_hash_iterate(m->htable, atomlang_hash_gray, (void*)vm);
}

void atomlang_class_bind (atomlang_class_t *c, const char *key, atomlang_value_t value) {
    if (VALUE_ISA_CLASS(value)) {
        atomlang_class_t *obj = VALUE_AS_CLASS(value);
        obj->has_outer = true;
    }
    atomlang_hash_insert(c->htable, VALUE_FROM_CSTRING(NULL, key), value);
}

atomlang_class_t *atomlang_class_getsuper (atomlang_class_t *c) {
    return c->superclass;
}

bool atomlang_class_grow (atomlang_class_t *c, uint32_t n) {
    if (c->ivars) mem_free(c->ivars);
    if (c->nivars + n >= MAX_IVARS) return false;
    c->nivars += n;
    c->ivars = (atomlang_value_t *)mem_alloc(NULL, c->nivars * sizeof(atomlang_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) c->ivars[i] = VALUE_FROM_NULL;
    return true;
}

bool atomlang_class_setsuper (atomlang_class_t *baseclass, atomlang_class_t *superclass) {
    if (!superclass) return true;
    baseclass->superclass = superclass;

    atomlang_class_t *supermeta = (superclass) ? atomlang_class_get_meta(superclass) : NULL;
    uint32_t n1 = (supermeta) ? supermeta->nivars : 0;
    if (n1) if (!atomlang_class_grow (atomlang_class_get_meta(baseclass), n1)) return false;

    uint32_t n2 = (superclass) ? superclass->nivars : 0;
    if (n2) if (!atomlang_class_grow (baseclass, n2)) return false;

    return true;
}

bool atomlang_class_setsuper_extern (atomlang_class_t *baseclass, const char *identifier) {
    if (identifier) baseclass->superlook = string_dup(identifier);
    return true;
}

atomlang_class_t *atomlang_class_new_single (atomlang_vm *vm, const char *identifier, uint32_t nivar) {
    atomlang_class_t *c = (atomlang_class_t *)mem_alloc(NULL, sizeof(atomlang_class_t));
    assert(c);

    c->isa = atomlang_class_class;
    c->identifier = string_dup(identifier);
    c->superclass = NULL;
    c->nivars = nivar;
    c->htable = atomlang_hash_create(0, atomlang_value_hash, atomlang_value_equals, atomlang_hash_keyfree, NULL);
    if (nivar) {
        c->ivars = (atomlang_value_t *)mem_alloc(NULL, nivar * sizeof(atomlang_value_t));
        for (uint32_t i=0; i<nivar; ++i) c->ivars[i] = VALUE_FROM_NULL;
    }

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) c);
    return c;
}

atomlang_class_t *atomlang_class_new_pair (atomlang_vm *vm, const char *identifier, atomlang_class_t *superclass, uint32_t nivar, uint32_t nsvar) {
    if (!identifier) return NULL;

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s meta", identifier);
    
    atomlang_class_t *meta = atomlang_class_new_single(vm, buffer, nsvar);
    meta->objclass = atomlang_class_object;
    atomlang_class_setsuper(meta, atomlang_class_class);

    atomlang_class_t *c = atomlang_class_new_single(vm, identifier, nivar);
    c->objclass = meta;

    atomlang_class_setsuper(c, (superclass) ? superclass : atomlang_class_object);

    return c;
}

atomlang_class_t *atomlang_class_get_meta (atomlang_class_t *c) {
    if (c->objclass == atomlang_class_object) return c;
    return c->objclass;
}

bool atomlang_class_is_meta (atomlang_class_t *c) {
    return (c->objclass == atomlang_class_object);
}

bool atomlang_class_is_anon (atomlang_class_t *c) {
    return (string_casencmp(c->identifier, ATOMLANG_VM_ANONYMOUS_PREFIX, strlen(ATOMLANG_VM_ANONYMOUS_PREFIX)) == 0);
}

uint32_t atomlang_class_count_ivars (atomlang_class_t *c) {
    return (uint32_t)c->nivars;
}

int16_t atomlang_class_add_ivar (atomlang_class_t *c, const char *identifier) {
    #pragma unused(identifier)
    ++c->nivars;
    return c->nivars-1; 
}

void atomlang_class_dump (atomlang_class_t *c) {
    atomlang_hash_dump(c->htable);
}

void atomlang_class_setxdata (atomlang_class_t *c, void *xdata) {
    c->xdata = xdata;
}

void atomlang_class_serialize (atomlang_class_t *c, json_t *json) {
    const char *label = json_get_label(json, c->identifier);
    json_begin_object(json, label);
    
    json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_CLASS);     
    json_add_cstring(json, ATOMLANG_JSON_LABELIDENTIFIER, c->identifier);    
    
    if ((c->superclass) && (c->superclass->identifier) && (strcmp(c->superclass->identifier, ATOMLANG_CLASS_OBJECT_NAME) != 0)) {
        json_add_cstring(json, ATOMLANG_JSON_LABELSUPER, c->superclass->identifier);
    } else if (c->superlook) {
        json_add_cstring(json, ATOMLANG_JSON_LABELSUPER, c->superlook);
    }

    atomlang_class_t *meta = atomlang_class_get_meta(c);

    json_add_int(json, ATOMLANG_JSON_LABELNIVAR, c->nivars);
    if ((c != meta) && (meta->nivars > 0)) json_add_int(json, ATOMLANG_JSON_LABELSIVAR, meta->nivars);

    if (c->is_struct) json_add_bool(json, ATOMLANG_JSON_LABELSTRUCT, true);

    if (c->htable) {
        atomlang_hash_iterate(c->htable, atomlang_hash_serialize, (void *)json);
    }

    if (c != meta) {
        if ((meta->htable) && (atomlang_hash_count(meta->htable) > 0)) {
            json_begin_array(json, ATOMLANG_JSON_LABELMETA);
            atomlang_hash_iterate(meta->htable, atomlang_hash_serialize, (void *)json);
            json_end_array(json);
        }
    }

    json_end_object(json);
}

atomlang_class_t *atomlang_class_deserialize (atomlang_vm *vm, json_value *json) {
    if (json->type != json_object) return NULL;
    if (json->u.object.length < 3) return NULL;

    json_value *value = json->u.object.values[1].value;
    const char *key = json->u.object.values[1].name;

    if (string_casencmp(key, ATOMLANG_JSON_LABELIDENTIFIER, strlen(key)) != 0) return NULL;
    assert(value->type == json_string);

    atomlang_class_t *c = atomlang_class_new_pair(vm, value->u.string.ptr, NULL, 0, 0);
    DEBUG_DESERIALIZE("DESERIALIZE CLASS: %p %s\n", c, value->u.string.ptr);

    atomlang_class_t *meta = atomlang_class_get_meta(c);

    uint32_t n = json->u.object.length;
    for (uint32_t i=2; i<n; ++i) { 

        value = json->u.object.values[i].value;
        key = json->u.object.values[i].name;

        if (value->type != json_object) {

            if (string_casencmp(key, ATOMLANG_JSON_LABELSUPER, strlen(key)) == 0) {

                if (strcmp(value->u.string.ptr, ATOMLANG_CLASS_OBJECT_NAME) != 0) {
                    c->xdata = (void *)string_dup(value->u.string.ptr);
                }
                continue;
            }

            if (string_casencmp(key, ATOMLANG_JSON_LABELNIVAR, strlen(key)) == 0) {
                atomlang_class_grow(c, (uint32_t)value->u.integer);
                continue;
            }

            if (string_casencmp(key, ATOMLANG_JSON_LABELSIVAR, strlen(key)) == 0) {
                atomlang_class_grow(meta, (uint32_t)value->u.integer);
                continue;
            }

            if (string_casencmp(key, ATOMLANG_JSON_LABELSTRUCT, strlen(key)) == 0) {
                c->is_struct = true;
                continue;
            }

            if (string_casencmp(key, ATOMLANG_JSON_LABELMETA, strlen(key)) == 0) {
                uint32_t m = value->u.array.length;
                for (uint32_t j=0; j<m; ++j) {
                    json_value *r = value->u.array.values[j];
                    if (r->type != json_object) continue;
                    atomlang_object_t *obj = atomlang_object_deserialize(vm, r);
                    if (!obj) goto abort_load;

                    const char *identifier = obj->identifier;
                    if (OBJECT_ISA_FUNCTION(obj)) obj = (atomlang_object_t *)atomlang_closure_new(vm, (atomlang_function_t *)obj);
                    if (obj) atomlang_class_bind(meta, identifier, VALUE_FROM_OBJECT(obj));
                    else goto abort_load;
                }
                continue;
            }

            goto abort_load;
        }

        if (value->type == json_object) {
            atomlang_object_t *obj = atomlang_object_deserialize(vm, value);
            if (!obj) goto abort_load;

            const char *identifier = obj->identifier;
            if (OBJECT_ISA_FUNCTION(obj)) obj = (atomlang_object_t *)atomlang_closure_new(vm, (atomlang_function_t *)obj);
            atomlang_class_bind(c, identifier, VALUE_FROM_OBJECT(obj));
        }
    }

    return c;

abort_load:
    return NULL;
}

static void atomlang_class_free_internal (atomlang_vm *vm, atomlang_class_t *c, bool skip_base) {
    if (skip_base && (atomlang_iscore_class(c) || atomlang_isopt_class(c))) return;

    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)c, true));

    if (c->xdata && vm) {
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (atomlang_object_t *)c);
    }

    if (c->identifier) mem_free((void *)c->identifier);
    if (c->superlook) mem_free((void *)c->superlook);
    
    if (!skip_base) {
        atomlang_hash_iterate(c->htable, atomlang_hash_interalfree, NULL);
        atomlang_hash_iterate(c->htable, atomlang_hash_valuefree, NULL);
    }

    atomlang_hash_free(c->htable);
    if (c->ivars) mem_free((void *)c->ivars);
    mem_free((void *)c);
}

void atomlang_class_free_core (atomlang_vm *vm, atomlang_class_t *c) {
    atomlang_class_free_internal(vm, c, false);
}

void atomlang_class_free (atomlang_vm *vm, atomlang_class_t *c) {
    atomlang_class_free_internal(vm, c, true);
}

inline atomlang_object_t *atomlang_class_lookup (atomlang_class_t *c, atomlang_value_t key) {
    while (c) {
        atomlang_value_t *v = atomlang_hash_lookup(c->htable, key);
        if (v) return (atomlang_object_t *)v->p;
        c = c->superclass;
    }
    return NULL;
}

atomlang_class_t *atomlang_class_lookup_class_identifier (atomlang_class_t *c, const char *identifier) {
    while (c) {
        if (string_cmp(c->identifier, identifier) == 0) return c;
        c = c->superclass;
    }
    return NULL;
}

inline atomlang_closure_t *atomlang_class_lookup_closure (atomlang_class_t *c, atomlang_value_t key) {
    atomlang_object_t *obj = atomlang_class_lookup(c, key);
    if (obj && OBJECT_ISA_CLOSURE(obj)) return (atomlang_closure_t *)obj;
    return NULL;
}

inline atomlang_closure_t *atomlang_class_lookup_constructor (atomlang_class_t *c, uint32_t nparams) {
    if (c->xdata) {
        if (nparams == 0) {
            STATICVALUE_FROM_STRING(key, CLASS_INTERNAL_INIT_NAME, strlen(CLASS_INTERNAL_INIT_NAME));
            return (atomlang_closure_t *)atomlang_class_lookup(c, key);
        }

        char name[256]; snprintf(name, sizeof(name), "%s%d", CLASS_INTERNAL_INIT_NAME, nparams);
        STATICVALUE_FROM_STRING(key, name, strlen(name));
        return (atomlang_closure_t *)atomlang_class_lookup(c, key);
    }

    STATICVALUE_FROM_STRING(key, CLASS_CONSTRUCTOR_NAME, strlen(CLASS_CONSTRUCTOR_NAME));
    return (atomlang_closure_t *)atomlang_class_lookup(c, key);
}

uint32_t atomlang_class_size (atomlang_vm *vm, atomlang_class_t *c) {
    SET_OBJECT_VISITED_FLAG(c, true);
    
    uint32_t class_size = sizeof(atomlang_class_t) + (c->nivars * sizeof(atomlang_value_t)) + string_size(c->identifier);

    uint32_t hash_size = 0;
    atomlang_hash_iterate2(c->htable, atomlang_hash_internalsize, (void *)&hash_size, (void *)vm);
    hash_size += atomlang_hash_memsize(c->htable);

    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (c->xdata && delegate->bridge_size)
        class_size += delegate->bridge_size(vm, c->xdata);

    SET_OBJECT_VISITED_FLAG(c, false);
    return class_size;
}

void atomlang_class_blacken (atomlang_vm *vm, atomlang_class_t *c) {
    atomlang_vm_memupdate(vm, atomlang_class_size(vm, c));

    atomlang_gray_object(vm, (atomlang_object_t *)c->objclass);

    atomlang_gray_object(vm, (atomlang_object_t *)c->superclass);

    atomlang_hash_iterate(c->htable, atomlang_hash_gray, (void *)vm);

    for (uint32_t i=0; i<c->nivars; ++i) {
        atomlang_gray_value(vm, c->ivars[i]);
    }
}

atomlang_function_t *atomlang_function_new (atomlang_vm *vm, const char *identifier, uint16_t nparams, uint16_t nlocals, uint16_t ntemps, void *code) {
    atomlang_function_t *f = (atomlang_function_t *)mem_alloc(NULL, sizeof(atomlang_function_t));
    assert(f);

    f->isa = atomlang_class_function;
    f->identifier = (identifier) ? string_dup(identifier) : NULL;
    f->tag = EXEC_TYPE_NATIVE;
    f->nparams = nparams;
    f->nlocals = nlocals;
    f->ntemps = ntemps;
    f->nupvalues = 0;


    if (code != NULL) {
        f->useargs = false;
        f->bytecode = (uint32_t *)code;
        marray_init(f->cpool);
        marray_init(f->pvalue);
        marray_init(f->pname);
    }

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*)f);
    return f;
}

atomlang_function_t *atomlang_function_new_internal (atomlang_vm *vm, const char *identifier, atomlang_c_internal exec, uint16_t nparams) {
    atomlang_function_t *f = atomlang_function_new(vm, identifier, nparams, 0, 0, NULL);
    f->tag = EXEC_TYPE_INTERNAL;
    f->internal = exec;
    return f;
}

atomlang_function_t *atomlang_function_new_special (atomlang_vm *vm, const char *identifier, uint16_t index, void *getter, void *setter) {
    atomlang_function_t *f = atomlang_function_new(vm, identifier, 0, 0, 0, NULL);
    f->tag = EXEC_TYPE_SPECIAL;
    f->index = index;
    f->special[0] = getter;
    f->special[1] = setter;
    return f;
}

atomlang_function_t *atomlang_function_new_bridged (atomlang_vm *vm, const char *identifier, void *xdata) {
    atomlang_function_t *f = atomlang_function_new(vm, identifier, 0, 0, 0, NULL);
    f->tag = EXEC_TYPE_BRIDGED;
    f->xdata = xdata;
    return f;
}

uint16_t atomlang_function_cpool_add (atomlang_vm *vm, atomlang_function_t *f, atomlang_value_t v) {
    assert(f->tag == EXEC_TYPE_NATIVE);

    size_t n = marray_size(f->cpool);
    for (size_t i=0; i<n; i++) {
        atomlang_value_t v2 = marray_get(f->cpool, i);
        if (atomlang_value_equals(v,v2)) {
            atomlang_value_free(NULL, v);
            return (uint16_t)i;
        }
    }

    if ((vm) && (atomlang_value_isobject(v))) atomlang_vm_transfer(vm, VALUE_AS_OBJECT(v));

    marray_push(atomlang_value_t, f->cpool, v);
    return (uint16_t)marray_size(f->cpool)-1;
}

atomlang_value_t atomlang_function_cpool_get (atomlang_function_t *f, uint16_t i) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    return marray_get(f->cpool, i);
}

atomlang_list_t *atomlang_function_params_get (atomlang_vm *vm, atomlang_function_t *f) {
    #pragma unused(vm)
    atomlang_list_t *list = NULL;
    
    if (f->tag == EXEC_TYPE_NATIVE) {
    } else if (f->tag == EXEC_TYPE_BRIDGED && f->xdata) {
    } else if (f->tag == EXEC_TYPE_INTERNAL) {
    }
    
    return list;
}

void atomlang_function_setxdata (atomlang_function_t *f, void *xdata) {
    f->xdata = xdata;
}

static void atomlang_function_array_serialize (atomlang_function_t *f, json_t *json, atomlang_value_r r) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    size_t n = marray_size(r);

    for (size_t i=0; i<n; i++) {
        atomlang_value_t v = marray_get(r, i);
        atomlang_value_serialize(NULL, v, json);
    }
}

static void atomlang_function_array_dump (atomlang_function_t *f, atomlang_value_r r) {
    assert(f->tag == EXEC_TYPE_NATIVE);
    size_t n = marray_size(r);

    for (size_t i=0; i<n; i++) {
        atomlang_value_t v = marray_get(r, i);

        if (VALUE_ISA_NULL(v)) {
            printf("%05zu\tNULL\n", i);
            continue;
        }
        
        if (VALUE_ISA_UNDEFINED(v)) {
            printf("%05zu\tUNDEFINED\n", i);
            continue;
        }
        
        if (VALUE_ISA_BOOL(v)) {
            printf("%05zu\tBOOL: %d\n", i, (v.n == 0) ? 0 : 1);
            continue;
        }
        
        if (VALUE_ISA_INT(v)) {
            printf("%05zu\tINT: %" PRId64 "\n", i, (int64_t)v.n);
            continue;
        }
        
        if (VALUE_ISA_FLOAT(v)) {
            printf("%05zu\tFLOAT: %g\n", i, (double)v.f);
            continue;
        }
        
        if (VALUE_ISA_FUNCTION(v)) {
            atomlang_function_t *vf = VALUE_AS_FUNCTION(v);
            printf("%05zu\tFUNC: %s\n", i, (vf->identifier) ? vf->identifier : "$anon");
            continue;
        }
        
        if (VALUE_ISA_CLASS(v)) {
            atomlang_class_t *c = VALUE_AS_CLASS(v);
            printf("%05zu\tCLASS: %s\n", i, (c->identifier) ? c->identifier: "$anon");
            continue;
            
        }
        
        if (VALUE_ISA_STRING(v)) {
            printf("%05zu\tSTRING: %s\n", i, VALUE_AS_CSTRING(v));
            continue;
        }
        
        if (VALUE_ISA_LIST(v)) {
            atomlang_list_t *value = VALUE_AS_LIST(v);
            size_t count = marray_size(value->array);
            printf("%05zu\tLIST: %zu items\n", i, count);
            continue;
            
        }
        
        if (VALUE_ISA_MAP(v)) {
            atomlang_map_t *map = VALUE_AS_MAP(v);
            printf("%05zu\tMAP: %u items\n", i, atomlang_hash_count(map->hash));
            continue;
        }
        
        assert(0);
    }
}

static void atomlang_function_bytecode_serialize (atomlang_function_t *f, json_t *json) {
    if (!f->bytecode || !f->ninsts) {
        json_add_null(json, ATOMLANG_JSON_LABELBYTECODE);
        return;
    }

    uint32_t ninst = f->ninsts;
    uint32_t length = ninst * 2 * sizeof(uint32_t);
    uint8_t *hexchar = (uint8_t*) mem_alloc(NULL, sizeof(uint8_t) * length);

    for (uint32_t k=0, i=0; i < ninst; ++i) {
        uint32_t value = f->bytecode[i];

        for (int32_t j=2*sizeof(value)-1; j>=0; --j) {
            uint8_t c = "0123456789ABCDEF"[((value >> (j*4)) & 0xF)];
            hexchar[k++] = c;
        }
    }

    json_add_string(json, ATOMLANG_JSON_LABELBYTECODE, (const char *)hexchar, length);
    mem_free(hexchar);
    
    if (!f->lineno) return;
    
    ninst = f->ninsts;
    length = ninst * 2 * sizeof(uint32_t);
    hexchar = (uint8_t*) mem_alloc(NULL, sizeof(uint8_t) * length);
    
    for (uint32_t k=0, i=0; i < ninst; ++i) {
        uint32_t value = f->lineno[i];
        
        for (int32_t j=2*sizeof(value)-1; j>=0; --j) {
            uint8_t c = "0123456789ABCDEF"[((value >> (j*4)) & 0xF)];
            hexchar[k++] = c;
        }
    }
    
    json_add_string(json, ATOMLANG_JSON_LABELLINENO, (const char *)hexchar, length);
    mem_free(hexchar);
}

uint32_t *atomlang_bytecode_deserialize (const char *buffer, size_t len, uint32_t *n) {
    uint32_t ninst = (uint32_t)len / 8;
    uint32_t *bytecode = (uint32_t *)mem_alloc(NULL, sizeof(uint32_t) * (ninst + 1));    

    for (uint32_t j=0; j<ninst; ++j) {
        register uint32_t v = 0;

        for (uint32_t i=(j*8); i<=(j*8)+7; ++i) {

            register uint32_t c = buffer[i];

            if (c >= 'A' && c <= 'F') {
                c = c - 'A' + 10;
            } else if (c >= '0' && c <= '9') {
                c -= '0';
            } else goto abort_conversion;

            v = v << 4 | c;

        }

        bytecode[j] = v;
    }

    *n = ninst;
    return bytecode;

abort_conversion:
    *n = 0;
    if (bytecode) mem_free(bytecode);
    return NULL;
}

void atomlang_function_dump (atomlang_function_t *f, code_dump_function codef) {
    printf("Function: %s\n", (f->identifier) ? f->identifier : "$anon");
    printf("Params:%d Locals:%d Temp:%d Upvalues:%d Tag:%d xdata:%p\n", f->nparams, f->nlocals, f->ntemps, f->nupvalues, f->tag, f->xdata);

    if (f->tag == EXEC_TYPE_NATIVE) {
        if (marray_size(f->cpool)) printf("======= CONST POOL =======\n");
        atomlang_function_array_dump(f, f->cpool);
        
        if (marray_size(f->pname)) printf("======= PARAM NAMES =======\n");
        atomlang_function_array_dump(f, f->pname);
        
        if (marray_size(f->pvalue)) printf("======= PARAM VALUES =======\n");
        atomlang_function_array_dump(f, f->pvalue);

        printf("======= BYTECODE =======\n");
        if ((f->bytecode) && (codef)) codef(f->bytecode);
    }

    printf("\n");
}

void atomlang_function_special_serialize (atomlang_function_t *f, const char *key, json_t *json) {
    const char *label = json_get_label(json, key);
    json_begin_object(json, label);

    json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_FUNCTION);    
    json_add_cstring(json, ATOMLANG_JSON_LABELIDENTIFIER, key);                
    json_add_int(json, ATOMLANG_JSON_LABELTAG, f->tag);

    json_add_int(json, ATOMLANG_JSON_LABELNPARAM, f->nparams);
    json_add_bool(json, ATOMLANG_JSON_LABELARGS, f->useargs);
    json_add_int(json, ATOMLANG_JSON_LABELINDEX, f->index);

    if (f->special[0]) {
        atomlang_function_t *f2 = (atomlang_function_t*)f->special[0];
        f2->identifier = ATOMLANG_JSON_GETTER;
        atomlang_function_serialize(f2, json);
        f2->identifier = NULL;
    }
    if (f->special[1]) {
        atomlang_function_t *f2 = (atomlang_function_t*)f->special[1];
        f2->identifier = ATOMLANG_JSON_SETTER;
        atomlang_function_serialize(f2, json);
        f2->identifier = NULL;
    }

    json_end_object(json);
}

void atomlang_function_serialize (atomlang_function_t *f, json_t *json) {

    if (f->tag == EXEC_TYPE_SPECIAL) {
        atomlang_function_special_serialize(f, f->identifier, json);
        return;
    }

    const char *identifier = f->identifier;
    char temp[256];
    if (!identifier) {snprintf(temp, sizeof(temp), "$anon_%p", f); identifier = temp;}

    const char *label = json_get_label(json, identifier);
    json_begin_object(json, label);
    
    json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_FUNCTION);  
    json_add_cstring(json, ATOMLANG_JSON_LABELIDENTIFIER, identifier);       
    json_add_int(json, ATOMLANG_JSON_LABELTAG, f->tag);

    json_add_int(json, ATOMLANG_JSON_LABELNPARAM, f->nparams);
    json_add_bool(json, ATOMLANG_JSON_LABELARGS, f->useargs);

    if (f->tag == EXEC_TYPE_NATIVE) {
        json_add_int(json, ATOMLANG_JSON_LABELNLOCAL, f->nlocals);
        json_add_int(json, ATOMLANG_JSON_LABELNTEMP, f->ntemps);
        json_add_int(json, ATOMLANG_JSON_LABELNUPV, f->nupvalues);
        json_add_double(json, ATOMLANG_JSON_LABELPURITY, f->purity);

        atomlang_function_bytecode_serialize(f, json);

        json_begin_array(json, ATOMLANG_JSON_LABELPOOL);
        atomlang_function_array_serialize(f, json, f->cpool);
        json_end_array(json);
        
        if (marray_size(f->pvalue)) {
            json_begin_array(json, ATOMLANG_JSON_LABELPVALUES);
            atomlang_function_array_serialize(f, json, f->pvalue);
        json_end_array(json);
    }

        if (marray_size(f->pname)) {
            json_begin_array(json, ATOMLANG_JSON_LABELPNAMES);
            atomlang_function_array_serialize(f, json, f->pname);
            json_end_array(json);
        }
    }
    
    json_end_object(json);
}

atomlang_function_t *atomlang_function_deserialize (atomlang_vm *vm, json_value *json) {
    atomlang_function_t *f = atomlang_function_new(vm, NULL, 0, 0, 0, NULL);

    DEBUG_DESERIALIZE("DESERIALIZE FUNCTION: %p\n", f);

    bool identifier_parsed = false;
    bool getter_parsed = false;
    bool setter_parsed = false;
    bool index_parsed = false;
    bool bytecode_parsed = false;
    bool cpool_parsed = false;
    bool nparams_parsed = false;
    bool nlocals_parsed = false;
    bool ntemp_parsed = false;
    bool nupvalues_parsed = false;
    bool nargs_parsed = false;
    bool tag_parsed = false;

    uint32_t n = json->u.object.length;
    for (uint32_t i=1; i<n; ++i) {
        const char *label = json->u.object.values[i].name;
        json_value *value = json->u.object.values[i].value;
        size_t label_size = strlen(label);

        if (string_casencmp(label, ATOMLANG_JSON_LABELIDENTIFIER, label_size) == 0) {
            if (value->type != json_string) goto abort_load;
            if (identifier_parsed) goto abort_load;
            if (strncmp(value->u.string.ptr, "$anon", 5) != 0) {
                f->identifier = string_dup(value->u.string.ptr);
                DEBUG_DESERIALIZE("IDENTIFIER: %s\n", value->u.string.ptr);
            }
            identifier_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELTAG, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (tag_parsed) goto abort_load;
            f->tag = (uint16_t)value->u.integer;
            tag_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELINDEX, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (index_parsed) goto abort_load;
            f->index = (uint16_t)value->u.integer;
            index_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_GETTER, strlen(ATOMLANG_JSON_GETTER)) == 0) {
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (getter_parsed) goto abort_load;
            atomlang_function_t *getter = atomlang_function_deserialize(vm, value);
            if (!getter) goto abort_load;
            f->special[0] = atomlang_closure_new(vm, getter);
            getter_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_SETTER, strlen(ATOMLANG_JSON_SETTER)) == 0) {
            if (f->tag != EXEC_TYPE_SPECIAL) goto abort_load;
            if (setter_parsed) goto abort_load;
            atomlang_function_t *setter = atomlang_function_deserialize(vm, value);
            if (!setter) goto abort_load;
            f->special[1] = atomlang_closure_new(vm, setter);
            setter_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELNPARAM, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nparams_parsed) goto abort_load;
            f->nparams = (uint16_t)value->u.integer;
            nparams_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELNLOCAL, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nlocals_parsed) goto abort_load;
            f->nlocals = (uint16_t)value->u.integer;
            nlocals_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELNTEMP, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (ntemp_parsed) goto abort_load;
            f->ntemps = (uint16_t)value->u.integer;
            ntemp_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELNUPV, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            if (nupvalues_parsed) goto abort_load;
            f->nupvalues = (uint16_t)value->u.integer;
            nupvalues_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELARGS, label_size) == 0) {
            if (value->type != json_boolean) goto abort_load;
            if (nargs_parsed) goto abort_load;
            f->useargs = (bool)value->u.boolean;
            nargs_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELBYTECODE, label_size) == 0) {
            if (bytecode_parsed) goto abort_load;
            if (value->type == json_null) {

                f->ninsts = 0;
                f->bytecode = (uint32_t *)mem_alloc(NULL, sizeof(uint32_t) * (f->ninsts + 1));
            } else {
                if (value->type != json_string) goto abort_load;
                if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
                f->bytecode = atomlang_bytecode_deserialize(value->u.string.ptr, value->u.string.length, &f->ninsts);
            }
            bytecode_parsed = true;
            continue;
        }

        if (string_casencmp(label, ATOMLANG_JSON_LABELLINENO, label_size) == 0) {
            if (value->type == json_string) f->lineno = atomlang_bytecode_deserialize(value->u.string.ptr, value->u.string.length, &f->ninsts);
        }
        
        if (string_casencmp(label, ATOMLANG_JSON_LABELPNAMES, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                if (r->type != json_string) goto abort_load;
                marray_push(atomlang_value_t, f->pname, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
            }
        }
        
        if (string_casencmp(label, ATOMLANG_JSON_LABELPVALUES, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            
            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                switch (r->type) {
                    case json_integer:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_INT((atomlang_int_t)r->u.integer));
                        break;
                        
                    case json_double:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_FLOAT((atomlang_float_t)r->u.dbl));
                        break;
                        
                    case json_boolean:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_BOOL(r->u.boolean));
                        break;
                        
                    case json_string:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
                        break;
                        
                    case json_object:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_UNDEFINED);
                        break;
                        
                    case json_null:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_NULL);
                        break;
                        
                    case json_none:
                    case json_array:
                        marray_push(atomlang_value_t, f->pvalue, VALUE_FROM_NULL);
                        break;
                }
            }
        }
        
        if (string_casencmp(label, ATOMLANG_JSON_LABELPOOL, label_size) == 0) {
            if (value->type != json_array) goto abort_load;
            if (f->tag != EXEC_TYPE_NATIVE) goto abort_load;
            if (cpool_parsed) goto abort_load;
            cpool_parsed = true;

            uint32_t m = value->u.array.length;
            for (uint32_t j=0; j<m; ++j) {
                json_value *r = value->u.array.values[j];
                switch (r->type) {
                    case json_integer:
                        atomlang_function_cpool_add(NULL, f, VALUE_FROM_INT((atomlang_int_t)r->u.integer));
                        break;

                    case json_double:
                        atomlang_function_cpool_add(NULL, f, VALUE_FROM_FLOAT((atomlang_float_t)r->u.dbl));
                        break;

                    case json_boolean:
                        atomlang_function_cpool_add(NULL, f, VALUE_FROM_BOOL(r->u.boolean));
                        break;

                    case json_string:
                        atomlang_function_cpool_add(vm, f, VALUE_FROM_STRING(NULL, r->u.string.ptr, r->u.string.length));
                        break;

                    case json_object: {
                        atomlang_object_t *obj = atomlang_object_deserialize(vm, r);
                        if (!obj) goto abort_load;
                        atomlang_function_cpool_add(NULL, f, VALUE_FROM_OBJECT(obj));
                        break;
                    }

                    case json_array: {
                        uint32_t count = r->u.array.length;
                        atomlang_list_t *list = atomlang_list_new (NULL, count);
                        if (!list) continue;

                        for (uint32_t k=0; k<count; ++k) {
                            json_value *jsonv = r->u.array.values[k];
                            atomlang_value_t v;

                            switch (jsonv->type) {
                                case json_integer: v = VALUE_FROM_INT((atomlang_int_t)jsonv->u.integer); break;
                                case json_double: v = VALUE_FROM_FLOAT((atomlang_float_t)jsonv->u.dbl); break;
                                case json_boolean: v = VALUE_FROM_BOOL(jsonv->u.boolean); break;
                                case json_string: v = VALUE_FROM_STRING(vm, jsonv->u.string.ptr, jsonv->u.string.length); break;
                                default: goto abort_load;
                            }

                            marray_push(atomlang_value_t, list->array, v);
                        }
                        atomlang_function_cpool_add(vm, f, VALUE_FROM_OBJECT(list));
                        break;
                    }

                    case json_none:
                    case json_null:
                        atomlang_function_cpool_add(NULL, f, VALUE_FROM_NULL);
                        break;
                }
            }
        }
    }

    return f;

abort_load:

    return NULL;
}

void atomlang_function_free (atomlang_vm *vm, atomlang_function_t *f) {
    if (!f) return;

    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)f, true));

    if (f->xdata && vm) {
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (atomlang_object_t *)f);
    }

    if (f->identifier) mem_free((void *)f->identifier);
    if (f->tag == EXEC_TYPE_NATIVE) {
        if (f->bytecode) mem_free((void *)f->bytecode);
        if (f->lineno) mem_free((void *)f->lineno);
        
        size_t n = marray_size(f->pvalue);
        for (size_t i=0; i<n; i++) {
            atomlang_value_t v = marray_get(f->pvalue, i);
            atomlang_value_free(NULL, v);
        }
        marray_destroy(f->pvalue);
        
        n = marray_size(f->pname);
        for (size_t i=0; i<n; i++) {
            atomlang_value_t v = marray_get(f->pname, i);
            atomlang_value_free(NULL, v);
        }
        marray_destroy(f->pname);
        
        marray_destroy(f->cpool);
    }
    mem_free((void *)f);
}

uint32_t atomlang_function_size (atomlang_vm *vm, atomlang_function_t *f) {
    SET_OBJECT_VISITED_FLAG(f, true);
    
    uint32_t func_size = sizeof(atomlang_function_t) + string_size(f->identifier);

    if (f->tag == EXEC_TYPE_NATIVE) {
        if (f->bytecode) func_size += f->ninsts * sizeof(uint32_t);
        size_t n = marray_size(f->cpool);
        for (size_t i=0; i<n; i++) {
            atomlang_value_t v = marray_get(f->cpool, i);
            func_size += atomlang_value_size(vm, v);
        }
    } else if (f->tag == EXEC_TYPE_SPECIAL) {
        if (f->special[0]) func_size += atomlang_closure_size(vm, (atomlang_closure_t *)f->special[0]);
        if ((f->special[1]) && (f->special[0] != f->special[1])) func_size += atomlang_closure_size(vm, (atomlang_closure_t *)f->special[1]);
    } else if (f->tag == EXEC_TYPE_BRIDGED) {
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (f->xdata && delegate->bridge_size)
            func_size += delegate->bridge_size(vm, f->xdata);
    }

    SET_OBJECT_VISITED_FLAG(f, false);
    return func_size;
}

void atomlang_function_blacken (atomlang_vm *vm, atomlang_function_t *f) {
    atomlang_vm_memupdate(vm, atomlang_function_size(vm, f));

    if (f->tag == EXEC_TYPE_SPECIAL) {
        if (f->special[0]) atomlang_gray_object(vm, (atomlang_object_t *)f->special[0]);
        if (f->special[1]) atomlang_gray_object(vm, (atomlang_object_t *)f->special[1]);
    }

    if (f->tag == EXEC_TYPE_NATIVE) {
        size_t n = marray_size(f->cpool);
        for (size_t i=0; i<n; i++) {
            atomlang_value_t v = marray_get(f->cpool, i);
            atomlang_gray_value(vm, v);
        }
    }
}

atomlang_closure_t *atomlang_closure_new (atomlang_vm *vm, atomlang_function_t *f) {
    atomlang_closure_t *closure = (atomlang_closure_t *)mem_alloc(NULL, sizeof(atomlang_closure_t));
    assert(closure);

    closure->isa = atomlang_class_closure;
    closure->vm = vm;
    closure->f = f;

    uint16_t nupvalues = (f) ? f->nupvalues : 0;
    closure->upvalue = (nupvalues) ? (atomlang_upvalue_t **)mem_alloc(NULL, sizeof(atomlang_upvalue_t*) * (f->nupvalues + 1)) : NULL;

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*)closure);
    return closure;
}

void atomlang_closure_free (atomlang_vm *vm, atomlang_closure_t *closure) {
    #pragma unused(vm)
    if (closure->refcount > 0) return;
    
    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)closure, true));
    if (closure->upvalue) mem_free(closure->upvalue);
    mem_free(closure);
}

uint32_t atomlang_closure_size (atomlang_vm *vm, atomlang_closure_t *closure) {
    #pragma unused(vm)
    SET_OBJECT_VISITED_FLAG(closure, true);

    uint32_t closure_size = sizeof(atomlang_closure_t);
    atomlang_upvalue_t **upvalue = closure->upvalue;
    while (upvalue && upvalue[0]) {
        closure_size += sizeof(atomlang_upvalue_t*);
        ++upvalue;
    }
    
    SET_OBJECT_VISITED_FLAG(closure, false);
    return closure_size;
}

void atomlang_closure_inc_refcount (atomlang_vm *vm, atomlang_closure_t *closure) {
    if (closure->refcount == 0) atomlang_gc_temppush(vm, (atomlang_object_t *)closure);
    ++closure->refcount;
}

void atomlang_closure_dec_refcount (atomlang_vm *vm, atomlang_closure_t *closure) {
    if (closure->refcount == 1) atomlang_gc_tempnull(vm, (atomlang_object_t *)closure);
    if (closure->refcount >= 1) --closure->refcount;
}

void atomlang_closure_blacken (atomlang_vm *vm, atomlang_closure_t *closure) {
    atomlang_vm_memupdate(vm, atomlang_closure_size(vm, closure));

    atomlang_gray_object(vm, (atomlang_object_t*)closure->f);

    atomlang_upvalue_t **upvalue = closure->upvalue;
    while (upvalue && upvalue[0]) {
        atomlang_gray_object(vm, (atomlang_object_t*)upvalue[0]);
        ++upvalue;
    }
    
    if (closure->context) atomlang_gray_object(vm, closure->context);
}

atomlang_upvalue_t *atomlang_upvalue_new (atomlang_vm *vm, atomlang_value_t *value) {
    atomlang_upvalue_t *upvalue = (atomlang_upvalue_t *)mem_alloc(NULL, sizeof(atomlang_upvalue_t));

    upvalue->isa = atomlang_class_upvalue;
    upvalue->value = value;
    upvalue->closed = VALUE_FROM_NULL;
    upvalue->next = NULL;

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) upvalue);
    return upvalue;
}

void atomlang_upvalue_free(atomlang_vm *vm, atomlang_upvalue_t *upvalue) {
    #pragma unused(vm)
    
    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)upvalue, true));
    mem_free(upvalue);
}

uint32_t atomlang_upvalue_size (atomlang_vm *vm, atomlang_upvalue_t *upvalue) {
    #pragma unused(vm, upvalue)
    
    SET_OBJECT_VISITED_FLAG(upvalue, true);
    uint32_t upvalue_size = sizeof(atomlang_upvalue_t);
    SET_OBJECT_VISITED_FLAG(upvalue, false);
    
    return upvalue_size;
}

void atomlang_upvalue_blacken (atomlang_vm *vm, atomlang_upvalue_t *upvalue) {
    atomlang_vm_memupdate(vm, atomlang_upvalue_size(vm, upvalue));
    atomlang_gray_value(vm, *upvalue->value);
    atomlang_gray_value(vm, upvalue->closed);
}

atomlang_fiber_t *atomlang_fiber_new (atomlang_vm *vm, atomlang_closure_t *closure, uint32_t nstack, uint32_t nframes) {
    atomlang_fiber_t *fiber = (atomlang_fiber_t *)mem_alloc(NULL, sizeof(atomlang_fiber_t));
    assert(fiber);

    fiber->isa = atomlang_class_fiber;
    fiber->caller = NULL;
    fiber->result = VALUE_FROM_NULL;

    if (nstack < DEFAULT_MINSTACK_SIZE) nstack = DEFAULT_MINSTACK_SIZE;
    fiber->stack = (atomlang_value_t *)mem_alloc(NULL, sizeof(atomlang_value_t) * nstack);
    fiber->stacktop = fiber->stack;
    fiber->stackalloc = nstack;

    if (nframes < DEFAULT_MINCFRAME_SIZE) nframes = DEFAULT_MINCFRAME_SIZE;
    fiber->frames = (atomlang_callframe_t *)mem_alloc(NULL, sizeof(atomlang_callframe_t) * nframes);
    fiber->framesalloc = nframes;
    fiber->nframes = 1;

    fiber->upvalues = NULL;

    atomlang_callframe_t *frame = &fiber->frames[0];
    if (closure) {
        frame->closure = closure;
        frame->ip = (closure->f->tag == EXEC_TYPE_NATIVE) ? closure->f->bytecode : NULL;
    }
    frame->dest = 0;
    frame->stackstart = fiber->stack;

    frame->stackstart[0] = VALUE_FROM_OBJECT(fiber);
    
    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) fiber);
    return fiber;
}

void atomlang_fiber_free (atomlang_vm *vm, atomlang_fiber_t *fiber) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)fiber, true));
    if (fiber->error) mem_free(fiber->error);
    mem_free(fiber->stack);
    mem_free(fiber->frames);
    mem_free(fiber);
}

void atomlang_fiber_reassign (atomlang_fiber_t *fiber, atomlang_closure_t *closure, uint16_t nargs) {
    atomlang_callframe_t *frame = &fiber->frames[0];
    frame->closure = closure;
    frame->ip = (closure->f->tag == EXEC_TYPE_NATIVE) ? closure->f->bytecode : NULL;

    frame->dest = 0;
    frame->stackstart = fiber->stack;

    fiber->nframes = 1;
    fiber->upvalues = NULL;

    fiber->stacktop += FN_COUNTREG(closure->f, nargs);
}

void atomlang_fiber_reset (atomlang_fiber_t *fiber) {
    fiber->caller = NULL;
    fiber->result = VALUE_FROM_NULL;
    fiber->nframes = 0;
    fiber->upvalues = NULL;
    fiber->stacktop = fiber->stack;
}

void atomlang_fiber_seterror (atomlang_fiber_t *fiber, const char *error) {
    if (fiber->error) mem_free(fiber->error);
    fiber->error = (char *)string_dup(error);
}

uint32_t atomlang_fiber_size (atomlang_vm *vm, atomlang_fiber_t *fiber) {
    SET_OBJECT_VISITED_FLAG(fiber, true);
    
    uint32_t fiber_size = sizeof(atomlang_fiber_t);
    fiber_size += fiber->stackalloc * sizeof(atomlang_value_t);
    fiber_size += fiber->framesalloc * sizeof(atomlang_callframe_t);

    for (atomlang_value_t* slot = fiber->stack; slot < fiber->stacktop; ++slot) {
        fiber_size += atomlang_value_size(vm, *slot);
    }

    fiber_size += string_size(fiber->error);
    fiber_size += atomlang_object_size(vm, (atomlang_object_t *)fiber->caller);

    SET_OBJECT_VISITED_FLAG(fiber, false);
    return fiber_size;
}

void atomlang_fiber_blacken (atomlang_vm *vm, atomlang_fiber_t *fiber) {
    atomlang_vm_memupdate(vm, atomlang_fiber_size(vm, fiber));
    
    for (uint32_t i=0; i < fiber->nframes; ++i) {
        atomlang_gray_object(vm, (atomlang_object_t *)fiber->frames[i].closure);
		atomlang_gray_object(vm, (atomlang_object_t *)fiber->frames[i].args);
    }

    for (atomlang_value_t *slot = fiber->stack; slot < fiber->stacktop; ++slot) {
        atomlang_gray_value(vm, *slot);
    }

    atomlang_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue) {
        atomlang_gray_object(vm, (atomlang_object_t *)upvalue);
        upvalue = upvalue->next;
    }

    atomlang_gray_object(vm, (atomlang_object_t *)fiber->caller);
}

void atomlang_object_serialize (atomlang_object_t *obj, json_t *json) {
    if (obj->isa == atomlang_class_function)
        atomlang_function_serialize((atomlang_function_t *)obj, json);
    else if (obj->isa == atomlang_class_class)
        atomlang_class_serialize((atomlang_class_t *)obj, json);
    else assert(0);
}

atomlang_object_t *atomlang_object_deserialize (atomlang_vm *vm, json_value *entry) {

    if (entry->type != json_object) return NULL;
    if (entry->u.object.length == 0) return NULL;

    const char *label = entry->u.object.values[0].name;
    json_value *value = entry->u.object.values[0].value;
    
    if (string_casencmp(label, ATOMLANG_JSON_LABELTYPE, 4) != 0) {
        atomlang_map_t *m = atomlang_map_deserialize(vm, entry);
        return (atomlang_object_t *)m;
    }
    
    if (value->type != json_string) return NULL;

    if (string_casencmp(value->u.string.ptr, ATOMLANG_JSON_FUNCTION, value->u.string.length) == 0) {
        atomlang_function_t *f = atomlang_function_deserialize(vm, entry);
        return (atomlang_object_t *)f;
    }

    if (string_casencmp(value->u.string.ptr, ATOMLANG_JSON_CLASS, value->u.string.length) == 0) {
        atomlang_class_t *c = atomlang_class_deserialize(vm, entry);
        return (atomlang_object_t *)c;
    }

    if ((string_casencmp(value->u.string.ptr, ATOMLANG_JSON_MAP, value->u.string.length) == 0) ||
        (string_casencmp(value->u.string.ptr, ATOMLANG_JSON_ENUM, value->u.string.length) == 0)) {
        atomlang_map_t *m = atomlang_map_deserialize(vm, entry);
        return (atomlang_object_t *)m;
    }
    
    if (string_casencmp(value->u.string.ptr, ATOMLANG_JSON_RANGE, value->u.string.length) == 0) {
        atomlang_range_t *range = atomlang_range_deserialize(vm, entry);
        return (atomlang_object_t *)range;
    }

    DEBUG_DESERIALIZE("atomlang_object_deserialize unknown type");
    return NULL;
}
#undef REPORT_JSON_ERROR

const char *atomlang_object_debug (atomlang_object_t *obj, bool is_free) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return "";

    if (OBJECT_ISA_INT(obj)) return "INT";
    if (OBJECT_ISA_FLOAT(obj)) return "FLOAT";
    if (OBJECT_ISA_BOOL(obj)) return "BOOL";
    if (OBJECT_ISA_NULL(obj)) return "NULL";

    static char buffer[512];
    if (OBJECT_ISA_FUNCTION(obj)) {
        const char *name = ((atomlang_function_t*)obj)->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "FUNCTION %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_CLOSURE(obj)) {
        const char *name = (is_free) ? NULL : ((atomlang_closure_t*)obj)->f->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "CLOSURE %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_CLASS(obj)) {
        const char *name = ((atomlang_class_t*)obj)->identifier;
        if (!name) name = "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "CLASS %p %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_STRING(obj)) {
        snprintf(buffer, sizeof(buffer), "STRING %p %s", obj, ((atomlang_string_t*)obj)->s);
        return buffer;
    }

    if (OBJECT_ISA_INSTANCE(obj)) {
        atomlang_class_t *c = (is_free) ? NULL : ((atomlang_instance_t*)obj)->objclass;
        const char *name = (c && c->identifier) ? c->identifier : "ANONYMOUS";
        snprintf(buffer, sizeof(buffer), "INSTANCE %p OF %s", obj, name);
        return buffer;
    }

    if (OBJECT_ISA_RANGE(obj)) {
        snprintf(buffer, sizeof(buffer), "RANGE %p %ld %ld", obj, (long)((atomlang_range_t*)obj)->from, (long)((atomlang_range_t*)obj)->to);
        return buffer;
    }

    if (OBJECT_ISA_LIST(obj)) {
        snprintf(buffer, sizeof(buffer), "LIST %p (%ld items)", obj, (long)marray_size(((atomlang_list_t*)obj)->array));
        return buffer;
    }

    if (OBJECT_ISA_MAP(obj)) {
         snprintf(buffer, sizeof(buffer), "MAP %p (%ld items)", obj, (long)atomlang_hash_count(((atomlang_map_t*)obj)->hash));
         return buffer;
    }

    if (OBJECT_ISA_FIBER(obj)) {
        snprintf(buffer, sizeof(buffer), "FIBER %p", obj);
        return buffer;
    }

    if (OBJECT_ISA_UPVALUE(obj)) {
        snprintf(buffer, sizeof(buffer), "UPVALUE %p", obj);
        return buffer;
    }

    return "N/A";
}

void atomlang_object_free (atomlang_vm *vm, atomlang_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return;
    
    if (obj->gc.free) obj->gc.free(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) atomlang_class_free(vm, (atomlang_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) atomlang_function_free(vm, (atomlang_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) atomlang_closure_free(vm, (atomlang_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) atomlang_instance_free(vm, (atomlang_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) atomlang_list_free(vm, (atomlang_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) atomlang_map_free(vm, (atomlang_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) atomlang_fiber_free(vm, (atomlang_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) atomlang_range_free(vm, (atomlang_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) atomlang_module_free(vm, (atomlang_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) atomlang_string_free(vm, (atomlang_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) atomlang_upvalue_free(vm, (atomlang_upvalue_t *)obj);
    else assert(0);
}

uint32_t atomlang_object_size (atomlang_vm *vm, atomlang_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return 0;
    
    if (obj->gc.visited) return 0;
    
    if (obj->gc.size) return obj->gc.size(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) return atomlang_class_size(vm, (atomlang_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) return atomlang_function_size(vm, (atomlang_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) return atomlang_closure_size(vm, (atomlang_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) return atomlang_instance_size(vm, (atomlang_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) return atomlang_list_size(vm, (atomlang_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) return atomlang_map_size(vm, (atomlang_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) return atomlang_fiber_size(vm, (atomlang_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) return atomlang_range_size(vm, (atomlang_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) return atomlang_module_size(vm, (atomlang_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) return atomlang_string_size(vm, (atomlang_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) return atomlang_upvalue_size(vm, (atomlang_upvalue_t *)obj);
    return 0;
}

void atomlang_object_blacken (atomlang_vm *vm, atomlang_object_t *obj) {
    if ((!obj) || (!OBJECT_IS_VALID(obj))) return;

    if (obj->gc.blacken) obj->gc.blacken(vm, obj);
    else if (OBJECT_ISA_CLASS(obj)) atomlang_class_blacken(vm, (atomlang_class_t *)obj);
    else if (OBJECT_ISA_FUNCTION(obj)) atomlang_function_blacken(vm, (atomlang_function_t *)obj);
    else if (OBJECT_ISA_CLOSURE(obj)) atomlang_closure_blacken(vm, (atomlang_closure_t *)obj);
    else if (OBJECT_ISA_INSTANCE(obj)) atomlang_instance_blacken(vm, (atomlang_instance_t *)obj);
    else if (OBJECT_ISA_LIST(obj)) atomlang_list_blacken(vm, (atomlang_list_t *)obj);
    else if (OBJECT_ISA_MAP(obj)) atomlang_map_blacken(vm, (atomlang_map_t *)obj);
    else if (OBJECT_ISA_FIBER(obj)) atomlang_fiber_blacken(vm, (atomlang_fiber_t *)obj);
    else if (OBJECT_ISA_RANGE(obj)) atomlang_range_blacken(vm, (atomlang_range_t *)obj);
    else if (OBJECT_ISA_MODULE(obj)) atomlang_module_blacken(vm, (atomlang_module_t *)obj);
    else if (OBJECT_ISA_STRING(obj)) atomlang_string_blacken(vm, (atomlang_string_t *)obj);
    else if (OBJECT_ISA_UPVALUE(obj)) atomlang_upvalue_blacken(vm, (atomlang_upvalue_t *)obj);
}

atomlang_instance_t *atomlang_instance_new (atomlang_vm *vm, atomlang_class_t *c) {
    atomlang_instance_t *instance = (atomlang_instance_t *)mem_alloc(NULL, sizeof(atomlang_instance_t));

    instance->isa = atomlang_class_instance;
    instance->objclass = c;
    
    if (c->nivars) instance->ivars = (atomlang_value_t *)mem_alloc(NULL, c->nivars * sizeof(atomlang_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) instance->ivars[i] = VALUE_FROM_NULL;
    
    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) instance);
    return instance;
}

atomlang_instance_t *atomlang_instance_clone (atomlang_vm *vm, atomlang_instance_t *src_instance) {
    atomlang_class_t *c = src_instance->objclass;
  
    atomlang_instance_t *instance = (atomlang_instance_t *)mem_alloc(NULL, sizeof(atomlang_instance_t));
    instance->isa = atomlang_class_instance;
    instance->objclass = c;
    
    if (atomlang_class_is_anon(c)) {
    }
    
    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);

    instance->xdata = (src_instance->xdata && delegate->bridge_clone) ? delegate->bridge_clone(vm, src_instance->xdata) : NULL;
    
    if (c->nivars) instance->ivars = (atomlang_value_t *)mem_alloc(NULL, c->nivars * sizeof(atomlang_value_t));
    for (uint32_t i=0; i<c->nivars; ++i) instance->ivars[i] = src_instance->ivars[i];

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) instance);
    return instance;
}

void atomlang_instance_setivar (atomlang_instance_t *instance, uint32_t idx, atomlang_value_t value) {
    if (idx < instance->objclass->nivars) instance->ivars[idx] = value;
}

void atomlang_instance_setxdata (atomlang_instance_t *i, void *xdata) {
    i->xdata = xdata;
}

void atomlang_instance_deinit (atomlang_vm *vm, atomlang_instance_t *i) {
    STATICVALUE_FROM_STRING(key, CLASS_DESTRUCTOR_NAME, strlen(CLASS_DESTRUCTOR_NAME));
    
    atomlang_closure_t *closure = atomlang_class_lookup_closure(i->objclass, key);
    if (closure) atomlang_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(i), NULL, 0);
}

void atomlang_instance_free (atomlang_vm *vm, atomlang_instance_t *i) {
    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)i, true));

    if (i->xdata && vm) {
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (delegate->bridge_free) delegate->bridge_free(vm, (atomlang_object_t *)i);
    }

    if (i->ivars) mem_free(i->ivars);
    mem_free((void *)i);
}

atomlang_closure_t *atomlang_instance_lookup_event (atomlang_instance_t *i, const char *name) {

    STATICVALUE_FROM_STRING(key, name, strlen(name));
    atomlang_class_t *c = i->objclass;
    while (c) {
        atomlang_value_t *v = atomlang_hash_lookup(c->htable, key);

        if ((v) && (OBJECT_ISA_CLOSURE(v->p))) return (atomlang_closure_t *)v->p;
        c = c->superclass;
    }
    return NULL;
}

atomlang_value_t atomlang_instance_lookup_property (atomlang_vm *vm, atomlang_instance_t *i, atomlang_value_t key) {
    atomlang_closure_t *closure = atomlang_class_lookup_closure(i->objclass, key);
    if (!closure) return VALUE_NOT_VALID;
    
    atomlang_function_t *func = closure->f;
    if (!func || func->tag != EXEC_TYPE_SPECIAL) return VALUE_NOT_VALID;
    
    if (FUNCTION_ISA_GETTER(func)) {
        atomlang_closure_t *getter = (atomlang_closure_t *)func->special[EXEC_TYPE_SPECIAL_GETTER];
        if (atomlang_vm_runclosure(vm, getter, VALUE_FROM_NULL, NULL, 0)) return atomlang_vm_result(vm);
    }
    
    return i->ivars[func->index];
}

uint32_t atomlang_instance_size (atomlang_vm *vm, atomlang_instance_t *i) {
    SET_OBJECT_VISITED_FLAG(i, true);
    
    uint32_t instance_size = sizeof(atomlang_instance_t) + (i->objclass->nivars * sizeof(atomlang_value_t));

    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (i->xdata && delegate->bridge_size)
        instance_size += delegate->bridge_size(vm, i->xdata);

    SET_OBJECT_VISITED_FLAG(i, false);
    return instance_size;
}

void atomlang_instance_blacken (atomlang_vm *vm, atomlang_instance_t *i) {
    atomlang_vm_memupdate(vm, atomlang_instance_size(vm, i));

    atomlang_gray_object(vm, (atomlang_object_t *)i->objclass);

    for (uint32_t j=0; j<i->objclass->nivars; ++j) {
        atomlang_gray_value(vm, i->ivars[j]);
    }
    
    if (i->xdata) {
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (delegate->bridge_blacken) delegate->bridge_blacken(vm, i->xdata);
    }
}

void atomlang_instance_serialize (atomlang_instance_t *instance, json_t *json) {
    atomlang_class_t *c = instance->objclass;
    
    const char *label = json_get_label(json, NULL);
    json_begin_object(json, label);
    
    json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_INSTANCE);
    json_add_cstring(json, ATOMLANG_JSON_CLASS, c->identifier);
    
    if (c->nivars) {
        json_begin_array(json, ATOMLANG_JSON_LABELIVAR);
        for (uint32_t i=0; i<c->nivars; ++i) {
            atomlang_value_serialize(NULL, instance->ivars[i], json);
        }
        json_end_array(json);
    }
    
    json_end_object(json);
}

bool atomlang_instance_isstruct (atomlang_instance_t *i) {
    return i->objclass->is_struct;
}

static bool hash_value_compare_cb (atomlang_value_t v1, atomlang_value_t v2, void *data) {
    #pragma unused (data)
    return atomlang_value_equals(v1, v2);
}

bool atomlang_value_vm_equals (atomlang_vm *vm, atomlang_value_t v1, atomlang_value_t v2) {
    bool result = atomlang_value_equals(v1, v2);
    if (result || !vm) return result;

    if (!(VALUE_ISA_INSTANCE(v1) && VALUE_ISA_INSTANCE(v2))) return false;

    atomlang_instance_t *obj1 = (atomlang_instance_t *)VALUE_AS_OBJECT(v1);
    atomlang_instance_t *obj2 = (atomlang_instance_t *)VALUE_AS_OBJECT(v2);

    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (obj1->xdata && obj2->xdata && delegate->bridge_equals) {
        return delegate->bridge_equals(vm, obj1->xdata, obj2->xdata);
    }

    return false;
}

bool atomlang_value_equals (atomlang_value_t v1, atomlang_value_t v2) {

    if (v1.isa != v2.isa) return false;

    if ((v1.isa == atomlang_class_int) || (v1.isa == atomlang_class_bool) || (v1.isa == atomlang_class_null)) {
        return (v1.n == v2.n);
    } else if (v1.isa == atomlang_class_float) {
        #if ATOMLANG_ENABLE_DOUBLE
        return (fabs(v1.f - v2.f) < EPSILON);
        #else
        return (fabsf(v1.f - v2.f) < EPSILON);
        #endif
    } else if (v1.isa == atomlang_class_string) {
        atomlang_string_t *s1 = VALUE_AS_STRING(v1);
        atomlang_string_t *s2 = VALUE_AS_STRING(v2);
        if (s1->hash != s2->hash) return false;
        if (s1->len != s2->len) return false;
        return (memcmp(s1->s, s2->s, s1->len) == 0);
    } else if (v1.isa == atomlang_class_range) {
        atomlang_range_t *r1 = VALUE_AS_RANGE(v1);
        atomlang_range_t *r2 = VALUE_AS_RANGE(v2);
        return ((r1->from == r2->from) && (r1->to == r2->to));
    } else if (v1.isa == atomlang_class_list) {
        atomlang_list_t *list1 = VALUE_AS_LIST(v1);
        atomlang_list_t *list2 = VALUE_AS_LIST(v2);
        if (marray_size(list1->array) != marray_size(list2->array)) return false;
        size_t count = marray_size(list1->array);
        for (size_t i=0; i<count; ++i) {
            atomlang_value_t value1 = marray_get(list1->array, i);
            atomlang_value_t value2 = marray_get(list2->array, i);
            if (!atomlang_value_equals(value1, value2)) return false;
        }
        return true;
    } else if (v1.isa == atomlang_class_map) {
        atomlang_map_t *map1 = VALUE_AS_MAP(v1);
        atomlang_map_t *map2 = VALUE_AS_MAP(v2);
        return atomlang_hash_compare(map1->hash, map2->hash, hash_value_compare_cb, NULL);
    }

    atomlang_object_t *obj1 = VALUE_AS_OBJECT(v1);
    atomlang_object_t *obj2 = VALUE_AS_OBJECT(v2);
    if (obj1->isa != obj2->isa) return false;

    return (obj1 == obj2);
}

uint32_t atomlang_value_hash (atomlang_value_t value) {
    if (value.isa == atomlang_class_string)
        return VALUE_AS_STRING(value)->hash;

    if ((value.isa == atomlang_class_int) || (value.isa == atomlang_class_bool) || (value.isa == atomlang_class_null))
        return atomlang_hash_compute_int(value.n);

    if (value.isa == atomlang_class_float)
        return atomlang_hash_compute_float(value.f);

    return atomlang_hash_compute_buffer((const char *)value.p, sizeof(atomlang_object_t*));
}

inline atomlang_class_t *atomlang_value_getclass (atomlang_value_t v) {
    if ((v.isa == atomlang_class_class) && (v.p->objclass == atomlang_class_object)) return (atomlang_class_t *)v.p;
    if ((v.isa == atomlang_class_instance) || (v.isa == atomlang_class_class)) return (v.p) ? v.p->objclass : NULL;
    return v.isa;
}

inline atomlang_class_t *atomlang_value_getsuper (atomlang_value_t v) {
    atomlang_class_t *c = atomlang_value_getclass(v);
    return (c && c->superclass) ? c->superclass : NULL;
}

void atomlang_value_free (atomlang_vm *vm, atomlang_value_t v) {
    if (!atomlang_value_isobject(v)) return;
    atomlang_object_free(vm, VALUE_AS_OBJECT(v));
}

static void atomlang_map_serialize_iterator (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t v, void *data) {
    #pragma unused(hashtable)
    assert(key.isa == atomlang_class_string);

    json_t *json = (json_t *)data;
    const char *key_value = VALUE_AS_STRING(key)->s;

    atomlang_value_serialize(key_value, v, json);
}

void atomlang_value_serialize (const char *key, atomlang_value_t v, json_t *json) {
    if (VALUE_ISA_NULL(v)) {
        json_add_null(json, key);
        return;
    }
    
    if (VALUE_ISA_UNDEFINED(v)) {
        if (json_option_isset(json, json_opt_no_undef)) {
            json_add_null(json, key);
        } else {
            json_begin_object(json, key);
            json_end_object(json);
        }
        return;
    }

    if (VALUE_ISA_BOOL(v)) {
        json_add_bool(json, key, (v.n == 0) ? false : true);
        return;
    }

    if (VALUE_ISA_INT(v)) {
        json_add_int(json, key, (int64_t)v.n);
        return;
    }

    if (VALUE_ISA_FLOAT(v)) {
        json_add_double(json, key, (double)v.f);
        return;
    }

    if (VALUE_ISA_FUNCTION(v)) {
        json_set_label(json, key);
        atomlang_function_serialize(VALUE_AS_FUNCTION(v), json);
        return;
    }
    
    if (VALUE_ISA_CLOSURE(v)) {
        json_set_label(json, key);
        atomlang_function_serialize(VALUE_AS_CLOSURE(v)->f, json);
        return;
    }

    if (VALUE_ISA_CLASS(v)) {
        json_set_label(json, key);
        atomlang_class_serialize(VALUE_AS_CLASS(v), json);
        return;
    }

    if (VALUE_ISA_STRING(v)) {
        atomlang_string_t *value = VALUE_AS_STRING(v);
        json_add_string(json, key, value->s, value->len);
        return;
    }

    if (VALUE_ISA_LIST(v)) {
        atomlang_list_t *value = VALUE_AS_LIST(v);
        json_begin_array(json, key);
        size_t count = marray_size(value->array);
        for (size_t j=0; j<count; j++) {
            atomlang_value_t item = marray_get(value->array, j);

            atomlang_value_serialize(NULL, item, json);
        }
        json_end_array(json);
        return;
    }

    if (VALUE_ISA_MAP(v)) {
        atomlang_map_t *value = VALUE_AS_MAP(v);
        json_begin_object(json, key);
        if (!json_option_isset(json, json_opt_no_maptype)) json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_MAP);
        atomlang_hash_iterate(value->hash, atomlang_map_serialize_iterator, json);
        json_end_object(json);
        return;
    }
    
    if (VALUE_ISA_RANGE(v)) {
        json_set_label(json, key);
        atomlang_range_serialize(VALUE_AS_RANGE(v), json);
        return;
    }

    if (VALUE_ISA_INSTANCE(v)) {
        json_set_label(json, key);
        atomlang_instance_serialize(VALUE_AS_INSTANCE(v), json);
        return;
    }
    
    if (VALUE_ISA_FIBER(v)) {
        return;
    }
    
    assert(0);
}

bool atomlang_value_isobject (atomlang_value_t v) {

    if ((v.isa == NULL) || (v.isa == atomlang_class_int) || (v.isa == atomlang_class_float) ||
        (v.isa == atomlang_class_bool) || (v.isa == atomlang_class_null) || (v.p == NULL)) return false;
    
    if ((v.isa == atomlang_class_string) || (v.isa == atomlang_class_object) || (v.isa == atomlang_class_function) ||
        (v.isa == atomlang_class_closure) || (v.isa == atomlang_class_fiber) || (v.isa == atomlang_class_class) ||
        (v.isa == atomlang_class_instance) || (v.isa == atomlang_class_module) || (v.isa == atomlang_class_list) ||
        (v.isa == atomlang_class_map) || (v.isa == atomlang_class_range) || (v.isa == atomlang_class_upvalue)) return true;
    
    return false;
}

uint32_t atomlang_value_size (atomlang_vm *vm, atomlang_value_t v) {
    return (atomlang_value_isobject(v)) ? atomlang_object_size(vm, (atomlang_object_t*)v.p) : 0;
}

void *atomlang_value_xdata (atomlang_value_t value) {
    if (VALUE_ISA_INSTANCE(value)) {
        atomlang_instance_t *i = VALUE_AS_INSTANCE(value);
        return i->xdata;
    } else if (VALUE_ISA_CLASS(value)) {
        atomlang_class_t *c = VALUE_AS_CLASS(value);
        return c->xdata;
    }
    return NULL;
}

const char *atomlang_value_name (atomlang_value_t value) {
    if (VALUE_ISA_INSTANCE(value)) {
        atomlang_instance_t *instance = VALUE_AS_INSTANCE(value);
        return instance->objclass->identifier;
    } else if (VALUE_ISA_CLASS(value)) {
        atomlang_class_t *c = VALUE_AS_CLASS(value);
        return c->identifier;
    }
    return NULL;
}

void atomlang_value_dump (atomlang_vm *vm, atomlang_value_t v, char *buffer, uint16_t len) {
    const char *type = NULL;
    const char *value = NULL;
    char        sbuffer[1024];

    if (buffer == NULL) buffer = sbuffer;
    if (len == 0) len = sizeof(sbuffer);

    if (v.isa == NULL) {
        type = "INVALID!";
        snprintf(buffer, len, "%s", type);
        value = buffer;
    } else if (v.isa == atomlang_class_bool) {
        type = "BOOL";
        value = (v.n == 0) ? "false" : "true";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    } else if (v.isa == atomlang_class_null) {
        type = (v.n == 0) ? "NULL" : "UNDEFINED";
        snprintf(buffer, len, "%s", type);
        value = buffer;
    } else if (v.isa == atomlang_class_int) {
        type = "INT";
        snprintf(buffer, len, "(%s) %" PRId64, type, v.n);
        value = buffer;
    } else if (v.isa == atomlang_class_float) {
        type = "FLOAT";
        snprintf(buffer, len, "(%s) %g", type, v.f);
        value = buffer;
    } else if (v.isa == atomlang_class_function) {
        type = "FUNCTION";
        value = VALUE_AS_FUNCTION(v)->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_FUNCTION(v));
        value = buffer;
    } else if (v.isa == atomlang_class_closure) {
        type = "CLOSURE";
        atomlang_function_t *f = VALUE_AS_CLOSURE(v)->f;
        value = (f->identifier) ? (f->identifier) : "anon";
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_CLOSURE(v));
        value = buffer;
    } else if (v.isa == atomlang_class_class) {
        type = "CLASS";
        value = VALUE_AS_CLASS(v)->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, VALUE_AS_CLASS(v));
        value = buffer;
    } else if (v.isa == atomlang_class_string) {
        type = "STRING";
        atomlang_string_t *s = VALUE_AS_STRING(v);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == atomlang_class_instance) {
        type = "INSTANCE";
        atomlang_instance_t *i = VALUE_AS_INSTANCE(v);
        atomlang_class_t *c = i->objclass;
        value = c->identifier;
        snprintf(buffer, len, "(%s) %s (%p)", type, value, i);
        value = buffer;
    } else if (v.isa == atomlang_class_list) {
        type = "LIST";
        atomlang_value_t sval = convert_value2string(vm, v);
        atomlang_string_t *s = VALUE_AS_STRING(sval);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == atomlang_class_map) {
        type = "MAP";
        atomlang_value_t sval = convert_value2string(vm, v);
        atomlang_string_t *s = VALUE_AS_STRING(sval);
        snprintf(buffer, len, "(%s) %.*s (%p)", type, s->len, s->s, s);
        value = buffer;
    } else if (v.isa == atomlang_class_range) {
        type = "RANGE";
        atomlang_range_t *r = VALUE_AS_RANGE(v);
        snprintf(buffer, len, "(%s) from %" PRId64 " to %" PRId64, type, r->from, r->to);
        value = buffer;
    } else if (v.isa == atomlang_class_object) {
        type = "OBJECT";
        value = "N/A";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    } else if (v.isa == atomlang_class_fiber) {
        type = "FIBER";
        snprintf(buffer, len, "(%s) %p", type, v.p);
        value = buffer;
    } else {
        type = "N/A";
        value = "N/A";
        snprintf(buffer, len, "(%s) %s", type, value);
        value = buffer;
    }

    if (buffer == sbuffer) printf("%s\n", value);
}

atomlang_list_t *atomlang_list_new (atomlang_vm *vm, uint32_t n) {
    if (n > MAX_ALLOCATION) return NULL;

    atomlang_list_t *list = (atomlang_list_t *)mem_alloc(NULL, sizeof(atomlang_list_t));

    list->isa = atomlang_class_list;
    marray_init(list->array);
    marray_resize(atomlang_value_t, list->array, n + MARRAY_DEFAULT_SIZE);

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) list);
    return list;
}

atomlang_list_t *atomlang_list_from_array (atomlang_vm *vm, uint32_t n, atomlang_value_t *p) {
    atomlang_list_t *list = (atomlang_list_t *)mem_alloc(NULL, sizeof(atomlang_list_t));

    list->isa = atomlang_class_list;
    marray_init(list->array);

    for (size_t i=0; i<n; ++i) marray_push(atomlang_value_t, list->array, p[i]);

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) list);
    return list;
}

void atomlang_list_free (atomlang_vm *vm, atomlang_list_t *list) {
    #pragma unused(vm)
    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)list, true));
    marray_destroy(list->array);
    mem_free((void *)list);
}

void atomlang_list_append_list (atomlang_vm *vm, atomlang_list_t *list1, atomlang_list_t *list2) {
    #pragma unused(vm)

    size_t count = marray_size(list2->array);
    for (size_t i=0; i<count; ++i) {
        marray_push(atomlang_value_t, list1->array, marray_get(list2->array, i));
    }
}

uint32_t atomlang_list_size (atomlang_vm *vm, atomlang_list_t *list) {
    SET_OBJECT_VISITED_FLAG(list, true);
    
    uint32_t internal_size = 0;
    size_t count = marray_size(list->array);
    for (size_t i=0; i<count; ++i) {
        internal_size += atomlang_value_size(vm, marray_get(list->array, i));
    }
    internal_size += sizeof(atomlang_list_t);
    
    SET_OBJECT_VISITED_FLAG(list, false);
    return internal_size;
}

void atomlang_list_blacken (atomlang_vm *vm, atomlang_list_t *list) {
    atomlang_vm_memupdate(vm, atomlang_list_size(vm, list));

    size_t count = marray_size(list->array);
    for (size_t i=0; i<count; ++i) {
        atomlang_gray_value(vm, marray_get(list->array, i));
    }
}

atomlang_map_t *atomlang_map_new (atomlang_vm *vm, uint32_t n) {
    atomlang_map_t *map = (atomlang_map_t *)mem_alloc(NULL, sizeof(atomlang_map_t));

    map->isa = atomlang_class_map;
    map->hash = atomlang_hash_create(n, atomlang_value_hash, atomlang_value_equals, NULL, NULL);

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) map);
    return map;
}

void atomlang_map_free (atomlang_vm *vm, atomlang_map_t *map) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)map, true));
    atomlang_hash_free(map->hash);
    mem_free((void *)map);
}

void atomlang_map_append_map (atomlang_vm *vm, atomlang_map_t *map1, atomlang_map_t *map2) {
    #pragma unused(vm)

    atomlang_hash_append(map1->hash, map2->hash);
}

void atomlang_map_insert (atomlang_vm *vm, atomlang_map_t *map, atomlang_value_t key, atomlang_value_t value) {
    #pragma unused(vm)
    atomlang_hash_insert(map->hash, key, value);
}

static atomlang_map_t *atomlang_map_deserialize (atomlang_vm *vm, json_value *json) {
    uint32_t n = json->u.object.length;
    atomlang_map_t *map = atomlang_map_new(vm, n);

    DEBUG_DESERIALIZE("DESERIALIZE MAP: %p\n", map);

    for (uint32_t i=0; i<n; ++i) { 
        const char *label = json->u.object.values[i].name;
        json_value *jsonv = json->u.object.values[i].value;

        atomlang_value_t key = VALUE_FROM_CSTRING(vm, label);
        atomlang_value_t value;

        switch (jsonv->type) {
            case json_integer:
                value = VALUE_FROM_INT((atomlang_int_t)jsonv->u.integer); break;
            case json_double:
                value = VALUE_FROM_FLOAT((atomlang_float_t)jsonv->u.dbl); break;
            case json_boolean:
                value = VALUE_FROM_BOOL(jsonv->u.boolean); break;
            case json_string:
                value = VALUE_FROM_STRING(vm, jsonv->u.string.ptr, jsonv->u.string.length); break;
            case json_null:
                value = VALUE_FROM_NULL; break;
            case json_object: {
                atomlang_object_t *obj = atomlang_object_deserialize(vm, jsonv);
                value = (obj) ? VALUE_FROM_OBJECT(obj) : VALUE_FROM_NULL;
                break;
            }
            case json_array: {
                
            }
            default:
                goto abort_load;
        }

        atomlang_map_insert(NULL, map, key, value);
    }
    return map;

abort_load:

    return NULL;
}

uint32_t atomlang_map_size (atomlang_vm *vm, atomlang_map_t *map) {
    SET_OBJECT_VISITED_FLAG(map, true);
    
    uint32_t hash_size = 0;
    atomlang_hash_iterate2(map->hash, atomlang_hash_internalsize, (void *)&hash_size, (void *)vm);
    hash_size += atomlang_hash_memsize(map->hash);
    hash_size += sizeof(atomlang_map_t);
    
    SET_OBJECT_VISITED_FLAG(map, false);
    return hash_size;
}

void atomlang_map_blacken (atomlang_vm *vm, atomlang_map_t *map) {
    atomlang_vm_memupdate(vm, atomlang_map_size(vm, map));
    atomlang_hash_iterate(map->hash, atomlang_hash_gray, (void *)vm);
}

atomlang_range_t *atomlang_range_new (atomlang_vm *vm, atomlang_int_t from_range, atomlang_int_t to_range, bool inclusive) {
    atomlang_range_t *range = mem_alloc(NULL, sizeof(atomlang_range_t));

    range->isa = atomlang_class_range;
    range->from = from_range;
    range->to = (inclusive) ? to_range : --to_range;

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) range);
    return range;
}

void atomlang_range_free (atomlang_vm *vm, atomlang_range_t *range) {
    #pragma unused(vm)

    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)range, true));
    mem_free((void *)range);
}

uint32_t atomlang_range_size (atomlang_vm *vm, atomlang_range_t *range) {
    #pragma unused(vm, range)
    
    SET_OBJECT_VISITED_FLAG(range, true);
    uint32_t range_size = sizeof(atomlang_range_t);
    SET_OBJECT_VISITED_FLAG(range, false);
    
    return range_size;
}

void atomlang_range_serialize (atomlang_range_t *r, json_t *json) {
    const char *label = json_get_label(json, NULL);
    json_begin_object(json, label);
    json_add_cstring(json, ATOMLANG_JSON_LABELTYPE, ATOMLANG_JSON_RANGE);                    
    json_add_int(json, ATOMLANG_JSON_LABELFROM, r->from);
    json_add_int(json, ATOMLANG_JSON_LABELTO, r->to);
    json_end_object(json);
}

atomlang_range_t *atomlang_range_deserialize (atomlang_vm *vm, json_value *json) {
    json_int_t from = 0;
    json_int_t to = 0;
    
    uint32_t n = json->u.object.length;
    for (uint32_t i=1; i<n; ++i) { 
        const char *label = json->u.object.values[i].name;
        json_value *value = json->u.object.values[i].value;
        size_t label_size = strlen(label);
        
        if (string_casencmp(label, ATOMLANG_JSON_LABELFROM, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            from = value->u.integer;
            continue;
        }
        
        if (string_casencmp(label, ATOMLANG_JSON_LABELTO, label_size) == 0) {
            if (value->type != json_integer) goto abort_load;
            to = value->u.integer;
            continue;
        }
    }
    
    return atomlang_range_new(vm, (atomlang_int_t)from, (atomlang_int_t)to, true);
    
abort_load:
    return NULL;
}

void atomlang_range_blacken (atomlang_vm *vm, atomlang_range_t *range) {
    atomlang_vm_memupdate(vm, atomlang_range_size(vm, range));
}

inline atomlang_value_t atomlang_string_to_value (atomlang_vm *vm, const char *s, uint32_t len) {
    atomlang_string_t *obj = mem_alloc(NULL, sizeof(atomlang_string_t));
    if (len == AUTOLENGTH) len = (uint32_t)strlen(s);

    uint32_t alloc = MAXNUM(len+1, DEFAULT_MINSTRING_SIZE);
    char *ptr = mem_alloc(NULL, alloc);
    memcpy(ptr, s, len);

    obj->isa = atomlang_class_string;
    obj->s = ptr;
    obj->len = len;
    obj->alloc = alloc;
    obj->hash = atomlang_hash_compute_buffer((const char *)ptr, len);

    atomlang_value_t value;
    value.isa = atomlang_class_string;
    value.p = (atomlang_object_t *)obj;

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) obj);
    return value;
}

inline atomlang_string_t *atomlang_string_new (atomlang_vm *vm, char *s, uint32_t len, uint32_t alloc) {
    atomlang_string_t *obj = mem_alloc(NULL, sizeof(atomlang_string_t));
    if (len == AUTOLENGTH) len = (uint32_t)strlen(s);

    obj->isa = atomlang_class_string;
    obj->s = (char *)s;
    obj->len = len;
    obj->alloc = (alloc) ? alloc : len;
    if (s && len) obj->hash = atomlang_hash_compute_buffer((const char *)s, len);

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) obj);
    return obj;
}

inline void atomlang_string_set (atomlang_string_t *obj, char *s, uint32_t len) {
    obj->s = (char *)s;
    obj->len = len;
    obj->hash = atomlang_hash_compute_buffer((const char *)s, len);
}

inline void atomlang_string_free (atomlang_vm *vm, atomlang_string_t *value) {
    #pragma unused(vm)
    DEBUG_FREE("FREE %s", atomlang_object_debug((atomlang_object_t *)value, true));
    if (value->alloc) mem_free(value->s);
    mem_free(value);
}

uint32_t atomlang_string_size (atomlang_vm *vm, atomlang_string_t *string) {
    #pragma unused(vm)
    SET_OBJECT_VISITED_FLAG(string, true);
    uint32_t string_size = (sizeof(atomlang_string_t)) + string->alloc;
    SET_OBJECT_VISITED_FLAG(string, false);
    
    return string_size;
}

void atomlang_string_blacken (atomlang_vm *vm, atomlang_string_t *string) {
    atomlang_vm_memupdate(vm, atomlang_string_size(vm, string));
}

inline atomlang_value_t atomlang_value_from_error(const char* msg) {
    return ((atomlang_value_t){.isa = NULL, .p = ((atomlang_object_t *)msg)});
}

inline atomlang_value_t atomlang_value_from_object(void *obj) {
    return ((atomlang_value_t){.isa = (((atomlang_object_t *)(obj))->isa), .p = (atomlang_object_t *)(obj)});
}

inline atomlang_value_t atomlang_value_from_int(atomlang_int_t n) {
    return ((atomlang_value_t){.isa = atomlang_class_int, .n = (n)});
}

inline atomlang_value_t atomlang_value_from_float(atomlang_float_t f) {
    return ((atomlang_value_t){.isa = atomlang_class_float, .f = (f)});
}

inline atomlang_value_t atomlang_value_from_null(void) {
    return ((atomlang_value_t){.isa = atomlang_class_null, .n = 0});
}

inline atomlang_value_t atomlang_value_from_undefined(void) {
    return ((atomlang_value_t){.isa = atomlang_class_null, .n = 1});
}

inline atomlang_value_t atomlang_value_from_bool(bool b) {
    return ((atomlang_value_t){.isa = atomlang_class_bool, .n = (b)});
}