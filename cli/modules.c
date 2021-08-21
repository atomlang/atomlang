/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "modules.h"


#define NEW_OBJ(Ty) (Ty*)malloc(sizeof(Ty))

// Dellocate module object, allocated by NEW_OBJ(). Called by the freeObj
// callback.
#define FREE_OBJ(ptr) free(ptr)

void initObj(Obj* obj, ObjType type) {
  obj->type = type;
}

void objGetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    if (strcmp(attrib.string, "closed") == 0) {
      ATMLReturnBool(vm, file->closed);
      return;
    }
  }

  return; // Attribute not found.
}

bool objSetAttrib(ATMLVM* vm, void* instance, uint32_t id, ATMLStringPtr attrib) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    // Nothing to change.
  }

  return false;
}

void freeObj(ATMLVM* vm, void* instance, uint32_t id) {
  Obj* obj = (Obj*)instance;
  ASSERT(obj->type == (ObjType)id, OOPS);

  // If the file isn't closed, close it to flush it's buffer.
  if (obj->type == OBJ_FILE) {
    File* file = (File*)obj;
    if (!file->closed) {
      if (fclose(file->fp) != 0) { /* TODO: error! */ }
      file->closed = true;
    }
  }

  FREE_OBJ(obj);
}

const char* getObjName(uint32_t id) {
  switch ((ObjType)id) {
    case OBJ_FILE: return "File";
  }
  return NULL;
}

#include "thirdparty/cwalk/cwalk.h"
  #if defined(_WIN32) && (defined(_MSC_VER) || defined(__TINYC__))
  #include "thirdparty/dirent/dirent.h"
#else
  #include <dirent.h>
#endif

#if defined(_WIN32)
  #include <windows.h>
  #include <direct.h>
  #define get_cwd _getcwd
#else
  #include <unistd.h>
  #define get_cwd getcwd
#endif

// The cstring pointer buffer size used in path.join(p1, p2, ...). Tune this
// value as needed.
#define MAX_JOIN_PATHS 8


bool pathIsAbsolute(const char* path) {
  return cwk_path_is_absolute(path);
}

void pathGetDirName(const char* path, size_t* length) {
  cwk_path_get_dirname(path, length);
}

size_t pathNormalize(const char* path, char* buff, size_t buff_size) {
  return cwk_path_normalize(path, buff, buff_size);
}

size_t pathJoin(const char* path_a, const char* path_b, char* buffer,
  size_t buff_size) {
  return cwk_path_join(path_a, path_b, buffer, buff_size);
}

/*****************************************************************************/
/* PATH INTERNAL FUNCTIONS                                                   */
/*****************************************************************************/

static inline bool pathIsFileExists(const char* path) {
  FILE* file = fopen(path, "r");
  if (file != NULL) {
    fclose(file);
    return true;
  }
  return false;
}

// Reference: https://stackoverflow.com/a/12510903/10846399
static inline bool pathIsDirectoryExists(const char* path) {
  DIR* dir = opendir(path);
  if (dir) { /* Directory exists. */
    closedir(dir);
    return true;
  } else if (errno == ENOENT) { /* Directory does not exist. */
  } else { /* opendir() failed for some other reason. */
  }

  return false;
}

static inline bool pathIsExists(const char* path) {
  return pathIsFileExists(path) || pathIsDirectoryExists(path);
}

static inline size_t pathAbs(const char* path, char* buff, size_t buff_size) {

  char cwd[FILENAME_MAX];

  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return cwk_path_get_absolute(cwd, path, buff, buff_size);
}

/*****************************************************************************/
/* PATH MODULES FUNCTIONS                                                    */
/*****************************************************************************/

static void _pathSetStyleUnix(ATMLVM* vm) {
  bool value;
  if (!ATMLGetArgBool(vm, 1, &value)) return;
  cwk_path_set_style((value) ? CWK_STYLE_UNIX : CWK_STYLE_WINDOWS);
}

static void _pathGetCWD(ATMLVM* vm) {
  char cwd[FILENAME_MAX];
  if (get_cwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: Handle error.
  }
  ATMLReturnString(vm, cwd);
}

