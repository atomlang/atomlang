/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */


// includes
#include "atml_debug.h"
#include <stdio.h>
#include "atml_core.h"
#include "atml_var.h"
#include "atml_vm.h"

static const char* op_names[] = {
  #define OPCODE(name, params, stack) #name,
  #include "ATML_opcodes.h"
  #undef OPCODE
};

#define STR_AND_LEN(str) str, (uint32_t)strlen(str)

void dumpValue(ATMLVM* vm, Var value, ATMLByteBuffer* buff) {
  String* repr = toRepr(vm, value);
  vmPushTempRef(vm, &repr->_super);
  ATMLByteBufferAddString(buff, vm, repr->data, repr->length);
  vmPopTempRef(vm);
}

void dumpFunctionCode(ATMLVM* vm, Function* func, ATMLByteBuffer* buff) {

#define INDENTATION "  "
#define ADD_CHAR(vm, buff, c) ATMLByteBufferWrite(buff, vm, c)
#define INT_WIDTH 5
#define ADD_INTEGER(vm, buff, value, width)                          \
  do {                                                               \
    char sbuff[STR_INT_BUFF_SIZE];                                   \
    int length;                                                      \
    if ((width) > 0) length = sprintf(sbuff, "%*d", (width), value); \
    else length = sprintf(sbuff, "%d", value);                       \
    ATMLByteBufferAddString(buff, vm, sbuff, (uint32_t)length);        \
  } while(false)

  uint32_t i = 0;
  uint8_t* opcodes = func->fn->opcodes.data;
  uint32_t* lines = func->fn->oplines.data;
  uint32_t line = 1, last_line = 0;

  ATMLByteBufferAddString(buff, vm,
                        STR_AND_LEN("Instruction Dump of function '"));
  ATMLByteBufferAddString(buff, vm, STR_AND_LEN(func->name));
  ATMLByteBufferAddString(buff, vm, STR_AND_LEN("' \""));
  ATMLByteBufferAddString(buff, vm, STR_AND_LEN(func->owner->path->data));
  ATMLByteBufferAddString(buff, vm, STR_AND_LEN("\"\n"));

#define READ_BYTE() (opcodes[i++])
#define READ_SHORT() (i += 2, opcodes[i - 2] << 8 | opcodes[i-1])

#define NO_ARGS() ADD_CHAR(vm, buff, '\n')
#define BYTE_ARG()                                 \
  do {                                             \
    ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH); \
    ADD_CHAR(vm, buff, '\n');                      \
  } while (false)

