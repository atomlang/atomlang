#ifndef ATOMLANG_H
#define POCKETLANG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define ATML_VERSION_MAJOR 0
#define ATML_VERSION_MINOR 1
#define ATML_VERSION_PATCH 0

#define ATML_VERSION_STRING "0.1.0"

#ifdef _MSC_VER
  #define _ATML_EXPORT __declspec(dllexport)
  #define _ATML_IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
  #define _ATML_EXPORT __attribute__((visibility ("default")))
  #define _ATML_IMPORT
#else
  #define _ATML_EXPORT
  #define _ATML_IMPORT
#endif

#ifdef ATML_DLL
  #ifdef ATML_COMPILE
    #define ATML_PUBLIC _ATML_EXPORT
  #else
    #define ATML_PUBLIC _ATML_IMPORT
  #endif
#else
  #define ATML_PUBLIC
#endif

#define ATML_IMPLICIT_MAIN_NAME "$(SourceBody)"

typedef struct ATMLVM ATMLVM;

typedef struct ATMLHandle ATMLHandle;

typedef void* ATMLVar;

typedef enum {
  ATML_NULL,
  ATML_BOOL,
  ATML_NUMBER,
  ATML_STRING,
  ATML_LIST,
  ATML_MAP,
  ATML_RANGE,
  ATML_SCRIPT,
  ATML_FUNCTION,
  ATML_FIBER,
  ATML_CLASS,
  ATML_INST,
} ATMLVarType;

typedef struct ATMLStringPtr ATMLStringPtr;
typedef struct ATMLConfiguration ATMLConfiguration;
typedef struct ATMLCompileOptions ATMLCompileOptions;

typedef enum {
  ATML_ERROR_COMPILE = 0, 
  ATML_ERROR_RUNTIME,     
  ATML_ERROR_STACKTRACE,  
} ATMLErrorType;

typedef enum {
  ATML_RESULT_SUCCESS = 0,    // Successfully finished the execution.

  // Unexpected EOF while compiling the source. This is another compile time
  // error that will ONLY be returned if we're compiling with the REPL mode set
  // in the compile options. We need this specific error to indicate the host
  // application to add another line to the last input. If REPL is not enabled,
  // this will be ATML_RESULT_COMPILE_ERROR.
  ATML_RESULT_UNEXPECTED_EOF,

  ATML_RESULT_COMPILE_ERROR,  // Compilation failed.
  ATML_RESULT_RUNTIME_ERROR,  // An error occurred at runtime.
} ATMLResult;

typedef void (*ATMLNativeFn)(ATMLVM* vm);

typedef void* (*ATMLReallocFn)(void* memory, size_t new_size, void* user_data);

// Error callback function pointer. For runtime error it'll call first with
// ATML_ERROR_RUNTIME followed by multiple callbacks with ATML_ERROR_STACKTRACE.
// The error messages should be written to stderr.
typedef void (*ATMLErrorFn) (ATMLVM* vm, ATMLErrorType type,
                           const char* file, int line,
                           const char* message);

// A function callback to write [text] to stdout.
typedef void (*ATMLWriteFn) (ATMLVM* vm, const char* text);

// A function callback to read a line from stdin. The returned string shouldn't
// contain a line ending (\n or \r\n).
typedef ATMLStringPtr (*ATMLReadFn) (ATMLVM* vm);

// A function callback, that'll be called when a native instance (wrapper) is
// freed by by the garbage collector, to indicate that pocketlang is done with
// the native instance.
typedef void (*ATMLInstFreeFn) (ATMLVM* vm, void* instance, uint32_t id);

// A function callback to get the name of the native instance from pocketlang,
// using it's [id]. The returned string won't be copied by pocketlang so it's
// expected to be alived since the instance is alive and recomended to return
// a C literal string.
typedef const char* (*ATMLInstNameFn) (uint32_t id);

