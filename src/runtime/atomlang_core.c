#include <inttypes.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "atomlang_core.h"
#include "atomlang_hash.h"
#include "atomlang_value.h"
#include "atomlang_debug.h"
#include "atomlang_opcodes.h"
#include "atomlang_macros.h"
#include "atomlang_memory.h"
#include "atomlang_vmmacros.h"

static bool core_inited = false;        // initialize global classes just once
static uint32_t refcount = 0;           // protect deallocation of global classes

// boxed
atomlang_class_t *atomlang_class_int;
atomlang_class_t *atomlang_class_float;
atomlang_class_t *atomlang_class_bool;
atomlang_class_t *atomlang_class_null;
// objects
atomlang_class_t *atomlang_class_string;
atomlang_class_t *atomlang_class_object;
atomlang_class_t *atomlang_class_function;
atomlang_class_t *atomlang_class_closure;
atomlang_class_t *atomlang_class_fiber;
atomlang_class_t *atomlang_class_class;
atomlang_class_t *atomlang_class_instance;
atomlang_class_t *atomlang_class_module;
atomlang_class_t *atomlang_class_list;
atomlang_class_t *atomlang_class_map;
atomlang_class_t *atomlang_class_range;
atomlang_class_t *atomlang_class_upvalue;
atomlang_class_t *atomlang_class_system;

typedef enum {
    number_format_any,
    number_format_int,
    number_format_float
} number_format_type;

typedef enum {
    introspection_info_all,
    introspection_info_methods,
    introspection_info_variables
} introspection_info_type;

static void map_keys_array (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused (hashtable, value)
    atomlang_list_t *list = (atomlang_list_t *)data;
    marray_push(atomlang_value_t, list->array, key);
}

static atomlang_value_t convert_string2number (atomlang_string_t *string, number_format_type number_format) {
    // empty string case
    if (string->len == 0) return (number_format == number_format_float) ? VALUE_FROM_FLOAT(0.0) : VALUE_FROM_INT(0);

    register const char *s = string->s;
    register uint32_t len = string->len;
    register int32_t sign = 1;

    // check sign first
    if ((s[0] == '-') || (s[0] == '+')) {
        if (s[0] == '-') sign = -1;
        ++s; --len;
    }

    // check special HEX, OCT, BIN cases
    if ((s[0] == '0') && (len > 2)) {
        int c = toupper(s[1]);
        bool isHexBinOct = ((c == 'B') || (c == 'O') || (c == 'X'));
        if (isHexBinOct) {
            int64_t n = 0;
            if (c == 'B') n = number_from_bin(&s[2], len-2);
            else if (c == 'O') n = number_from_oct(&s[2], len-2);
            else if (c == 'X') n = number_from_hex(s, len);
            if (sign == -1) n = -n;
            return (number_format == number_format_float) ? VALUE_FROM_FLOAT((atomlang_float_t)n) : VALUE_FROM_INT((atomlang_int_t)n);
        }
    }

    // if dot character is contained into the string than force the float_preferred flag
    if (number_format == number_format_any && (strchr(string->s, '.') != NULL)) number_format = number_format_float;
    
    // default case
    return (number_format == number_format_float) ? VALUE_FROM_FLOAT(strtod(string->s, NULL)) : VALUE_FROM_INT(strtoll(string->s, NULL, 0));
}

static bool convert_object_int (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t v = convert_value2int(vm, GET_VALUE(0));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Int.");
    RETURN_VALUE(v, rindex);
}

static bool convert_object_float (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t v = convert_value2float(vm, GET_VALUE(0));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Float.");
    RETURN_VALUE(v, rindex);
}

static bool convert_object_bool (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t v = convert_value2bool(vm, GET_VALUE(0));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Bool.");
    RETURN_VALUE(v, rindex);
}

static bool convert_object_string (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t v = convert_value2string(vm, GET_VALUE(0));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to String.");
    RETURN_VALUE(v, rindex);
}

static inline atomlang_value_t convert_map2string (atomlang_vm *vm, atomlang_map_t *map) {
    // allocate initial memory to a 512 buffer
    uint32_t len = 512;
    char *buffer = mem_alloc(NULL, len+1);
    buffer[0] = '[';
    uint32_t pos = 1;

    // get keys list
    uint32_t count = atomlang_hash_count(map->hash);
    atomlang_list_t *list = atomlang_list_new(vm, count);
    atomlang_hash_iterate(map->hash, map_keys_array, (void *)list);

    count = (uint32_t) marray_size(list->array);
    for (uint32_t i=0; i<count; ++i) {
        atomlang_value_t key = marray_get(list->array, i);
        atomlang_value_t *valueptr = atomlang_hash_lookup(map->hash, key);
        atomlang_value_t value = (valueptr) ? *valueptr : VALUE_FROM_NULL;
        
        // key conversion
        if (!VALUE_ISA_STRING(key)) {
            key = convert_value2string(vm, key);
        }
        atomlang_string_t *key_string = (VALUE_ISA_STRING(key)) ? VALUE_AS_STRING(key) : NULL;
        
        // value conversion
        if (VALUE_ISA_MAP(value) && (VALUE_AS_MAP(value) == map)) {
            // to avoid infinite loop
            value = VALUE_FROM_NULL;
        }
        if (!VALUE_ISA_STRING(value)) {
            value = convert_value2string(vm, value);
        }
        atomlang_string_t *value_string = (VALUE_ISA_STRING(value)) ? VALUE_AS_STRING(value) : NULL;
        
        // KEY
        char *s1 = (key_string) ? key_string->s : "N/A";
        uint32_t len1 = (key_string) ? key_string->len : 3;

        // VALUE
        char *s2 = (value_string) ? value_string->s : "N/A";
        uint32_t len2 = (value_string) ? value_string->len : 3;

        // check if buffer needs to be reallocated
        if (len1 + len2 + pos + 4 > len) {
            len = (len1 + len2 + pos + 4) + len;
            buffer = mem_realloc(NULL, buffer, len);
        }

        // copy key string to new buffer
        memcpy(buffer+pos, s1, len1);
        pos += len1;

        // copy ':' key/value separator
        memcpy(buffer+pos, ":", 1);
        pos += 1;

        // copy value string to new buffer
        memcpy(buffer+pos, s2, len2);
        pos += len2;

        // add entries separator
        if (i+1 < count) {
            memcpy(buffer+pos, ",", 1);
            pos += 1;
        }
    }

    // Write latest ] character
    memcpy(buffer+pos, "]", 1);
    buffer[++pos] = 0;

    atomlang_value_t result = VALUE_FROM_STRING(vm, buffer, pos);
    mem_free(buffer);
    return result;
}

static inline atomlang_value_t convert_list2string (atomlang_vm *vm, atomlang_list_t *list) {
    // allocate initial memory to a 512 buffer
    uint32_t len = 512;
    char *buffer = mem_alloc(NULL, len+1);
    buffer[0] = '[';
    uint32_t pos = 1;

    // loop to perform string concat
    uint32_t count = (uint32_t) marray_size(list->array);
    for (uint32_t i=0; i<count; ++i) {
        atomlang_value_t value = marray_get(list->array, i);
        atomlang_string_t *string;
        if (VALUE_ISA_LIST(value) && (VALUE_AS_LIST(value) == list)) {
            string = NULL;
        } else {
            atomlang_value_t value2 = convert_value2string(vm, value);
            string = VALUE_ISA_VALID(value2) ? VALUE_AS_STRING(value2) : NULL;
        }

        char *s1 = (string) ? string->s : "N/A";
        uint32_t len1 = (string) ? string->len : 3;

        // check if buffer needs to be reallocated
        if (len1+pos+2 > len) {
            len = (len1+pos+2) + len;
            buffer = mem_realloc(NULL, buffer, len);
        }

        // copy string to new buffer
        memcpy(buffer+pos, s1, len1);
        pos += len1;

        // add separator
        if (i+1 < count) {
            memcpy(buffer+pos, ",", 1);
            pos += 1;
        }
    }

    // Write latest ] character
    memcpy(buffer+pos, "]", 1);
    buffer[++pos] = 0;

    atomlang_value_t result = VALUE_FROM_STRING(vm, buffer, pos);
    mem_free(buffer);
    return result;
}

inline atomlang_value_t convert_value2int (atomlang_vm *vm, atomlang_value_t v) {
    if (VALUE_ISA_INT(v)) return v;

    // handle conversion for basic classes
    if (VALUE_ISA_FLOAT(v)) return VALUE_FROM_INT((atomlang_int_t)v.f);
    if (VALUE_ISA_BOOL(v)) return VALUE_FROM_INT(v.n);
    if (VALUE_ISA_NULL(v)) return VALUE_FROM_INT(0);
    if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_INT(0);
    if (VALUE_ISA_STRING(v)) return convert_string2number(VALUE_AS_STRING(v), number_format_int);

    // check if class implements the Int method
    atomlang_closure_t *closure = atomlang_vm_fastlookup(vm, atomlang_value_getclass(v), ATOMLANG_INT_INDEX);

    // sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_int)) ||
        atomlang_vm_getclosure(vm) == closure) return VALUE_FROM_ERROR(NULL);

    // execute closure and return its value
    if (atomlang_vm_runclosure(vm, closure, v, NULL, 0)) return atomlang_vm_result(vm);

    return VALUE_FROM_ERROR(NULL);
}

inline atomlang_value_t convert_value2float (atomlang_vm *vm, atomlang_value_t v) {
    if (VALUE_ISA_FLOAT(v)) return v;

    // handle conversion for basic classes
    if (VALUE_ISA_INT(v)) return VALUE_FROM_FLOAT((atomlang_float_t)v.n);
    if (VALUE_ISA_BOOL(v)) return VALUE_FROM_FLOAT((atomlang_float_t)v.n);
    if (VALUE_ISA_NULL(v)) return VALUE_FROM_FLOAT(0);
    if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_FLOAT(0);
    if (VALUE_ISA_STRING(v)) return convert_string2number(VALUE_AS_STRING(v), number_format_float);

    // check if class implements the Float method
    atomlang_closure_t *closure = atomlang_vm_fastlookup(vm, atomlang_value_getclass(v), ATOMLANG_FLOAT_INDEX);

    // sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_float)) ||
        atomlang_vm_getclosure(vm) == closure) return VALUE_FROM_ERROR(NULL);

    // execute closure and return its value
    if (atomlang_vm_runclosure(vm, closure, v, NULL, 0)) return atomlang_vm_result(vm);

    return VALUE_FROM_ERROR(NULL);
}

inline atomlang_value_t convert_value2bool (atomlang_vm *vm, atomlang_value_t v) {
    if (VALUE_ISA_BOOL(v)) return v;

    // handle conversion for basic classes
    if (VALUE_ISA_INT(v)) return VALUE_FROM_BOOL(v.n != 0);
    if (VALUE_ISA_FLOAT(v)) return VALUE_FROM_BOOL(v.f != 0.0);
    if (VALUE_ISA_NULL(v)) return VALUE_FROM_FALSE;
    if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_FALSE;
    if (VALUE_ISA_STRING(v)) {
        atomlang_string_t *string = VALUE_AS_STRING(v);
        if (string->len == 0) return VALUE_FROM_FALSE;
        return VALUE_FROM_BOOL((strcmp(string->s, "false") != 0));
    }

    // check if class implements the Bool method
    atomlang_closure_t *closure = atomlang_vm_fastlookup(vm, atomlang_value_getclass(v), ATOMLANG_BOOL_INDEX);

    // sanity check (and break recursion)
    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_bool)) ||
        atomlang_vm_getclosure(vm) == closure) return VALUE_FROM_BOOL(1);

    // execute closure and return its value
    if (atomlang_vm_runclosure(vm, closure, v, NULL, 0)) return atomlang_vm_result(vm);

    return VALUE_FROM_ERROR(NULL);
}

inline atomlang_value_t convert_value2string (atomlang_vm *vm, atomlang_value_t v) {
    if (VALUE_ISA_STRING(v)) return v;

    if (VALUE_ISA_INT(v)) {
        char buffer[512];
        #if ATOMLANG_ENABLE_INT64
        snprintf(buffer, sizeof(buffer), "%" PRId64, v.n);
        #else
        snprintf(buffer, sizeof(buffer), "%d", v.n);
        #endif
        return VALUE_FROM_CSTRING(vm, buffer);

    }
    if (VALUE_ISA_BOOL(v)) return VALUE_FROM_CSTRING(vm, (v.n) ? "true" : "false");
    if (VALUE_ISA_NULL(v)) return VALUE_FROM_CSTRING(vm, "null");
    if (VALUE_ISA_UNDEFINED(v)) return VALUE_FROM_CSTRING(vm, "undefined");
    
    if (VALUE_ISA_FLOAT(v)) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%g", v.f);
        return VALUE_FROM_CSTRING(vm, buffer);
    }

    if (VALUE_ISA_CLASS(v)) {
        const char *identifier = (VALUE_AS_CLASS(v)->identifier);
        if (!identifier) identifier = "anonymous class";
        return VALUE_FROM_CSTRING(vm, identifier);
    }

    if (VALUE_ISA_FUNCTION(v)) {
        const char *identifier = (VALUE_AS_FUNCTION(v)->identifier);
        if (!identifier) identifier = "anonymous func";
        return VALUE_FROM_CSTRING(vm, identifier);
    }

    if (VALUE_ISA_CLOSURE(v)) {
        const char *identifier = (VALUE_AS_CLOSURE(v)->f->identifier);
        if (!identifier) identifier = "anonymous closure";
        return VALUE_FROM_CSTRING(vm, identifier);
    }

    if (VALUE_ISA_LIST(v)) {
        atomlang_list_t *list = VALUE_AS_LIST(v);
        return convert_list2string(vm, list);
    }

    if (VALUE_ISA_MAP(v)) {
        atomlang_map_t *map = VALUE_AS_MAP(v);
        return convert_map2string(vm, map);
    }

    if (VALUE_ISA_RANGE(v)) {
        atomlang_range_t *r = VALUE_AS_RANGE(v);
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%" PRId64 "...%" PRId64, r->from, r->to);
        return VALUE_FROM_CSTRING(vm, buffer);
    }
    
    if (VALUE_ISA_FIBER(v)) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "Fiber %p", VALUE_AS_OBJECT(v));
        return VALUE_FROM_CSTRING(vm, buffer);
    }
    
    atomlang_closure_t *closure = atomlang_vm_fastlookup(vm, atomlang_value_getclass(v), ATOMLANG_STRING_INDEX);

    if ((!closure) || ((closure->f->tag == EXEC_TYPE_INTERNAL) && (closure->f->internal == convert_object_string)) || atomlang_vm_getclosure(vm) == closure) {
		if (VALUE_ISA_INSTANCE(v)) {
			atomlang_instance_t *instance = VALUE_AS_INSTANCE(v);
			if (vm && instance->xdata) {
				atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
				if (delegate->bridge_string) {
					uint32_t len = 0;
					const char *s = delegate->bridge_string(vm, instance->xdata, &len);
					if (s) return VALUE_FROM_STRING(vm, s, len);
				}
			} else {
				char buffer[512];
                const char *identifier = (instance->objclass->identifier);
                if (!identifier) identifier = "anonymous class";
                snprintf(buffer, sizeof(buffer), "instance of %s (%p)", identifier, instance);
                return VALUE_FROM_CSTRING(vm, buffer);
            }
        }
        return VALUE_FROM_ERROR(NULL);
    }

    if (atomlang_vm_runclosure(vm, closure, v, NULL, 0)) {
        atomlang_value_t result = atomlang_vm_result(vm);
        if (!VALUE_ISA_STRING(result)) return VALUE_FROM_CSTRING(vm, "null");
        return result;
    }

    return VALUE_FROM_ERROR(NULL);
}

static bool object_class (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_OBJECT(c), rindex);
}

static bool object_meta (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_OBJECT(atomlang_class_get_meta(c)), rindex);
}

static bool object_respond (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);
    
    bool result = false;
    if (VALUE_ISA_STRING(key)) result = (atomlang_class_lookup(c, key) != NULL);
    
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static void collect_introspection (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data1, void *data2, void *data3) {
    #pragma unused (hashtable, data3)
    atomlang_list_t *list = (atomlang_list_t *)data1;
    introspection_info_type mask = *((introspection_info_type *)data2);
    
    // EXEC_TYPE_NATIVE     = 0
    // EXEC_TYPE_INTERNAL   = 1
    // EXEC_TYPE_BRIDGED    = 2
    // EXEC_TYPE_SPECIAL    = 3
    
    atomlang_closure_t *closure = VALUE_AS_CLOSURE(value);
    atomlang_function_t *func = closure->f;
    bool is_var = (func->tag == EXEC_TYPE_SPECIAL);
    
    if ((mask == introspection_info_all) ||
        ((mask == introspection_info_variables) && (is_var)) ||
        ((mask == introspection_info_methods) && (!is_var))) {
            marray_push(atomlang_value_t, list->array, key);
    }
}

