/*
 *  Copyright (c) 2020-2021 AtomLanguage Developers
 *  Distributed Under The MIT License
 */

#ifndef VAR_H
#define VAR_H

#include "atml_buffers.h"
#include "atml_internal.h"

#if defined(_MSC_VER) || __STDC_VERSION__ >= 199901L // std >= c99
  #define DYNAMIC_TAIL_ARRAY
#else
  #define DYNAMIC_TAIL_ARRAY 0
#endif

// Number of maximum import statements in a script.
#define MAX_IMPORT_SCRIPTS 16

// There are 2 main implemenation of Var's internal representation. First one
// is NaN-tagging, and the second one is union-tagging. (read below for more).
#if VAR_NAN_TAGGING
  typedef uint64_t Var;
#else
  typedef struct Var Var;
#endif


#if VAR_NAN_TAGGING

// Masks and payloads.
#define _MASK_SIGN  ((uint64_t)0x8000000000000000)
#define _MASK_QNAN  ((uint64_t)0x7ffc000000000000)
#define _MASK_TYPE  ((uint64_t)0x0003000000000000)
#define _MASK_CONST ((uint64_t)0x0004000000000000)

#define _MASK_INTEGER (_MASK_QNAN | (uint64_t)0x0002000000000000)
#define _MASK_OBJECT  (_MASK_QNAN | (uint64_t)0x8000000000000000)

#define _PAYLOAD_INTEGER ((uint64_t)0x00000000ffffffff)
#define _PAYLOAD_OBJECT  ((uint64_t)0x0000ffffffffffff)

// Primitive types.
#define VAR_NULL      (_MASK_QNAN | (uint64_t)0x0000000000000000)
#define VAR_UNDEFINED (_MASK_QNAN | (uint64_t)0x0001000000000000)
#define VAR_VOID      (_MASK_QNAN | (uint64_t)0x0001000000000001)
#define VAR_FALSE     (_MASK_QNAN | (uint64_t)0x0001000000000002)
#define VAR_TRUE      (_MASK_QNAN | (uint64_t)0x0001000000000003)

// Encode types.
#define VAR_BOOL(value) ((value)? VAR_TRUE : VAR_FALSE)
#define VAR_INT(value)  (_MASK_INTEGER | (uint32_t)(int32_t)(value))
#define VAR_NUM(value)  (doubleToVar(value))
#define VAR_OBJ(value) /* [value] is an instance of Object */ \
  ((Var)(_MASK_OBJECT | (uint64_t)(uintptr_t)(&value->_super)))

// Const casting.
#define ADD_CONST(value)    ((value) | _MASK_CONST)
#define REMOVE_CONST(value) ((value) & ~_MASK_CONST)

// Check types.
#define IS_CONST(value) ((value & _MASK_CONST) == _MASK_CONST)
#define IS_NULL(value)  ((value) == VAR_NULL)
#define IS_UNDEF(value) ((value) == VAR_UNDEFINED)
#define IS_FALSE(value) ((value) == VAR_FALSE)
#define IS_TRUE(value)  ((value) == VAR_TRUE)
#define IS_BOOL(value)  (IS_TRUE(value) || IS_FALSE(value))
#define IS_INT(value)   ((value & _MASK_INTEGER) == _MASK_INTEGER)
#define IS_NUM(value)   ((value & _MASK_QNAN) != _MASK_QNAN)
#define IS_OBJ(value)   ((value & _MASK_OBJECT) == _MASK_OBJECT)

// Evaluate to true if the var is an object and type of [obj_type].
#define IS_OBJ_TYPE(var, obj_type) IS_OBJ(var) && AS_OBJ(var)->type == obj_type

#define IS_STR_EQ(s1, s2)          \
 (((s1)->hash == (s2)->hash) &&    \
 ((s1)->length == (s2)->length) && \
 (memcmp((const void*)(s1)->data, (const void*)(s2)->data, (s1)->length) == 0))

#define IS_CSTR_EQ(str, cstr, len, chash)  \
 (((str)->hash == chash) &&                \
 ((str)->length == len) &&                 \
 (memcmp((const void*)(str)->data, (const void*)(cstr), len) == 0))

// Decode types.
#define AS_BOOL(value) ((value) == VAR_TRUE)
#define AS_INT(value)  ((int32_t)((value) & _PAYLOAD_INTEGER))
#define AS_NUM(value)  (varToDouble(value))
#define AS_OBJ(value)  ((Object*)(value & _PAYLOAD_OBJECT))

