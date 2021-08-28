#ifndef __ATOMLANG_PARSER__
#define __ATOMLANG_PARSER__

#include "atomlang_compiler.h"
#include "debug_macros.h"
#include "atomlang_ast.h"

typedef struct atomlang_parser_t    atomlang_parser_t;

atomlang_parser_t   *atomlang_parser_create (const char *source, size_t len, uint32_t fileid, bool is_static);
void                atomlang_parser_free (atomlang_parser_t *parser);
gnode_t            *atomlang_parser_run (atomlang_parser_t *parser, atomlang_delegate_t *delegate);

#endif