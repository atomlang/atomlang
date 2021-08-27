#include <inttypes.h>
#include "atomlang_vm.h"
#include "atomlang_core.h"
#include "atomlang_hash.h"
#include "atomlang_json.h"
#include "atomlang_utils.h"
#include "atomlang_macros.h"
#include "atomlang_opcodes.h"
#include "atomlang_vmmacros.h"
#include "atomlang_opt_json.h"

#define ATOMLANG_JSON_STRINGIFY_NAME     "stringify"
#define ATOMLANG_JSON_PARSE_NAME         "parse"
#define STATIC_BUFFER_SIZE              8192

static atomlang_class_t                  *atomlang_class_json = NULL;
static uint32_t                         refcount = 0;

static bool JSON_stringify (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    atomlang_value_t value = GET_VALUE(1);
    
    if (VALUE_ISA_STRING(value)) {
        const int nchars = 5;
        const char *v = VALUE_AS_STRING(value)->s;
        size_t vlen = VALUE_AS_STRING(value)->len;

        if (vlen < 4096-nchars) {
            char vbuffer2[4096];
            vlen = snprintf(vbuffer2, sizeof(vbuffer2), "\"%s\"", v);
            RETURN_VALUE(VALUE_FROM_STRING(vm, vbuffer2, (uint32_t)vlen), rindex);
        } else {
            char *vbuffer2 = mem_alloc(NULL, vlen + nchars);
            vlen = snprintf(vbuffer2, vlen + nchars, "\"%s\"", v);
            RETURN_VALUE(VALUE_FROM_OBJECT(atomlang_string_new(vm, vbuffer2, (uint32_t)vlen, 0)), rindex);
        }
    }
    
    char vbuffer[512];
    const char *v = NULL;
    
    if (VALUE_ISA_NULL(value) || (VALUE_ISA_UNDEFINED(value))) v = "null";
    else if (VALUE_ISA_FLOAT(value)) {snprintf(vbuffer, sizeof(vbuffer), "%f", value.f); v = vbuffer;}
    else if (VALUE_ISA_BOOL(value)) v = (value.n) ? "true" : "false";
    else if (VALUE_ISA_INT(value)) {
        #if ATOMLANG_ENABLE_INT64
        snprintf(vbuffer, sizeof(vbuffer), "%" PRId64 "", value.n);
        #else
        snprintf(vbuffer, sizeof(vbuffer), "%d", value.n);
        #endif
        v = vbuffer;
    }
    if (v) RETURN_VALUE(VALUE_FROM_CSTRING(vm, v), rindex);
    
    json_t *json = json_new();
    json_set_option(json, json_opt_no_maptype);
    json_set_option(json, json_opt_no_undef);
    json_set_option(json, json_opt_prettify);
    atomlang_value_serialize(NULL, value, json);
    
    size_t len = 0;
    const char *jbuffer = json_buffer(json, &len);
    const char *buffer = string_ndup(jbuffer, len);
    
    atomlang_string_t *s = atomlang_string_new(vm, (char *)buffer, (uint32_t)len, 0);
    json_free(json);
    
    RETURN_VALUE(VALUE_FROM_OBJECT(s), rindex);
}

static atomlang_value_t JSON_value (atomlang_vm *vm, json_value *json) {
    switch (json->type) {
        case json_none:
        case json_null:
            return VALUE_FROM_NULL;
            
        case json_object: {
            atomlang_object_t *obj = atomlang_object_deserialize(vm, json);
            atomlang_value_t objv = (obj) ? VALUE_FROM_OBJECT(obj) : VALUE_FROM_NULL;
            return objv;
        }
            
        case json_array: {
            unsigned int length = json->u.array.length;
            atomlang_list_t *list = atomlang_list_new(vm, length);
            for (unsigned int i = 0; i < length; ++i) {
                atomlang_value_t value = JSON_value(vm, json->u.array.values[i]);
                marray_push(atomlang_value_t, list->array, value);
            }
            return VALUE_FROM_OBJECT(list);
        }
            
        case json_integer:
            return VALUE_FROM_INT(json->u.integer);
            
        case json_double:
            return VALUE_FROM_FLOAT(json->u.dbl);
            
        case json_string:
            return VALUE_FROM_STRING(vm, json->u.string.ptr, json->u.string.length);
            
        case json_boolean:
            return VALUE_FROM_BOOL(json->u.boolean);
    }
    
    return VALUE_FROM_NULL;
}

static bool JSON_parse (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs < 2) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    atomlang_value_t value = GET_VALUE(1);
    if (!VALUE_ISA_STRING(value)) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    atomlang_string_t *string = VALUE_AS_STRING(value);
    json_value *json = json_parse(string->s, string->len);
    if (!json) RETURN_VALUE(VALUE_FROM_NULL, rindex);
    
    RETURN_VALUE(JSON_value(vm, json), rindex);
}

static void create_optional_class (void) {
    atomlang_class_json = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_JSON_NAME, NULL, 0, 0);
    atomlang_class_t *json_meta = atomlang_class_get_meta(atomlang_class_json);
    
    atomlang_class_bind(json_meta, ATOMLANG_JSON_STRINGIFY_NAME, NEW_CLOSURE_VALUE(JSON_stringify));
    atomlang_class_bind(json_meta, ATOMLANG_JSON_PARSE_NAME, NEW_CLOSURE_VALUE(JSON_parse));
    
    SETMETA_INITED(atomlang_class_json);
}

bool atomlang_isjson_class (atomlang_class_t *c) {
    return (c == atomlang_class_json);
}

const char *atomlang_json_name (void) {
    return ATOMLANG_CLASS_JSON_NAME;
}

void atomlang_json_register (atomlang_vm *vm) {
    if (!atomlang_class_json) create_optional_class();
    ++refcount;
    
    if (!vm || atomlang_vm_ismini(vm)) return;
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_JSON_NAME, VALUE_FROM_OBJECT(atomlang_class_json));
}

void atomlang_json_free (void) {
    if (!atomlang_class_json) return;
    if (--refcount) return;
    
    atomlang_class_free_core(NULL, atomlang_class_get_meta(atomlang_class_json));
    atomlang_class_free_core(NULL, atomlang_class_json);
    
    atomlang_class_json = NULL;
}