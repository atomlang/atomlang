/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

// includes
#include "internal.h"
#include <ctype.h>
#include "utils.h"

const char* read_line(uint32_t* length) {
  const int size = 1024;
  char* mem = (char*)malloc(size);
  if (fgets(mem, size, stdin) == NULL) {
  }
  size_t len = strlen(mem);

  mem[len - 1] = '\0';
  if (length != NULL) *length = (uint32_t)(len - 1);

  return mem;
}

static void readLine(ByteBuffer* buff) {
  do {
    char c = (char)fgetc(stdin);
    if (c == EOF || c == '\n') break;

    byteBufferWrite(buff, (uint8_t)c);
  } while (true);

  byteBufferWrite(buff, '\0');
}

static inline bool is_str_empty(const char* line) {
  ASSERT(line != NULL, OOPS);

  for (const char* c = line; *c != '\0'; c++) {
    if (!isspace(*c)) return false;
  }
  return true;
}

int repl(ATMLVM* vm, const ATMLCompileOptions* options) {

  VmUserData* user_data = (VmUserData*)ATMLGetUserData(vm);
  user_data->repl_mode = true;

  ATMLHandle* module = ATMLNewModule(vm, "$(REPL)");

  ByteBuffer lines;
  byteBufferInit(&lines);

  ByteBuffer line;
  byteBufferInit(&line);

  bool need_more_lines = false;

  bool done = false;
  do {

    if (!need_more_lines) {
      printf(">>> ");
    } else {
      printf("... ");
    }

    readLine(&line);
    bool is_empty = is_str_empty((const char*)line.data);

    if (is_empty && !need_more_lines) {
      byteBufferClear(&line);
      ASSERT(lines.count == 0, OOPS);
      continue;
    }

    if (lines.count != 0) byteBufferWrite(&lines, '\n');
    byteBufferAddString(&lines, (const char*)line.data, line.count - 1);
    byteBufferClear(&line);
    byteBufferWrite(&lines, '\0');

    ATMLStringPtr source_ptr = { (const char*)lines.data, NULL, NULL };
    ATMLResult result = ATMLCompileModule(vm, module, source_ptr, options);

    if (result == ATML_RESULT_UNEXPECTED_EOF) {
      ASSERT(lines.count > 0 && lines.data[lines.count - 1] == '\0', OOPS);
      lines.count -= 1; 
      need_more_lines = true;
      continue;
    }

    need_more_lines = false;
    byteBufferClear(&lines);

    if (result != ATML_RESULT_SUCCESS) continue;

    ATMLHandle* _main = ATMLGetFunction(vm, module, ATML_IMPLICIT_MAIN_NAME);
    ATMLHandle* fiber = ATMLNewFiber(vm, _main);
    result = ATMLRunFiber(vm, fiber, 0, NULL);
    ATMLReleaseHandle(vm, _main);
    ATMLReleaseHandle(vm, fiber);

  } while (!done);

  byteBufferClear(&lines);
  ATMLReleaseHandle(vm, module);

  return 0;
}