// A get arribute callback, called by pocket VM when trying to get an attribute
// from a native type. to return the value of the attribute use 'ATMLReturn...()'
// functions. DON'T set an error to the VM if the attribute not exists. Example
// if the '.as_string' attribute doesn't exists, pocket VM will use a default
// to string value.
typedef void (*ATMLInstGetAttribFn) (ATMLVM* vm, void* instance, uint32_t id,
                                   ATMLStringPtr attrib);

// Use ATMLGetArg...(vm, 0, ptr) function to get the value of the attribute
// and use 0 as the argument index, using any other arg index value cause UB.
//
// If the attribute dones't exists DON'T set an error, instead return false.
// Pocket VM will handle it, On success update the native instance and return
// true. And DON'T ever use 'ATMLReturn...()' in the attribute setter It's is a
// void return function.
typedef bool (*ATMLInstSetAttribFn) (ATMLVM* vm, void* instance, uint32_t id,
                                   ATMLStringPtr attrib);

// A function callback symbol for clean/free the ATMLStringResult.
typedef void (*ATMLResultDoneFn) (ATMLVM* vm, ATMLStringPtr result);

// A function callback to resolve the import script name from the [from] path
// to an absolute (or relative to the cwd). This is required to solve same
// script imported with different relative path. Set the string attribute to
// NULL to indicate if it's failed to resolve.
typedef ATMLStringPtr (*ATMLResolvePathFn) (ATMLVM* vm, const char* from,
                                        const char* path);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Set the string attribute to NULL
// to indicate if it's failed to load the script.
typedef ATMLStringPtr (*ATMLLoadScriptFn) (ATMLVM* vm, const char* path);

/*****************************************************************************/
/* POCKETLANG PUBLIC API                                                     */
/*****************************************************************************/

// Create a new ATMLConfiguration with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
ATML_PUBLIC ATMLConfiguration ATMLNewConfiguration(void);

// Create a new ATMLCompilerOptions with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
ATML_PUBLIC ATMLCompileOptions ATMLNewCompilerOptions(void);

// Allocate, initialize and returns a new VM.
ATML_PUBLIC ATMLVM* ATMLNewVM(ATMLConfiguration* config);

// Clean the VM and dispose all the resources allocated by the VM.
ATML_PUBLIC void ATMLFreeVM(ATMLVM* vm);

// Update the user data of the vm.
ATML_PUBLIC void ATMLSetUserData(ATMLVM* vm, void* user_data);

// Returns the associated user data.
ATML_PUBLIC void* ATMLGetUserData(const ATMLVM* vm);

// Create a new handle for the [value]. This is useful to keep the [value]
// alive once it acquired from the stack. Do not use the [value] once
// creating a new handle for it instead get the value from the handle by
// using ATMLGetHandleValue() function.
ATML_PUBLIC ATMLHandle* ATMLNewHandle(ATMLVM* vm, ATMLVar value);

// Return the ATMLVar pointer in the handle, the returned pointer will be valid
// till the handle is released.
ATML_PUBLIC ATMLVar ATMLGetHandleValue(const ATMLHandle* handle);

// Release the handle and allow its value to be garbage collected. Always call
// this for every handles before freeing the VM.
ATML_PUBLIC void ATMLReleaseHandle(ATMLVM* vm, ATMLHandle* handle);

// Add a global [value] to the given module.
ATML_PUBLIC void ATMLModuleAddGlobal(ATMLVM* vm, ATMLHandle* module,
                                 const char* name, ATMLHandle* value);

// Add a native function to the given module. If [arity] is -1 that means
// The function has variadic parameters and use ATMLGetArgc() to get the argc.
ATML_PUBLIC void ATMLModuleAddFunction(ATMLVM* vm, ATMLHandle* module,
                                   const char* name,
                                   ATMLNativeFn fptr, int arity);

// Returns the function from the [module] as a handle, if not found it'll
// return NULL.
ATML_PUBLIC ATMLHandle* ATMLGetFunction(ATMLVM* vm, ATMLHandle* module,
                                  const char* name);

