#ifndef __ATOMLANG_MACROS__
#define __ATOMLANG_MACROS__

#include "atomlang_config.h"

#define AUTOLENGTH                          UINT32_MAX

#define UNUSED_PARAM(_x)                    (void)(_x)
#define UNUSED_PARAM2(_x,_y)                UNUSED_PARAM(_x),UNUSED_PARAM(_y)
#define UNUSED_PARAM3(_x,_y,_z)             UNUSED_PARAM(_x),UNUSED_PARAM(_y),UNUSED_PARAM(_z)

// MARK: -
#define VALUE_AS_OBJECT(x)                  ((x).p)
#define VALUE_AS_STRING(x)                  ((atomlang_string_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_FIBER(x)                   ((atomlang_fiber_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_FUNCTION(x)                ((atomlang_function_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_PROPERTY(x)                ((atomlang_property_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CLOSURE(x)                 ((atomlang_closure_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CLASS(x)                   ((atomlang_class_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_INSTANCE(x)                ((atomlang_instance_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_LIST(x)                    ((atomlang_list_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_MAP(x)                     ((atomlang_map_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_RANGE(x)                   ((atomlang_range_t *)VALUE_AS_OBJECT(x))
#define VALUE_AS_CSTRING(x)                 (VALUE_AS_STRING(x)->s)
#define VALUE_AS_ERROR(x)                   ((const char *)(x).p)
#define VALUE_AS_FLOAT(x)                   ((x).f)
#define VALUE_AS_INT(x)                     ((x).n)
#define VALUE_AS_BOOL(x)                    ((x).n)

// MARK: -
#if ATOMLANG_USE_HIDDEN_INITIALIZERS
#define VALUE_FROM_ERROR(msg)               (atomlang_value_from_error(msg))
#define VALUE_NOT_VALID                     VALUE_FROM_ERROR(NULL)
#define VALUE_FROM_OBJECT(obj)              (atomlang_value_from_object(obj))
#define VALUE_FROM_STRING(_vm,_s,_len)      (atomlang_string_to_value(_vm, _s, _len))
#define VALUE_FROM_CSTRING(_vm,_s)          (atomlang_string_to_value(_vm, _s, AUTOLENGTH))
#define VALUE_FROM_INT(x)                   (atomlang_value_from_int(x))
#define VALUE_FROM_FLOAT(x)                 (atomlang_value_from_float(x))
#define VALUE_FROM_NULL                     (atomlang_value_from_null())
#define VALUE_FROM_UNDEFINED                (atomlang_value_from_undefined())
#define VALUE_FROM_BOOL(x)                  (atomlang_value_from_bool(x))
#define VALUE_FROM_FALSE                    VALUE_FROM_BOOL(0)
#define VALUE_FROM_TRUE                     VALUE_FROM_BOOL(1)
#else
#define VALUE_FROM_ERROR(msg)               ((atomlang_value_t){.isa = NULL, .p = ((atomlang_object_t *)msg)})
#define VALUE_NOT_VALID                     VALUE_FROM_ERROR(NULL)
#define VALUE_FROM_OBJECT(obj)              ((atomlang_value_t){.isa = ((atomlang_object_t *)(obj)->isa), .p = (atomlang_object_t *)(obj)})
#define VALUE_FROM_STRING(_vm,_s,_len)      (atomlang_string_to_value(_vm, _s, _len))
#define VALUE_FROM_CSTRING(_vm,_s)          (atomlang_string_to_value(_vm, _s, AUTOLENGTH))
#define VALUE_FROM_INT(x)                   ((atomlang_value_t){.isa = atomlang_class_int, .n = (x)})
#define VALUE_FROM_FLOAT(x)                 ((atomlang_value_t){.isa = atomlang_class_float, .f = (x)})
#define VALUE_FROM_NULL                     ((atomlang_value_t){.isa = atomlang_class_null, .n = 0})
#define VALUE_FROM_UNDEFINED                ((atomlang_value_t){.isa = atomlang_class_null, .n = 1})
#define VALUE_FROM_BOOL(x)                  ((atomlang_value_t){.isa = atomlang_class_bool, .n = (x)})
#define VALUE_FROM_FALSE                    VALUE_FROM_BOOL(0)
#define VALUE_FROM_TRUE                     VALUE_FROM_BOOL(1)
#endif 