static void collect_introspection_extended (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data1, void *data2, void *data3) {
    #pragma unused (hashtable)
    
    if (!VALUE_ISA_CLOSURE(value)) return;
    
    atomlang_map_t *map = (atomlang_map_t *)data1;
    introspection_info_type mask = *((introspection_info_type *)data2);
    atomlang_vm *vm = (atomlang_vm *)data3;
    
    atomlang_closure_t *closure = VALUE_AS_CLOSURE(value);
    atomlang_function_t *func = closure->f;
    bool is_var = (func->tag == EXEC_TYPE_SPECIAL);
    
    // create info map
    atomlang_map_t *info = atomlang_map_new(vm, 16);
    
    // name
    // description?
    // isvar
        // index
        // read-only
        // type?
    // parameters (array of maps)
        // name
        // type?
        // index
        // value?
    
    atomlang_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "name"), VALUE_FROM_CSTRING(vm, VALUE_AS_STRING(key)->s));
    atomlang_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "isvar"), VALUE_FROM_BOOL(is_var));
    if (is_var) {
        if (func->index < ATOMLANG_COMPUTED_INDEX) atomlang_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "index"), VALUE_FROM_INT(func->index));
        atomlang_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "readonly"), VALUE_FROM_BOOL(func->special[0] != NULL && func->special[1] == NULL));
    } else {
        atomlang_list_t *params = atomlang_function_params_get(vm, func);
        if (params) atomlang_map_insert(vm, info, VALUE_FROM_CSTRING(vm, "params"), VALUE_FROM_OBJECT(params));
    }
    
    if ((mask == introspection_info_all) ||
        ((mask == introspection_info_variables) && (is_var)) ||
        ((mask == introspection_info_methods) && (!is_var))) {
        atomlang_map_insert(vm, map, key, VALUE_FROM_OBJECT(info));
    }
}

static bool object_real_introspection (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex, introspection_info_type mask) {
    bool extended = ((nargs >= 2) && (VALUE_ISA_BOOL(args[1]) && (VALUE_AS_BOOL(args[1]) == true)));
    bool scan_super = ((nargs >= 3) && (VALUE_ISA_BOOL(args[2]) && (VALUE_AS_BOOL(args[2]) == true)));
    
    atomlang_hash_iterate3_fn callback = (extended) ? collect_introspection_extended : collect_introspection;
    atomlang_object_t *data = (extended) ? (atomlang_object_t *)atomlang_map_new(vm, 256) : (atomlang_object_t *)atomlang_list_new(vm, 256);
    
    atomlang_value_t value = GET_VALUE(0);
    atomlang_class_t *c = (VALUE_ISA_CLASS(value)) ? VALUE_AS_CLASS(value) : atomlang_value_getclass(value);
    while (c) {
        atomlang_hash_t *htable = c->htable;
        atomlang_hash_iterate3(htable, callback, (void *)data, (void *)&mask, (void *)vm);
        c = (!scan_super) ? NULL : c->superclass;
    }
    
    RETURN_VALUE(VALUE_FROM_OBJECT(data), rindex);
}

static bool object_methods (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return object_real_introspection(vm, args, nargs, rindex, introspection_info_methods);
}

static bool object_properties (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return object_real_introspection(vm, args, nargs, rindex, introspection_info_variables);
}

static bool object_introspection (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return object_real_introspection(vm, args, nargs, rindex, introspection_info_all);
}

static bool object_internal_size (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_int_t size = atomlang_value_size(vm, GET_VALUE(0));
    if (size == 0) size = sizeof(atomlang_value_t);
    RETURN_VALUE(VALUE_FROM_INT(size), rindex);
}

static bool object_is (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    atomlang_class_t *c1 = atomlang_value_getclass(GET_VALUE(0));
    atomlang_class_t *c2 = VALUE_AS_CLASS(GET_VALUE(1));

    while (c1 != c2 && c1->superclass != NULL) {
        c1 = c1->superclass;
    }

    RETURN_VALUE(VALUE_FROM_BOOL(c1 == c2), rindex);
}

static bool object_eqq (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    atomlang_value_t v1 = GET_VALUE(0);
    atomlang_value_t v2 = GET_VALUE(1);

    // compare class first
    if (atomlang_value_getclass(v1) != atomlang_value_getclass(v2))
        RETURN_VALUE(VALUE_FROM_FALSE, rindex);

    // then compare value
    RETURN_VALUE(VALUE_FROM_BOOL(atomlang_value_vm_equals(vm, v1, v2)), rindex);
}

static bool object_neqq (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    object_eqq(vm, args, nargs, rindex);
    atomlang_value_t value = GET_VALUE(rindex);
    if (VALUE_ISA_BOOL(value)) {
         RETURN_VALUE(VALUE_FROM_BOOL(!VALUE_AS_BOOL(value)), rindex);
    }
    return true;
}

static bool object_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    if (atomlang_value_vm_equals(vm, GET_VALUE(0), GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    RETURN_VALUE(VALUE_FROM_INT(1), rindex);
}

static bool object_not (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    // !obj
    // if obj is NULL then result is true
    // everything else must be false
    RETURN_VALUE(VALUE_FROM_BOOL(VALUE_ISA_NULLCLASS(GET_VALUE(0))), rindex);
}

static bool object_load (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    // if there is a possibility that atomlang_vm_runclosure is called then it is MANDATORY to save arguments before the call
    atomlang_value_t target = GET_VALUE(0);
    atomlang_value_t key = GET_VALUE(1);
    
    // check if meta class needs to be initialized (it means if it contains valued static ivars)
    // meta classes must be inited somewhere, this problem does not exist with instances since object creation itself trigger a class init
    if (VALUE_ISA_CLASS(target)) {
        atomlang_class_t *c = VALUE_AS_CLASS(target);
        atomlang_class_t *meta = atomlang_class_get_meta(c);
        if (!meta->is_inited) {
            meta->is_inited = true;
            atomlang_closure_t *closure = atomlang_class_lookup_constructor(meta, 0);
            if (closure) {
                if (!atomlang_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
            }
        }
    }
    
    // retrieve class and process key
    atomlang_class_t *c = atomlang_value_getclass(target);
    atomlang_instance_t *instance = VALUE_ISA_INSTANCE(target) ? VALUE_AS_INSTANCE(target) : NULL;
    
    // key is an int its an optimization for faster loading of ivar
    if (VALUE_ISA_INT(key)) {
        // sanity check
        uint32_t nivar = c->nivars;
        uint32_t nindex = (uint32_t)key.n;
        if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index in load operation (1).");
        
        if (instance) RETURN_VALUE(instance->ivars[nindex], rindex);    // instance case
        RETURN_VALUE(c->ivars[nindex], rindex);                         // class case
    }
    
    // key must be a string in this version
    if (!VALUE_ISA_STRING(key)) {
        RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
    }
    
    // lookup key in class c
    atomlang_object_t *obj = (atomlang_object_t *)atomlang_class_lookup(c, key);
    if (!obj) {
        // not explicitly declared so check for dynamic property in bridge case
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (instance && instance->xdata && delegate->bridge_getundef) {
            if (delegate->bridge_getundef(vm, instance->xdata, target, VALUE_AS_CSTRING(key), rindex)) return true;
        }
    }
    if (!obj) goto execute_notfound;
    
    if (OBJECT_ISA_CLOSURE(obj)) {
        atomlang_closure_t *closure = (atomlang_closure_t *)obj;
        if (!closure || !closure->f) goto execute_notfound;
        
        // execute optimized default getter
        if (FUNCTION_ISA_SPECIAL(closure->f)) {
            if (FUNCTION_ISA_DEFAULT_GETTER(closure->f)) {
                // sanity check
                uint32_t nivar = c->nivars;
                uint32_t nindex = closure->f->index;
                if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index in load operation (2).");
                
                if (instance) RETURN_VALUE(instance->ivars[closure->f->index], rindex);
                RETURN_VALUE(c->ivars[closure->f->index], rindex);
            }
            
            if (FUNCTION_ISA_GETTER(closure->f)) {
                // returns a function to be executed using the return false trick
                RETURN_CLOSURE(VALUE_FROM_OBJECT((atomlang_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER]), rindex);
            }
            goto execute_notfound;
        }
    }
    
    RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
    
execute_notfound: {
    // in case of not found error return the notfound function to be executed (MANDATORY)
    atomlang_closure_t *closure = (atomlang_closure_t *)atomlang_class_lookup(c, atomlang_vm_keyindex(vm, ATOMLANG_NOTFOUND_INDEX));
    RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
}
}

static bool object_store (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs, rindex)

    // if there is a possibility that atomlang_vm_runfunc is called then it is MANDATORY to save arguments before the call
    atomlang_value_t target = GET_VALUE(0);
    atomlang_value_t key = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);

    // check if meta class needs to be initialized (it means if it contains valued static ivars)
    // meta classes must be inited somewhere, this problem does not exist with classes since object creation itself trigger a class init
    if (VALUE_ISA_CLASS(target)) {
        atomlang_class_t *c = VALUE_AS_CLASS(target);
        atomlang_class_t *meta = atomlang_class_get_meta(c);
        if (!meta->is_inited) {
            meta->is_inited = true;
            atomlang_closure_t *closure = atomlang_class_lookup_constructor(meta, 0);
            if (closure) {
                if (!atomlang_vm_runclosure(vm, closure, VALUE_FROM_OBJECT(meta), NULL, 0)) return false;
            }
        }
    }

    // retrieve class and process key
    atomlang_class_t *c = atomlang_value_getclass(target);
    atomlang_instance_t *instance = VALUE_ISA_INSTANCE(target) ? VALUE_AS_INSTANCE(target) : NULL;

    // key is an int its an optimization for faster loading of ivar
    if (VALUE_ISA_INT(key)) {
        // sanity check
        uint32_t nivar = c->nivars;
        uint32_t nindex = (uint32_t)key.n;
        if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index in store operation (1).");
        
        // check for struct
        if (VALUE_ISA_INSTANCE(value) && (atomlang_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
            atomlang_instance_t *instance_copy = atomlang_instance_clone(vm, VALUE_AS_INSTANCE(value));
            value = VALUE_FROM_OBJECT(instance_copy);
        }
        
        if (instance) instance->ivars[nindex] = value;
        else c->ivars[nindex] = value;
        RETURN_NOVALUE();
    }

    // key must be a string in this version
    if (!VALUE_ISA_STRING(key)) {
        RETURN_ERROR("Unable to lookup non string value into class %s", c->identifier);
    }

    // lookup key in class c
    atomlang_object_t *obj = atomlang_class_lookup(c, key);
    if (!obj) {
        // not explicitly declared so check for dynamic property in bridge case
        atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
        if (instance && instance->xdata && delegate->bridge_setundef) {
            if (delegate->bridge_setundef(vm, instance->xdata, target, VALUE_AS_CSTRING(key), value)) RETURN_NOVALUE();
        }
    }
    if (!obj) goto execute_notfound;

    atomlang_closure_t *closure;
    if (OBJECT_ISA_CLOSURE(obj)) {
        closure = (atomlang_closure_t *)obj;
        if (!closure || !closure->f) goto execute_notfound;

        // check for special functions case
        if (FUNCTION_ISA_SPECIAL(closure->f)) {
            // execute optimized default setter
            if (FUNCTION_ISA_DEFAULT_SETTER(closure->f)) {
                // sanity check
                uint32_t nivar = c->nivars;
                uint32_t nindex = closure->f->index;
                if (nindex >= nivar) RETURN_ERROR("Out of bounds ivar index in store operation (2).");
                
                // check for struct
                if (VALUE_ISA_INSTANCE(value) && (atomlang_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
                    atomlang_instance_t *instance_copy = atomlang_instance_clone(vm, VALUE_AS_INSTANCE(value));
                    value = VALUE_FROM_OBJECT(instance_copy);
                }
                
                if (instance) instance->ivars[nindex] = value;
                else c->ivars[nindex] = value;

                RETURN_NOVALUE();
            }

            if (FUNCTION_ISA_SETTER(closure->f)) {
                // returns a function to be executed using the return false trick
                RETURN_CLOSURE(VALUE_FROM_OBJECT((atomlang_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_SETTER]), rindex);
            }
            goto execute_notfound;
        }
    }

    RETURN_NOVALUE();

execute_notfound:
    // in case of not found error return the notfound function to be executed (MANDATORY)
    closure = (atomlang_closure_t *)atomlang_class_lookup(c, atomlang_vm_keyindex(vm, ATOMLANG_NOTFOUND_INDEX));
    RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
}

static bool object_notfound (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs,rindex)
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1); // vm_getslot(vm, rindex);
    RETURN_ERROR("Unable to find %s into class %s", VALUE_ISA_STRING(key) ? VALUE_AS_CSTRING(key) : "N/A", c->identifier);
}

static bool object_bind (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(rindex)

    // sanity check first
    if (nargs < 3) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_STRING(GET_VALUE(1))) RETURN_ERROR("First argument must be a String.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(2))) RETURN_ERROR("Second argument must be a Closure.");

    atomlang_object_t *object = NULL;
    if (VALUE_ISA_INSTANCE(GET_VALUE(0)) || VALUE_ISA_CLASS(GET_VALUE(0))) {
        object = VALUE_AS_OBJECT(GET_VALUE(0));
    } else {
        RETURN_ERROR("bind method can be applied only to instances or classes.");
    }

    atomlang_string_t *key = VALUE_AS_STRING(GET_VALUE(1));
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));

    // in this version core classes are shared among all VM instances and
    // this could be an issue in case of bound methods so it would be probably
    // a good idea to play safe and forbid bind on core classes
    if (atomlang_iscore_class(c)) {
        RETURN_ERROR("Unable to bind method to a ATOMLANG core class.");
    }

    // check if instance has already an anonymous class added to its hierarchy
    if (!atomlang_class_is_anon(c)) {
        // no super anonymous class found so create a new one, set its super as c, and add it to the hierarchy
        char *name = atomlang_vm_anonymous(vm);
        
        // cg needs to be disabled here because it could run inside class allocation triggering a free for the meta class
        atomlang_gc_setenabled(vm, false);
        atomlang_class_t *anon = atomlang_class_new_pair(vm, name, c, 0, 0);
        atomlang_gc_setenabled(vm, true);
        object->objclass = anon;
        c = anon;

        // store anonymous class (and its meta) into VM context
        atomlang_vm_setvalue(vm, name, VALUE_FROM_OBJECT(anon));
    }
    
    // set closure context (only if class or instance)
    // VALUE_AS_CLOSURE(GET_VALUE(2))->context = object;

    // add closure to anonymous class
    atomlang_class_bind(c, key->s, GET_VALUE(2));
    RETURN_NOVALUE();
}

static bool object_unbind (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(rindex)

    // sanity check first
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_STRING(GET_VALUE(1))) RETURN_ERROR("Argument must be a String.");

    // remove key/value from hash table
    atomlang_class_t *c = atomlang_value_getclass(GET_VALUE(0));
    atomlang_object_t *obj = (atomlang_object_t *)atomlang_class_lookup(c, GET_VALUE(1));
    
    // clear closure context
    if (obj && OBJECT_ISA_CLOSURE(obj)) ((atomlang_closure_t *)obj)->context = NULL;
    
    // remove key from class hash table
    atomlang_hash_remove(c->htable, GET_VALUE(1));

    RETURN_NOVALUE();
}

static bool object_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_ERROR("Forbidden Object execution.");
}

static bool object_clone (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    if (VALUE_ISA_INSTANCE(GET_VALUE(0))) {
        atomlang_instance_t *instance = (atomlang_instance_t *)(GET_VALUE(0).p);
        atomlang_instance_t *clone = atomlang_instance_clone(vm, instance);
        RETURN_VALUE(VALUE_FROM_OBJECT(clone), rindex);
    }
    
    // more cases to add in the future
    RETURN_ERROR("Unable to clone non instance object.");
}

static bool list_count (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_INT(marray_size(list->array)), rindex);
}

static bool list_contains (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_value_t element = GET_VALUE(1);
    atomlang_value_t result = VALUE_FROM_FALSE;

    register uint32_t count = (uint32_t)marray_size(list->array);
    register atomlang_int_t i = 0;

    while (i < count) {
        if (atomlang_value_vm_equals(vm, marray_get(list->array, i), element)) {
            result = VALUE_FROM_TRUE;
            break;
         }
        ++i;
     }

    RETURN_VALUE(result, rindex);
}

static bool list_indexOf (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_value_t element = GET_VALUE(1);
    atomlang_value_t result = VALUE_FROM_INT(-1);

    register uint32_t count = (uint32_t)marray_size(list->array);
    register atomlang_int_t i = 0;

    while (i < count) {
        if (atomlang_value_vm_equals(vm, marray_get(list->array, i), element)) {
            result = VALUE_FROM_INT(i);
            break;
        }
        ++i;
    }

    RETURN_VALUE(result, rindex);
}

