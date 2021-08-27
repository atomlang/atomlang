// includes
#include <stdlib.h>
#include <string.h>
#include "atomlang_vm.h"
#include "atomlang_core.h"
#include "atomlang_hash.h"
#include "atomlang_utils.h"
#include "atomlang_macros.h"
#include "atomlang_vmmacros.h"
#include "atomlang_opcodes.h"
#include "atomlang_debug.h"

#include "atomlang_opt_env.h"

#if defined(_WIN32)
#define setenv(_key, _value_, _unused)      _putenv_s(_key, _value_)
#define unsetenv(_key)                      _putenv_s(_key, "")
#endif

static atomlang_class_t              *atomlang_class_env = NULL;
static uint32_t                     refcount = 0;

static int                          argc = -1;
static atomlang_list_t               *argv = NULL;

static bool atomlang_env_get(atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    if(!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("Environment variable key must be a string.");
    }

    char *key = VALUE_AS_CSTRING(args[1]);
    char *value = getenv(key);
    atomlang_value_t rt = VALUE_FROM_UNDEFINED;

    if (value) {
        rt = VALUE_FROM_STRING(vm, value, (uint32_t)strlen(value));
    }

    RETURN_VALUE(rt, rindex);
}

static bool atomlang_env_set(atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(nargs)
    
    if(!VALUE_ISA_STRING(args[1]) || (!VALUE_ISA_STRING(args[2]) && !VALUE_ISA_NULL(args[2]))) {
        RETURN_ERROR("Environment variable key and value must both be strings.");
    }

    atomlang_string_t *key = VALUE_AS_STRING(args[1]);
    atomlang_string_t *value = (VALUE_ISA_STRING(args[2])) ? VALUE_AS_STRING(args[2]) : NULL;

    int rt = (value) ? setenv(key->s, value->s, 1) : unsetenv(key->s);
    RETURN_VALUE(VALUE_FROM_INT(rt), rindex);
}

static bool atomlang_env_keys(atomlang_vm *vm, atomlang_value_t *args, uint16_t nparams, uint32_t rindex) {
    #pragma unused(args, nparams)
    
    extern char **environ;
    atomlang_list_t *keys = atomlang_list_new(vm, 16);
    
    for (char **env = environ; *env; ++env) {
        char *entry = *env;
        
        uint32_t len = 0;
        for (uint32_t i=0; entry[len]; ++i, ++len) {
            if (entry[i] == '=') break;
        }
        atomlang_value_t key = VALUE_FROM_STRING(vm, entry, len);
        marray_push(atomlang_value_t, keys->array, key);
    }

    RETURN_VALUE(VALUE_FROM_OBJECT(keys), rindex);
}

static bool atomlang_env_argc(atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
   #pragma unused(vm, args, nargs)
   RETURN_VALUE((argc != -1) ? VALUE_FROM_INT(argc) : VALUE_FROM_NULL, rindex);
}

static bool atomlang_env_argv(atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
   #pragma unused(vm, args, nargs)
   RETURN_VALUE((argv) ? VALUE_FROM_OBJECT(argv) : VALUE_FROM_NULL, rindex);
}

static void create_optional_class (void) {
    atomlang_class_env = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_ENV_NAME, NULL, 0, 0);
    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_env);
    
    atomlang_class_bind(meta, "get", NEW_CLOSURE_VALUE(atomlang_env_get));
    atomlang_class_bind(meta, "set", NEW_CLOSURE_VALUE(atomlang_env_set));
    atomlang_class_bind(meta, "keys", NEW_CLOSURE_VALUE(atomlang_env_keys));
    
    atomlang_class_bind(meta, ATOMLANG_INTERNAL_LOADAT_NAME, NEW_CLOSURE_VALUE(atomlang_env_get));
    atomlang_class_bind(meta, ATOMLANG_INTERNAL_STOREAT_NAME, NEW_CLOSURE_VALUE(atomlang_env_set));

    atomlang_closure_t *closure = NULL;
    closure = computed_property_create(NULL, NEW_FUNCTION(atomlang_env_argc), NULL);
    atomlang_class_bind(meta, "argc", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(atomlang_env_argv), NULL);
    atomlang_class_bind(meta, "argv", VALUE_FROM_OBJECT(closure));

    SETMETA_INITED(atomlang_class_env);
}

void atomlang_env_register(atomlang_vm *vm) {
    if (!atomlang_class_env) create_optional_class();
    ++refcount;
    
    if (!vm || atomlang_vm_ismini(vm)) return;
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_ENV_NAME, VALUE_FROM_OBJECT(atomlang_class_env));
}

void atomlang_env_register_args(atomlang_vm *vm, uint32_t _argc, const char **_argv) {
    argc = _argc;
    argv = atomlang_list_new(vm, argc);
    for (int i = 0; i < _argc; ++i) {
        atomlang_value_t arg = VALUE_FROM_CSTRING(vm, _argv[i]);
        marray_push(atomlang_value_t, argv->array, arg);
    }
}

void atomlang_env_free (void) {
    if (!atomlang_class_env) return;
    if (--refcount) return;

    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_env);
    computed_property_free(meta, "argc", true);
    computed_property_free(meta, "argv", true);
    atomlang_class_free_core(NULL, meta);
    atomlang_class_free_core(NULL, atomlang_class_env);

    atomlang_class_env = NULL;
}

bool atomlang_isenv_class (atomlang_class_t *c) {
    return (c == atomlang_class_env);
}

const char *atomlang_env_name (void) {
    return ATOMLANG_CLASS_ENV_NAME;
}