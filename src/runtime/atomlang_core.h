#ifndef __ATOMLANG_CORE__
#define __ATOMLANG_CORE__

#include "atomlang_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

ATOMLANG_API atomlang_class_t  *atomlang_core_class_from_name (const char *name);
ATOMLANG_API void              atomlang_core_free (void);
ATOMLANG_API const char      **atomlang_core_identifiers (void);
ATOMLANG_API void              atomlang_core_init (void);
ATOMLANG_API void              atomlang_core_register (atomlang_vm *vm);
ATOMLANG_API bool              atomlang_iscore_class (atomlang_class_t *c);

atomlang_value_t convert_value2bool (atomlang_vm *vm, atomlang_value_t v);
atomlang_value_t convert_value2float (atomlang_vm *vm, atomlang_value_t v);
atomlang_value_t convert_value2int (atomlang_vm *vm, atomlang_value_t v);
atomlang_value_t convert_value2string (atomlang_vm *vm, atomlang_value_t v);

atomlang_closure_t *computed_property_create (atomlang_vm *vm, atomlang_function_t *getter_func, atomlang_function_t *setter_func);
void               computed_property_free (atomlang_class_t *c, const char *name, bool remove_flag);

#ifdef __cplusplus
}
#endif

#endif