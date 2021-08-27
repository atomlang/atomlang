#ifndef __ATOMLANG_OPTIONALS__
#define __ATOMLANG_OPTIONALS__

#ifndef ATOMLANG_INCLUDE_MATH
#define ATOMLANG_INCLUDE_MATH
#endif

#ifdef ATOMLANG_INCLUDE_MATH
#define ATOMLANG_MATH_REGISTER(_vm)          atomlang_math_register(_vm)
#define ATOMLANG_MATH_FREE()                 atomlang_math_free()
#define ATOMLANG_MATH_NAME()                 atomlang_math_name()
#define ATOMLANG_ISMATH_CLASS(_c)            atomlang_ismath_class(_c)
#include "atomlang_opt_math.h"
#else
#define ATOMLANG_MATH_REGISTER(_vm)
#define ATOMLANG_MATH_FREE()
#define ATOMLANG_MATH_NAME()                 NULL
#define ATOMLANG_ISMATH_CLASS(_c)            false
#endif

#ifndef ATOMLANG_INCLUDE_JSON
#define ATOMLANG_INCLUDE_JSON
#endif

#ifdef ATOMLANG_INCLUDE_JSON
#define ATOMLANG_JSON_REGISTER(_vm)          atomlang_json_register(_vm)
#define ATOMLANG_JSON_FREE()                 atomlang_json_free()
#define ATOMLANG_JSON_NAME()                 atomlang_json_name()
#define ATOMLANG_ISJSON_CLASS(_c)            atomlang_isjson_class(_c)
#include "atomlang_opt_json.h"
#else
#define ATOMLANG_JSON_REGISTER(_vm)
#define ATOMLANG_JSON_FREE()
#define ATOMLANG_JSON_NAME()                 NULL
#define ATOMLANG_ISJSON_CLASS(_c)            false
#endif

#ifndef ATOMLANG_INCLUDE_ENV
#define ATOMLANG_INCLUDE_ENV
#endif

#ifdef ATOMLANG_INCLUDE_ENV
#define ATOMLANG_ENV_REGISTER(_vm)           atomlang_env_register(_vm)
#define ATOMLANG_ENV_FREE()                  atomlang_env_free()
#define ATOMLANG_ENV_NAME()                  atomlang_env_name()
#define ATOMLANG_ISENV_CLASS(_c)             atomlang_isenv_class(_c)
#include "atomlang_opt_env.h"
#else
#define ATOMLANG_ENV_REGISTER(_vm)
#define ATOMLANG_ENV_FREE()
#define ATOMLANG_ENV_NAME()                  NULL
#define ATOMLANG_ISENV_CLASS(_c)             false
#endif

#ifndef ATOMLANG_INCLUDE_FILE
#define ATOMLANG_INCLUDE_FILE
#endif

#ifdef ATOMLANG_INCLUDE_FILE
#define ATOMLANG_FILE_REGISTER(_vm)           atomlang_file_register(_vm)
#define ATOMLANG_FILE_FREE()                  atomlang_file_free()
#define ATOMLANG_FILE_NAME()                  atomlang_file_name()
#define ATOMLANG_ISFILE_CLASS(_c)             atomlang_isfile_class(_c)
#include "atomlang_opt_file.h"
#else
#define ATOMLANG_FILE_REGISTER(_vm)
#define ATOMLANG_FILE_FREE()
#define ATOMLANG_FILE_NAME()                  NULL
#define ATOMLANG_ISFILE_CLASS(_c)             false
#endif

#ifdef _MSC_VER
#define INLINE								__inline
#else
#define INLINE								inline
#endif

INLINE static const char **atomlang_optional_identifiers(void) {
    static const char *list[] = {
        #ifdef ATOMLANG_INCLUDE_MATH
        ATOMLANG_CLASS_MATH_NAME,
        #endif
        #ifdef ATOMLANG_INCLUDE_ENV
        ATOMLANG_CLASS_ENV_NAME,
        #endif
        #ifdef ATOMLANG_INCLUDE_JSON
        ATOMLANG_CLASS_JSON_NAME,
        #endif
        #ifdef ATOMLANG_INCLUDE_FILE
        ATOMLANG_CLASS_FILE_NAME,
        #endif
        NULL};
    return list;
}

#endif