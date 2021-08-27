#ifndef __ATOMLANG_FILE__
#define __ATOMLANG_FILE__

#define ATOMLANG_CLASS_FILE_NAME             "File"

#include "atomlang_value.h"

void atomlang_file_register (atomlang_vm *vm);
void atomlang_file_free (void);
bool atomlang_isfile_class (atomlang_class_t *c);
const char *atomlang_file_name (void);

#endif