// Compile the [module] with the provided [source]. Set the compiler options
// with the the [options] argument or set to NULL for default options.
ATML_PUBLIC ATMLResult ATMLCompileModule(ATMLVM* vm, ATMLHandle* module,
                                   ATMLStringPtr source,
                                   const ATMLCompileOptions* options);

// Interpret the source and return the result. Once it's done with the source
// and path 'on_done' will be called to clean the string if it's not NULL.
// Set the compiler options with the the [options] argument or it can be set to
// NULL for default options.
ATML_PUBLIC ATMLResult ATMLInterpretSource(ATMLVM* vm,
                                     ATMLStringPtr source,
                                     ATMLStringPtr path,
                                     const ATMLCompileOptions* options);

// Runs the fiber's function with the provided arguments (param [arc] is the
// argument count and [argv] are the values). It'll returns it's run status
// result (success or failure) if you need the yielded or returned value use
// the ATMLFiberGetReturnValue() function, and use ATMLFiberIsDone() function to
// check if the fiber can be resumed with ATMLFiberResume() function.
ATML_PUBLIC ATMLResult ATMLRunFiber(ATMLVM* vm, ATMLHandle* fiber,
                              int argc, ATMLHandle** argv);

// Resume a yielded fiber with an optional [value]. (could be set to NULL)
// It'll returns it's run status result (success or failure) if you need the
// yielded or returned value use the ATMLFiberGetReturnValue() function.
ATML_PUBLIC ATMLResult ATMLResumeFiber(ATMLVM* vm, ATMLHandle* fiber, ATMLVar value);

/*****************************************************************************/
/* POCKETLANG PUBLIC TYPE DEFINES                                            */
/*****************************************************************************/

// A string pointer wrapper to pass c string between host application and
// pocket VM. With a on_done() callback to clean it when the pocket VM is done
// with the string.
struct ATMLStringPtr {
  const char* string;     //< The string result.
  ATMLResultDoneFn on_done; //< Called once vm done with the string.
  void* user_data;        //< User related data.

  // These values are provided by the pocket VM to the host application, you're
  // not expected to set this when provideing string to the pocket VM.
  uint32_t length;  //< Length of the string.
  uint32_t hash;    //< Its 32 bit FNV-1a hash.
};

struct ATMLConfiguration {

  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  ATMLReallocFn realloc_fn;

  ATMLErrorFn error_fn;
  ATMLWriteFn write_fn;
  ATMLReadFn read_fn;

  ATMLInstFreeFn inst_free_fn;
  ATMLInstNameFn inst_name_fn;
  ATMLInstGetAttribFn inst_get_attrib_fn;
  ATMLInstSetAttribFn inst_set_attrib_fn;

  ATMLResolvePathFn resolve_path_fn;
  ATMLLoadScriptFn load_script_fn;

  // User defined data associated with VM.
  void* user_data;
};

struct ATMLCompileOptions {

  // Compile debug version of the source.
  bool debug;

  bool repl_mode;

};

ATML_PUBLIC void ATMLSetRuntimeError(ATMLVM* vm, const char* message);

ATML_PUBLIC ATMLVarType ATMLGetValueType(const ATMLVar value);

ATML_PUBLIC int ATMLGetArgc(const ATMLVM* vm);

ATML_PUBLIC bool ATMLCheckArgcRange(ATMLVM* vm, int argc, int min, int max);

ATML_PUBLIC ATMLVar ATMLGetArg(const ATMLVM* vm, int arg);

// The functions below are used to extract the function arguments from the
// stack as a type. They will first validate the argument's type and set a
// runtime error if it's not and return false. Otherwise it'll set the [value]
// with the extracted value.
//
// NOTE: The arguments are 1 based (to get the first argument use 1 not 0).
//       Only use arg index 0 to get the value of attribute setter call.