static bool list_loadat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t    *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_value_t value = GET_VALUE(1);
    if (!VALUE_ISA_INT(value)) RETURN_ERROR("An integer index is required to access a list item.");

    register int32_t index = (int32_t)VALUE_AS_INT(value);
    register uint32_t count = (uint32_t)marray_size(list->array);

    if (index < 0) index = count + index;
    if ((index < 0) || ((uint32_t)index >= count)) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);

    RETURN_VALUE(marray_get(list->array, index), rindex);
}

static bool list_storeat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs, rindex)
    atomlang_list_t    *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_value_t idxvalue = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);
    if (!VALUE_ISA_INT(idxvalue)) RETURN_ERROR("An integer index is required to access a list item.");

    register int32_t index = (int32_t)VALUE_AS_INT(idxvalue);
    register uint32_t count = (uint32_t)marray_size(list->array);

    if (index < 0) index = count + index;
    if (index < 0) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, count-1);

    if ((uint32_t)index >= count) {
        // handle list resizing here
        marray_resize(atomlang_value_t, list->array, index-count+MIN_LIST_RESIZE);
        if (!list->array.p) RETURN_ERROR("Not enough memory to resize List.");
        marray_nset(list->array, index+1);
        for (int32_t i=count; i<=(index+MIN_LIST_RESIZE); ++i) {
            marray_set(list->array, i, VALUE_FROM_NULL);
        }
        marray_set(list->array, index, value);
    }

    marray_set(list->array, index, value);
    RETURN_NOVALUE();
}

static bool list_push (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_value_t value = GET_VALUE(1);
    marray_push(atomlang_value_t, list->array, value);
    RETURN_VALUE(VALUE_FROM_INT(marray_size(list->array)), rindex);
}

static bool list_pop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_list_t    *list = VALUE_AS_LIST(GET_VALUE(0));
    size_t count = marray_size(list->array);
    if (count < 1) RETURN_ERROR("Unable to pop a value from an empty list.");
    atomlang_value_t value = marray_pop(list->array);
    RETURN_VALUE(value, rindex);
}

static bool list_remove (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    if (!VALUE_ISA_INT(GET_VALUE(1))) RETURN_ERROR("Parameter must be of type Int.");

    atomlang_int_t index = VALUE_AS_INT(GET_VALUE(1));
    size_t count = marray_size(list->array);
    if ((index < 0) || (index >= count)) RETURN_ERROR("Out of bounds index.");

    // remove an item means move others down
    memmove(&list->array.p[index], &list->array.p[index+1], ((count-1)-index) * sizeof(atomlang_value_t));
    list->array.n -= 1;

    RETURN_VALUE(GET_VALUE(0), rindex);
}

static bool list_iterator (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));

    // check for empty list first
    register uint32_t count = (uint32_t)marray_size(list->array);
    if (count == 0) RETURN_VALUE(VALUE_FROM_FALSE, rindex);

    // check for start of iteration
    if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);

    // extract value
    atomlang_value_t value = GET_VALUE(1);

    // check error condition
    if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");

    // compute new value
    atomlang_int_t n = value.n;
    if (n+1 < count) {
        ++n;
    } else {
        RETURN_VALUE(VALUE_FROM_FALSE, rindex);
    }

    // return new iterator
    RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool list_iterator_next (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    register int32_t index = (int32_t)VALUE_AS_INT(GET_VALUE(1));
    RETURN_VALUE(marray_get(list->array, index), rindex);
}

static bool list_loop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));    // closure to execute
    atomlang_value_t value = GET_VALUE(0);                            // self parameter
    atomlang_list_t *list = VALUE_AS_LIST(value);
    register atomlang_int_t n = marray_size(list->array);            // times to execute the loop
    register atomlang_int_t i = 0;

    nanotime_t t1 = nanotime();
    while (i < n) {
        if (!atomlang_vm_runclosure(vm, closure, value, &marray_get(list->array, i), 1)) return false;
        ++i;
    }
    nanotime_t t2 = nanotime();
    RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool list_reverse (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
  if (nargs > 1) RETURN_ERROR("Incorrect number of arguments.");
  atomlang_value_t value = GET_VALUE(0);                            // self parameter
  atomlang_list_t *list = VALUE_AS_LIST(value);
  uint32_t count = (uint32_t)marray_size(list->array);
  atomlang_int_t i = 0;

  while (i < count/2) {
    atomlang_value_t tmp = marray_get(list->array, count-i-1);
    marray_set(list->array, count-i-1,  marray_get(list->array, i));
    marray_set(list->array, i,  tmp);
    i++;
  }

  RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

static bool list_reversed (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs > 1) RETURN_ERROR("Incorrect number of arguments.");
    
    // self parameter
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    atomlang_list_t *newlist = atomlang_list_new(vm, (uint32_t)list->array.n);
    uint32_t count = (uint32_t)marray_size(list->array);
    atomlang_int_t i = 0;

    while (i < count) {
        marray_push(atomlang_value_t, newlist->array, marray_get(list->array, count-i-1));
        ++i;
    }

    RETURN_VALUE(VALUE_FROM_OBJECT(newlist), rindex);
}

typedef bool (list_comparison_callback) (atomlang_vm *vm, atomlang_value_t val1, atomlang_value_t val2);

static bool list_default_number_compare (atomlang_vm *vm, atomlang_value_t val1, atomlang_value_t val2) {
    atomlang_float_t n1 = convert_value2float(vm, val1).f;
    atomlang_float_t n2 = convert_value2float(vm, val2).f;
    return n1 > n2;
}

static bool list_default_string_compare (atomlang_vm *vm, atomlang_value_t val1, atomlang_value_t val2) {
    atomlang_string_t *s1 = VALUE_AS_STRING(convert_value2string(vm, val1));
    atomlang_string_t *s2 = VALUE_AS_STRING(convert_value2string(vm, val2));
    return (strcmp(s1->s, s2->s) > 0);
}

static bool compare_values (atomlang_vm *vm, atomlang_value_t selfvalue, atomlang_value_t val1, atomlang_value_t val2, atomlang_closure_t *predicate) {
    atomlang_value_t params[2] = {val1, val2};
    if (!atomlang_vm_runclosure(vm, predicate, selfvalue, params, 2)) return false;
    atomlang_value_t result = atomlang_vm_result(vm);
    
    //the conversion will make sure that the comparison function only returns a
    //truthy value that can be interpreted as the result of a comparison
    //(i.e. only integer, bool, float, null, undefined, or string)
    atomlang_value_t truthy_value = convert_value2bool(vm, result);
    return truthy_value.n;
}

static uint32_t partition (atomlang_vm *vm, atomlang_value_t *array, int32_t low, int32_t high, atomlang_value_t selfvalue, atomlang_closure_t *predicate, list_comparison_callback *callback) {
    atomlang_value_t pivot = array[high];
    int32_t i = low - 1;
    
    for (int32_t j = low; j <= high - 1; j++) {
        if ((predicate && !compare_values(vm, selfvalue, array[j], pivot, predicate)) || (callback && !callback(vm, array[j], pivot))) {
            ++i;
            atomlang_value_t temp = array[i]; //swap a[i], a[j]
            array[i] = array[j];
            array[j] = temp;
        }
    }
    
    atomlang_value_t temp = array[i + 1];
    array[i + 1] = array[high];
    array[high] = temp;
    
    return i + 1;
}

static void quicksort (atomlang_vm *vm, atomlang_value_t *array, int32_t low, int32_t high, atomlang_value_t selfvalue, atomlang_closure_t *predicate, list_comparison_callback *callback) {
    if (atomlang_vm_isaborted(vm)) return;
    
    if (low < high) {
        int32_t pi = partition(vm, array, low, high, selfvalue, predicate, callback);
        quicksort(vm, array, low, pi - 1, selfvalue, predicate, callback);
        quicksort(vm, array, pi + 1, high, selfvalue, predicate, callback);
    }
}

static bool list_sort (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    //the predicate is the comparison function passed to list.sort() if any
    atomlang_closure_t *predicate = NULL;
    list_comparison_callback *callback = NULL;
    if (nargs >=2 && VALUE_ISA_CLOSURE(GET_VALUE(1))) predicate = VALUE_AS_CLOSURE(GET_VALUE(1));
    
    atomlang_value_t selfvalue = GET_VALUE(0); // self parameter
    atomlang_list_t *list = VALUE_AS_LIST(selfvalue);
    
    int32_t count = (int32_t)marray_size(list->array);
    if (count > 1) {
        if (predicate == NULL) {
            atomlang_value_t first_value = marray_get(list->array, 0);
            if (VALUE_ISA_INT(first_value) || (VALUE_ISA_FLOAT(first_value))) callback = list_default_number_compare;
            else callback = list_default_string_compare;
        }
        quicksort(vm, list->array.p, 0, count-1, selfvalue, predicate, callback);
    }
    
    RETURN_VALUE(VALUE_FROM_OBJECT((atomlang_object_t *)list), rindex);
}

static bool list_sorted (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    //the predicate is the comparison function passed to list.sort() if any
    atomlang_closure_t *predicate = NULL;
    list_comparison_callback *callback = NULL;
    if (nargs >=2 && VALUE_ISA_CLOSURE(GET_VALUE(1))) predicate = VALUE_AS_CLOSURE(GET_VALUE(1));
    
    atomlang_value_t selfvalue = GET_VALUE(0); // self parameter
    atomlang_list_t *list = VALUE_AS_LIST(selfvalue);
    
    int32_t count = (int32_t)marray_size(list->array);
    
    // do not transfer newlist to GC because it could be freed during predicate closure execution
    // (because newlist is not yet in any stack)
    atomlang_list_t *newlist = atomlang_list_new(NULL, (uint32_t)count);

    //memcpy should be faster than pushing element by element
    memcpy(newlist->array.p, list->array.p, sizeof(atomlang_value_t)*count);
    newlist->array.m = list->array.m;
    newlist->array.n = list->array.n;
    if (count > 1) {
        if (predicate == NULL) {
            atomlang_value_t first_value = marray_get(list->array, 0);
            if (VALUE_ISA_INT(first_value) || (VALUE_ISA_FLOAT(first_value))) callback = list_default_number_compare;
            else callback = list_default_string_compare;
        }
        quicksort(vm, newlist->array.p, 0, (int32_t)count-1, selfvalue, predicate, callback);
    }

    atomlang_vm_transfer(vm, (atomlang_object_t*) newlist);
    RETURN_VALUE(VALUE_FROM_OBJECT(newlist), rindex);
}

static bool list_map (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("One argument is needed by the map function.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
    
    atomlang_value_t selfvalue = GET_VALUE(0);    // self parameter
    atomlang_closure_t *predicate = VALUE_AS_CLOSURE(GET_VALUE(1));
    atomlang_list_t *list = VALUE_AS_LIST(selfvalue);
    size_t count = marray_size(list->array);
    
    // do not transfer newlist to GC because it could be freed during predicate closure execution
    atomlang_list_t *newlist = atomlang_list_new(NULL, (uint32_t)count);
    newlist->array.m = list->array.m;
    newlist->array.n = list->array.n;
    for (uint32_t i = 0; i < count; i++) {
        atomlang_value_t *value = &marray_get(list->array, i);
        if (!atomlang_vm_runclosure(vm, predicate, selfvalue, value, 1)) return false;
        atomlang_value_t result = atomlang_vm_result(vm);
        marray_set(newlist->array, i, result);
    }
    
    atomlang_vm_transfer(vm, (atomlang_object_t*) newlist);
    RETURN_VALUE(VALUE_FROM_OBJECT(newlist), rindex);
}

static bool list_filter (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("One argument is needed by the filter function.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");
    
    atomlang_value_t selfvalue = GET_VALUE(0);    // self parameter
    atomlang_closure_t *predicate = VALUE_AS_CLOSURE(GET_VALUE(1));
    atomlang_list_t *list = VALUE_AS_LIST(selfvalue);
    size_t count = marray_size(list->array);
    
    // do not transfer newlist to GC because it could be freed during predicate closure execution
    atomlang_list_t *newlist = atomlang_list_new(NULL, (uint32_t)count);
    for (uint32_t i = 0; i < count; i++) {
        atomlang_value_t *value = &marray_get(list->array, i);
        if (!atomlang_vm_runclosure(vm, predicate, selfvalue, value, 1)) return false;
        atomlang_value_t result = atomlang_vm_result(vm);
        atomlang_value_t truthy_value = convert_value2bool(vm, result);
        if (truthy_value.n) {
            marray_push(atomlang_value_t, newlist->array, *value);
        }
    }
    
    atomlang_vm_transfer(vm, (atomlang_object_t*) newlist);
    RETURN_VALUE(VALUE_FROM_OBJECT(newlist), rindex);
}

static bool list_reduce (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3) RETURN_ERROR("Two arguments are needed by the reduce function.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(2))) RETURN_ERROR("Argument 2 must be a Closure.");
    
    atomlang_value_t selfvalue = GET_VALUE(0); // self parameter
    atomlang_value_t start = GET_VALUE(1);     // start parameter
    atomlang_closure_t *predicate = VALUE_AS_CLOSURE(GET_VALUE(2));
    
    atomlang_list_t *list = VALUE_AS_LIST(selfvalue);
    size_t count = marray_size(list->array);
    for (uint32_t i = 0; i < count; i++) {
        atomlang_value_t params[2] = {start, marray_get(list->array, i)};
        if (!atomlang_vm_runclosure(vm, predicate, selfvalue, params, 2)) return false;
        start = atomlang_vm_result(vm);
    }
    
    RETURN_VALUE(start, rindex);
}

static bool list_join (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(0));
    const char *sep = NULL;
    if ((nargs > 1) && VALUE_ISA_STRING(GET_VALUE(1))) sep = VALUE_AS_CSTRING(GET_VALUE(1));

    // create a new empty buffer
    uint32_t alloc = (uint32_t) (marray_size(list->array) * 64);
    if (alloc == 0) alloc = 256;
    uint32_t len = 0;
    uint32_t seplen = (sep) ? VALUE_AS_STRING(GET_VALUE(1))->len : 0;
    char *_buffer = mem_alloc(vm, alloc);
    CHECK_MEM_ALLOC(_buffer);

    register atomlang_int_t n = marray_size(list->array);
    register atomlang_int_t i = 0;

    // traverse list and append each item
    while (i < n) {
        atomlang_value_t value = convert_value2string(vm, marray_get(list->array, i));
        if (VALUE_ISA_ERROR(value)) {
            mem_free(_buffer);
            RETURN_VALUE(value, rindex);
        }

        // compute string to append
        const char *s2 = VALUE_AS_STRING(value)->s;
        uint32_t req = VALUE_AS_STRING(value)->len;
        uint32_t free_mem = alloc - len;

        // check if buffer needs to be reallocated
        if (free_mem < req + seplen + 1) {
            uint64_t to_alloc = alloc + (req + seplen) * 2 + 4096;
            _buffer = mem_realloc(vm, _buffer, (uint32_t)to_alloc);
            if (!_buffer) {
                mem_free(_buffer);
                RETURN_ERROR_SIMPLE();
            }
            alloc = (uint32_t)to_alloc;
        }

        // copy s2 to into buffer
        memcpy(_buffer+len, s2, req);
        len += req;

        // NULL terminate the C string
        _buffer[len] = 0;
        
        // check for separator string
        if (i+1 < n && seplen) {
            memcpy(_buffer+len, sep, seplen);
            len += seplen;
            _buffer[len] = 0;
        }

        ++i;
    }

    atomlang_string_t *result = atomlang_string_new(vm, _buffer, len, alloc);
    RETURN_VALUE(VALUE_FROM_OBJECT(result), rindex);
}

static bool list_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if ((nargs != 2) || (!VALUE_ISA_INT(GET_VALUE(1)))) RETURN_ERROR("An Int value is expected as argument of list_exec.");

    uint32_t n = (uint32_t)VALUE_AS_INT(GET_VALUE(1));
    atomlang_list_t *list = atomlang_list_new(vm, n);
    if (!list) RETURN_ERROR("Maximum List allocation size reached (%d).", MAX_ALLOCATION);

    for (uint32_t i=0; i<n; ++i) {
        marray_push(atomlang_value_t, list->array, VALUE_FROM_NULL);
    }

    RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

static bool map_count (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_INT(atomlang_hash_count(map->hash)), rindex);
}

static bool map_keys (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    uint32_t count = atomlang_hash_count(map->hash);

    atomlang_list_t *list = atomlang_list_new(vm, count);
    atomlang_hash_iterate(map->hash, map_keys_array, (void *)list);
    RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

#if ATOMLANG_MAP_DOTSUGAR
static bool map_load (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    // called when a map is accessed with dot notation
    // for example:
    // var map = ["key1": 10];
    // return map.key1;
    // in this case (since we override object_load super) try to access
    // class first and if nothing is found access its internal hash table
    
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);
    if (VALUE_ISA_NOTVALID(key)) RETURN_ERROR("Invalid map key.");

    // check class first (so user will not be able to break anything)
    atomlang_object_t *obj = (atomlang_object_t *)atomlang_class_lookup(atomlang_class_map, key);
    if (obj) {
        if (OBJECT_ISA_CLOSURE(obj)) {
            atomlang_closure_t *closure = (atomlang_closure_t *)obj;
            if (closure && closure->f) {
                // execute optimized default getter
                if (FUNCTION_ISA_SPECIAL(closure->f)) {
                    if (FUNCTION_ISA_GETTER(closure->f)) {
                        // returns a function to be executed using the return false trick
                        RETURN_CLOSURE(VALUE_FROM_OBJECT((atomlang_closure_t *)closure->f->special[EXEC_TYPE_SPECIAL_GETTER]), rindex);
                    }
                }
            }
        }
        RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
    }
    
    // then check its internal hash
    atomlang_value_t *value = atomlang_hash_lookup(map->hash, key);
    RETURN_VALUE((value) ? *value : VALUE_FROM_NULL, rindex);
}
    #endif

