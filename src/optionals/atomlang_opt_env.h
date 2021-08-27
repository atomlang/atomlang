#ifndef ATOMLANG_ENV_H
#define ATOMLANG_ENV_H

#define ATOMLANG_CLASS_ENV_NAME "ENV"

#include "atomlang_value.h"

void atomlang_env_register (atomlang_vm *vm);
void atomlang_env_register_args(atomlang_vm *vm, uint32_t _argc, const char **_argv);
void atomlang_env_free (void);
bool atomlang_isenv_class (atomlang_class_t *c);
const char *atomlang_env_name (void);

#endif