#define STATICVALUE_FROM_STRING(_v,_s,_l)   atomlang_string_t __temp = {.isa = atomlang_class_string, .s = (char *)_s, .len = (uint32_t)_l, }; \
                                            __temp.hash = atomlang_hash_compute_buffer(__temp.s, __temp.len); \
                                            atomlang_value_t _v = {.isa = atomlang_class_string, .p = (atomlang_object_t *)&__temp };

#define VALUE_ISA_FUNCTION(v)               (v.isa == atomlang_class_function)
#define VALUE_ISA_INSTANCE(v)               (v.isa == atomlang_class_instance)
#define VALUE_ISA_CLOSURE(v)                (v.isa == atomlang_class_closure)
#define VALUE_ISA_FIBER(v)                  (v.isa == atomlang_class_fiber)
#define VALUE_ISA_CLASS(v)                  (v.isa == atomlang_class_class)
#define VALUE_ISA_STRING(v)                 (v.isa == atomlang_class_string)
#define VALUE_ISA_INT(v)                    (v.isa == atomlang_class_int)
#define VALUE_ISA_FLOAT(v)                  (v.isa == atomlang_class_float)
#define VALUE_ISA_BOOL(v)                   (v.isa == atomlang_class_bool)
#define VALUE_ISA_LIST(v)                   (v.isa == atomlang_class_list)
#define VALUE_ISA_MAP(v)                    (v.isa == atomlang_class_map)
#define VALUE_ISA_RANGE(v)                  (v.isa == atomlang_class_range)
#define VALUE_ISA_BASIC_TYPE(v)             (VALUE_ISA_STRING(v) || VALUE_ISA_INT(v) || VALUE_ISA_FLOAT(v) || VALUE_ISA_BOOL(v))
#define VALUE_ISA_NULLCLASS(v)              (v.isa == atomlang_class_null)
#define VALUE_ISA_NULL(v)                   ((v.isa == atomlang_class_null) && (v.n == 0))
#define VALUE_ISA_UNDEFINED(v)              ((v.isa == atomlang_class_null) && (v.n == 1))
#define VALUE_ISA_CLASS(v)                  (v.isa == atomlang_class_class)
#define VALUE_ISA_CALLABLE(v)               (VALUE_ISA_FUNCTION(v) || VALUE_ISA_CLASS(v) || VALUE_ISA_FIBER(v))
#define VALUE_ISA_VALID(v)                  (v.isa != NULL)
#define VALUE_ISA_NOTVALID(v)               (v.isa == NULL)
#define VALUE_ISA_ERROR(v)                  VALUE_ISA_NOTVALID(v)

// MARK: -
#define OBJECT_ISA_INT(obj)                 (obj->isa == atomlang_class_int)
#define OBJECT_ISA_FLOAT(obj)               (obj->isa == atomlang_class_float)
#define OBJECT_ISA_BOOL(obj)                (obj->isa == atomlang_class_bool)
#define OBJECT_ISA_NULL(obj)                (obj->isa == atomlang_class_null)
#define OBJECT_ISA_CLASS(obj)               (obj->isa == atomlang_class_class)
#define OBJECT_ISA_FUNCTION(obj)            (obj->isa == atomlang_class_function)
#define OBJECT_ISA_CLOSURE(obj)             (obj->isa == atomlang_class_closure)
#define OBJECT_ISA_INSTANCE(obj)            (obj->isa == atomlang_class_instance)
#define OBJECT_ISA_LIST(obj)                (obj->isa == atomlang_class_list)
#define OBJECT_ISA_MAP(obj)                 (obj->isa == atomlang_class_map)
#define OBJECT_ISA_STRING(obj)              (obj->isa == atomlang_class_string)
#define OBJECT_ISA_UPVALUE(obj)             (obj->isa == atomlang_class_upvalue)
#define OBJECT_ISA_FIBER(obj)               (obj->isa == atomlang_class_fiber)
#define OBJECT_ISA_RANGE(obj)               (obj->isa == atomlang_class_range)
#define OBJECT_ISA_MODULE(obj)              (obj->isa == atomlang_class_module)
#define OBJECT_IS_VALID(obj)                (obj->isa != NULL)

