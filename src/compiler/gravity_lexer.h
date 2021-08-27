#ifndef __ATOMLANG_LEXER__
#define __ATOMLANG_LEXER__

#include "atomlang_delegate.h"
#include "atomlang_token.h"
#include "debug_macros.h"

typedef struct atomlang_lexer_t atomlang_lexer_t;

atomlang_lexer_t    *atomlang_lexer_create (const char *source, size_t len, uint32_t fileid, bool is_static);
void                atomlang_lexer_free (atomlang_lexer_t *lexer);
void                atomlang_lexer_setdelegate (atomlang_lexer_t *lexer, atomlang_delegate_t *delegate);

gtoken_t            atomlang_lexer_next (atomlang_lexer_t *lexer);
gtoken_t            atomlang_lexer_peek (atomlang_lexer_t *lexer);
void                atomlang_lexer_skip_line (atomlang_lexer_t *lexer);

uint32_t            atomlang_lexer_lineno (atomlang_lexer_t *lexer);
gtoken_s            atomlang_lexer_token (atomlang_lexer_t *lexer);
void                atomlang_lexer_token_dump (gtoken_s token);
gtoken_s            atomlang_lexer_token_next (atomlang_lexer_t *lexer);
gtoken_t            atomlang_lexer_token_type (atomlang_lexer_t *lexer);


#if ATOMLANG_LEXER_DEGUB
void                atomlang_lexer_debug (atomlang_lexer_t *lexer);
#endif

#endif