#define SHORT_ARG()                                 \
  do {                                              \
    ADD_INTEGER(vm, buff, READ_SHORT(), INT_WIDTH); \
    ADD_CHAR(vm, buff, '\n');                       \
  } while (false)

  while (i < func->fn->opcodes.count) {
    ASSERT_INDEX(i, func->fn->opcodes.count);

    line = lines[i];
    if (line != last_line) {
      last_line = line;
      ATMLByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION));
      ADD_INTEGER(vm, buff, line, INT_WIDTH - 1);
      ADD_CHAR(vm, buff, ':');

    } else {
      ATMLByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION "     "));
    }


    ATMLByteBufferAddString(buff, vm, STR_AND_LEN(INDENTATION));
    ADD_INTEGER(vm, buff, i, INT_WIDTH - 1);
    ATMLByteBufferAddString(buff, vm, STR_AND_LEN("  "));

    const char* op_name = op_names[opcodes[i]];
    uint32_t op_length = (uint32_t)strlen(op_name);
    ATMLByteBufferAddString(buff, vm, op_name, op_length);
    for (uint32_t j = 0; j < 16 - op_length; j++) { // Padding.
      ADD_CHAR(vm, buff, ' ');
    }

    Opcode op = (Opcode)func->fn->opcodes.data[i++];
    switch (op) {
      case OP_PUSH_CONSTANT:
      {
        int index = READ_SHORT();
        ASSERT_INDEX((uint32_t)index, func->owner->literals.count);
        Var value = func->owner->literals.data[index];

        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ADD_CHAR(vm, buff, ' ');
        dumpValue(vm, value, buff);
        ADD_CHAR(vm, buff, '\n');
        break;
      }

      case OP_PUSH_NULL:
      case OP_PUSH_0:
      case OP_PUSH_TRUE:
      case OP_PUSH_FALSE:
      case OP_SWAP:
        NO_ARGS();
        break;

      case OP_PUSH_LIST:     SHORT_ARG(); break;
      case OP_PUSH_INSTANCE:
      {
        int ty_index = READ_BYTE();
        ASSERT_INDEX((uint32_t)ty_index, func->owner->classes.count);
        uint32_t name_ind = func->owner->classes.data[ty_index]->name;
        ASSERT_INDEX(name_ind, func->owner->names.count);
        String* ty_name = func->owner->names.data[name_ind];

        ADD_INTEGER(vm, buff, ty_index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" [Ty:"));
        ATMLByteBufferAddString(buff, vm, ty_name->data, ty_name->length);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }
      case OP_PUSH_MAP:      NO_ARGS();   break;
      case OP_LIST_APPEND:   NO_ARGS();   break;
      case OP_MAP_INSERT:    NO_ARGS();   break;
      case OP_INST_APPEND:   NO_ARGS();   break;

      case OP_PUSH_LOCAL_0:
      case OP_PUSH_LOCAL_1:
      case OP_PUSH_LOCAL_2:
      case OP_PUSH_LOCAL_3:
      case OP_PUSH_LOCAL_4:
      case OP_PUSH_LOCAL_5:
      case OP_PUSH_LOCAL_6:
      case OP_PUSH_LOCAL_7:
      case OP_PUSH_LOCAL_8:
      case OP_PUSH_LOCAL_N:
      {

        int arg;
        if (op == OP_PUSH_LOCAL_N) {
          arg = READ_BYTE();
          ADD_INTEGER(vm, buff, arg, INT_WIDTH);

        } else {
          arg = (int)(op - OP_PUSH_LOCAL_0);
          for (int j = 0; j < INT_WIDTH; j++) ADD_CHAR(vm, buff, ' ');
        }

        if (arg < func->arity) {
          ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (param:"));
          ADD_INTEGER(vm, buff, arg, 1);
          ATMLByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));

        } else {
          ADD_CHAR(vm, buff, '\n');
        }

      } break;

      case OP_STORE_LOCAL_0:
      case OP_STORE_LOCAL_1:
      case OP_STORE_LOCAL_2:
      case OP_STORE_LOCAL_3:
      case OP_STORE_LOCAL_4:
      case OP_STORE_LOCAL_5:
      case OP_STORE_LOCAL_6:
      case OP_STORE_LOCAL_7:
      case OP_STORE_LOCAL_8:
      case OP_STORE_LOCAL_N:
      {
        int arg;
        if (op == OP_STORE_LOCAL_N) {
          arg = READ_BYTE();
          ADD_INTEGER(vm, buff, arg, INT_WIDTH);

        } else {
          arg = (int)(op - OP_STORE_LOCAL_0);
          for (int j = 0; j < INT_WIDTH; j++) ADD_CHAR(vm, buff, ' ');
        }

        if (arg < func->arity) {

          ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (param:"));
          ADD_INTEGER(vm, buff, arg, 1);
          ATMLByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));

        } else {
          ADD_CHAR(vm, buff, '\n');
        }

      } break;

      case OP_PUSH_GLOBAL:
      case OP_STORE_GLOBAL:
      {
        int index = READ_BYTE();
        int name_index = func->owner->global_names.data[index];
        String* name = func->owner->names.data[name_index];

        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        ATMLByteBufferAddString(buff, vm, name->data, name->length);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
        break;
      }

      case OP_PUSH_FN:
      {
        int fn_index = READ_BYTE();
        const char* name = func->owner->functions.data[fn_index]->name;

        ADD_INTEGER(vm, buff, fn_index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" [Fn:"));
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(name));
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }

      case OP_PUSH_TYPE:
      {
        int ty_index = READ_BYTE();
        ASSERT_INDEX((uint32_t)ty_index, func->owner->classes.count);
        uint32_t name_ind = func->owner->classes.data[ty_index]->name;
        ASSERT_INDEX(name_ind, func->owner->names.count);
        String* ty_name = func->owner->names.data[name_ind];

        ADD_INTEGER(vm, buff, ty_index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" [Ty:"));
        ATMLByteBufferAddString(buff, vm, ty_name->data, ty_name->length);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));

        break;
      }

      case OP_PUSH_BUILTIN_FN:
      {
        int index = READ_BYTE();
        const char* name = getBuiltinFunctionName(vm, index);

        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" [Fn:"));
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(name));
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("]\n"));
        break;
      }

      case OP_POP:    NO_ARGS(); break;
      case OP_IMPORT:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];

        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        ATMLByteBufferAddString(buff, vm, name->data, name->length);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
        break;
      }

      case OP_CALL:
        ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (argc)\n"));
        break;

      case OP_TAIL_CALL:
        // Prints: %5d (argc)\n
        ADD_INTEGER(vm, buff, READ_BYTE(), INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (argc)\n"));
        break;

      case OP_ITER_TEST: NO_ARGS(); break;

      case OP_ITER:
      case OP_JUMP:
      case OP_JUMP_IF:
      case OP_JUMP_IF_NOT:
      {
        int offset = READ_SHORT();

        // Prints: %5d (ip:%d)\n
        ADD_INTEGER(vm, buff, offset, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (ip:"));
        ADD_INTEGER(vm, buff, i + offset, 0);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));
        break;
      }

      case OP_LOOP:
      {
        int offset = READ_SHORT();

        // Prints: %5d (ip:%d)\n
        ADD_INTEGER(vm, buff, -offset, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" (ip:"));
        ADD_INTEGER(vm, buff, i - offset, 0);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(")\n"));
        break;
      }

      case OP_RETURN: NO_ARGS(); break;

      case OP_GET_ATTRIB:
      case OP_GET_ATTRIB_KEEP:
      case OP_SET_ATTRIB:
      {
        int index = READ_SHORT();
        String* name = func->owner->names.data[index];

        // Prints: %5d '%s'\n
        ADD_INTEGER(vm, buff, index, INT_WIDTH);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN(" '"));
        ATMLByteBufferAddString(buff, vm, name->data, name->length);
        ATMLByteBufferAddString(buff, vm, STR_AND_LEN("'\n"));
      } break;

      case OP_GET_SUBSCRIPT:
      case OP_GET_SUBSCRIPT_KEEP:
      case OP_SET_SUBSCRIPT:
        NO_ARGS();
        break;

      case OP_NEGATIVE:
      case OP_NOT:
      case OP_BIT_NOT:
      case OP_ADD:
      case OP_SUBTRACT:
      case OP_MULTIPLY:
      case OP_DIVIDE:
      case OP_MOD:
      case OP_BIT_AND:
      case OP_BIT_OR:
      case OP_BIT_XOR:
      case OP_BIT_LSHIFT:
      case OP_BIT_RSHIFT:
      case OP_EQEQ:
      case OP_NOTEQ:
      case OP_LT:
      case OP_LTEQ:
      case OP_GT:
      case OP_GTEQ:
      case OP_RANGE:
      case OP_IN:
      case OP_REPL_PRINT:
      case OP_END:
        NO_ARGS();
        break;

      default:
        UNREACHABLE();
        break;
    }
  }

  ADD_CHAR(vm, buff, '\0');

