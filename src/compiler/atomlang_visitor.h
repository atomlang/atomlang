#ifndef __ATOMLANG_VISITOR__
#define __ATOMLANG_VISITOR__

#include "atomlang_ast.h"

#define visit(node) gvisit(self, node)

typedef struct gvisitor {
    uint32_t    nerr;           
    void        *data;          
    bool        bflag;          
    void        *delegate;      

    void (* visit_pre)(struct gvisitor *self, gnode_t *node);
    void (* visit_post)(struct gvisitor *self, gnode_t *node);

    void (* visit_list_stmt)(struct gvisitor *self, gnode_compound_stmt_t *node);
    void (* visit_compound_stmt)(struct gvisitor *self, gnode_compound_stmt_t *node);
    void (* visit_label_stmt)(struct gvisitor *self, gnode_label_stmt_t *node);
    void (* visit_flow_stmt)(struct gvisitor *self, gnode_flow_stmt_t *node);
    void (* visit_jump_stmt)(struct gvisitor *self, gnode_jump_stmt_t *node);
    void (* visit_loop_stmt)(struct gvisitor *self, gnode_loop_stmt_t *node);
    void (* visit_empty_stmt)(struct gvisitor *self, gnode_empty_stmt_t *node);

    void (* visit_function_decl)(struct gvisitor *self, gnode_function_decl_t *node);
    void (* visit_variable_decl)(struct gvisitor *self, gnode_variable_decl_t *node);
    void (* visit_enum_decl)(struct gvisitor *self, gnode_enum_decl_t *node);
    void (* visit_class_decl)(struct gvisitor *self, gnode_class_decl_t *node);
    void (* visit_module_decl)(struct gvisitor *self, gnode_module_decl_t *node);

    void (* visit_binary_expr)(struct gvisitor *self, gnode_binary_expr_t *node);
    void (* visit_unary_expr)(struct gvisitor *self, gnode_unary_expr_t *node);
    void (* visit_file_expr)(struct gvisitor *self, gnode_file_expr_t *node);
    void (* visit_literal_expr)(struct gvisitor *self, gnode_literal_expr_t *node);
    void (* visit_identifier_expr)(struct gvisitor *self, gnode_identifier_expr_t *node);
    void (* visit_keyword_expr)(struct gvisitor *self, gnode_keyword_expr_t *node);
    void (* visit_list_expr)(struct gvisitor *self, gnode_list_expr_t *node);
    void (* visit_postfix_expr)(struct gvisitor *self, gnode_postfix_expr_t *node);
} gvisitor_t;

void gvisit(gvisitor_t *self, gnode_t *node);

#endif