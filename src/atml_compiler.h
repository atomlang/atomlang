#ifndef COMPILER_H
#define COMPILER_H

// includes
#include "atml_internal.h"
#include "atml_var.h"

typedef enum {
  #define OPCODE(name, _, __) OP_##name,
  #include "atml_opcodes.h"
  #undef OPCODE
} Opcode;

typedef struct Compiler Compiler;

ATMLResult compile(ATMLVM* vm, Script* script, const char* source,
                 const ATMLCompileOptions* options);

void compilerMarkObjects(ATMLVM* vm, Compiler* compiler);

#endif
