/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */


#ifndef DEBUG_H
#define DEBUG_H

// includes
#include "atml_internal.h"
#include "atml_var.h"

void dumpValue(ATMLVM* vm, Var value, ATMLByteBuffer* buff);

void dumpFunctionCode(ATMLVM* vm, Function* func, ATMLByteBuffer* buff);

void dumpGlobalValues(ATMLVM* vm);

void dumpStackFrame(ATMLVM* vm);

#endif 