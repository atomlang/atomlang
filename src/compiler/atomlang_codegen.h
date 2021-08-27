#ifndef __ATOMLANG_CODEGEN__
#define __ATOMLANG_CODEGEN__

#include "atomlang_ast.h"
#include "atomlang_value.h"
#include "atomlang_delegate.h"

atomlang_function_t *atomlang_codegen(gnode_t *node, atomlang_delegate_t *delegate, atomlang_vm *vm, bool add_debug);

#endif