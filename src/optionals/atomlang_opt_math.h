#ifndef __ATOMLANG_MATH__
#define __ATOMLANG_MATH__

#define ATOMLANG_CLASS_MATH_NAME             "Math"

#include "atomlang_value.h"

void atomlang_math_register (atomlang_vm *vm);
void atomlang_math_free (void);
bool atomlang_ismath_class (atomlang_class_t *c);
const char *atomlang_math_name (void);

#endif