static void _pathAbspath(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  char abspath[FILENAME_MAX];
  size_t len = pathAbs(path, abspath, sizeof(abspath));
  ATMLReturnStringLength(vm, abspath, len);
}

static void _pathRelpath(ATMLVM* vm) {
  const char* from, * path;
  if (!ATMLGetArgString(vm, 1, &from, NULL)) return;
  if (!ATMLGetArgString(vm, 2, &path, NULL)) return;

  char abs_from[FILENAME_MAX];
  pathAbs(from, abs_from, sizeof(abs_from));

  char abs_path[FILENAME_MAX];
  pathAbs(path, abs_path, sizeof(abs_path));

  char result[FILENAME_MAX];
  size_t len = cwk_path_get_relative(abs_from, abs_path,
    result, sizeof(result));
  ATMLReturnStringLength(vm, result, len);
}

static void _pathJoin(ATMLVM* vm) {
  const char* paths[MAX_JOIN_PATHS + 1]; // +1 for NULL.
  int argc = ATMLGetArgc(vm);

  if (argc > MAX_JOIN_PATHS) {
    ATMLSetRuntimeError(vm, "Cannot join more than " STRINGIFY(MAX_JOIN_PATHS)
                           "paths.");
    return;
  }

  for (int i = 0; i < argc; i++) {
    ATMLGetArgString(vm, i + 1, &paths[i], NULL);
  }
  paths[argc] = NULL;

  char result[FILENAME_MAX];
  size_t len = cwk_path_join_multiple(paths, result, sizeof(result));
  ATMLReturnStringLength(vm, result, len);
}

static void _pathNormalize(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  char result[FILENAME_MAX];
  size_t len = cwk_path_normalize(path, result, sizeof(result));
  ATMLReturnStringLength(vm, result, len);
}

static void _pathBaseName(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  const char* base_name;
  size_t length;
  cwk_path_get_basename(path, &base_name, &length);
  ATMLReturnString(vm, base_name);
}

static void _pathDirName(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  size_t length;
  cwk_path_get_dirname(path, &length);
  ATMLReturnStringLength(vm, path, length);
}

static void _pathIsPathAbs(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  ATMLReturnBool(vm, cwk_path_is_absolute(path));
}

static void _pathGetExtension(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  const char* ext;
  size_t length;
  if (cwk_path_get_extension(path, &ext, &length)) {
    ATMLReturnStringLength(vm, ext, length);
  } else {
    ATMLReturnStringLength(vm, NULL, 0);
  }
}

static void _pathExists(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;
  ATMLReturnBool(vm, pathIsExists(path));
}

static void _pathIsFile(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;
  ATMLReturnBool(vm, pathIsFileExists(path));
}

static void _pathIsDir(ATMLVM* vm) {
  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;
  ATMLReturnBool(vm, pathIsDirectoryExists(path));
}

void registerModulePath(ATMLVM* vm) {
  ATMLHandle* path = ATMLNewModule(vm, "path");

  ATMLModuleAddFunction(vm, path, "setunix",   _pathSetStyleUnix, 1);
  ATMLModuleAddFunction(vm, path, "getcwd",    _pathGetCWD,       0);
  ATMLModuleAddFunction(vm, path, "abspath",   _pathAbspath,      1);
  ATMLModuleAddFunction(vm, path, "relpath",   _pathRelpath,      2);
  ATMLModuleAddFunction(vm, path, "join",      _pathJoin,        -1);
  ATMLModuleAddFunction(vm, path, "normalize", _pathNormalize,    1);
  ATMLModuleAddFunction(vm, path, "basename",  _pathBaseName,     1);
  ATMLModuleAddFunction(vm, path, "dirname",   _pathDirName,      1);
  ATMLModuleAddFunction(vm, path, "isabspath", _pathIsPathAbs,    1);
  ATMLModuleAddFunction(vm, path, "getext",    _pathGetExtension, 1);
  ATMLModuleAddFunction(vm, path, "exists",    _pathExists,       1);
  ATMLModuleAddFunction(vm, path, "isfile",    _pathIsFile,       1);
  ATMLModuleAddFunction(vm, path, "isdir",     _pathIsDir,        1);

  ATMLReleaseHandle(vm, path);
}

