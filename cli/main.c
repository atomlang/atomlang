/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

// includes
#include "internal.h"
#include "modules.h"
#include "thirdparty/argparse/argparse.h"

int repl(ATMLVM* vm, const ATMLCompileOptions* options);
const char* read_line(uint32_t* length);

void onResultDone(ATMLVM* vm, ATMLStringPtr result) {
  if ((bool)result.user_data) {
    free((void*)result.string);
  }
}

void errorFunction(ATMLVM* vm, ATMLErrorType type, const char* file, int line,
                   const char* message) {

  VmUserData* ud = (VmUserData*)ATMLGetUserData(vm);
  bool repl = (ud) ? ud->repl_mode : false;

  if (type == ATML_ERROR_COMPILE) {

    if (repl) fprintf(stderr, "Error: %s\n", message);
    else fprintf(stderr, "Error: %s\n       at \"%s\":%i\n",
                 message, file, line);

  } else if (type == ATML_ERROR_RUNTIME) {
    fprintf(stderr, "Error: %s\n", message);

  } else if (type == ATML_ERROR_STACKTRACE) {
    if (repl) fprintf(stderr, "  %s() [line:%i]\n", message, line);
    else fprintf(stderr, "  %s() [\"%s\":%i]\n", message, file, line);
  }
}

void writeFunction(ATMLVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

ATMLStringPtr readFunction(ATMLVM* vm) {
  ATMLStringPtr result;
  result.string = read_line(NULL);
  result.on_done = onResultDone;
  result.user_data = (void*)true;
  return result;
}

ATMLStringPtr resolvePath(ATMLVM* vm, const char* from, const char* path) {
  ATMLStringPtr result;
  result.on_done = onResultDone;

  size_t from_dir_len;
  pathGetDirName(from, &from_dir_len);


  if (from_dir_len == 0 || pathIsAbsolute(path)) {
    size_t length = strlen(path);

    char* resolved = (char*)malloc(length + 1);
    pathNormalize(path, resolved, length + 1);

    result.string = resolved;
    result.user_data = (void*)true;

  } else {
    char from_dir[FILENAME_MAX];
    strncpy(from_dir, from, from_dir_len);
    from_dir[from_dir_len] = '\0';

    char fullpath[FILENAME_MAX];
    size_t length = pathJoin(from_dir, path, fullpath, sizeof(fullpath));

    char* resolved = (char*)malloc(length + 1);
    pathNormalize(fullpath, resolved, length + 1);

    result.string = resolved;
    result.user_data = (void*)true;
  }

  return result;
}

ATMLStringPtr loadScript(ATMLVM* vm, const char* path) {

  ATMLStringPtr result;
  result.on_done = onResultDone;

  FILE* file = fopen(path, "r");
  if (file == NULL) {
    
    result.string = NULL;
    return result;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* buff = (char*)malloc((size_t)(file_size) + 1);

  size_t read = fread(buff, sizeof(char), file_size, file);
  buff[read] = '\0';
  fclose(file);

  result.string = buff;
  result.user_data = (void*)true;

  return result;
}

static ATMLVM* intializeAtomlangVM() {
  ATMLConfiguration config = ATMLNewConfiguration();
  config.error_fn = errorFunction;
  config.write_fn = writeFunction;
  config.read_fn = readFunction;

  config.inst_free_fn = freeObj;
  config.inst_name_fn = getObjName;
  config.inst_get_attrib_fn = objGetAttrib;
  config.inst_set_attrib_fn = objSetAttrib;

  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  return ATMLNewVM(&config);
}

int main(int argc, const char** argv) {

  const char* usage[] = {
    "atomlang ... [-c cmd | file] ...",
    NULL,
  };

  const char* cmd = NULL;
  int debug = false, help = false, quiet = false, version = false;
  struct argparse_option cli_opts[] = {
      OPT_STRING('c', "cmd", (void*)&cmd,
        "Evaluate and run the passed string.", NULL, 0, 0),

      OPT_BOOLEAN('d', "debug", (void*)&debug,
        "Compile and run the debug version.", NULL, 0, 0),

      OPT_BOOLEAN('h', "help",  (void*)&help,
        "Prints this help message and exit.", NULL, 0, 0),

      OPT_BOOLEAN('q', "quiet", (void*)&quiet,
        "Don't print version and copyright statement on REPL startup.",
        NULL, 0, 0),

      OPT_BOOLEAN('v', "version", &version,
        "Prints the atomlang version and exit.", NULL, 0, 0),
      OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, cli_opts, usage, 0);
  argc = argparse_parse(&argparse, argc, argv);

  if (help) { 
    argparse_usage(&argparse);
    return 0;
  }

  if (version) { 
    fprintf(stdout, "atomlang %s\n", ATML_VERSION_STRING);
    return 0;
  }

  int exitcode = 0;

  ATMLVM* vm = intializeAtomlangVM();
  VmUserData user_data;
  user_data.repl_mode = false;
  ATMLSetUserData(vm, &user_data);

  registerModules(vm);

  ATMLCompileOptions options = ATMLNewCompilerOptions();
  options.debug = debug;

  if (cmd != NULL) { 

    ATMLStringPtr source = { cmd, NULL, NULL, 0, 0 };
    ATMLStringPtr path = { "$(Source)", NULL, NULL, 0, 0 };
    ATMLResult result = ATMLInterpretSource(vm, source, path, NULL);
    exitcode = (int)result;

  } else if (argc == 0) { 

    if (!quiet) {
      printf("%s", CLI_NOTICE);
    }

    options.repl_mode = true;
    exitcode = repl(vm, &options);

  } else { 

    ATMLStringPtr resolved = resolvePath(vm, ".", argv[0]);
    ATMLStringPtr source = loadScript(vm, resolved.string);

    if (source.string != NULL) {
      ATMLResult result = ATMLInterpretSource(vm, source, resolved, &options);
      exitcode = (int)result;
    } else {
      fprintf(stderr, "Error: cannot open file at \"%s\"\n", resolved.string);
      if (resolved.on_done != NULL) resolved.on_done(vm, resolved);
      if (source.on_done != NULL) source.on_done(vm, source);
    }
  }

  ATMLFreeVM(vm);
  return exitcode;
}