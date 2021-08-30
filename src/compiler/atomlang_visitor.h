#ifndef __GRAVITY_VISITOR__
#define __GRAVITY_VISITOR__

#include "atomlang_ast.h"

#define visit(node) gvisit(self, node)

typedef struct gvisitor {
	uint32_t nerr;
	void *data;
	bool bflag;
	void *delegate;

	void (* visit_pre)(struct gvisitor *self, gnode_t *node);
	void (* visit_post)(struct gvisitor *self, gnode_t *node);

} gvisitor_t;

void givist(gvisitor_t *self, gnode_t *node);


#endif