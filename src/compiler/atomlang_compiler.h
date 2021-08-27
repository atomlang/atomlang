#ifndef __ATOMLANG_COMPILER__
#define __ATOMLANG_COMPILER__

#include "atomlang_delegate.h"
#include "debug_macros.h"
#include "atomlang_utils.h"
#include "atomlang_value.h"
#include "atomlang_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atomlang_compiler_t   atomlang_compiler_t;

ATOMLANG_API atomlang_compiler_t  *atomlang_compiler_create (atomlang_delegate_t *delegate);
ATOMLANG_API atomlang_closure_t   *atomlang_compiler_run (atomlang_compiler_t *compiler, const char *source, size_t len, uint32_t fileid, bool is_static, bool add_debug);

ATOMLANG_API gnode_t  *atomlang_compiler_ast (atomlang_compiler_t *compiler);
ATOMLANG_API void      atomlang_compiler_free (atomlang_compiler_t *compiler);
ATOMLANG_API json_t   *atomlang_compiler_serialize (atomlang_compiler_t *compiler, atomlang_closure_t *closure);
ATOMLANG_API bool      atomlang_compiler_serialize_infile (atomlang_compiler_t *compiler, atomlang_closure_t *closure, const char *path);
ATOMLANG_API void      atomlang_compiler_transfer (atomlang_compiler_t *compiler, atomlang_vm *vm);

#ifdef __cplusplus
}
#endif

#endif