// Undefin everything defined for this function.
#undef INDENTATION
#undef ADD_CHAR
#undef STR_AND_LEN
#undef READ_BYTE
#undef READ_SHORT
#undef BYTE_ARG
#undef SHORT_ARG

}

//void dumpValue(ATMLVM* vm, Var value) {
//  ATMLByteBuffer buff;
//  ATMLByteBufferInit(&buff);
//  _dumpValueInternal(vm, value, &buff);
//  ATMLByteBufferWrite(&buff, vm, '\0');
//  printf("%s", (const char*)buff.data);
//}
//
//void dumpFunctionCode(ATMLVM* vm, Function* func) {
//  ATMLByteBuffer buff;
//  ATMLByteBufferInit(&buff);
//  _dumpFunctionCodeInternal(vm, func, &buff);
//  ATMLByteBufferWrite(&buff, vm, '\0');
//  printf("%s", (const char*)buff.data);
//}

void dumpGlobalValues(ATMLVM* vm) {
  Fiber* fiber = vm->fiber;
  int frame_ind = fiber->frame_count - 1;
  ASSERT(frame_ind >= 0, OOPS);
  CallFrame* frame = &fiber->frames[frame_ind];
  Script* scr = frame->fn->owner;

  for (uint32_t i = 0; i < scr->global_names.count; i++) {
    String* name = scr->names.data[scr->global_names.data[i]];
    Var value = scr->globals.data[i];
    printf("%10s = ", name->data);

    // Dump value. TODO: refactor.
    ATMLByteBuffer buff;
    ATMLByteBufferInit(&buff);
    dumpValue(vm, value, &buff);
    ATMLByteBufferWrite(&buff, vm, '\0');
    printf("%s", (const char*)buff.data);
    ATMLByteBufferClear(&buff, vm);

    printf("\n");
  }
}

void dumpStackFrame(ATMLVM* vm) {
  Fiber* fiber = vm->fiber;
  int frame_ind = fiber->frame_count - 1;
  ASSERT(frame_ind >= 0, OOPS);
  CallFrame* frame = &fiber->frames[frame_ind];
  Var* sp = fiber->sp - 1;

  printf("Frame[%d]\n", frame_ind);
  for (; sp >= frame->rbp; sp--) {
    printf("       ");

    // Dump value. TODO: refactor.
    ATMLByteBuffer buff;
    ATMLByteBufferInit(&buff);
    dumpValue(vm, *sp, &buff);
    ATMLByteBufferWrite(&buff, vm, '\0');
    printf("%s", (const char*)buff.data);
    ATMLByteBufferClear(&buff, vm);

    printf("\n");
  }
}

#undef _STR_AND_LEN