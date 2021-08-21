/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

// includes
#include <atomlang.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char* code =
  "  import Vector # The native module.                  \n"
  "  print('Module        =', Vector)                    \n"
  "                                                      \n"
  "  vec1 = Vector.new(1, 2) # Calling native method.    \n"
  "  print('vec1          =', 'Vector.new(1, 2)')        \n"
  "  print()                                             \n"
  "                                                      \n"
  "  # Using the native getter.                          \n"
  "  print('vec1.x        =', vec1.x)                    \n"
  "  print('vec1.y        =', vec1.y)                    \n"
  "  print('vec1.length   =', vec1.length)               \n"
  "  print()                                             \n"
  "                                                      \n"
  "  # Using the native setter.                          \n"
  "  vec1.x = 3; vec1.y = 4;                             \n"
  "  print('vec1.x        =', vec1.x)                    \n"
  "  print('vec1.y        =', vec1.y)                    \n"
  "  print('vec1.length   =', vec1.length)               \n"
  "  print()                                             \n"
  "                                                      \n"
  "  vec2 = Vector.new(5, 6)                             \n"
  "  vec3 = Vector.add(vec1, vec2)                       \n"
  "  print('vec3          =', 'Vector.add(vec1, vec2)')  \n"
  "  print('vec3.x        =', vec3.x)                    \n"
  "  print('vec3.y        =', vec3.y)                    \n"
  "                                                      \n"
  ;

typedef enum {
  OBJ_VECTOR = 0,
} ObjType;

typedef struct {
  double x, y; 
} Vector;

const char* getObjName(uint32_t id) {
  switch ((ObjType)id) {
    case OBJ_VECTOR: return "Vector";
  }
  return NULL; 
}

void objGetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib) {
  
  switch ((ObjType)id) {
    case OBJ_VECTOR: {
      Vector* vector = ((Vector*)instance);

      if (strcmp(attrib.string, "x") == 0) {
        ATMLReturnNumber(vm, vector->x);
        return;
        
      } else if (strcmp(attrib.string, "y") == 0) {
        ATMLReturnNumber(vm, vector->y);
        return;

      } else if (strcmp(attrib.string, "length") == 0) {
        double length = sqrt(pow(vector->x, 2) + pow(vector->y, 2));
        ATMLReturnNumber(vm, length);
        return;

      }
    } break;
  }
  
  return;
}

bool objSetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib) {
  
  switch ((ObjType)id) {
    case OBJ_VECTOR: {
      Vector* vector = ((Vector*)instance);

      if (strcmp(attrib.string, "x") == 0) {
        double x; 
        if (!ATMLGetArgNumber(vm, 0, &x)) return false;
        vector->x = x;
        return true;
        
      } else if (strcmp(attrib.string, "y") == 0) {
        double y; 
        if (!ATMLGetArgNumber(vm, 0, &y)) return false;
        vector->y = y;
        return true;
        
      }
    } break;
  }
  
  return false;
}

void freeObj(ATMLVM* vm, void* instance, uint32_t id) {
  free((void*)instance); 
}

void _vecNew(ATMLVM* vm) {
  double x, y; 
  
  if (!ATMLGetArgNumber(vm, 1, &x)) return;
  if (!ATMLGetArgNumber(vm, 2, &y)) return;
  
  Vector* vec = (Vector*)malloc(sizeof(Vector));
  vec->x = x, vec->y = y;
  ATMLReturnInstNative(vm, (void*)vec, OBJ_VECTOR);
}

void _vecAdd(ATMLVM* vm) {
  Vector *v1, *v2;
  if (!ATMLGetArgInst(vm, 1, OBJ_VECTOR, (void**)&v1)) return;
  if (!ATMLGetArgInst(vm, 2, OBJ_VECTOR, (void**)&v2)) return;
  
  Vector* v3 = (Vector*)malloc(sizeof(Vector));
  v3->x = v1->x + v2->x;
  v3->y = v1->y + v2->y;  

  ATMLReturnInstNative(vm, (void*)v3, OBJ_VECTOR);
}

void registerVector(ATMLVM* vm) {
  ATMLHandle* vector = ATMLNewModule(vm, "Vector");

  ATMLModuleAddFunction(vm, vector, "new",  _vecNew,  2);
  ATMLModuleAddFunction(vm, vector, "add",  _vecAdd,  2);

  ATMLReleaseHandle(vm, vector);
}

void reportError(ATMLVM* vm, ATMLErrorType type,
                 const char* file, int line,
                 const char* message) {
  fprintf(stderr, "Error: %s\n", message);
}

void stdoutWrite(ATMLVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}


int main(int argc, char** argv) {

  ATMLConfiguration config = ATMLNewConfiguration();
  config.error_fn           = reportError;
  config.write_fn           = stdoutWrite;
  config.inst_free_fn       = freeObj;
  config.inst_name_fn       = getObjName;
  config.inst_get_attrib_fn = objGetAttrib;
  config.inst_set_attrib_fn = objSetAttrib;

  ATMLVM* vm = ATMLNewVM(&config);
  registerVector(vm);
  
  ATMLStringPtr source = { code, NULL, NULL, 0, 0 };
  ATMLStringPtr path = { "./some/path/", NULL, NULL, 0, 0 };
  
  ATMLResult result = ATMLInterpretSource(vm, source, path, NULL/*options*/);
  ATMLFreeVM(vm);
  
  return result;
}