#define AS_STRING(value)  ((String*)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->data)
#define AS_ARRAY(value)   ((List*)AS_OBJ(value))
#define AS_MAP(value)     ((Map*)AS_OBJ(value))
#define AS_RANGE(value)   ((Range*)AS_OBJ(value))

#else

// TODO: Union tagging implementation of all the above macros ignore macros
//       starts with an underscore.

typedef enum {
  VAR_UNDEFINED, //< Internal type for exceptions.
  VAR_NULL,      //< Null pointer type.
  VAR_BOOL,      //< Yin and yang of software.
  VAR_INT,       //< Only 32bit integers (for consistence with Nan-Tagging).
  VAR_FLOAT,     //< Floats are stored as (64bit) double.

  VAR_OBJECT,    //< Base type for all \ref var_Object types.
} VarType;

struct Var {
  VarType type;
  union {
    bool _bool;
    int _int;
    double _float;
    Object* _obj;
  };
};

#endif // VAR_NAN_TAGGING

typedef struct Object Object;
typedef struct String String;
typedef struct List List;
typedef struct Map Map;
typedef struct Range Range;
typedef struct Script Script;
typedef struct Function Function;
typedef struct Fiber Fiber;
typedef struct Class Class;
typedef struct Instance Instance;

// Declaration of buffer objects of different types.
DECLARE_BUFFER(Uint, uint32_t)
DECLARE_BUFFER(Byte, uint8_t)
DECLARE_BUFFER(Var, Var)
DECLARE_BUFFER(String, String*)
DECLARE_BUFFER(Function, Function*)
DECLARE_BUFFER(Class, Class*)

// Add all the characters to the buffer, byte buffer can also be used as a
// buffer to write string (like a string stream). Note that this will not
// add a null byte '\0' at the end.
void ATMLByteBufferAddString(ATMLByteBuffer* self, ATMLVM* vm, const char* str,
                           uint32_t length);

typedef enum {
  OBJ_STRING,
  OBJ_LIST,
  OBJ_MAP,
  OBJ_RANGE,
  OBJ_SCRIPT,
  OBJ_FUNC,
  OBJ_FIBER,
  OBJ_CLASS,
  OBJ_INST,
} ObjectType;

// Base struct for all heap allocated objects.
struct Object {
  ObjectType type;  //< Type of the object in \ref var_Object_Type.
  bool is_marked;   //< Marked when garbage collection's marking phase.
  Object* next;     //< Next object in the heap allocated link list.
};

struct String {
  Object _super;

  uint32_t hash;      //< 32 bit hash value of the string.
  uint32_t length;    //< Length of the string in \ref data.
  uint32_t capacity;  //< Size of allocated \ref data.
  char data[DYNAMIC_TAIL_ARRAY];
};

struct List {
  Object _super;

  ATMLVarBuffer elements; //< Elements of the array.
};

typedef struct {
  Var key;   
  Var value; 
} MapEntry;

struct Map {
  Object _super;

  uint32_t capacity; //< Allocated entry's count.
  uint32_t count;    //< Number of entries in the map.
  MapEntry* entries; //< Pointer to the contiguous array.
};

struct Range {
  Object _super;

  double from; 
  double to;   
};

struct Script {
  Object _super;

  // For core libraries the module and the path are same and points to the
  // same String objects.
  String* module; //< Module name of the script.
  String* path;   //< Path of the script.

  ATMLVarBuffer globals;         //< Script level global variables.
  ATMLUintBuffer global_names;   //< Name map to index in globals.

  ATMLFunctionBuffer functions;  //< Functions of the script.
  ATMLClassBuffer classes;       //< Classes of the script.

  ATMLStringBuffer names;        //< Name literals, attribute names, etc.
  ATMLVarBuffer literals;        //< Script literal constant values.

  Function* body;              //< Script body is an anonymous function.

  bool initialized;
};

// Script function pointer.
typedef struct {
  ATMLByteBuffer opcodes;  //< Buffer of opcodes.
  ATMLUintBuffer oplines;  //< Line number of opcodes for debug (1 based).
  int stack_size;        //< Maximum size of stack required.
} Fn;

struct Function {
  Object _super;

  const char* name; //< Name in the script [owner] or C literal.
  Script* owner;    //< Owner script of the function.
  int arity;        //< Number of argument the function expects.

  // Docstring of the function, currently it's just the C string literal
  // pointer, refactor this into String* so that we can support public
  // native functions to provide a docstring.
  const char* docstring;

  bool is_native;        //< True if Native function.
  union {
    ATMLNativeFn native;   //< Native function pointer.
    Fn* fn;              //< Script function pointer.
  };
};