static void _fileOpen(ATMLVM* vm) {

  int argc = ATMLGetArgc(vm);
  if (!ATMLCheckArgcRange(vm, argc, 1, 2)) return;

  const char* path;
  if (!ATMLGetArgString(vm, 1, &path, NULL)) return;

  const char* mode_str = "r";
  FileAccessMode mode = FMODE_READ;

  if (argc == 2) {
    if (!ATMLGetArgString(vm, 2, &mode_str, NULL)) return;

    // Check if the mode string is valid, and update the mode value.
    do {
      if (strcmp(mode_str, "r")  == 0) { mode = FMODE_READ;       break; }
      if (strcmp(mode_str, "w")  == 0) { mode = FMODE_WRITE;      break; }
      if (strcmp(mode_str, "a")  == 0) { mode = FMODE_APPEND;     break; }
      if (strcmp(mode_str, "r+") == 0) { mode = FMODE_READ_EXT;   break; }
      if (strcmp(mode_str, "w+") == 0) { mode = FMODE_WRITE_EXT;  break; }
      if (strcmp(mode_str, "a+") == 0) { mode = FMODE_APPEND_EXT; break; }

      // TODO: (fmt, ...) va_arg for runtime error public api.
      // If we reached here, that means it's an invalid mode string.
      ATMLSetRuntimeError(vm, "Invalid mode string.");
      return;
    } while (false);
  }

  FILE* fp = fopen(path, mode_str);

  if (fp != NULL) {
    File* file = NEW_OBJ(File);
    initObj(&file->_super, OBJ_FILE);
    file->fp = fp;
    file->mode = mode;
    file->closed = false;

    ATMLReturnInstNative(vm, (void*)file, OBJ_FILE);

  } else {
    ATMLReturnNull(vm);
  }
}

static void _fileRead(ATMLVM* vm) {
  File* file;
  if (!ATMLGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;

  if (file->closed) {
    ATMLSetRuntimeError(vm, "Cannot read from a closed file.");
    return;
  }

  if ((file->mode != FMODE_READ) && ((_FMODE_EXT & file->mode) == 0)) {
    ATMLSetRuntimeError(vm, "File is not readable.");
    return;
  }

  char buff[2048];
  fread((void*)buff, sizeof(char), sizeof(buff), file->fp);
  ATMLReturnString(vm, (const char*)buff);
}

static void _fileWrite(ATMLVM* vm) {
  File* file;
  const char* text; uint32_t length;
  if (!ATMLGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;
  if (!ATMLGetArgString(vm, 2, &text, &length)) return;

  if (file->closed) {
    ATMLSetRuntimeError(vm, "Cannot write to a closed file.");
    return;
  }

  if ((file->mode != FMODE_WRITE) && ((_FMODE_EXT & file->mode) == 0)) {
    ATMLSetRuntimeError(vm, "File is not writable.");
    return;
  }

  fwrite(text, sizeof(char), (size_t)length, file->fp);
}

static void _fileClose(ATMLVM* vm) {
  File* file;
  if (!ATMLGetArgInst(vm, 1, OBJ_FILE, (void**)&file)) return;

  if (file->closed) {
    ATMLSetRuntimeError(vm, "File already closed.");
    return;
  }

  if (fclose(file->fp) != 0) {
    ATMLSetRuntimeError(vm, "fclose() failed!\n"                     \
                      "  at " __FILE__ ":" STRINGIFY(__LINE__) ".");
  }
  file->closed = true;
}

void registerModuleFile(ATMLVM* vm) {
  ATMLHandle* file = ATMLNewModule(vm, "File");

  ATMLModuleAddFunction(vm, file, "open",  _fileOpen, -1);
  ATMLModuleAddFunction(vm, file, "read",  _fileRead,  1);
  ATMLModuleAddFunction(vm, file, "write", _fileWrite, 2);
  ATMLModuleAddFunction(vm, file, "close", _fileClose, 1);

  ATMLReleaseHandle(vm, file);
}

void registerModules(ATMLVM* vm) {
  registerModuleFile(vm);
  registerModulePath(vm);
}
