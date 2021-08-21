/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

#ifndef COMMON_H
#define COMMON_H

// includes
#include <atomlang.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"

#define CLI_NOTICE                                                            \
  "AtomLang " ATML_VERSION_STRING " (https://github.com/atomlang/atomlang/)\n" \
  "Copyright(c) 2020 - 2021 AtomLangDevelopers.\n"                                 \
  "Free and open source software under the terms of the MIT license.\n"

typedef struct {
  bool repl_mode;
} VmUserData;

#endif
