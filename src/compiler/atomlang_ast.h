#ifndef __ATOMMLANG_AST__
#define __ATOMMLANG_AST__

#include "debug_macros.h"
#include "atomlang_array.h"
#include "atomlang_token.h"

typedef enum {
    NODE_LIST_STAT, NODE_COMPOUND_STAT, NODE_LABEL_STAT, NODE_FLOW_STAT, NODE_JUMP_STAT, NODE_LOOP_STAT, NODE_EMPTY_STAT,

    NODE_ENUM_DECL, NODE_FUNCTION_DECL, NODE_VARIABLE_DECL, NODE_CLASS_DECL, NODE_MODULE_DECL, NODE_VARIABLE,

    NODE_BINARY_EXPR, NODE_UNARY_EXPR, NODE_FILE_EXPR, NODE_LIST_EXPR, NODE_LITERAL_EXPR, NODE_IDENTIFIER_EXPR,
    NODE_POSTFIX_EXPR, NODE_KEYWORD_EXPR,

    NODE_CALL_EXPR, NODE_SUBSCRIPT_EXPR, NODE_ACCESS_EXPR
} gnode_n;

typedef enum {
    LOCATION_LOCAL,
    LOCATION_GLOBAL,
    LOCATION_UPVALUE,
    LOCATION_CLASS_IVAR_SAME,
    LOCATION_CLASS_IVAR_OUTER
} gnode_location_type;

typedef struct {
    gnode_n     tag;                        
    uint32_t    refcount;                  
    uint32_t    block_length;              
    gtoken_s    token;                     
    bool        is_assignment;             
    void        *decl;                     
} gnode_t;

typedef struct {
    gnode_t     *node;                      
    uint32_t    index;                      
    uint32_t    selfindex;                  
    bool        is_direct;                  
} gupvalue_t;

typedef marray_t(gnode_t *)                 gnode_r;
typedef marray_t(gupvalue_t *)              gupvalue_r;

#ifndef ATOMMLANG_SYMBOLTABLE_DEFINED
#define ATOMMLANG_SYMBOLTABLE_DEFINED
typedef struct symboltable_t                symboltable_t;
#endif

typedef struct {
    gnode_location_type type;               
    uint16_t            index;              
    uint16_t            nup;                
} gnode_location_t;

typedef struct {
    gnode_t             base;               
    symboltable_t       *symtable;          
    gnode_r             *stmts;             
    uint32_t            nclose;             
} gnode_compound_stmt_t;
typedef gnode_compound_stmt_t gnode_list_stmt_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *expr;              
    gnode_t             *stmt;              
    uint32_t            label_case;         
} gnode_label_stmt_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *cond;              
    gnode_t             *stmt;              
    gnode_t             *elsestmt;          
} gnode_flow_stmt_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *cond;              
    gnode_t             *stmt;              
    gnode_t             *expr;              
    uint32_t            nclose;             
} gnode_loop_stmt_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *expr;              
} gnode_jump_stmt_t;


typedef struct {
    gnode_t             base;               
    gnode_t             *env;               
    gtoken_t            access;             
    gtoken_t            storage;            
    symboltable_t       *symtable;          
    const char          *identifier;        
    gnode_r             *params;            
    gnode_compound_stmt_t *block;           
    uint16_t            nlocals;            
    uint16_t            nparams;            
    bool                has_defaults;       
    bool                is_closure;         
    gupvalue_r          *uplist;            
} gnode_function_decl_t;
typedef gnode_function_decl_t gnode_function_expr_t;

typedef struct {
    gnode_t             base;               
    gtoken_t            type;               
    gtoken_t            access;             
    gtoken_t            storage;            
    gnode_r             *decls;             
} gnode_variable_decl_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *env;               
    const char          *identifier;       
    const char          *annotation_type;  
    gnode_t             *expr;             
    gtoken_t            access;            
    uint16_t            index;             
    bool                upvalue;           
    bool                iscomputed;        
    gnode_variable_decl_t   *vdecl;        
} gnode_var_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *env;               
    gtoken_t            access;             
    gtoken_t            storage;            
    symboltable_t       *symtable;          
    const char          *identifier;        
} gnode_enum_decl_t;