typedef struct {
  const uint8_t* ip;  //< Pointer to the next instruction byte code.
  const Function* fn; //< Function of the frame.
  Var* rbp;           //< Stack base pointer. (%rbp)
} CallFrame;

typedef enum {
  FIBER_NEW,     //< Fiber haven't started yet.
  FIBER_RUNNING, //< Fiber is currently running.
  FIBER_YIELDED, //< Yielded fiber, can be resumed.
  FIBER_DONE,    //< Fiber finished and cannot be resumed.
} FiberState;

struct Fiber {
  Object _super;

  FiberState state;

  // The root function of the fiber. (For script it'll be the script's implicit
  // body function).
  Function* func;

  // The stack of the execution holding locals and temps. A heap will be
  // allocated and grow as needed.
  Var* stack;
  int stack_size; //< Capacity of the allocated stack.

  // The stack pointer (%rsp) pointing to the stack top.
  Var* sp;

  // The stack base pointer of the current frame. It'll be updated before
  // calling a native function. (`fiber->ret` === `curr_call_frame->rbp`). And
  // also updated if the stack is reallocated (that's when it's about to get
  // overflowed.
  Var* ret;

  // Heap allocated array of call frames will grow as needed.
  CallFrame* frames;
  int frame_capacity; //< Capacity of the frames array.
  int frame_count; //< Number of frame entry in frames.

  // Caller of this fiber if it has one, NULL otherwise.
  Fiber* caller;

  // Runtime error initially NULL, heap allocated.
  String* error;
};

struct Class {
  Object _super;

  Script* owner; //< The script it belongs to.
  uint32_t name; //< Index of the type's name in the script's name buffer.

  Function* ctor; //< The constructor function.
  ATMLUintBuffer field_names; //< Buffer of field names.
  // TODO: ordered names buffer for binary search.
};

typedef struct {
  Class* type;        //< Class this instance belongs to.
  ATMLVarBuffer fields; //< Var buffer of the instance.
} Inst;

struct Instance {
  Object _super;

  const char* name;  //< Name of the type it belongs to.

  bool is_native;    //< True if it's a native type instance.
  uint32_t native_id;   //< Unique ID of this native instance.

  union {
    void* native;  //< C struct pointer. // TODO:
    Inst* ins;     //< Module instance pointer.
  };
};

/*****************************************************************************/
/* "CONSTRUCTORS"                                                            */
/*****************************************************************************/

// Initialize the object with it's default value.
void varInitObject(Object* self, ATMLVM* vm, ObjectType type);

// Allocate new String object with from [text] with a given [length] and return
// String*.
String* newStringLength(ATMLVM* vm, const char* text, uint32_t length);

#if 0 
  static inline String* newString(ATMLVM* vm, const char* text) {
    uint32_t length = (text == NULL) ? 0 : (uint32_t)strlen(text);
    return newStringLength(vm, text, length);
  }
#else 
  #define newString(vm, text) \
    newStringLength(vm, text, (!text) ? 0 : (uint32_t)strlen(text))
#endif

// Allocate new List and return List*.
List* newList(ATMLVM* vm, uint32_t size);

// Allocate new Map and return Map*.
Map* newMap(ATMLVM* vm);

Range* newRange(ATMLVM* vm, double from, double to);

Script* newScript(ATMLVM* vm, String* name, bool is_core);

Function* newFunction(ATMLVM* vm, const char* name, int length, Script* owner,
                      bool is_native, const char* docstring);

Fiber* newFiber(ATMLVM* vm, Function* fn);

Class* newClass(ATMLVM* vm, Script* scr, const char* name, uint32_t length);

Instance* newInstance(ATMLVM* vm, Class* ty, bool initialize);

Instance* newInstanceNative(ATMLVM* vm, void* data, uint32_t id);

void markObject(ATMLVM* vm, Object* self);

// Mark the reachable values at the mark-and-sweep phase of the garbage
// collection.
void markValue(ATMLVM* vm, Var self);

void markVarBuffer(ATMLVM* vm, ATMLVarBuffer* self);

void markStringBuffer(ATMLVM* vm, ATMLStringBuffer* self);

void markFunctionBuffer(ATMLVM* vm, ATMLFunctionBuffer* self);

void popMarkedObjects(ATMLVM* vm);

List* rangeAsList(ATMLVM* vm, Range* self);

String* stringLower(ATMLVM* vm, String* self);

String* stringUpper(ATMLVM* vm, String* self);

String* stringStrip(ATMLVM* vm, String* self);

String* stringFormat(ATMLVM* vm, const char* fmt, ...);

