#pragma once

#include <atomlang.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"

#define CLI_NOTICE                                                            \
  "atomlangLang " PK_VERSION_STRING " (https://github.com/atomlang/atomlang/)\n" \
  "Copyright(c) 2020 - 2021 AtomlangDevs.\n"                                 \
  "Free and open source software under the terms of the MIT license.\n"

typedef struct {
  bool repl_mode;
} VmUserData;