// MARK: -
#define LIST_COUNT(v)                       (marray_size(VALUE_AS_LIST(v)->array))
#define LIST_VALUE_AT_INDEX(v, idx)         (marray_get(VALUE_AS_LIST(v)->array, idx))

// MARK: -
#define ATOMLANG_JSON_FUNCTION               "function"
#define ATOMLANG_JSON_CLASS                  "class"
#define ATOMLANG_JSON_RANGE                  "range"
#define ATOMLANG_JSON_INSTANCE               "instance"
#define ATOMLANG_JSON_ENUM                   "enum"
#define ATOMLANG_JSON_MAP                    "map"
#define ATOMLANG_JSON_VAR                    "var"
#define ATOMLANG_JSON_GETTER                 "$get"
#define ATOMLANG_JSON_SETTER                 "$set"

#define ATOMLANG_JSON_LABELTAG               "tag"
#define ATOMLANG_JSON_LABELNAME              "name"
#define ATOMLANG_JSON_LABELTYPE              "type"
#define ATOMLANG_JSON_LABELVALUE             "value"
#define ATOMLANG_JSON_LABELIDENTIFIER        "identifier"
#define ATOMLANG_JSON_LABELPOOL              "pool"
#define ATOMLANG_JSON_LABELPVALUES           "pvalues"
#define ATOMLANG_JSON_LABELPNAMES            "pnames"
#define ATOMLANG_JSON_LABELMETA              "meta"
#define ATOMLANG_JSON_LABELBYTECODE          "bytecode"
#define ATOMLANG_JSON_LABELLINENO            "lineno"
#define ATOMLANG_JSON_LABELNPARAM            "nparam"
#define ATOMLANG_JSON_LABELNLOCAL            "nlocal"
#define ATOMLANG_JSON_LABELNTEMP             "ntemp"
#define ATOMLANG_JSON_LABELNUPV              "nup"
#define ATOMLANG_JSON_LABELARGS              "args"
#define ATOMLANG_JSON_LABELINDEX             "index"
#define ATOMLANG_JSON_LABELSUPER             "super"
#define ATOMLANG_JSON_LABELNIVAR             "nivar"
#define ATOMLANG_JSON_LABELSIVAR             "sivar"
#define ATOMLANG_JSON_LABELPURITY            "purity"
#define ATOMLANG_JSON_LABELREADONLY          "readonly"
#define ATOMLANG_JSON_LABELSTORE             "store"
#define ATOMLANG_JSON_LABELINIT              "init"
#define ATOMLANG_JSON_LABELSTATIC            "static"
#define ATOMLANG_JSON_LABELPARAMS            "params"
#define ATOMLANG_JSON_LABELSTRUCT            "struct"
#define ATOMLANG_JSON_LABELFROM              "from"
#define ATOMLANG_JSON_LABELTO                "to"
#define ATOMLANG_JSON_LABELIVAR              "ivar"

#define ATOMLANG_VM_ANONYMOUS_PREFIX         "$$"

// MARK: -

#if 1
#define DEBUG_ASSERT(condition, message)    do { \
                                                if (!(condition)) { \
                                                    fprintf(stderr, "[%s:%d] Assert failed in %s(): %s\n", \
                                                    __FILE__, __LINE__, __func__, message); \
                                                    abort(); \
                                                } \
                                            } \
                                            while(0)
#else
#define DEBUG_ASSERT(condition, message)
#endif

#endif