ATML_PUBLIC bool ATMLGetArgBool(ATMLVM* vm, int arg, bool* value);
ATML_PUBLIC bool ATMLGetArgNumber(ATMLVM* vm, int arg, double* value);
ATML_PUBLIC bool ATMLGetArgString(ATMLVM* vm, int arg,
                              const char** value, uint32_t* length);
ATML_PUBLIC bool ATMLGetArgInst(ATMLVM* vm, int arg, uint32_t id, void** value);
ATML_PUBLIC bool ATMLGetArgValue(ATMLVM* vm, int arg, ATMLVarType type, ATMLVar* value);

// The functions follow are used to set the return value of the current native
// function's. Don't use it outside a registered native function.

ATML_PUBLIC void ATMLReturnNull(ATMLVM* vm);
ATML_PUBLIC void ATMLReturnBool(ATMLVM* vm, bool value);
ATML_PUBLIC void ATMLReturnNumber(ATMLVM* vm, double value);
ATML_PUBLIC void ATMLReturnString(ATMLVM* vm, const char* value);
ATML_PUBLIC void ATMLReturnStringLength(ATMLVM* vm, const char* value, size_t len);
ATML_PUBLIC void ATMLReturnValue(ATMLVM* vm, ATMLVar value);
ATML_PUBLIC void ATMLReturnHandle(ATMLVM* vm, ATMLHandle* handle);

ATML_PUBLIC void ATMLReturnInstNative(ATMLVM* vm, void* data, uint32_t id);

// Returns the cstring pointer of the given string. Make sure if the [value] is
// a string before calling this function, otherwise it'll fail an assertion.
ATML_PUBLIC const char* ATMLStringGetData(const ATMLVar value);

// Returns the return value or if it's yielded, the yielded value of the fiber
// as ATMLVar, this value lives on stack and will die (popped) once the fiber
// resumed use handle to keep it alive.
ATML_PUBLIC ATMLVar ATMLFiberGetReturnValue(const ATMLHandle* fiber);

// Returns true if the fiber is finished it's execution and cannot be resumed
// anymore.
ATML_PUBLIC bool ATMLFiberIsDone(const ATMLHandle* fiber);

/*****************************************************************************/
/* POCKETLANG TYPE FUNCTIONS                                                 */
/*****************************************************************************/

// The functions below will allocate a new object and return's it's value
// wrapped around a handler.

ATML_PUBLIC ATMLHandle* ATMLNewString(ATMLVM* vm, const char* value);
ATML_PUBLIC ATMLHandle* ATMLNewStringLength(ATMLVM* vm, const char* value, size_t len);
ATML_PUBLIC ATMLHandle* ATMLNewList(ATMLVM* vm);
ATML_PUBLIC ATMLHandle* ATMLNewMap(ATMLVM* vm);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
ATML_PUBLIC ATMLHandle* ATMLNewModule(ATMLVM* vm, const char* name);

// Create and return a new fiber around the function [fn].
ATML_PUBLIC ATMLHandle* ATMLNewFiber(ATMLVM* vm, ATMLHandle* fn);

// Create and return a native instance around the [data]. The [id] is the
// unique id of the instance, this would be used to check if two instances are
// equal and used to get the name of the instance using NativeTypeNameFn
// callback.
ATML_PUBLIC ATMLHandle* ATMLNewInstNative(ATMLVM* vm, void* data, uint32_t id);

// TODO: Create a primitive (non garbage collected) variable buffer (or a
// fixed size array) to store them and make the handle points to the variable
// in that buffer, this will prevent us from invoking an allocation call for
// each time we want to pass a primitive type.

//ATML_PUBLIC ATMLVar ATMLPushNull(ATMLVM* vm);
//ATML_PUBLIC ATMLVar ATMLPushBool(ATMLVM* vm, bool value);
//ATML_PUBLIC ATMLVar ATMLPushNumber(ATMLVM* vm, double value);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // POCKETLANG_H