static bool map_loadat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    // called when a map is accessed with [] notation
    // for example:
    // var map = ["key1": 10];
    // return map["key1"];
    // in this case ALWAYS access its internal hash table
    
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);
    if (VALUE_ISA_NOTVALID(key)) RETURN_ERROR("Invalid map key.");
    
    atomlang_value_t *value = atomlang_hash_lookup(map->hash, key);
    RETURN_VALUE((value) ? *value : VALUE_FROM_NULL, rindex);
}

static bool map_haskey (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);

    atomlang_value_t *value = atomlang_hash_lookup(map->hash, key);
    RETURN_VALUE((value) ? VALUE_FROM_TRUE : VALUE_FROM_FALSE, rindex);
}

static bool map_storeat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs, rindex)
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);

    atomlang_hash_insert(map->hash, key, value);
    RETURN_NOVALUE();
}

static bool map_remove (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    atomlang_value_t key = GET_VALUE(1);

    bool existed = atomlang_hash_remove(map->hash, key);
    RETURN_VALUE(VALUE_FROM_BOOL(existed), rindex);
}

static bool map_loop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));    // closure to execute
    atomlang_value_t value = GET_VALUE(0);                            // self parameter
    atomlang_map_t *map = VALUE_AS_MAP(GET_VALUE(0));
    register atomlang_int_t n = atomlang_hash_count(map->hash);        // times to execute the loop
    register atomlang_int_t i = 0;

    // build keys array
    // do not transfer newlist to GC because it could be freed during closure execution
    atomlang_list_t *list = atomlang_list_new(NULL, (uint32_t)n);
    atomlang_hash_iterate(map->hash, map_keys_array, (void *)list);

    nanotime_t t1 = nanotime();
    while (i < n) {
        if (!atomlang_vm_runclosure(vm, closure, value, &marray_get(list->array, i), 1)) return false;
        ++i;
    }
    nanotime_t t2 = nanotime();
    
    atomlang_vm_transfer(vm, (atomlang_object_t*) list);
    RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool map_iterator (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FALSE, rindex);
}


static bool range_count (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    atomlang_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
    atomlang_int_t count = (range->to > range->from) ? (range->to - range->from) : (range->from - range->to);
    RETURN_VALUE(VALUE_FROM_INT(count+1), rindex);
}

static bool range_from (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    atomlang_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_INT(range->from), rindex);
}

static bool range_to (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    atomlang_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_INT(range->to), rindex);
}

static bool range_loop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));    // closure to execute
    atomlang_value_t value = GET_VALUE(0);                           // self parameter
    atomlang_range_t *range = VALUE_AS_RANGE(value);
    bool is_forward = range->from < range->to;

    nanotime_t t1 = nanotime();
    if (is_forward) {
        register atomlang_int_t n = range->to;
        register atomlang_int_t i = range->from;
        while (i <= n) {
            if (!atomlang_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(i), 1)) return false;
            ++i;
        }
    } else {
        register atomlang_int_t n = range->from;            // 5...1
        register atomlang_int_t i = range->to;
        while (n >= i) {
            if (!atomlang_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(n), 1)) return false;
            --n;
        }
    }
    nanotime_t t2 = nanotime();
    RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool range_iterator (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));

    // check for invalid range first
    if (range->to < range->from) RETURN_VALUE(VALUE_FROM_FALSE, rindex);

    // check for start of iteration
    if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(range->from), rindex);

    // extract value
    atomlang_value_t value = GET_VALUE(1);

    // check error condition
    if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");

    // compute new value
    atomlang_int_t n = value.n;
    if (range->from < range->to) {
        ++n;
        if (n > range->to) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
    } else {
        --n;
        if (n < range->to) RETURN_VALUE(VALUE_FROM_FALSE, rindex);
    }

    // return new iterator
    RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool range_iterator_next (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(GET_VALUE(1), rindex);
}

static bool range_contains (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_range_t *range = VALUE_AS_RANGE(GET_VALUE(0));
    atomlang_value_t value = GET_VALUE(1);

    // check error condition
    if (!VALUE_ISA_INT(value)) RETURN_ERROR("A numeric value is expected.");

    RETURN_VALUE(VALUE_FROM_BOOL((value.n >= range->from) && (value.n <= range->to)), rindex);
}

static bool range_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if ((nargs != 3) || !VALUE_ISA_INT(GET_VALUE(1)) || !VALUE_ISA_INT(GET_VALUE(2))) RETURN_ERROR("Two Int values are expected as argument of Range creation.");

    uint32_t n1 = (uint32_t)VALUE_AS_INT(GET_VALUE(1));
    uint32_t n2 = (uint32_t)VALUE_AS_INT(GET_VALUE(2));

    atomlang_range_t *range = atomlang_range_new(vm, n1, n2, true);
    RETURN_VALUE(VALUE_FROM_OBJECT(range), rindex);
}

static bool class_name (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_class_t *c = (GET_VALUE(0).p);
    RETURN_VALUE(VALUE_FROM_CSTRING(vm, c->identifier), rindex);
}

static bool class_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (VALUE_ISA_CLASS(GET_VALUE(0))) {
        atomlang_class_t *c = (atomlang_class_t *)GET_VALUE(0).p;
        if (atomlang_iscore_class(c)) {
            STATICVALUE_FROM_STRING(exec_key, ATOMLANG_INTERNAL_EXEC_NAME, strlen(ATOMLANG_INTERNAL_EXEC_NAME));
            atomlang_closure_t *closure = (atomlang_closure_t *)atomlang_class_lookup_closure(c, exec_key);
            if (closure) RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
        }
    }

    // if 1st argument is not a class that means that this execution is part of a inner classes chained init
    // so retrieve class from callable object (that I am sure it is the right class)
    // this is more an hack than an elegation solution, I really hope to find out a better way
    if (!VALUE_ISA_CLASS(GET_VALUE(0))) args[0] = *(args-1);

    // retrieve class (with sanity check)
    if (!VALUE_ISA_CLASS(GET_VALUE(0))) RETURN_ERROR("Unable to execute non class object.");
    atomlang_class_t *c = (atomlang_class_t *)GET_VALUE(0).p;
    
    // lazy loading extern superclass
    if (c->superlook) {
        atomlang_class_t *super = NULL;
        atomlang_value_t supervalue = atomlang_vm_getvalue(vm, c->superlook, (int32_t)strlen(c->superlook));
        if (VALUE_ISA_CLASS(supervalue)) super = VALUE_AS_CLASS(supervalue);
        else RETURN_ERROR("Unable to find superclass %s for class %s.", c->superlook, c->identifier);
        
        mem_free(c->superlook);
        c->superlook = NULL;
        c->superclass = super;
        
        // force superclass initialization
        STATICVALUE_FROM_STRING(key, ATOMLANG_INTERNAL_EXEC_NAME, strlen(ATOMLANG_INTERNAL_EXEC_NAME));
        atomlang_closure_t *super_closure = atomlang_class_lookup_closure(atomlang_class_get_meta(c->superclass), key);
        if (super_closure) RETURN_CLOSURE(VALUE_FROM_OBJECT(super_closure), rindex);
    }

    // perform alloc (then check for init)
    atomlang_gc_setenabled(vm, false);
    atomlang_instance_t *instance = atomlang_instance_new(vm, c);
    
    // special case: check if superclass is an xdata class
    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (c->superclass->xdata && delegate->bridge_initinstance) {
        delegate->bridge_initinstance(vm, c->superclass->xdata, VALUE_FROM_NULL, instance, NULL, 1);
    }

    // if is inner class then ivar 0 is reserved for a reference to its outer class
    if (c->has_outer) atomlang_instance_setivar(instance, 0, atomlang_vm_getslot(vm, 0));

    // check for constructor function (-1 because self implicit parameter does not count)
    atomlang_closure_t *closure = (atomlang_closure_t *)atomlang_class_lookup_constructor(c, nargs-1);

    // replace first parameter (self) to newly allocated instance
    args[0] = VALUE_FROM_OBJECT(instance);

    // if constructor found in this class then executes it
    if (closure) {
        // as with func call even in constructor if less arguments are passed then fill the holes with UNDEFINED values
        if (nargs < closure->f->nparams) {
            uint16_t rargs = nargs;
            while (rargs < closure->f->nparams) {
                args[rargs] = VALUE_FROM_UNDEFINED;
                ++rargs;
            }
        }
        
        atomlang_gc_setenabled(vm, true);
        RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
    }

    // no closure found (means no constructor found in this class)
    if (c->xdata && delegate->bridge_initinstance) {
        // even if no closure is found try to execute the default bridge init instance (if class is bridged)
        if (nargs != 1) RETURN_ERROR("No init with %d parameters found in class %s", nargs-1, c->identifier);
        delegate->bridge_initinstance(vm, c->xdata, args[0], instance, args, nargs);
    }
    
    atomlang_gc_setenabled(vm, true);
    // in any case set destination register to newly allocated instance
    RETURN_VALUE(VALUE_FROM_OBJECT(instance), rindex);
}

static bool closure_disassemble (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)

    atomlang_closure_t *closure = (atomlang_closure_t *)(GET_VALUE(0).p);
    if (closure->f->tag != EXEC_TYPE_NATIVE) RETURN_VALUE(VALUE_FROM_NULL, rindex);

    const char *buffer = atomlang_disassemble(vm, closure->f, (const char *)closure->f->bytecode, closure->f->ninsts, false);
    if (!buffer) RETURN_VALUE(VALUE_FROM_NULL, rindex);

    RETURN_VALUE(atomlang_string_to_value(vm, buffer, AUTOLENGTH), rindex);
}

static bool closure_apply (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3) RETURN_ERROR("Two arguments are needed by the apply function.");
    if (!VALUE_ISA_LIST(GET_VALUE(2))) RETURN_ERROR("A list of arguments is required in the apply function.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(0));
    atomlang_value_t self_value = GET_VALUE(1);
    atomlang_list_t *list = VALUE_AS_LIST(GET_VALUE(2));

    if (!atomlang_vm_runclosure(vm, closure, self_value, list->array.p, (uint16_t)marray_size(list->array))) return false;
    atomlang_value_t result = atomlang_vm_result(vm);

    RETURN_VALUE(result, rindex);
}

static bool closure_bind (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("An argument is required by the bind function.");
    
    if (!VALUE_ISA_CLOSURE(GET_VALUE(0))) {
        // Houston, we have a problem
        RETURN_NOVALUE();
    }
    
    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(0));
    if (VALUE_ISA_NULL(GET_VALUE(1))) {
        closure->context = NULL;
    } else if (atomlang_value_isobject(GET_VALUE(1))) {
        closure->context = VALUE_AS_OBJECT(GET_VALUE(1));
    }
    
    RETURN_NOVALUE();
}

static bool function_closure (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    atomlang_function_t *func = VALUE_AS_FUNCTION(GET_VALUE(0));
    atomlang_closure_t *closure = atomlang_closure_new(vm, func);
    RETURN_VALUE(VALUE_FROM_OBJECT(closure), rindex);
}

static bool function_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (!VALUE_ISA_FUNCTION(GET_VALUE(0))) RETURN_ERROR("Unable to convert Object to closure");
    
    atomlang_function_t *func = VALUE_AS_FUNCTION(GET_VALUE(0));
    atomlang_closure_t *closure = atomlang_closure_new(vm, func);
    
    // fix the default arg values case
    // to be really correct and safe, stack size should be checked here but I know in advance that a minimum DEFAULT_MINSTACK_SIZE
    // is guaratee before calling a function, so just make sure that you do not use more than DEFAULT_MINSTACK_SIZE arguments
    // DEFAULT_MINSTACK_SIZE is 256 and in case of a 256 function arguments a maximum registers error would be returned
    // so I can assume to be always safe here
    while (nargs < func->nparams) {
        uint32_t index = (func->nparams - nargs);
        args[index] = VALUE_FROM_UNDEFINED;
        ++nargs;
    }
    
    RETURN_CLOSURE(VALUE_FROM_OBJECT(closure), rindex);
}

static bool operator_float_add (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v1, true);
    INTERNAL_CONVERT_FLOAT(v2, true);
    RETURN_VALUE(VALUE_FROM_FLOAT(v1.f + v2.f), rindex);
}

static bool operator_float_sub (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v1, true);
    INTERNAL_CONVERT_FLOAT(v2, true);
    RETURN_VALUE(VALUE_FROM_FLOAT(v1.f - v2.f), rindex);
}

static bool operator_float_div (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v1, true);
    INTERNAL_CONVERT_FLOAT(v2, true);

    if (v2.f == 0.0) RETURN_ERROR("Division by 0 error.");
    RETURN_VALUE(VALUE_FROM_FLOAT(v1.f / v2.f), rindex);
}

static bool operator_float_mul (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v1, true);
    INTERNAL_CONVERT_FLOAT(v2, true);
    RETURN_VALUE(VALUE_FROM_FLOAT(v1.f * v2.f), rindex);
}

static bool operator_float_rem (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v1, true);
    INTERNAL_CONVERT_FLOAT(v2, true);

    // compute floating point modulus
    #if ATOMLANG_ENABLE_DOUBLE
    RETURN_VALUE(VALUE_FROM_FLOAT(remainder(v1.f, v2.f)), rindex);
    #else
    RETURN_VALUE(VALUE_FROM_FLOAT(remainderf(v1.f, v2.f)), rindex);
    #endif
}

static bool operator_float_and (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_float_or (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_float_neg (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(-GET_VALUE(0).f), rindex);
}

static bool operator_float_not (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_BOOL(!GET_VALUE(0).f), rindex);
}

static bool operator_float_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v2, true);
    
    // simpler equality test
    if (VALUE_ISA_VALID(v2)) {
        if (v1.f == v2.f) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
        if (v1.f > v2.f) RETURN_VALUE(VALUE_FROM_INT(1), rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_INT(-1), rindex);
}

static bool function_float_round (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    #if ATOMLANG_ENABLE_DOUBLE
    RETURN_VALUE(VALUE_FROM_FLOAT(round(GET_VALUE(0).f)), rindex);
    #else
    RETURN_VALUE(VALUE_FROM_FLOAT(roundf(GET_VALUE(0).f)), rindex);
    #endif
}

static bool function_float_floor (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    #if ATOMLANG_ENABLE_DOUBLE
    RETURN_VALUE(VALUE_FROM_FLOAT(floor(GET_VALUE(0).f)), rindex);
    #else
    RETURN_VALUE(VALUE_FROM_FLOAT(floorf(GET_VALUE(0).f)), rindex);
    #endif
}

static bool function_float_ceil (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    #if ATOMLANG_ENABLE_DOUBLE
    RETURN_VALUE(VALUE_FROM_FLOAT(ceil(GET_VALUE(0).f)), rindex);
    #else
    RETURN_VALUE(VALUE_FROM_FLOAT(ceilf(GET_VALUE(0).f)), rindex);
    #endif
}

static bool float_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Float casting.");

    atomlang_value_t v = convert_value2float(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Float.");
    RETURN_VALUE(v, rindex);
}

static bool float_degrees (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    // Convert the float from radians to degrees
    RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).f*180/3.141592653589793), rindex);
}

static bool float_radians (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    // Convert the float from degrees to radians
    RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).f*3.141592653589793/180.0), rindex);
}

static bool float_isclose (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_VALUE(VALUE_FROM_BOOL(true), rindex);
    
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_FLOAT(v2, true);
    atomlang_float_t rel_tol = 1e-09;
    atomlang_float_t abs_tol=0.0;
    if (nargs > 2 && VALUE_ISA_FLOAT(GET_VALUE(2))) rel_tol = VALUE_AS_FLOAT(GET_VALUE(2));
    if (nargs > 3 && VALUE_ISA_FLOAT(GET_VALUE(3))) abs_tol = VALUE_AS_FLOAT(GET_VALUE(3));
    
    // abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)
    
    #if ATOMLANG_ENABLE_DOUBLE
    atomlang_float_t abs_diff = fabs(v1.f - v2.f);
    atomlang_float_t abs_a = fabs(v1.f);
    atomlang_float_t abs_b = fabs(v2.f);
    atomlang_float_t abs_max = fmax(abs_a, abs_b);
    atomlang_float_t result = fmax(rel_tol * abs_max, abs_tol);
    #else
    atomlang_float_t abs_diff = fabsf(v1.f - v2.f);
    atomlang_float_t abs_a = fabsf(v1.f);
    atomlang_float_t abs_b = fabsf(v2.f);
    atomlang_float_t abs_max = fmaxf(abs_a, abs_b);
    atomlang_float_t result = fmaxf(rel_tol * abs_max, abs_tol);
    #endif
    
    RETURN_VALUE(VALUE_FROM_BOOL(abs_diff <= result), rindex);
}

