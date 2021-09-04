/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#ifndef COMMON_H
#define COMMON_H

#include <atomlang.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Note that the cli itself is not a part of the atomlang compiler, instead
// it's a host application to run atomlang from the command line. We're
// embedding the atomlang VM and we can only use its public APIs, not any
// internals of it, including assertion macros. So that we've copyied the
// "common.h" header. This can be moved to "src/include/common.h" and include
// as optional header, which is something I don't like because it makes
// atomlang contain 2 headers (we'll always try to be minimal).
#include "common.h"

#define CLI_NOTICE                                                            \
  "atomlangLang " PK_VERSION_STRING " (https://github.com/atomlang/atomlang/)\n" \
  "Copyright(c) 2020 - 2021 AtomlangDevs.\n"                                 \
  "Free and open source software under the terms of the MIT license.\n"

 // FIXME: the vm user data of cli.
typedef struct {
  bool repl_mode;
} VmUserData;

#endif // COMMON_H
