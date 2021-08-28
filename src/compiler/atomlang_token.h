#ifndef __ATOMLANG_TOKEN__
#define __ATOMLANG_TOKEN__

#include <stdint.h>
#include "debug_macros.h"

typedef enum {
    TOK_EOF    = 0, TOK_ERROR, TOK_COMMENT, TOK_STRING, TOK_NUMBER, TOK_IDENTIFIER, TOK_SPECIAL, TOK_MACRO,

    TOK_KEY_FUNC, TOK_KEY_SUPER, TOK_KEY_DEFAULT, TOK_KEY_TRUE, TOK_KEY_FALSE, TOK_KEY_IF,
    TOK_KEY_ELSE, TOK_KEY_SWITCH, TOK_KEY_BREAK, TOK_KEY_CONTINUE, TOK_KEY_RETURN, TOK_KEY_WHILE,
    TOK_KEY_REPEAT, TOK_KEY_FOR, TOK_KEY_IN, TOK_KEY_ENUM, TOK_KEY_CLASS, TOK_KEY_STRUCT, TOK_KEY_PRIVATE,
    TOK_KEY_FILE, TOK_KEY_INTERNAL, TOK_KEY_PUBLIC, TOK_KEY_STATIC, TOK_KEY_EXTERN, TOK_KEY_LAZY, TOK_KEY_CONST,
    TOK_KEY_VAR, TOK_KEY_MODULE, TOK_KEY_IMPORT, TOK_KEY_CASE, TOK_KEY_EVENT, TOK_KEY_NULL, TOK_KEY_UNDEFINED,
    TOK_KEY_ISA, TOK_KEY_CURRFUNC, TOK_KEY_CURRARGS,

    TOK_OP_SHIFT_LEFT, TOK_OP_SHIFT_RIGHT, TOK_OP_MUL, TOK_OP_DIV, TOK_OP_REM, TOK_OP_BIT_AND, TOK_OP_ADD, TOK_OP_SUB,
    TOK_OP_BIT_OR, TOK_OP_BIT_XOR, TOK_OP_BIT_NOT, TOK_OP_RANGE_EXCLUDED, TOK_OP_RANGE_INCLUDED, TOK_OP_LESS, TOK_OP_LESS_EQUAL,
    TOK_OP_GREATER, TOK_OP_GREATER_EQUAL, TOK_OP_ISEQUAL, TOK_OP_ISNOTEQUAL, TOK_OP_ISIDENTICAL, TOK_OP_ISNOTIDENTICAL,
    TOK_OP_PATTERN_MATCH, TOK_OP_AND, TOK_OP_OR, TOK_OP_TERNARY, TOK_OP_ASSIGN, TOK_OP_MUL_ASSIGN, TOK_OP_DIV_ASSIGN,
    TOK_OP_REM_ASSIGN, TOK_OP_ADD_ASSIGN, TOK_OP_SUB_ASSIGN, TOK_OP_SHIFT_LEFT_ASSIGN, TOK_OP_SHIFT_RIGHT_ASSIGN,
    TOK_OP_BIT_AND_ASSIGN, TOK_OP_BIT_OR_ASSIGN, TOK_OP_BIT_XOR_ASSIGN, TOK_OP_NOT,

    TOK_OP_SEMICOLON, TOK_OP_OPEN_PARENTHESIS, TOK_OP_COLON, TOK_OP_COMMA, TOK_OP_DOT, TOK_OP_CLOSED_PARENTHESIS,
    TOK_OP_OPEN_SQUAREBRACKET, TOK_OP_CLOSED_SQUAREBRACKET, TOK_OP_OPEN_CURLYBRACE, TOK_OP_CLOSED_CURLYBRACE,

    TOK_END
} gtoken_t;

typedef enum {
    LITERAL_STRING, LITERAL_FLOAT, LITERAL_INT, LITERAL_BOOL, LITERAL_STRING_INTERPOLATED
} gliteral_t;

typedef enum {
    BUILTIN_NONE, BUILTIN_LINE, BUILTIN_COLUMN, BUILTIN_FILE, BUILTIN_FUNC, BUILTIN_CLASS
} gbuiltin_t;

struct gtoken_s {
    gtoken_t            type;           
    uint32_t            lineno;         
    uint32_t            colno;          
    uint32_t            position;       
    uint32_t            bytes;          
    uint32_t            length;         
    uint32_t            fileid;         
    gbuiltin_t          builtin;        
    const char          *value;         
};
typedef struct gtoken_s         gtoken_s;

#define NO_TOKEN                (gtoken_s){0,0,0,0,0,0,0,0,NULL}
#define UNDEF_TOKEN             (gtoken_s){TOK_KEY_UNDEFINED,0,0,0,0,0,0,0,NULL}
#define TOKEN_BYTES(_tok)       _tok.bytes
#define TOKEN_VALUE(_tok)       _tok.value

gtoken_t        token_keyword (const char *buffer, int32_t len);
void            token_keywords_indexes (uint32_t *idx_start, uint32_t *idx_end);
const char     *token_literal_name (gliteral_t value);
const char     *token_name (gtoken_t token);
gtoken_t        token_special_builtin(gtoken_s *token);
const char     *token_string (gtoken_s token, uint32_t *len);

bool            token_isassignment (gtoken_t token);
bool            token_isaccess_specifier (gtoken_t token);
bool            token_iscompound_statement (gtoken_t token);
bool            token_isdeclaration_statement (gtoken_t token);
bool            token_iseof (gtoken_t token);
bool            token_isempty_statement (gtoken_t token);
bool            token_iserror (gtoken_t token);
bool            token_isexpression_statement (gtoken_t token);
bool            token_isflow_statement (gtoken_t token);
bool            token_isjump_statement (gtoken_t token);
bool            token_islabel_statement (gtoken_t token);
bool            token_isloop_statement (gtoken_t token);
bool            token_isidentifier (gtoken_t token);
bool            token_isimport_statement (gtoken_t token);
bool            token_ismacro (gtoken_t token);
bool            token_isoperator (gtoken_t token);
bool            token_isprimary_expression (gtoken_t token);
bool            token_isspecial_statement (gtoken_t token);
bool            token_isstatement (gtoken_t token);
bool            token_isstorage_specifier (gtoken_t token);
bool            token_isvariable_assignment (gtoken_t token);
bool            token_isvariable_declaration (gtoken_t token);

#endif