typedef struct {
    gnode_t             base;               
    bool                bridge;             
    bool                is_struct;          
    gnode_t             *env;               
    gtoken_t            access;             
    gtoken_t            storage;            
    const char          *identifier;        
    gnode_t             *superclass;        
    bool                super_extern;       
    gnode_r             *protocols;         
    gnode_r             *decls;             
    symboltable_t       *symtable;          
    void                *data;             
    uint32_t            nivar;             
    uint32_t            nsvar;             
} gnode_class_decl_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *env;               
    gtoken_t            access;             
    gtoken_t            storage;            
    const char          *identifier;         
    gnode_r             *decls;              
    symboltable_t       *symtable;           
} gnode_module_decl_t;

typedef struct {
    gnode_t             base;              
    gtoken_t            op;                
    gnode_t             *left;              
    gnode_t             *right;           
} gnode_binary_expr_t;

typedef struct {
    gnode_t             base;             
    gtoken_t            op;                
    gnode_t             *expr;              
} gnode_unary_expr_t;

typedef struct {
    gnode_t             base;               
    cstring_r           *identifiers;      
    gnode_location_t    location;           
} gnode_file_expr_t;

typedef struct {
    gnode_t             base;               
    gliteral_t          type;               
    uint32_t            len;                 
    union {
        char            *str;               
        double          d;                   
        int64_t         n64;                 
        gnode_r         *r;                  
    } value;
} gnode_literal_expr_t;

typedef struct {
    gnode_t             base;               
    const char          *value;             
    const char          *value2;            
    gnode_t             *symbol;             
    gnode_location_t    location;            
    gupvalue_t          *upvalue;            
} gnode_identifier_expr_t;

typedef struct {
    gnode_t             base;                
} gnode_keyword_expr_t;

typedef gnode_keyword_expr_t gnode_empty_stmt_t;
typedef gnode_keyword_expr_t gnode_base_t;

typedef struct {
    gnode_t             base;               
    gnode_t             *id;                
    gnode_r             *list;              
} gnode_postfix_expr_t;

typedef struct {
    gnode_t             base;               
    union {
        gnode_t         *expr;              
        gnode_r         *args;              
    };
} gnode_postfix_subexpr_t;

typedef struct {
    gnode_t             base;             
    bool                ismap;            
    gnode_r             *list1;           
    gnode_r             *list2;            
} gnode_list_expr_t;

gnode_t *gnode_block_stat_create (gnode_n type, gtoken_s token, gnode_r *stmts, gnode_t *decl, uint32_t block_length);
gnode_t *gnode_empty_stat_create (gtoken_s token, gnode_t *decl);
gnode_t *gnode_flow_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt1, gnode_t *stmt2, gnode_t *decl, uint32_t block_length);
gnode_t *gnode_jump_stat_create (gtoken_s token, gnode_t *expr, gnode_t *decl);
gnode_t *gnode_label_stat_create (gtoken_s token, gnode_t *expr, gnode_t *stmt, gnode_t *decl);
gnode_t *gnode_loop_stat_create (gtoken_s token, gnode_t *cond, gnode_t *stmt, gnode_t *expr, gnode_t *decl, uint32_t block_length);

gnode_t *gnode_class_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_t *superclass, gnode_r *protocols, gnode_r *declarations, bool is_struct, gnode_t *decl);
gnode_t *gnode_enum_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, symboltable_t *symtable, gnode_t *decl);
gnode_t *gnode_module_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations, gnode_t *decl);
gnode_t *gnode_variable_create (gtoken_s token, const char *identifier, const char *annotation_type, gnode_t *expr, gnode_t *decl, gnode_variable_decl_t *vdecl);
gnode_t *gnode_variable_decl_create (gtoken_s token, gtoken_t type, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *declarations, gnode_t *decl);

gnode_t *gnode_function_decl_create (gtoken_s token, const char *identifier, gtoken_t access_specifier, gtoken_t storage_specifier, gnode_r *params, gnode_compound_stmt_t *block, gnode_t *decl);