static bool float_min (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(ATOMLANG_FLOAT_MIN), rindex);
}

static bool float_max (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(ATOMLANG_FLOAT_MAX), rindex);
}

static bool operator_int_add (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);
    RETURN_VALUE(VALUE_FROM_INT(v1.n + v2.n), rindex);
}

static bool operator_int_sub (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);
    RETURN_VALUE(VALUE_FROM_INT(v1.n - v2.n), rindex);
}

static bool operator_int_div (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);

    if (v2.n == 0) RETURN_ERROR("Division by 0 error.");
    RETURN_VALUE(VALUE_FROM_INT(v1.n / v2.n), rindex);
}

static bool operator_int_mul (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);
    RETURN_VALUE(VALUE_FROM_INT(v1.n * v2.n), rindex);
}

static bool operator_int_rem (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);

    if (v2.n == 0) RETURN_ERROR("Reminder by 0 error.");
    RETURN_VALUE(VALUE_FROM_INT(v1.n % v2.n), rindex);
}

static bool operator_int_and (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_int_or (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs)
    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_int_neg (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_INT(-GET_VALUE(0).n), rindex);
}

static bool operator_int_not (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_BOOL(!GET_VALUE(0).n), rindex);
}

static bool operator_int_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (VALUE_ISA_FLOAT(args[1])) {
        // args[0] is INT and args[1] is FLOAT, in this case
        // args[0] must be manually converted to FLOAT before the call
        args[0] = VALUE_FROM_FLOAT((atomlang_float_t)args[0].n);
        return operator_float_cmp(vm, args, nargs, rindex);
    }

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_INT(v2, true);
    
    if (VALUE_ISA_VALID(v2)) {
        if (v1.n == v2.n) RETURN_VALUE(VALUE_FROM_INT(0), rindex);
        if (v1.n > v2.n) RETURN_VALUE(VALUE_FROM_INT(1), rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_INT(-1), rindex);
}

static bool int_loop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));    // closure to execute
    atomlang_value_t value = GET_VALUE(0);                           // self parameter
    register atomlang_int_t n = value.n;                             // times to execute the loop
    register atomlang_int_t i = 0;

    nanotime_t t1 = nanotime();
    while (i < n) {
        if (!atomlang_vm_runclosure(vm, closure, value, &VALUE_FROM_INT(i), 1)) return false;
        ++i;
    }
    nanotime_t t2 = nanotime();
    RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool int_random (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(args)
    if (nargs != 3) RETURN_ERROR("Int.random() expects 2 integer arguments");

    if (!VALUE_ISA_INT(GET_VALUE(1)) || !VALUE_ISA_INT(GET_VALUE(2))) RETURN_ERROR("Int.random() arguments must be integers");

    atomlang_int_t num1 = VALUE_AS_INT(GET_VALUE(1));
    atomlang_int_t num2 = VALUE_AS_INT(GET_VALUE(2));

    // Only Seed once
    static bool already_seeded = false;
    if (!already_seeded) {
        srand((unsigned)time(NULL));
        already_seeded = true;
    }

    int r;
    // if num1 is lower, consider it min, otherwise, num2 is min
    if (num1 < num2) {
        // returns a random integer between num1 and num2 inclusive
        r = (int)((rand() % (num2 - num1 + 1)) + num1);
    }
    else if (num1 > num2) {
        r = (int)((rand() % (num1 - num2 + 1)) + num2);
    }
    else {
        r = (int)num1;
    }
    RETURN_VALUE(VALUE_FROM_INT(r), rindex);
}

static bool int_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Int casting.");

    atomlang_value_t v = convert_value2int(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Int.");
    RETURN_VALUE(v, rindex);
}

static bool int_degrees (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    // Convert the int from radians to degrees
    RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).n*180/3.141592653589793), rindex);
}

static bool int_radians (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    // Convert the int from degrees to radians
    RETURN_VALUE(VALUE_FROM_FLOAT(GET_VALUE(0).n*3.141592653589793/180), rindex);
}

static bool int_min (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_INT(ATOMLANG_INT_MIN), rindex);
}

static bool int_max (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_INT(ATOMLANG_INT_MAX), rindex);
}

static bool operator_bool_add (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_add(vm, args, nargs, rindex);
}

static bool operator_bool_sub (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_sub(vm, args, nargs, rindex);
}

static bool operator_bool_div (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_div(vm, args, nargs, rindex);
}

static bool operator_bool_mul (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_mul(vm, args, nargs, rindex);
}

static bool operator_bool_rem (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_rem(vm, args, nargs, rindex);
}

static bool operator_bool_and (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_bool_or (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_bool_bitor (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n | v2.n), rindex);
}

static bool operator_bool_bitand (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n & v2.n), rindex);
}

static bool operator_bool_bitxor (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    RETURN_VALUE(VALUE_FROM_BOOL(v1.n ^ v2.n), rindex);
}

static bool operator_bool_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return operator_int_cmp(vm, args, nargs, rindex);
}

// unary operators
static bool operator_bool_neg (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_INT(-GET_VALUE(0).n), rindex);
}

static bool operator_bool_not (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    RETURN_VALUE(VALUE_FROM_INT(!GET_VALUE(0).n), rindex);
}

static bool bool_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in Bool casting.");

    atomlang_value_t v = convert_value2bool(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to Bool.");
    RETURN_VALUE(v, rindex);
}

static bool operator_string_add (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_STRING(v2, true);

    atomlang_string_t *s1 = VALUE_AS_STRING(v1);
    atomlang_string_t *s2 = VALUE_AS_STRING(v2);

    uint32_t len = s1->len + s2->len;
    char buffer[4096];
    char *s = NULL;

    // check if I can save an allocation
    if (len+1 < sizeof(buffer)) s = buffer;
    else {
        s = mem_alloc(vm, len+1);
        CHECK_MEM_ALLOC(s);
    }

    memcpy(s, s1->s, s1->len);
    memcpy(s+s1->len, s2->s, s2->len);

    atomlang_value_t v = VALUE_FROM_STRING(vm, s, len);
    if (s != NULL && s != buffer) mem_free(s);

    RETURN_VALUE(v, rindex);
}

static bool operator_string_sub (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_STRING(v2, true);

    atomlang_string_t *s1 = VALUE_AS_STRING(v1);
    atomlang_string_t *s2 = VALUE_AS_STRING(v2);
    
    // special case
    if (s2->len == 0) {
        RETURN_VALUE(VALUE_FROM_CSTRING(vm, s1->s), rindex);
    }
    
    // subtract s2 from s1
    char *found = string_strnstr(s1->s, s2->s, (size_t)s1->len);
    if (!found) RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, s1->len), rindex);

    // substring found
    // now check if entire substring must be considered
    uint32_t flen = (uint32_t)strlen(found);
    if (flen == s2->len) RETURN_VALUE(VALUE_FROM_STRING(vm, s1->s, (uint32_t)(found - s1->s)), rindex);
    // sanity check for malformed strings
    if (flen < s2->len) RETURN_ERROR("Malformed string.");

    // substring found but cannot be entirely considered
    uint32_t alloc = MAXNUM(s1->len + s2->len +1, DEFAULT_MINSTRING_SIZE);
    char *s = mem_alloc(vm, alloc);
    CHECK_MEM_ALLOC(s);

    uint32_t seek = (uint32_t)(found - s1->s);
    uint32_t len = seek + (flen - s2->len);
    memcpy(s, s1->s, seek);
    memcpy(s+seek, found+s2->len, flen - s2->len);

    atomlang_string_t *string = atomlang_string_new(vm, s, len, alloc);
    RETURN_VALUE(VALUE_FROM_OBJECT(string), rindex);
}

static bool operator_string_and (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);

    RETURN_VALUE(VALUE_FROM_BOOL(v1.n && v2.n), rindex);
}

static bool operator_string_or (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_BOOL(v1, true);
    INTERNAL_CONVERT_BOOL(v2, true);

    RETURN_VALUE(VALUE_FROM_BOOL(v1.n || v2.n), rindex);
}

static bool operator_string_neg (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_1VARIABLE(v1, 0);

    // reverse the string
    atomlang_string_t *s1 = VALUE_AS_STRING(v1);
    char *s = (char *)string_ndup(s1->s, s1->len);
    if (!utf8_reverse(s)) RETURN_ERROR("Unable to reverse a malformed string.");

    atomlang_string_t *string = atomlang_string_new(vm, s, s1->len, s1->len);
    RETURN_VALUE(VALUE_FROM_OBJECT(string), rindex);
}

static bool operator_string_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_2VARIABLES(v1, v2, 0, 1);
    INTERNAL_CONVERT_STRING(v2, true);

    if (VALUE_ISA_VALID(v2)) {
        atomlang_string_t *s1 = VALUE_AS_STRING(v1);
        atomlang_string_t *s2 = VALUE_AS_STRING(v2);
        RETURN_VALUE(VALUE_FROM_INT(strcmp(s1->s, s2->s)), rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_INT(-1), rindex);
}

static bool string_bytes (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    
    DECLARE_1VARIABLE(v1, 0);
    atomlang_string_t *s1 = VALUE_AS_STRING(v1);
    
    RETURN_VALUE(VALUE_FROM_INT(s1->len), rindex);
}

static bool string_length (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)

    DECLARE_1VARIABLE(v1, 0);
    atomlang_string_t *s1 = VALUE_AS_STRING(v1);
    uint32_t length = (s1->len) ? utf8_len(s1->s, s1->len) : 0;
    
    RETURN_VALUE(VALUE_FROM_INT(length), rindex);
}

static bool string_index (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)

    if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) {
        RETURN_ERROR("String.index() expects a string as an argument");
    }

    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_string_t *str_to_index = VALUE_AS_STRING(GET_VALUE(1));

    // search for the string
    char *ptr = string_strnstr(main_str->s, str_to_index->s, (size_t)main_str->len);

    // if it doesn't exist, return null
    if (ptr == NULL) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    // otherwise, return the difference, which is the index that the string starts at
    else {
        RETURN_VALUE(VALUE_FROM_INT(ptr - main_str->s), rindex);
    }
}

static bool string_contains (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)
    
    // sanity check
    if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) {
        RETURN_ERROR("String.index() expects a string as an argument");
    }
    
    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_string_t *str_to_index = VALUE_AS_STRING(GET_VALUE(1));
    
    // search for the string
    char *ptr = string_strnstr(main_str->s, str_to_index->s, (size_t)main_str->len);
    
    // return a Bool value
    RETURN_VALUE(VALUE_FROM_BOOL(ptr != NULL), rindex);
}

static bool string_count (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)

    if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) {
        RETURN_ERROR("String.count() expects a string as an argument");
    }

    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_string_t *str_to_count = VALUE_AS_STRING(GET_VALUE(1));

    int j = 0;
    int count = 0;

    // iterate through whole string
    for (int i = 0; i < main_str->len; ++i) {
        if (main_str->s[i] == str_to_count->s[j]) {
            // if the characters match and we are on the last character of the search
            // string, then we have found a match
            if (j == str_to_count->len - 1) {
                ++count;
                j = 0;
                continue;
            }
        }
        // reset if it isn't a match
        else {
            j = 0;
            continue;
        }
        // move forward in the search string if we found a match but we aren't
        // finished checking all the characters of the search string yet
        ++j;
    }

    RETURN_VALUE(VALUE_FROM_INT(count), rindex);
}


static bool string_repeat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if ((nargs != 2) || (!VALUE_ISA_INT(GET_VALUE(1)))) {
        RETURN_ERROR("String.repeat() expects an integer argument");
    }

    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_int_t times_to_repeat = VALUE_AS_INT(GET_VALUE(1));
    if (times_to_repeat < 1 || times_to_repeat > MAX_ALLOCATION) {
        RETURN_ERROR("String.repeat() expects a value >= 1 and < %d", MAX_ALLOCATION);
    }

    // figure out the size of the array we need to make to hold the new string
    uint32_t new_size = (uint32_t)(main_str->len * times_to_repeat);
    char *new_str = mem_alloc(vm, new_size+1);
    CHECK_MEM_ALLOC(new_str);

    uint32_t seek = 0;
    for (uint32_t i = 0; i < times_to_repeat; ++i) {
        memcpy(new_str+seek, main_str->s, main_str->len);
        seek += main_str->len;
    }

    atomlang_string_t *s = atomlang_string_new(vm, new_str, new_size, new_size);
    RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_number (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_value_t value = convert_string2number(VALUE_AS_STRING(GET_VALUE(0)), number_format_any);
    RETURN_VALUE(value, rindex);
}

static bool string_upper (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));

    char *ret = mem_alloc(vm, main_str->len + 1);
    CHECK_MEM_ALLOC(ret);
    strcpy(ret, main_str->s);

    // if no arguments passed, change the whole string to uppercase
    if (nargs == 1) {
        for (int i = 0; i <= main_str->len; ++i) {
         ret[i] = toupper(ret[i]);
        }
    }
    // otherwise, evaluate all the arguments
    else {
        for (int i = 1; i < nargs; ++i) {
            atomlang_value_t value = GET_VALUE(i);
            if (VALUE_ISA_INT(value)) {
                int32_t index = (int32_t)VALUE_AS_INT(value);

                if (index < 0) index = main_str->len + index;
                if ((index < 0) || ((uint32_t)index >= main_str->len)) {
                    mem_free(ret);
                    RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
                }

                ret[index] = toupper(ret[index]);
            }
            else {
                mem_free(ret);
                RETURN_ERROR("upper() expects either no arguments, or integer arguments.");
            }
        }
    }

    atomlang_string_t *s = atomlang_string_new(vm, ret, main_str->len, 0);
    RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_lower (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    atomlang_string_t *main_str = VALUE_AS_STRING(GET_VALUE(0));

    char *ret = mem_alloc(vm, main_str->len + 1);
    CHECK_MEM_ALLOC(ret);
    strcpy(ret, main_str->s);

    // if no arguments passed, change the whole string to lowercase
    if (nargs == 1) {
        for (int i = 0; i <= main_str->len; ++i) {
         ret[i] = tolower(ret[i]);
        }
    }
    // otherwise, evaluate all the arguments
    else {
        for (int i = 1; i < nargs; ++i) {
            atomlang_value_t value = GET_VALUE(i);
            if (VALUE_ISA_INT(value)) {
                int32_t index = (int32_t)VALUE_AS_INT(value);

                if (index < 0) index = main_str->len + index;
                if ((index < 0) || ((uint32_t)index >= main_str->len))  {
                    mem_free(ret);
                    RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, main_str->len - 1);
                }

                ret[index] = tolower(ret[index]);
            }
            else {
                mem_free(ret);
                RETURN_ERROR("lower() expects either no arguments, or integer arguments.");
            }
        }
    }

    atomlang_string_t *s = atomlang_string_new(vm, ret, main_str->len, 0);
    RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static bool string_loadat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_value_t value = GET_VALUE(1);

    int32_t first_index;
    int32_t second_index;

    if (VALUE_ISA_INT(value)) {
        first_index = (int32_t)VALUE_AS_INT(value);
        second_index = first_index;
    }
    else if (VALUE_ISA_RANGE(value)) {
        atomlang_range_t *range = VALUE_AS_RANGE(value);
        first_index = (int32_t)range->from;
        second_index = (int32_t)range->to;
    }
    else {
        RETURN_ERROR("An integer index or index range is required to access string items.");
    }

    if (first_index < 0) first_index = string->len + first_index;
    if ((first_index < 0) || ((uint32_t)first_index >= string->len)) RETURN_ERROR("Out of bounds error: first_index %d beyond bounds 0...%d", first_index, string->len-1);

    if (second_index < 0) second_index = string->len + second_index;
    if ((second_index < 0) || ((uint32_t)second_index >= string->len)) RETURN_ERROR("Out of bounds error: second_index %d beyond bounds 0...%d", second_index, string->len-1);

    uint32_t substr_len = first_index < second_index ? second_index - first_index + 1 : first_index - second_index + 1;

    bool is_forward = first_index <= second_index;
    if (!is_forward) {
        char *original = mem_alloc(vm, string->len);
        CHECK_MEM_ALLOC(original);

        // without copying it, we would be modifying the original string
        strncpy((char *)original, string->s, string->len);
        uint32_t original_len = (uint32_t) string->len;

        // Reverse the string, and reverse the indices
        first_index = original_len - first_index -1;

        // reverse the String
        int i = original_len - 1;
        int j = 0;
        char c;
        while (i > j) {
            c = original[i];
            original[i] = original[j];
            original[j] = c;
            --i;
            ++j;
        }

        atomlang_value_t s = VALUE_FROM_STRING(vm, original + first_index, substr_len);
        mem_free(original);

        RETURN_VALUE(s, rindex);
    }
    RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + first_index, substr_len), rindex);
}

