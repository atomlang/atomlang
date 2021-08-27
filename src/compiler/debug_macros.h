#ifndef __ATOMLANG_CMACROS__
#define __ATOMLANG_CMACROS__

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include "atomlang_memory.h"
#include "atomlang_config.h"

#define ATOMLANG_LEXEM_DEBUG             0
#define ATOMLANG_LEXER_DEGUB             0
#define ATOMLANG_PARSER_DEBUG            0
#define ATOMLANG_SEMA1_DEBUG             0
#define ATOMLANG_SEMA2_DEBUG             0
#define ATOMLANG_AST_DEBUG               0
#define ATOMLANG_LOOKUP_DEBUG            0
#define ATOMLANG_SYMTABLE_DEBUG          0
#define ATOMLANG_CODEGEN_DEBUG           0
#define ATOMLANG_OPCODE_DEBUG            0
#define ATOMLANG_BYTECODE_DEBUG          0
#define ATOMLANG_REGISTER_DEBUG          0
#define ATOMLANG_FREE_DEBUG              0
#define ATOMLANG_DESERIALIZE_DEBUG       0

#define PRINT_LINE(...)                 printf(__VA_ARGS__);printf("\n");fflush(stdout)

#if ATOMLANG_LEXER_DEGUB
#define DEBUG_LEXER(l)                  atomlang_lexer_debug(l)
#else
#define DEBUG_LEXER(...)
#endif

#if ATOMLANG_LEXEM_DEBUG
#define DEBUG_LEXEM(...)                do { if (!lexer->peeking) { \
                                            printf("(%03d, %03d, %02d) ", lexer->token.lineno, lexer->token.colno, lexer->token.position); \
                                            PRINT_LINE(__VA_ARGS__);} \
                                        } while(0)
#else
#define DEBUG_LEXEM(...)
#endif

#if ATOMLANG_PARSER_DEBUG
#define DEBUG_PARSER(...)               PRINT_LINE(__VA_ARGS__)
#else
#define atomlang_parser_debug(p)
#define DEBUG_PARSER(...)
#endif

#if ATOMLANG_SEMA1_DEBUG
#define DEBUG_SEMA1(...)                PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SEMA1(...)
#endif

#if ATOMLANG_SEMA2_DEBUG
#define DEBUG_SEMA2(...)                PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SEMA2(...)
#endif

#if ATOMLANG_LOOKUP_DEBUG
#define DEBUG_LOOKUP(...)               PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_LOOKUP(...)
#endif

#if ATOMLANG_SYMTABLE_DEBUG
#define DEBUG_SYMTABLE(...)             printf("%*s",ident*4," ");PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_SYMTABLE(...)
#endif

#if ATOMLANG_CODEGEN_DEBUG
#define DEBUG_CODEGEN(...)              PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_CODEGEN(...)
#endif

#if ATOMLANG_OPCODE_DEBUG
#define DEBUG_OPCODE(...)               PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_OPCODE(...)
#endif

#if ATOMLANG_BYTECODE_DEBUG
#define DEBUG_BYTECODE(...)             PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_BYTECODE(...)
#endif

#if ATOMLANG_REGISTER_DEBUG
#define DEBUG_REGISTER(...)             PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_REGISTER(...)
#endif

#if ATOMLANG_FREE_DEBUG
#define DEBUG_FREE(...)                 PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_FREE(...)
#endif

#if ATOMLANG_DESERIALIZE_DEBUG
#define DEBUG_DESERIALIZE(...)          PRINT_LINE(__VA_ARGS__)
#else
#define DEBUG_DESERIALIZE(...)
#endif

#define DEBUG_ALWAYS(...)               PRINT_LINE(__VA_ARGS__)

#endif