gnode_t *gnode_binary_expr_create (gtoken_t op, gnode_t *left, gnode_t *right, gnode_t *decl);
gnode_t *gnode_file_expr_create (gtoken_s token, cstring_r *list, gnode_t *decl);
gnode_t *gnode_identifier_expr_create (gtoken_s token, const char *identifier, const char *identifier2, gnode_t *decl);
gnode_t *gnode_keyword_expr_create (gtoken_s token, gnode_t *decl);
gnode_t *gnode_list_expr_create (gtoken_s token, gnode_r *list1, gnode_r *list2, bool ismap, gnode_t *decl);
gnode_t *gnode_literal_bool_expr_create (gtoken_s token, int32_t n, gnode_t *decl);
gnode_t *gnode_literal_float_expr_create (gtoken_s token, double f, gnode_t *decl);
gnode_t *gnode_literal_int_expr_create (gtoken_s token, int64_t n, gnode_t *decl);
gnode_t *gnode_literal_string_expr_create (gtoken_s token, char *s, uint32_t len, bool allocated, gnode_t *decl);
gnode_t *gnode_postfix_expr_create (gtoken_s token, gnode_t *id, gnode_r *list, gnode_t *decl);
gnode_t *gnode_postfix_subexpr_create (gtoken_s token, gnode_n type, gnode_t *expr, gnode_r *list, gnode_t *decl);
gnode_t *gnode_string_interpolation_create (gtoken_s token, gnode_r *r, gnode_t *decl);
gnode_t *gnode_unary_expr_create (gtoken_t op, gnode_t *expr, gnode_t *decl);

gnode_t    *gnode_duplicate (gnode_t *node, bool deep);
const char *gnode_identifier (gnode_t *node);

gnode_r    *gnode_array_create (void);
gnode_r    *gnode_array_remove_byindex(gnode_r *list, size_t index);
void        gnode_array_sethead(gnode_r *list, gnode_t *node);
gupvalue_t *gnode_function_add_upvalue(gnode_function_decl_t *f, gnode_var_t *symbol, uint16_t n);
gnode_t    *gnode2class (gnode_t *node, bool *isextern);
cstring_r  *cstring_array_create (void);
void_r     *void_array_create (void);

void    gnode_free (gnode_t *node);
bool    gnode_is_equal (gnode_t *node1, gnode_t *node2);
bool    gnode_is_expression (gnode_t *node);
bool    gnode_is_literal (gnode_t *node);
bool    gnode_is_literal_int (gnode_t *node);
bool    gnode_is_literal_number (gnode_t *node);
bool    gnode_is_literal_string (gnode_t *node);
void    gnode_literal_dump (gnode_literal_expr_t *node, char *buffer, int buffersize);

#define gnode_array_init(r)                 marray_init(*r)
#define gnode_array_size(r)                 ((r) ? marray_size(*r) : 0)
#define gnode_array_push(r, node)           marray_push(gnode_t*,*r,node)
#define gnode_array_pop(r)                  (marray_size(*r) ? marray_pop(*r) : NULL)
#define gnode_array_get(r, i)               (((i) >= 0 && (i) < marray_size(*r)) ? marray_get(*r, (i)) : NULL)
#define gnode_array_free(r)                 do {marray_destroy(*r); mem_free((void*)r);} while (0)
#define gtype_array_each(r, block, type)    {   size_t _len = gnode_array_size(r);                \
                                                for (size_t _i=0; _i<_len; ++_i) {                \
                                                    type val = (type)gnode_array_get(r, _i);      \
                                                    block;} \
                                            }
#define gnode_array_each(r, block)          gtype_array_each(r, block, gnode_t*)
#define gnode_array_eachbase(r, block)      gtype_array_each(r, block, gnode_base_t*)

#define cstring_array_free(r)               marray_destroy(*r)
#define cstring_array_push(r, s)            marray_push(const char*,*r,s)
#define cstring_array_each(r, block)        gtype_array_each(r, block, const char*)

#define NODE_TOKEN_TYPE(_node)              _node->base.token.type
#define NODE_TAG(_node)                     ((gnode_base_t *)_node)->base.tag
#define NODE_ISA(_node,_tag)                ((_node) && NODE_TAG(_node) == _tag)
#define NODE_ISA_FUNCTION(_node)            (NODE_ISA(_node, NODE_FUNCTION_DECL))
#define NODE_ISA_CLASS(_node)               (NODE_ISA(_node, NODE_CLASS_DECL))

#define NODE_SET_ENCLOSING(_node,_enc)      (((gnode_base_t *)_node)->base.enclosing = _enc)
#define NODE_GET_ENCLOSING(_node)           ((gnode_base_t *)_node)->base.enclosing

#endif