static bool string_storeat (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs, rindex)

    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_value_t idxvalue = GET_VALUE(1);
    if (!VALUE_ISA_INT(idxvalue)) RETURN_ERROR("An integer index is required to access a string item.");
    if (!VALUE_ISA_STRING(GET_VALUE(2))) RETURN_ERROR("A string needs to be assigned to a string index");

    atomlang_string_t *value = VALUE_AS_STRING(GET_VALUE(2));
    register int32_t index = (int32_t)VALUE_AS_INT(idxvalue);

    if (index < 0) index = string->len + index;
    if (index < 0 || index >= string->len) RETURN_ERROR("Out of bounds error: index %d beyond bounds 0...%d", index, string->len-1);
    if (index+value->len - 1 >= string->len) RETURN_ERROR("Out of bounds error: End of inserted string exceeds the length of the initial string");

    // this code is not UTF-8 safe
    for (int i = index; i < index+value->len; ++i) {
        string->s[i] = value->s[i-index];
    }

    // characters inside string changed so we need to re-compute hash
    string->hash = atomlang_hash_compute_buffer((const char *)string->s, string->len);

    RETURN_NOVALUE();
}

static bool string_split (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    // sanity check
    if ((nargs != 2) || (!VALUE_ISA_STRING(GET_VALUE(1)))) RETURN_ERROR("String.split() expects 1 string separator.");

    // setup arguments
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_string_t *substr = VALUE_AS_STRING(GET_VALUE(1));
    const char *sep = substr->s;
    uint32_t seplen = substr->len;
    
    // this is a quite complex situation
    // list should not be trasferred to GC bacause it could be freed by VALUE_FROM_STRING
    // and the same applied to each VALUE_FROM_STRING
    // but in order to use NULL as vm parameter I should keep track of each individual allocation here
    // and then transfer all of them to the VM at the end of the loop (in order to be able to have them
    // freed by the GC)
    // I think it is too much work, the easier solution is just to disable GC during execution loop
    
    atomlang_gc_setenabled(vm, false);

    // initialize the list to have a size of 0
    atomlang_list_t *list = atomlang_list_new(vm, 0);

    char *original = string->s;
    uint32_t slen = string->len;

    // if the separator is empty, then we split the string at every character
    if (seplen == 0) {
        for (uint32_t i=0; i<slen;) {
            uint32_t n = utf8_charbytes(original, 0);
            marray_push(atomlang_value_t, list->array, VALUE_FROM_STRING(vm, original, n));
            original += n;
            i += n;
        }
        atomlang_gc_setenabled(vm, true);
        RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
    }

    // split loop
    while (1) {
        char *p = string_strnstr(original, sep, (size_t)slen);
        if (p == NULL) {
			marray_push(atomlang_value_t, list->array, VALUE_FROM_STRING(vm, original, slen));
            break;
        }
        uint32_t vlen = (uint32_t)(p-original);
		marray_push(atomlang_value_t, list->array, VALUE_FROM_STRING(vm, original, vlen));

        // update pointer and slen
        original = p + seplen;
        slen -= vlen + seplen;
    }
    
    atomlang_gc_setenabled(vm, true);
    RETURN_VALUE(VALUE_FROM_OBJECT(list), rindex);
}

static bool string_find_replace (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    // sanity check
    if ((nargs != 3) || (!VALUE_ISA_STRING(GET_VALUE(1))) || (!VALUE_ISA_STRING(GET_VALUE(2)))) RETURN_ERROR("String.replace() expects 2 string arguments.");
    
    // setup arguments
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    atomlang_string_t *from = VALUE_AS_STRING(GET_VALUE(1));
    atomlang_string_t *to = VALUE_AS_STRING(GET_VALUE(2));
    size_t len = 0;
    
    // perform search and replace
    char *s = string_replace(string->s, from->s, to->s, &len);
    
    // return result
    if (s && len) RETURN_VALUE(VALUE_FROM_STRING(vm, s, (uint32_t)len), rindex);
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

static bool string_loop (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_ERROR("Incorrect number of arguments.");
    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("Argument must be a Closure.");

    atomlang_closure_t *closure = VALUE_AS_CLOSURE(GET_VALUE(1));    // closure to execute
    atomlang_value_t value = GET_VALUE(0);                            // self parameter
    atomlang_string_t *string = VALUE_AS_STRING(value);
    char *str = string->s;
    register atomlang_int_t n = string->len;  // Times to execute the loop
    register atomlang_int_t i = 0;

    nanotime_t t1 = nanotime();
    while (i < n) {
        atomlang_value_t v_str = VALUE_FROM_STRING(vm, str + i, 1);
        if (!atomlang_vm_runclosure(vm, closure, value, &v_str, 1)) return false;
        ++i;
    }
    nanotime_t t2 = nanotime();
    RETURN_VALUE(VALUE_FROM_INT(t2-t1), rindex);
}

static bool string_iterator (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));

    // check for empty string first
    if (string->len == 0) RETURN_VALUE(VALUE_FROM_FALSE, rindex);

    // check for start of iteration
    if (VALUE_ISA_NULL(GET_VALUE(1))) RETURN_VALUE(VALUE_FROM_INT(0), rindex);

    // extract value
    atomlang_value_t value = GET_VALUE(1);

    // check error condition
    if (!VALUE_ISA_INT(value)) RETURN_ERROR("Iterator expects a numeric value here.");

    // compute new value
    atomlang_int_t index = value.n;
    if (index+1 < string->len) {
        uint32_t n = utf8_charbytes(string->s + index, 0);
        index += n;
    } else {
        RETURN_VALUE(VALUE_FROM_FALSE, rindex);
    }

    // return new iterator
    RETURN_VALUE(VALUE_FROM_INT(index), rindex);
}

static bool string_iterator_next (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    int32_t index = (int32_t)VALUE_AS_INT(GET_VALUE(1));
    uint32_t n = utf8_charbytes(string->s + index, 0);
    RETURN_VALUE(VALUE_FROM_STRING(vm, string->s + index, n), rindex);
}

static bool string_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2) RETURN_ERROR("A single argument is expected in String casting.");

    atomlang_value_t v = convert_value2string(vm, GET_VALUE(1));
    if (VALUE_ISA_NOTVALID(v)) RETURN_ERROR("Unable to convert object to String.");
    RETURN_VALUE(v, rindex);
}

static bool string_trim (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    int32_t direction = 0; // means trim both left and right
    
    // optional second parameters to specify trim-left (1) or trim-right(2)
    // default is trim-both(0)
    if ((nargs == 2) && (VALUE_ISA_INT(GET_VALUE(1)))) {
        int32_t v = (int32_t)VALUE_AS_INT(GET_VALUE(1));
        if (v >= 0 && v <= 2) direction = v;
    }
    
    atomlang_string_t *src = VALUE_AS_STRING(GET_VALUE(0));
    const char *s = src->s;
    int32_t index_left = 0;
    int32_t index_right = src->len;
    
    // process left
    if (direction == 0 || direction == 1) {
        for (int32_t i=0; i<index_right; ++i) {
            if (isspace(s[i])) ++index_left;
            else break;
        }
    }
    
    // process right
    if (direction == 0 || direction == 2) {
        for (int32_t i=index_right-1; i>=index_left; --i) {
            if (isspace(s[i])) --index_right;
            else break;
        }
    }
    
    // index_left and index_right now points to the right indexes
    uint32_t trim_len = (index_left - 0) + (src->len - index_right);
    atomlang_value_t value = VALUE_FROM_STRING(vm, (const char *)&s[index_left], src->len - trim_len);
    RETURN_VALUE(value, rindex);
}

static bool string_raw (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_string_t *string = VALUE_AS_STRING(GET_VALUE(0));
    
    uint32_t ascii = 0;
    uint32_t n = utf8_charbytes(string->s, 0);
    for (uint32_t i=0; i<n; ++i) {
        // if (n > 1) {printf("%u (%d)\n", (uint8_t)string->s[i], (uint32_t)pow(10, n-(i+1)));}
		ascii += (uint32_t)((uint8_t)string->s[i] * pow(10, n - (i + 1)));
    }
    
    RETURN_VALUE(VALUE_FROM_INT(ascii), rindex);
}

static bool string_toclass (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t result = atomlang_vm_lookup(vm, GET_VALUE(0));
    
    if (VALUE_ISA_CLASS(result)) RETURN_VALUE(result, rindex);
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

static bool fiber_create (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)

    if (!VALUE_ISA_CLOSURE(GET_VALUE(1))) RETURN_ERROR("A function is expected as argument to Fiber.create.");

    atomlang_fiber_t *fiber = atomlang_fiber_new(vm, VALUE_AS_CLOSURE(GET_VALUE(1)), 0, 0);
    RETURN_VALUE(VALUE_FROM_OBJECT(fiber), rindex);
}

static bool fiber_run (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex, bool is_trying) {
    #pragma unused(nargs, rindex)

    // set rindex slot to NULL in order to falsify the if closure check performed by the VM
    atomlang_vm_setslot(vm, VALUE_FROM_NULL, rindex);
    
    atomlang_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
    if (fiber->caller != NULL) RETURN_ERROR("Fiber has already been called.");

    // always update elapsed time
    fiber->elapsedtime = (nanotime() - fiber->lasttime) / 1000000000.0f;
    
        // check if minimum timewait is passed
    if (fiber->timewait > 0.0f) {
        if (fiber->elapsedtime < fiber->timewait) RETURN_NOVALUE();
    }
    
    // remember who ran the fiber
    fiber->caller = atomlang_vm_fiber(vm);

    // set trying flag
    fiber->trying = is_trying;
    fiber->status = (is_trying) ? FIBER_TRYING : FIBER_RUNNING;

    // switch currently running fiber
    atomlang_vm_setfiber(vm, fiber);

    RETURN_FIBER();
}

static bool fiber_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return fiber_run(vm, args, nargs, rindex, false);
}

static bool fiber_try (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return fiber_run(vm, args, nargs, rindex, true);
}

static bool fiber_yield (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(args, nargs, rindex)

    // set rindex slot to NULL in order to falsify the if closure check performed by the VM
    atomlang_vm_setslot(vm, VALUE_FROM_NULL, rindex);

    // get currently executed fiber
    atomlang_fiber_t *fiber = atomlang_vm_fiber(vm);

    // reset wait time and update last time
    fiber->timewait = 0.0f;
    fiber->lasttime = nanotime();
    
    // in no caller then this is just a NOP
    if (fiber->caller) {
        atomlang_vm_setfiber(vm, fiber->caller);
    
        // unhook this fiber from the one that called it
        fiber->caller = NULL;
        fiber->trying = false;
    
        RETURN_FIBER();
    } else {
        RETURN_NOVALUE();
    }
}

static bool fiber_yield_time (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(args, nargs, rindex)
    
    // set rindex slot to NULL in order to falsify the if closure check performed by the VM
    atomlang_vm_setslot(vm, VALUE_FROM_NULL, rindex);
    
    // get currently executed fiber
    atomlang_fiber_t *fiber = atomlang_vm_fiber(vm);
    
    // if parameter is a float/int set its wait time (otherwise ignore it)
    if (VALUE_ISA_FLOAT(GET_VALUE(1))) {
        fiber->timewait = VALUE_AS_FLOAT(GET_VALUE(1));
    } else if (VALUE_ISA_INT(GET_VALUE(1))) {
        fiber->timewait = (atomlang_float_t)GET_VALUE(1).n;
    }
    
    // update last time
    fiber->lasttime = nanotime();
    
    // in no caller then this is just a NOP
    if (fiber->caller) {
        atomlang_vm_setfiber(vm, fiber->caller);

        // unhook this fiber from the one that called it
        fiber->caller = NULL;
        fiber->trying = false;

        RETURN_FIBER();
    } else {
        RETURN_NOVALUE();
    }
}

static bool fiber_done (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)

    // returns true is the fiber has terminated execution
    atomlang_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_BOOL(fiber->nframes == 0 || fiber->error), rindex);
}

static bool fiber_status (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    // status codes:
    // 0    never executed
    // 1    aborted with error
    // 2    terminated
    // 3    running
    // 4    trying
    
    atomlang_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
    if (fiber->error) RETURN_VALUE(VALUE_FROM_INT(FIBER_ABORTED_WITH_ERROR), rindex);
    if (fiber->nframes == 0) RETURN_VALUE(VALUE_FROM_INT(FIBER_TERMINATED), rindex);
    RETURN_VALUE(VALUE_FROM_INT(fiber->status), rindex);
}

static bool fiber_elapsed_time (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    atomlang_fiber_t *fiber = VALUE_AS_FIBER(GET_VALUE(0));
    RETURN_VALUE(VALUE_FROM_FLOAT(fiber->elapsedtime), rindex);
}

static bool fiber_abort (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    atomlang_value_t msg = (nargs > 0) ? GET_VALUE(1) : VALUE_FROM_NULL;
    if (!VALUE_ISA_STRING(msg)) RETURN_ERROR("Fiber.abort expects a string as argument.");

    atomlang_string_t *s = VALUE_AS_STRING(msg);
    RETURN_ERROR("%.*s", s->len, s->s);
}

static bool operator_null_add (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,nargs)
    // NULL + v2 = v2
    RETURN_VALUE(args[1], rindex);
}

static bool operator_null_sub (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    // NULL - v2 should be computed as -v2 in my opinion (since NULL is interpreted as zero)
    args[0] = VALUE_FROM_INT(0);
    return operator_int_sub(vm, args, nargs, rindex);
}

static bool operator_null_div (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    // NULL / v2 = 0
    RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_mul (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    // NULL * v2 = 0
    RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_rem (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    // NULL % v2 = 0
    RETURN_VALUE(VALUE_FROM_INT(0), rindex);
}

static bool operator_null_and (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_or (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,nargs)

    DECLARE_1VARIABLE(v2, 1);
    INTERNAL_CONVERT_BOOL(v2, true);
    RETURN_VALUE(VALUE_FROM_BOOL(0 || v2.n), rindex);
}

// unary operators
static bool operator_null_neg (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
}

static bool operator_null_not (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    // !null = true in all the tested programming languages
    RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
}

#if ATOMLANG_NULL_SILENT
static bool operator_null_silent (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs)
    atomlang_value_t key = GET_VALUE(1);
    if (VALUE_ISA_STRING(key)) {
        atomlang_object_t *obj = (atomlang_object_t *)atomlang_class_lookup(atomlang_class_null, key);
        if (obj) RETURN_VALUE(VALUE_FROM_OBJECT(obj), rindex);
    }
    
    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (delegate->report_null_errors) {
        RETURN_ERROR("Unable to find %s into null object", VALUE_ISA_STRING(key) ? VALUE_AS_CSTRING(key) : "N/A");
    }
    
    // every operation on NULL returns NULL
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

static bool operator_store_null_silent (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,args,nargs,rindex)
    atomlang_delegate_t *delegate = atomlang_vm_delegate(vm);
    if (delegate->report_null_errors) {
        atomlang_value_t key = GET_VALUE(1);
        RETURN_ERROR("Unable to find %s into null object", VALUE_ISA_STRING(key) ? VALUE_AS_CSTRING(key) : "N/A");
    }
        
    // do not touch any register, a store op on NULL is a NOP operation
    return true;
}
#endif

static bool operator_null_cmp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm,nargs)
    if (VALUE_ISA_UNDEFINED(args[0])) {
        // undefined case (undefined is equal ONLY to undefined)
        if (VALUE_ISA_UNDEFINED(args[1])) RETURN_VALUE(VALUE_FROM_BOOL(1), rindex);
        RETURN_VALUE(VALUE_FROM_BOOL(0), rindex);
    }

    args[0] = VALUE_FROM_INT(0);
    return operator_int_cmp(vm, args, nargs, rindex);
}

static bool null_exec (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

static bool null_iterator (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FALSE, rindex);
}

static bool system_nanotime (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(args,nargs)
    nanotime_t t = nanotime();
    RETURN_VALUE(VALUE_FROM_INT(t), rindex);
}

static bool system_realprint (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex, bool cr) {
    #pragma unused (rindex)
    for (uint16_t i=1; i<nargs; ++i) {
        atomlang_value_t v = GET_VALUE(i);
        INTERNAL_CONVERT_STRING(v, true);
        atomlang_string_t *s = VALUE_AS_STRING(v);
        printf("%.*s", s->len, s->s);
    }
    if (cr) printf("\n");
    RETURN_NOVALUE();
}

static bool system_put (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return system_realprint(vm, args, nargs, rindex, false);
}

static bool system_print (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    return system_realprint(vm, args, nargs, rindex, true);
}