// Create a new string by joining the 2 given string and return the result.
// Which would be faster than using "@@" format.
String* stringJoin(ATMLVM* vm, String* str1, String* str2);

#if 0 
  static inline void listAppend(ATMLVM* vm, List* self, Var value) {
    ATMLVarBufferWrite(&self->elements, vm, value);
  }
#else // Macro implementation.
  #define listAppend(vm, self, value) \
    ATMLVarBufferWrite(&self->elements, vm, value)
#endif

// Insert [value] to the list at [index] and shift down the rest of the
// elements.
void listInsert(ATMLVM* vm, List* self, uint32_t index, Var value);

// Remove and return element at [index].
Var listRemoveAt(ATMLVM* vm, List* self, uint32_t index);

// Create a new list by joining the 2 given list and return the result.
List* listJoin(ATMLVM* vm, List* l1, List* l2);

// Returns the value for the [key] in the map. If key not exists return
// VAR_UNDEFINED.
Var mapGet(Map* self, Var key);

// Add the [key], [value] entry to the map.
void mapSet(ATMLVM* vm, Map* self, Var key, Var value);

// Remove all the entries from the map.
void mapClear(ATMLVM* vm, Map* self);

// Remove the [key] from the map. If the key exists return it's value
// otherwise return VAR_NULL.
Var mapRemoveKey(ATMLVM* vm, Map* self, Var key);

// Returns true if the fiber has error, and if it has any the fiber cannot be
// resumed anymore.
bool fiberHasError(Fiber* fiber);

// Add the name (string literal) to the string buffer if not already exists and
// return it's index in the buffer.
uint32_t scriptAddName(Script* self, ATMLVM* vm, const char* name,
                       uint32_t length);

// Search for the type name in the script and return it's index in it's
// [classes] buffer. If not found returns -1.
int scriptGetClass(Script* script, const char* name, uint32_t length);

// Search for the function name in the script and return it's index in it's
// [functions] buffer. If not found returns -1.
int scriptGetFunc(Script* script, const char* name, uint32_t length);

// Search for the global variable name in the script and return it's index in
// it's [globals] buffer. If not found returns -1.
int scriptGetGlobals(Script* script, const char* name, uint32_t length);

// Add a global [value] to the [scrpt] and return its index.
uint32_t scriptAddGlobal(ATMLVM* vm, Script* script,
                         const char* name, uint32_t length,
                         Var value);

// This will allocate a new implicit main function for the script and assign to
// the script's body attribute. And the attribute initialized will be set to
// false for the new function. Note that the body of the script should be NULL
// before calling this function.
void scriptAddMain(ATMLVM* vm, Script* script);

// Get the attribut from the instance and set it [value]. On success return
// true, if the attribute not exists it'll return false but won't set an error.
bool instGetAttrib(ATMLVM* vm, Instance* inst, String* attrib, Var* value);

// Set the attribute to the instance and return true on success, if the
// attribute doesn't exists it'll return false but if the [value] type is
// incompatible, this will set an error to the VM, which you can check with
// VM_HAS_ERROR() macro function.
bool instSetAttrib(ATMLVM* vm, Instance* inst, String* attrib, Var value);

// Release all the object owned by the [self] including itself.
void freeObject(ATMLVM* vm, Object* self);

/*****************************************************************************/
/* UTILITY FUNCTIONS                                                         */
/*****************************************************************************/

// Internal method behind VAR_NUM(value) don't use it directly.
Var doubleToVar(double value);

// Internal method behind AS_NUM(value) don't use it directly.
double varToDouble(Var value);

// Returns the type name of the ATMLVarType enum value.
const char* getATMLVarTypeName(ATMLVarType type);

// Returns the type name of the ObjectType enum value.
const char* getObjectTypeName(ObjectType type);

// Returns the type name of the var [v].
const char* varTypeName(Var v);

// Returns true if both variables are the same (ie v1 is v2).
bool isValuesSame(Var v1, Var v2);

// Returns true if both variables are equal (ie v1 == v2).
bool isValuesEqual(Var v1, Var v2);

// Return the hash value of the variable. (variable should be hashable).
uint32_t varHashValue(Var v);

// Return true if the object type is hashable.
bool isObjectHashable(ObjectType type);

// Returns the string version of the [value].
String* toString(ATMLVM* vm, const Var value);

// Returns the representation version of the [value], similar to python's
// __repr__() method.
String * toRepr(ATMLVM * vm, const Var value);

// Returns the truthy value of the var.
bool toBool(Var v);

#endif // VAR_H