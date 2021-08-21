/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

// includes
#include "internal.h"

typedef enum {
  OBJ_FILE = 1,
} ObjType;

typedef struct {
  ObjType type;
} Obj;

typedef enum {

  FMODE_READ       = (1 << 0),
  FMODE_WRITE      = (1 << 1),
  FMODE_APPEND     = (1 << 2),

  _FMODE_EXT       = (1 << 3),
  FMODE_READ_EXT   = (_FMODE_EXT | FMODE_READ),
  FMODE_WRITE_EXT  = (_FMODE_EXT | FMODE_WRITE),
  FMODE_APPEND_EXT = (_FMODE_EXT | FMODE_APPEND),
} FileAccessMode;

typedef struct {
  Obj _super;

  FILE* fp;            
  FileAccessMode mode; 
  bool closed;         
} File;

void initObj(Obj* obj, ObjType type);

void objGetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib);

bool objSetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib);

void freeObj(ATMLVM* vm, void* instance, uint32_t id);

const char* getObjName(uint32_t id);

void registerModules(ATMLVM* vm);

bool pathIsAbsolute(const char* path);
void pathGetDirName(const char* path, size_t* length);
size_t pathNormalize(const char* path, char* buff, size_t buff_size);
size_t pathJoin(const char* from, const char* path, char* buffer,
                size_t buff_size);