static bool system_input (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    // optional argument is a boolean flag
    bool remove_trailing = true;
    if (nargs >= 2 && VALUE_ISA_BOOL(GET_VALUE(1))) remove_trailing = VALUE_AS_BOOL(GET_VALUE(1));
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // remove trailing newline captured by fgets (default true)
        if (remove_trailing) buffer[strlen(buffer) - 1] = 0;
        RETURN_VALUE(VALUE_FROM_CSTRING(vm, buffer), rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_NULL, rindex);
}

static bool system_get (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (args, nargs)
    atomlang_value_t key = GET_VALUE(1);
    if (!VALUE_ISA_STRING(key)) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    RETURN_VALUE(atomlang_vm_get(vm, VALUE_AS_CSTRING(key)), rindex);
}

static bool system_set (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (nargs, rindex)
    atomlang_value_t key = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);
    if (!VALUE_ISA_STRING(key)) RETURN_NOVALUE();

    bool result = atomlang_vm_set(vm, VALUE_AS_CSTRING(key), value);
    if (!result) RETURN_ERROR("Unable to apply System setting.");
    RETURN_NOVALUE();
}

static bool system_exit (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (vm, nargs, rindex)
    int n = (VALUE_ISA_INT(GET_VALUE(1))) ? (int)GET_VALUE(1).n : 0;
    exit(n);
    RETURN_NOVALUE();
}

atomlang_closure_t *computed_property_create (atomlang_vm *vm, atomlang_function_t *getter_func, atomlang_function_t *setter_func) {
    atomlang_gc_setenabled(vm, false);
    atomlang_closure_t *getter_closure = (getter_func) ? atomlang_closure_new(vm, getter_func) : NULL;
    atomlang_closure_t *setter_closure = (setter_func) ? atomlang_closure_new(vm, setter_func) : NULL;
    atomlang_function_t *f = atomlang_function_new_special(vm, NULL, ATOMLANG_COMPUTED_INDEX, getter_closure, setter_closure);
    atomlang_gc_setenabled(vm, true);
    return atomlang_closure_new(vm, f);
}

void computed_property_free (atomlang_class_t *c, const char *name, bool remove_flag) {
    STATICVALUE_FROM_STRING(key, name, strlen(name));
    atomlang_closure_t *closure = atomlang_class_lookup_closure(c, key);
    assert(closure);

    atomlang_closure_t *getter = (atomlang_closure_t *)closure->f->special[0];
    atomlang_closure_t *setter = (closure->f->special[0] != closure->f->special[1]) ? (atomlang_closure_t *)closure->f->special[1] : NULL;
    if (getter) {
        atomlang_function_t *f = getter->f;
        atomlang_closure_free(NULL, getter);
        atomlang_function_free(NULL, f);
    }
    if (setter) {
        atomlang_function_t *f = setter->f;
        atomlang_closure_free(NULL, setter);
        atomlang_function_free(NULL, f);
    }

    if (closure->f) atomlang_function_free(NULL, closure->f);
    atomlang_closure_free(NULL, closure);

    if (remove_flag) atomlang_hash_remove(c->htable, key);
}

void atomlang_core_init (void) {
    // this function must be executed ONCE
    if (core_inited) return;
    core_inited = true;

    atomlang_class_object = atomlang_class_new_single(NULL, ATOMLANG_CLASS_OBJECT_NAME, 0);
    atomlang_class_class = atomlang_class_new_single(NULL, ATOMLANG_CLASS_CLASS_NAME, 0);
    atomlang_class_setsuper(atomlang_class_class, atomlang_class_object);

    // manually set meta class and isa pointer for classes created without the atomlang_class_new_pair
    // when atomlang_class_new_single was called atomlang_class_class was NULL so the isa pointer must be reset
    atomlang_class_object->objclass = atomlang_class_class; atomlang_class_object->isa = atomlang_class_class;
    atomlang_class_class->objclass = atomlang_class_class; atomlang_class_class->isa = atomlang_class_class;

    // NULL in atomlang_class_new_pair and NEW_FUNCTION macro because I do not want them to be in the GC
    atomlang_class_function = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_FUNCTION_NAME, NULL, 0, 0);
    atomlang_class_fiber = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_FIBER_NAME, NULL, 0, 0);
    atomlang_class_instance = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_INSTANCE_NAME, NULL, 0, 0);
    atomlang_class_closure = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_CLOSURE_NAME, NULL, 0, 0);
    atomlang_class_upvalue = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_UPVALUE_NAME, NULL, 0, 0);
    atomlang_class_module = NULL;

    // create intrinsic classes (intrinsic datatypes are: Int, Float, Bool, Null, String, List, Map and Range)
    atomlang_class_int = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_INT_NAME, NULL, 0, 0);
    atomlang_class_float = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_FLOAT_NAME, NULL, 0, 0);
    atomlang_class_bool = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_BOOL_NAME, NULL, 0, 0);
    atomlang_class_null = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_NULL_NAME, NULL, 0, 0);
    atomlang_class_string = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_STRING_NAME, NULL, 0, 0);
    atomlang_class_list = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_LIST_NAME, NULL, 0, 0);
    atomlang_class_map = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_MAP_NAME, NULL, 0, 0);
    atomlang_class_range = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_RANGE_NAME, NULL, 0, 0);
    
    // OBJECT CLASS
    atomlang_class_bind(atomlang_class_object, ATOMLANG_CLASS_CLASS_NAME, NEW_CLOSURE_VALUE(object_class));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_OPERATOR_IS_NAME, NEW_CLOSURE_VALUE(object_is));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(object_cmp));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_OPERATOR_EQQ_NAME, NEW_CLOSURE_VALUE(object_eqq));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_OPERATOR_NEQQ_NAME, NEW_CLOSURE_VALUE(object_neqq));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_CLASS_INT_NAME, NEW_CLOSURE_VALUE(convert_object_int));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_CLASS_FLOAT_NAME, NEW_CLOSURE_VALUE(convert_object_float));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_CLASS_BOOL_NAME, NEW_CLOSURE_VALUE(convert_object_bool));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_CLASS_STRING_NAME, NEW_CLOSURE_VALUE(convert_object_string));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(object_load));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(object_store));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(object_notfound));
    atomlang_class_bind(atomlang_class_object, "_size", NEW_CLOSURE_VALUE(object_internal_size));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(object_not));
    atomlang_class_bind(atomlang_class_object, "bind", NEW_CLOSURE_VALUE(object_bind));
    atomlang_class_bind(atomlang_class_object, "unbind", NEW_CLOSURE_VALUE(object_unbind));
    atomlang_class_bind(atomlang_class_object, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(object_exec));
    atomlang_class_bind(atomlang_class_object, "clone", NEW_CLOSURE_VALUE(object_clone));

    // INTROSPECTION support added to OBJECT CLASS
    atomlang_class_bind(atomlang_class_object, "class", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(object_class), NULL)));
    atomlang_class_bind(atomlang_class_object, "meta", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(object_meta), NULL)));
    atomlang_class_bind(atomlang_class_object, "respondTo", NEW_CLOSURE_VALUE(object_respond));
    atomlang_class_bind(atomlang_class_object, "methods", NEW_CLOSURE_VALUE(object_methods));
    atomlang_class_bind(atomlang_class_object, "properties", NEW_CLOSURE_VALUE(object_properties));
    atomlang_class_bind(atomlang_class_object, "introspection", NEW_CLOSURE_VALUE(object_introspection));
    
    // NULL CLASS
    // Meta
    atomlang_class_t *null_meta = atomlang_class_get_meta(atomlang_class_null);
    atomlang_class_bind(null_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(null_exec));

    // CLASS CLASS
    atomlang_class_bind(atomlang_class_class, "name", NEW_CLOSURE_VALUE(class_name));
    atomlang_class_bind(atomlang_class_class, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(class_exec));

    // CLOSURE CLASS
    atomlang_class_bind(atomlang_class_closure, "disassemble", NEW_CLOSURE_VALUE(closure_disassemble));
    atomlang_class_bind(atomlang_class_closure, "apply", NEW_CLOSURE_VALUE(closure_apply));
    atomlang_class_bind(atomlang_class_closure, "bind", NEW_CLOSURE_VALUE(closure_bind));

    // FUNCTION CLASS
    atomlang_class_bind(atomlang_class_function, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(function_exec));
    atomlang_class_bind(atomlang_class_function, "closure", NEW_CLOSURE_VALUE(function_closure));
    
    // LIST CLASS
    atomlang_closure_t *closure = computed_property_create(NULL, NEW_FUNCTION(list_count), NULL);
    atomlang_class_bind(atomlang_class_list, "count", VALUE_FROM_OBJECT(closure));
    atomlang_class_bind(atomlang_class_list, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator));
    atomlang_class_bind(atomlang_class_list, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(list_iterator_next));
    atomlang_class_bind(atomlang_class_list, ATOMLANG_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(list_loadat));
    atomlang_class_bind(atomlang_class_list, ATOMLANG_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(list_storeat));
    atomlang_class_bind(atomlang_class_list, ATOMLANG_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(list_loop));
    atomlang_class_bind(atomlang_class_list, "join", NEW_CLOSURE_VALUE(list_join));
    atomlang_class_bind(atomlang_class_list, "push", NEW_CLOSURE_VALUE(list_push));
    atomlang_class_bind(atomlang_class_list, "pop", NEW_CLOSURE_VALUE(list_pop));
    atomlang_class_bind(atomlang_class_list, "contains", NEW_CLOSURE_VALUE(list_contains));
    atomlang_class_bind(atomlang_class_list, "remove", NEW_CLOSURE_VALUE(list_remove));
    atomlang_class_bind(atomlang_class_list, "indexOf", NEW_CLOSURE_VALUE(list_indexOf));
    atomlang_class_bind(atomlang_class_list, "reverse", NEW_CLOSURE_VALUE(list_reverse));
    atomlang_class_bind(atomlang_class_list, "reversed", NEW_CLOSURE_VALUE(list_reversed));
    atomlang_class_bind(atomlang_class_list, "sort", NEW_CLOSURE_VALUE(list_sort));
    atomlang_class_bind(atomlang_class_list, "sorted", NEW_CLOSURE_VALUE(list_sorted));
    atomlang_class_bind(atomlang_class_list, "map", NEW_CLOSURE_VALUE(list_map));
    atomlang_class_bind(atomlang_class_list, "filter", NEW_CLOSURE_VALUE(list_filter));
    atomlang_class_bind(atomlang_class_list, "reduce", NEW_CLOSURE_VALUE(list_reduce));
    // Meta
    atomlang_class_t *list_meta = atomlang_class_get_meta(atomlang_class_list);
    atomlang_class_bind(list_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(list_exec));

    // MAP CLASS
    atomlang_class_bind(atomlang_class_map, "keys", NEW_CLOSURE_VALUE(map_keys));
    atomlang_class_bind(atomlang_class_map, "remove", NEW_CLOSURE_VALUE(map_remove));
    closure = computed_property_create(NULL, NEW_FUNCTION(map_count), NULL);
    atomlang_class_bind(atomlang_class_map, "count", VALUE_FROM_OBJECT(closure));
    atomlang_class_bind(atomlang_class_map, ATOMLANG_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(map_loop));
    atomlang_class_bind(atomlang_class_map, ATOMLANG_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(map_loadat));
    atomlang_class_bind(atomlang_class_map, ATOMLANG_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(map_storeat));
    atomlang_class_bind(atomlang_class_map, "hasKey", NEW_CLOSURE_VALUE(map_haskey));
    atomlang_class_bind(atomlang_class_map, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(map_iterator));
    #if ATOMLANG_MAP_DOTSUGAR
    atomlang_class_bind(atomlang_class_map, ATOMLANG_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(map_load));
    atomlang_class_bind(atomlang_class_map, ATOMLANG_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(map_storeat));
    #endif

    // RANGE CLASS
    closure = computed_property_create(NULL, NEW_FUNCTION(range_count), NULL);
    atomlang_class_bind(atomlang_class_range, "count", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(range_from), NULL);
    atomlang_class_bind(atomlang_class_range, "from", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(range_to), NULL);
    atomlang_class_bind(atomlang_class_range, "to", VALUE_FROM_OBJECT(closure));
    atomlang_class_bind(atomlang_class_range, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator));
    atomlang_class_bind(atomlang_class_range, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(range_iterator_next));
    atomlang_class_bind(atomlang_class_range, "contains", NEW_CLOSURE_VALUE(range_contains));
    atomlang_class_bind(atomlang_class_range, ATOMLANG_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(range_loop));
    // Meta
    atomlang_class_t *range_meta = atomlang_class_get_meta(atomlang_class_range);
    atomlang_class_bind(range_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(range_exec));

    // INT CLASS
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_int_add));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_int_sub));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_int_div));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_int_mul));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_int_rem));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_int_and));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_int_or));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_int_cmp));
//    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_BITOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitor));
//    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_BITAND_NAME, NEW_CLOSURE_VALUE(operator_int_bitand));
//    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_BITXOR_NAME, NEW_CLOSURE_VALUE(operator_int_bitxor));
//    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_BITLS_NAME, NEW_CLOSURE_VALUE(operator_int_bitls));
//    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_BITRS_NAME, NEW_CLOSURE_VALUE(operator_int_bitrs));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_int_neg));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_int_not));
    atomlang_class_bind(atomlang_class_int, ATOMLANG_INTERNAL_LOOP_NAME, NEW_CLOSURE_VALUE(int_loop));
    closure = computed_property_create(NULL, NEW_FUNCTION(int_radians), NULL);
    atomlang_class_bind(atomlang_class_int, "radians", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(int_degrees), NULL);
    atomlang_class_bind(atomlang_class_int, "degrees", VALUE_FROM_OBJECT(closure));
    // Meta
    atomlang_class_t *int_meta = atomlang_class_get_meta(atomlang_class_int);
    atomlang_class_bind(int_meta, "random", NEW_CLOSURE_VALUE(int_random));
    atomlang_class_bind(int_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(int_exec));
    atomlang_class_bind(int_meta, "min", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(int_min), NULL)));
    atomlang_class_bind(int_meta, "max", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(int_max), NULL)));
    
    // FLOAT CLASS
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_float_add));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_float_sub));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_float_div));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_float_mul));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_float_rem));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_float_and));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_float_or));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_float_cmp));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_float_neg));
    atomlang_class_bind(atomlang_class_float, ATOMLANG_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_float_not));
    atomlang_class_bind(atomlang_class_float, "round", NEW_CLOSURE_VALUE(function_float_round));
    atomlang_class_bind(atomlang_class_float, "isClose", NEW_CLOSURE_VALUE(float_isclose));
    atomlang_class_bind(atomlang_class_float, "floor", NEW_CLOSURE_VALUE(function_float_floor));
    atomlang_class_bind(atomlang_class_float, "ceil", NEW_CLOSURE_VALUE(function_float_ceil));
    closure = computed_property_create(NULL, NEW_FUNCTION(float_radians), NULL);
    atomlang_class_bind(atomlang_class_float, "radians", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(float_degrees), NULL);
    atomlang_class_bind(atomlang_class_float, "degrees", VALUE_FROM_OBJECT(closure));
    // Meta
    atomlang_class_t *float_meta = atomlang_class_get_meta(atomlang_class_float);
    atomlang_class_bind(float_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(float_exec));
    atomlang_class_bind(float_meta, "min", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(float_min), NULL)));
    atomlang_class_bind(float_meta, "max", VALUE_FROM_OBJECT(computed_property_create(NULL, NEW_FUNCTION(float_max), NULL)));

    // BOOL CLASS
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_bool_add));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_bool_sub));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_bool_div));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_bool_mul));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_bool_rem));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_bool_and));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_bool_or));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_BOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitor));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_BAND_NAME, NEW_CLOSURE_VALUE(operator_bool_bitand));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_BXOR_NAME, NEW_CLOSURE_VALUE(operator_bool_bitxor));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_bool_cmp));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_bool_neg));
    atomlang_class_bind(atomlang_class_bool, ATOMLANG_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_bool_not));
    // Meta
    atomlang_class_t *bool_meta = atomlang_class_get_meta(atomlang_class_bool);
    atomlang_class_bind(bool_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(bool_exec));

    // STRING CLASS
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_string_add));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_string_sub));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_string_and));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_string_or));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_string_cmp));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_string_neg));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(string_loadat));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(string_storeat));
    closure = computed_property_create(NULL, NEW_FUNCTION(string_length), NULL);
    atomlang_class_bind(atomlang_class_string, "length", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(string_bytes), NULL);
    atomlang_class_bind(atomlang_class_string, "bytes", VALUE_FROM_OBJECT(closure));
    atomlang_class_bind(atomlang_class_string, "index", NEW_CLOSURE_VALUE(string_index));
    atomlang_class_bind(atomlang_class_string, "contains", NEW_CLOSURE_VALUE(string_contains));
    atomlang_class_bind(atomlang_class_string, "replace", NEW_CLOSURE_VALUE(string_find_replace));
    atomlang_class_bind(atomlang_class_string, "count", NEW_CLOSURE_VALUE(string_count));
    atomlang_class_bind(atomlang_class_string, "repeat", NEW_CLOSURE_VALUE(string_repeat));
    atomlang_class_bind(atomlang_class_string, "upper", NEW_CLOSURE_VALUE(string_upper));
    atomlang_class_bind(atomlang_class_string, "lower", NEW_CLOSURE_VALUE(string_lower));
    atomlang_class_bind(atomlang_class_string, "split", NEW_CLOSURE_VALUE(string_split));
    atomlang_class_bind(atomlang_class_string, "loop", NEW_CLOSURE_VALUE(string_loop));
    atomlang_class_bind(atomlang_class_string, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(string_iterator));
    atomlang_class_bind(atomlang_class_string, ITERATOR_NEXT_FUNCTION, NEW_CLOSURE_VALUE(string_iterator_next));
    atomlang_class_bind(atomlang_class_string, "number", NEW_CLOSURE_VALUE(string_number));
    atomlang_class_bind(atomlang_class_string, "trim", NEW_CLOSURE_VALUE(string_trim));
    atomlang_class_bind(atomlang_class_string, "raw", NEW_CLOSURE_VALUE(string_raw));
    atomlang_class_bind(atomlang_class_string, ATOMLANG_TOCLASS_NAME, NEW_CLOSURE_VALUE(string_toclass));
    
    // Meta
    atomlang_class_t *string_meta = atomlang_class_get_meta(atomlang_class_string);
    atomlang_class_bind(string_meta, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(string_exec));

    // FIBER CLASS
    atomlang_class_t *fiber_meta = atomlang_class_get_meta(atomlang_class_fiber);
    atomlang_class_bind(fiber_meta, "create", NEW_CLOSURE_VALUE(fiber_create));
    atomlang_class_bind(atomlang_class_fiber, "call", NEW_CLOSURE_VALUE(fiber_exec));
    atomlang_class_bind(atomlang_class_fiber, "try", NEW_CLOSURE_VALUE(fiber_try));
    atomlang_class_bind(fiber_meta, "yield", NEW_CLOSURE_VALUE(fiber_yield));
    atomlang_class_bind(fiber_meta, "yieldWaitTime", NEW_CLOSURE_VALUE(fiber_yield_time));
    atomlang_class_bind(atomlang_class_fiber, "status", NEW_CLOSURE_VALUE(fiber_status));
    atomlang_class_bind(atomlang_class_fiber, "isDone", NEW_CLOSURE_VALUE(fiber_done));
    atomlang_class_bind(atomlang_class_fiber, "elapsedTime", NEW_CLOSURE_VALUE(fiber_elapsed_time));
    atomlang_class_bind(fiber_meta, "abort", NEW_CLOSURE_VALUE(fiber_abort));

    // BASIC OPERATIONS added also to NULL CLASS (and UNDEFINED since they points to the same class)
    // this is required because every variable is initialized by default to NULL
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_ADD_NAME, NEW_CLOSURE_VALUE(operator_null_add));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_SUB_NAME, NEW_CLOSURE_VALUE(operator_null_sub));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_DIV_NAME, NEW_CLOSURE_VALUE(operator_null_div));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_MUL_NAME, NEW_CLOSURE_VALUE(operator_null_mul));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_REM_NAME, NEW_CLOSURE_VALUE(operator_null_rem));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_AND_NAME, NEW_CLOSURE_VALUE(operator_null_and));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_OR_NAME,  NEW_CLOSURE_VALUE(operator_null_or));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_CMP_NAME, NEW_CLOSURE_VALUE(operator_null_cmp));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_NEG_NAME, NEW_CLOSURE_VALUE(operator_null_neg));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_OPERATOR_NOT_NAME, NEW_CLOSURE_VALUE(operator_null_not));
    #if ATOMLANG_NULL_SILENT
    atomlang_class_bind(atomlang_class_null, ATOMLANG_INTERNAL_EXEC_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_INTERNAL_LOAD_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_INTERNAL_STORE_NAME, NEW_CLOSURE_VALUE(operator_store_null_silent));
    atomlang_class_bind(atomlang_class_null, ATOMLANG_INTERNAL_NOTFOUND_NAME, NEW_CLOSURE_VALUE(operator_null_silent));
    #endif
    atomlang_class_bind(atomlang_class_null, ITERATOR_INIT_FUNCTION, NEW_CLOSURE_VALUE(null_iterator));

    // SYSTEM class
    atomlang_class_system = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_SYSTEM_NAME, NULL, 0, 0);
    atomlang_class_t *system_meta = atomlang_class_get_meta(atomlang_class_system);
    atomlang_class_bind(system_meta, ATOMLANG_SYSTEM_NANOTIME_NAME, NEW_CLOSURE_VALUE(system_nanotime));
    atomlang_class_bind(system_meta, ATOMLANG_SYSTEM_PRINT_NAME, NEW_CLOSURE_VALUE(system_print));
    atomlang_class_bind(system_meta, ATOMLANG_SYSTEM_PUT_NAME, NEW_CLOSURE_VALUE(system_put));
    atomlang_class_bind(system_meta, ATOMLANG_SYSTEM_INPUT_NAME, NEW_CLOSURE_VALUE(system_input));
    atomlang_class_bind(system_meta, "exit", NEW_CLOSURE_VALUE(system_exit));

    closure = computed_property_create(NULL, NEW_FUNCTION(system_get), NEW_FUNCTION(system_set));
    atomlang_value_t value = VALUE_FROM_OBJECT(closure);
    atomlang_class_bind(system_meta, ATOMLANG_VM_GCENABLED, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_GCMINTHRESHOLD, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_GCTHRESHOLD, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_GCRATIO, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_MAXCALLS, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_MAXBLOCK, value);
    atomlang_class_bind(system_meta, ATOMLANG_VM_MAXRECURSION, value);
    
    // INIT META
    SETMETA_INITED(atomlang_class_int);
    SETMETA_INITED(atomlang_class_float);
    SETMETA_INITED(atomlang_class_bool);
    SETMETA_INITED(atomlang_class_null);
    SETMETA_INITED(atomlang_class_string);
    SETMETA_INITED(atomlang_class_object);
    SETMETA_INITED(atomlang_class_function);
    SETMETA_INITED(atomlang_class_closure);
    SETMETA_INITED(atomlang_class_fiber);
    SETMETA_INITED(atomlang_class_class);
    SETMETA_INITED(atomlang_class_instance);
    SETMETA_INITED(atomlang_class_list);
    SETMETA_INITED(atomlang_class_map);
    SETMETA_INITED(atomlang_class_range);
    SETMETA_INITED(atomlang_class_upvalue);
    SETMETA_INITED(atomlang_class_system);
    //SETMETA_INITED(atomlang_class_module);

    //mem_check(true);
}

