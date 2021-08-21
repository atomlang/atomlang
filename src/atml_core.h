/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

#ifndef CORE_H
#define CORE_H

// includes
#include "atml_internal.h"
#include "atml_var.h"

void initializeCore(ATMLVM* vm);

int findBuiltinFunction(const ATMLVM* vm, const char* name, uint32_t length);

Function* getBuiltinFunction(const ATMLVM* vm, int index);

const char* getBuiltinFunctionName(const ATMLVM* vm, int index);

Script* getCoreLib(const ATMLVM* vm, String* name);

Var varAdd(ATMLVM* vm, Var v1, Var v2);         
Var varSubtract(ATMLVM* vm, Var v1, Var v2);    
Var varMultiply(ATMLVM* vm, Var v1, Var v2);    
Var varDivide(ATMLVM* vm, Var v1, Var v2);      
Var varModulo(ATMLVM* vm, Var v1, Var v2);      

Var varBitAnd(ATMLVM* vm, Var v1, Var v2);      
Var varBitOr(ATMLVM* vm, Var v1, Var v2);       
Var varBitXor(ATMLVM* vm, Var v1, Var v2);      
Var varBitLshift(ATMLVM* vm, Var v1, Var v2);   
Var varBitRshift(ATMLVM* vm, Var v1, Var v2);   
Var varBitNot(ATMLVM* vm, Var v);               

bool varGreater(Var v1, Var v2); 
bool varLesser(Var v1, Var v2);  

bool varContains(ATMLVM* vm, Var elem, Var container);

Var varGetAttrib(ATMLVM* vm, Var on, String* attrib);

void varSetAttrib(ATMLVM* vm, Var on, String* name, Var value);

Var varGetSubscript(ATMLVM* vm, Var on, Var key);

void varsetSubscript(ATMLVM* vm, Var on, Var key, Var value);

#endif 