#pragma once 

#include "pk_internal.h"
#include "pk_var.h"

// Dump the value of the [value] without a new line at the end to the buffer
// [buff]. Note that this will not write a null byte at of the buffer
// (unlike dumpFunctionCode).
void dumpValue(PKVM* vm, Var value, pkByteBuffer* buff);

// Dump opcodes of the given function to the buffer [buff].
void dumpFunctionCode(PKVM* vm, Function* func, pkByteBuffer* buff);

// Dump the all the global values of the script.
void dumpGlobalValues(PKVM* vm);

// Dump the current (top most) stack call frame.
void dumpStackFrame(PKVM* vm);