void atomlang_core_free (void) {
    // free optionals first
    atomlang_opt_free();

    if (!core_inited) return;

    // check if others VM are still running
    if (--refcount) return;

    // this function should never be called
    // it is just called when we need to internally check for memory leaks

    // computed properties are not registered inside VM gc so they need to be manually freed here
    //mem_check(false);
    computed_property_free(atomlang_class_list, "count", true);
    computed_property_free(atomlang_class_map, "count", true);
    computed_property_free(atomlang_class_range, "count", true);
    computed_property_free(atomlang_class_string, "length", true);
    computed_property_free(atomlang_class_int, "radians", true);
    computed_property_free(atomlang_class_int, "degrees", true);
    computed_property_free(atomlang_class_float, "radians", true);
    computed_property_free(atomlang_class_float, "degrees", true);
    atomlang_class_t *system_meta = atomlang_class_get_meta(atomlang_class_system);
    computed_property_free(system_meta, ATOMLANG_VM_GCENABLED, true);

    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_int));
    atomlang_class_free_core(NULL, atomlang_class_int);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_float));
    atomlang_class_free_core(NULL, atomlang_class_float);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_bool));
    atomlang_class_free_core(NULL, atomlang_class_bool);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_string));
    atomlang_class_free_core(NULL, atomlang_class_string);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_null));
    atomlang_class_free_core(NULL, atomlang_class_null);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_function));
    atomlang_class_free_core(NULL, atomlang_class_function);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_closure));
    atomlang_class_free_core(NULL, atomlang_class_closure);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_fiber));
    atomlang_class_free_core(NULL, atomlang_class_fiber);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_instance));
    atomlang_class_free_core(NULL, atomlang_class_instance);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_list));
    atomlang_class_free_core(NULL, atomlang_class_list);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_map));
    atomlang_class_free_core(NULL, atomlang_class_map);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_range));
    atomlang_class_free_core(NULL, atomlang_class_range);
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_upvalue));
    atomlang_class_free_core(NULL, atomlang_class_upvalue);

    // before freeing the meta class we need to remove entries with duplicated functions
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_GCMINTHRESHOLD, strlen(ATOMLANG_VM_GCMINTHRESHOLD)); atomlang_hash_remove(system_meta->htable, key);}
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_GCTHRESHOLD, strlen(ATOMLANG_VM_GCTHRESHOLD)); atomlang_hash_remove(system_meta->htable, key);}
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_GCRATIO, strlen(ATOMLANG_VM_GCRATIO)); atomlang_hash_remove(system_meta->htable, key);}
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_MAXCALLS, strlen(ATOMLANG_VM_MAXCALLS)); atomlang_hash_remove(system_meta->htable, key);}
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_MAXBLOCK, strlen(ATOMLANG_VM_MAXBLOCK)); atomlang_hash_remove(system_meta->htable, key);}
    {STATICVALUE_FROM_STRING(key, ATOMLANG_VM_MAXRECURSION, strlen(ATOMLANG_VM_MAXRECURSION)); atomlang_hash_remove(system_meta->htable, key);}
    atomlang_class_free_core(NULL, system_meta);
    atomlang_class_free_core(NULL, atomlang_class_system);

    // object must be the last class to be freed
    atomlang_class_free_core(NULL, atomlang_class_class);
    atomlang_class_free_core(NULL, atomlang_class_object);
    //mem_check(true);

    atomlang_class_int = NULL;
    atomlang_class_float = NULL;
    atomlang_class_bool = NULL;
    atomlang_class_string = NULL;
    atomlang_class_object = NULL;
    atomlang_class_null = NULL;
    atomlang_class_function = NULL;
    atomlang_class_closure = NULL;
    atomlang_class_fiber = NULL;
    atomlang_class_class = NULL;
    atomlang_class_instance = NULL;
    atomlang_class_list = NULL;
    atomlang_class_map = NULL;
    atomlang_class_range = NULL;
    atomlang_class_upvalue = NULL;
    atomlang_class_system = NULL;
    atomlang_class_module = NULL;

    core_inited = false;
}

const char **atomlang_core_identifiers (void) {
    static const char *list[] = {ATOMLANG_CLASS_OBJECT_NAME, ATOMLANG_CLASS_CLASS_NAME, ATOMLANG_CLASS_BOOL_NAME, ATOMLANG_CLASS_NULL_NAME,
        ATOMLANG_CLASS_INT_NAME, ATOMLANG_CLASS_FLOAT_NAME, ATOMLANG_CLASS_FUNCTION_NAME, ATOMLANG_CLASS_FIBER_NAME, ATOMLANG_CLASS_STRING_NAME,
        ATOMLANG_CLASS_INSTANCE_NAME, ATOMLANG_CLASS_LIST_NAME, ATOMLANG_CLASS_MAP_NAME, ATOMLANG_CLASS_RANGE_NAME, ATOMLANG_CLASS_SYSTEM_NAME,
        ATOMLANG_CLASS_CLOSURE_NAME, ATOMLANG_CLASS_UPVALUE_NAME, NULL};
    return list;
}

void atomlang_core_register (atomlang_vm *vm) {
    atomlang_core_init();
    atomlang_opt_register(vm);
    ++refcount;
    if (!vm) return;

    // register core classes inside VM
    if (atomlang_vm_ismini(vm)) return;
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_OBJECT_NAME, VALUE_FROM_OBJECT(atomlang_class_object));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_CLASS_NAME, VALUE_FROM_OBJECT(atomlang_class_class));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_BOOL_NAME, VALUE_FROM_OBJECT(atomlang_class_bool));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_NULL_NAME, VALUE_FROM_OBJECT(atomlang_class_null));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_INT_NAME, VALUE_FROM_OBJECT(atomlang_class_int));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_FLOAT_NAME, VALUE_FROM_OBJECT(atomlang_class_float));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_FUNCTION_NAME, VALUE_FROM_OBJECT(atomlang_class_function));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_CLOSURE_NAME, VALUE_FROM_OBJECT(atomlang_class_closure));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_FIBER_NAME, VALUE_FROM_OBJECT(atomlang_class_fiber));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_STRING_NAME, VALUE_FROM_OBJECT(atomlang_class_string));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_INSTANCE_NAME, VALUE_FROM_OBJECT(atomlang_class_instance));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_LIST_NAME, VALUE_FROM_OBJECT(atomlang_class_list));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_MAP_NAME, VALUE_FROM_OBJECT(atomlang_class_map));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_RANGE_NAME, VALUE_FROM_OBJECT(atomlang_class_range));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_UPVALUE_NAME, VALUE_FROM_OBJECT(atomlang_class_upvalue));
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_SYSTEM_NAME, VALUE_FROM_OBJECT(atomlang_class_system));
}

atomlang_class_t *atomlang_core_class_from_name (const char *name) {
    if (name) {
        if (string_cmp(name, ATOMLANG_CLASS_OBJECT_NAME) == 0) return atomlang_class_object;
        if (string_cmp(name, ATOMLANG_CLASS_CLASS_NAME) == 0) return atomlang_class_class;
        if (string_cmp(name, ATOMLANG_CLASS_BOOL_NAME) == 0) return atomlang_class_bool;
        if (string_cmp(name, ATOMLANG_CLASS_NULL_NAME) == 0) return atomlang_class_null;
        if (string_cmp(name, ATOMLANG_CLASS_INT_NAME) == 0) return atomlang_class_int;
        if (string_cmp(name, ATOMLANG_CLASS_FLOAT_NAME) == 0) return atomlang_class_float;
        if (string_cmp(name, ATOMLANG_CLASS_FUNCTION_NAME) == 0) return atomlang_class_function;
        if (string_cmp(name, ATOMLANG_CLASS_CLOSURE_NAME) == 0) return atomlang_class_closure;
        if (string_cmp(name, ATOMLANG_CLASS_FIBER_NAME) == 0) return atomlang_class_fiber;
        if (string_cmp(name, ATOMLANG_CLASS_STRING_NAME) == 0) return atomlang_class_string;
        if (string_cmp(name, ATOMLANG_CLASS_INSTANCE_NAME) == 0) return atomlang_class_instance;
        if (string_cmp(name, ATOMLANG_CLASS_LIST_NAME) == 0) return atomlang_class_list;
        if (string_cmp(name, ATOMLANG_CLASS_MAP_NAME) == 0) return atomlang_class_map;
        if (string_cmp(name, ATOMLANG_CLASS_RANGE_NAME) == 0) return atomlang_class_range;
        if (string_cmp(name, ATOMLANG_CLASS_UPVALUE_NAME) == 0) return atomlang_class_upvalue;
        if (string_cmp(name, ATOMLANG_CLASS_SYSTEM_NAME) == 0) return atomlang_class_system;
    }
    return NULL;
}

bool atomlang_iscore_class (atomlang_class_t *c) {
    // first check if it is a class
    if ((c == atomlang_class_object) || (c == atomlang_class_class) || (c == atomlang_class_bool) ||
        (c == atomlang_class_null) || (c == atomlang_class_int) || (c == atomlang_class_float) ||
        (c == atomlang_class_function) || (c == atomlang_class_fiber) || (c == atomlang_class_string) ||
        (c == atomlang_class_instance) || (c == atomlang_class_list) || (c == atomlang_class_map) ||
        (c == atomlang_class_range) || (c == atomlang_class_system) || (c == atomlang_class_closure) ||
        (c == atomlang_class_upvalue)) return true;

    // if class check is false then check for meta
    return ((c == atomlang_class_get_meta(atomlang_class_object)) || (c == atomlang_class_get_meta(atomlang_class_class)) ||
            (c == atomlang_class_get_meta(atomlang_class_bool)) || (c == atomlang_class_get_meta(atomlang_class_null)) ||
            (c == atomlang_class_get_meta(atomlang_class_int)) || (c == atomlang_class_get_meta(atomlang_class_float)) ||
            (c == atomlang_class_get_meta(atomlang_class_function)) || (c == atomlang_class_get_meta(atomlang_class_fiber)) ||
            (c == atomlang_class_get_meta(atomlang_class_string)) || (c == atomlang_class_get_meta(atomlang_class_instance)) ||
            (c == atomlang_class_get_meta(atomlang_class_list)) || (c == atomlang_class_get_meta(atomlang_class_map)) ||
            (c == atomlang_class_get_meta(atomlang_class_range)) || (c == atomlang_class_get_meta(atomlang_class_system)) ||
            (c == atomlang_class_get_meta(atomlang_class_closure)) || (c == atomlang_class_get_meta(atomlang_class_upvalue)));
}