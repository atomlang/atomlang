#ifndef __ATOMLANG_JSON__
#define __ATOMLANG_JSON__

#define ATOMLANG_CLASS_JSON_NAME         "JSON"

#include "atomlang_value.h"

void atomlang_json_register (atomlang_vm *vm);
void atomlang_json_free (void);
bool atomlang_isjson_class (atomlang_class_t *c);
const char *atomlang_json_name (void);

#endif