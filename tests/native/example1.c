// includes
#include <atomlang.h>
#include <stdio.h>

static const char* code =
  "  from YourModule import variableToC \n"
  "  a = 42                             \n"
  "  b = variableToC(a)                 \n"
  "  print('[atom] b =', b)           \n"
  ;


static void variableToC(ATMLVM* vm) {
  
  double a;
  if (!ATMLGetArgNumber(vm, 1, &a)) return;
  
  printf("[C] a = %f\n", a);
  
  ATMLReturnNumber(vm, 3.14);
}

static void reportError(ATMLVM* vm, ATMLErrorType type,
                        const char* file, int line,
                        const char* message) {
  fprintf(stderr, "Error: %s\n", message);
}

static void stdoutWrite(ATMLVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

int main(int argc, char** argv) {

  ATMLConfiguration config = ATMLNewConfiguration();
  config.error_fn  = reportError;
  config.write_fn  = stdoutWrite;

  ATMLVM* vm = ATMLNewVM(&config);
  
  ATMLHandle* your_module = ATMLNewModule(vm, "YourModule");
  ATMLModuleAddFunction(vm, your_module, "variableToC",  variableToC, 1);
  ATMLReleaseHandle(vm, your_module);
  
  ATMLStringPtr source = { code, NULL, NULL, 0, 0 };
  ATMLStringPtr path = { "./some/path/", NULL, NULL, 0, 0 };
  
  ATMLResult result = ATMLInterpretSource(vm, source, path, NULL/*options*/);
  
  ATMLFreeVM(vm);
  
  return (int)result;
}
