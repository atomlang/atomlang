#ifndef __ATOMLANG_VALUES__
#define __ATOMLANG_VALUES__

#include "atomlang_memory.h"
#include "atomlang_utils.h"
#include "atomlang_array.h"
#include "atomlang_json.h"
#include "debug_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATOMLANG_VERSION						"0.8.5"     
#define ATOMLANG_VERSION_NUMBER				0x000805    
#define ATOMLANG_BUILD_DATE                  __DATE__

#ifndef ATOMLANG_ENABLE_DOUBLE
#define ATOMLANG_ENABLE_DOUBLE               1           
#endif

#ifndef ATOMLANG_ENABLE_INT64
#define ATOMLANG_ENABLE_INT64                1           
#endif

#ifndef ATOMLANG_COMPUTED_GOTO
#define ATOMLANG_COMPUTED_GOTO               1           
#endif

#ifndef ATOMLANG_NULL_SILENT
#define ATOMLANG_NULL_SILENT                 1           
#endif

#ifndef ATOMLANG_MAP_DOTSUGAR
#define ATOMLANG_MAP_DOTSUGAR                1           
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#undef ATOMLANG_COMPUTED_GOTO
#define ATOMLANG_COMPUTED_GOTO               0           
#endif

#define MAIN_FUNCTION                       "main"
#define ITERATOR_INIT_FUNCTION              "iterate"
#define ITERATOR_NEXT_FUNCTION              "next"
#define INITMODULE_NAME                     "$moduleinit"
#define CLASS_INTERNAL_INIT_NAME            "$init"
#define CLASS_CONSTRUCTOR_NAME              "init"
#define CLASS_DESTRUCTOR_NAME               "deinit"
#define SELF_PARAMETER_NAME                 "self"
#define OUTER_IVAR_NAME                     "outer"
#define GETTER_FUNCTION_NAME                "get"
#define SETTER_FUNCTION_NAME                "set"
#define SETTER_PARAMETER_NAME               "value"

#define GLOBALS_DEFAULT_SLOT                4096
#define CPOOL_INDEX_MAX                     4096        
#define CPOOL_VALUE_SUPER                   CPOOL_INDEX_MAX+1
#define CPOOL_VALUE_NULL                    CPOOL_INDEX_MAX+2
#define CPOOL_VALUE_UNDEFINED               CPOOL_INDEX_MAX+3
#define CPOOL_VALUE_ARGUMENTS               CPOOL_INDEX_MAX+4
#define CPOOL_VALUE_TRUE                    CPOOL_INDEX_MAX+5
#define CPOOL_VALUE_FALSE                   CPOOL_INDEX_MAX+6
#define CPOOL_VALUE_FUNC                    CPOOL_INDEX_MAX+7

#define MAX_INSTRUCTION_OPCODE              64              
#define MAX_REGISTERS                       256             
#define MAX_LOCALS                          200             
#define MAX_UPVALUES                        200             
#define MAX_INLINE_INT                      131072          
#define MAX_FIELDSxFLUSH                    64              
#define MAX_IVARS                           768             
#define MAX_ALLOCATION                      4194304         
#define MAX_CCALLS                          100             
#define MAX_MEMORY_BLOCK                    157286400      

#define DEFAULT_CONTEXT_SIZE                256            
#define DEFAULT_MINSTRING_SIZE              32             
#define DEFAULT_MINSTACK_SIZE               256            
#define DEFAULT_MINCFRAME_SIZE              32             
#define DEFAULT_CG_THRESHOLD                5*1024*1024    
#define DEFAULT_CG_MINTHRESHOLD             1024*1024      
#define DEFAULT_CG_RATIO                    0.5            

#define MAXNUM(a,b)                         ((a) > (b) ? a : b)
#define MINNUM(a,b)                         ((a) < (b) ? a : b)
#define EPSILON                             0.000001
#define MIN_LIST_RESIZE                     12      

#define ATOMLANG_DATA_REGISTER               UINT32_MAX
#define ATOMLANG_FIBER_REGISTER              UINT32_MAX-1
#define ATOMLANG_MSG_REGISTER                UINT32_MAX-2

#define ATOMLANG_BRIDGE_INDEX                UINT16_MAX
#define ATOMLANG_COMPUTED_INDEX              UINT16_MAX-1

#if !defined(ATOMLANG_API) && defined(_WIN32) && defined(BUILD_ATOMLANG_API)
  #define ATOMLANG_API __declspec(dllexport)
#else
  #define ATOMLANG_API
#endif


#if ATOMLANG_ENABLE_DOUBLE
typedef double                              atomlang_float_t;
#define ATOMLANG_FLOAT_MAX                   DBL_MAX
#define ATOMLANG_FLOAT_MIN                   DBL_MIN
#define FLOAT_MAX_DECIMALS                  16
#define FLOAT_EPSILON                       0.00001
#else
typedef float                               atomlang_float_t;
#define ATOMLANG_FLOAT_MAX                   FLT_MAX
#define ATOMLANG_FLOAT_MIN                   FLT_MIN
#define FLOAT_MAX_DECIMALS                  7
#define FLOAT_EPSILON                       0.00001
#endif

#if ATOMLANG_ENABLE_INT64
typedef int64_t                             atomlang_int_t;
#define ATOMLANG_INT_MAX                     9223372036854775807
#define ATOMLANG_INT_MIN                     (-ATOMLANG_INT_MAX-1LL)
#else
typedef int32_t                             atomlang_int_t;
#define ATOMLANG_INT_MAX                     2147483647
#define ATOMLANG_INT_MIN                     -2147483648
#endif

typedef struct atomlang_class_s              atomlang_class_t;
typedef struct atomlang_class_s              atomlang_object_t;

typedef struct {
    atomlang_class_t         *isa;           
    union {                                 
        atomlang_int_t       n;             
        atomlang_float_t     f;             
        atomlang_object_t    *p;            
    };
} atomlang_value_t;

extern atomlang_class_t *atomlang_class_object;
extern atomlang_class_t *atomlang_class_bool;
extern atomlang_class_t *atomlang_class_null;
extern atomlang_class_t *atomlang_class_int;
extern atomlang_class_t *atomlang_class_float;
extern atomlang_class_t *atomlang_class_function;
extern atomlang_class_t *atomlang_class_closure;
extern atomlang_class_t *atomlang_class_fiber;
extern atomlang_class_t *atomlang_class_class;
extern atomlang_class_t *atomlang_class_string;
extern atomlang_class_t *atomlang_class_instance;
extern atomlang_class_t *atomlang_class_list;
extern atomlang_class_t *atomlang_class_map;
extern atomlang_class_t *atomlang_class_module;
extern atomlang_class_t *atomlang_class_range;
extern atomlang_class_t *atomlang_class_upvalue;

typedef marray_t(atomlang_value_t)        atomlang_value_r;   
#ifndef ATOMLANG_HASH_DEFINED
#define ATOMLANG_HASH_DEFINED
typedef struct atomlang_hash_t            atomlang_hash_t;    
#endif

#ifndef ATOMLANG_VM_DEFINED
#define ATOMLANG_VM_DEFINED
typedef struct atomlang_vm                atomlang_vm;        
#endif

typedef bool (*atomlang_c_internal)(atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex);
typedef uint32_t (*atomlang_gc_callback)(atomlang_vm *vm, atomlang_object_t *obj);

typedef enum {
    EXEC_TYPE_SPECIAL_GETTER = 0,       
    EXEC_TYPE_SPECIAL_SETTER = 1,       
} atomlang_special_index;

typedef enum {
    EXEC_TYPE_NATIVE,        
    EXEC_TYPE_INTERNAL,         
    EXEC_TYPE_BRIDGED,          
    EXEC_TYPE_SPECIAL           
} atomlang_exec_type;

typedef struct atomlang_gc_s {
    bool                    isdark;         
    bool                    visited;        
    atomlang_gc_callback     free;           
    atomlang_gc_callback     size;           
    atomlang_gc_callback     blacken;        
    atomlang_object_t        *next;         
} atomlang_gc_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    void                    *xdata;         
    const char              *identifier;   
    uint16_t                nparams;        
    uint16_t                nlocals;        
    uint16_t                ntemps;         
    uint16_t                nupvalues;      
    atomlang_exec_type       tag;            
    union {
        struct {
            atomlang_value_r cpool;          
            atomlang_value_r pvalue;         
            atomlang_value_r pname;          
            uint32_t        ninsts;         
            uint32_t        *bytecode;      
            uint32_t        *lineno;        
            float           purity;         
            bool            useargs;        
        };

        atomlang_c_internal  internal;       

        struct {
            uint16_t        index;         
            void            *special[2];   
        };
    };
} atomlang_function_t;

typedef struct upvalue_s {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             
    
    atomlang_value_t         *value;         
    atomlang_value_t         closed;         
    struct upvalue_s        *next;          
} atomlang_upvalue_t;

typedef struct atomlang_closure_s {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    atomlang_vm              *vm;            
    atomlang_function_t      *f;             
    atomlang_object_t        *context;       
    atomlang_upvalue_t       **upvalue;      
    uint32_t                refcount;       
} atomlang_closure_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    atomlang_value_r         array;          
} atomlang_list_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;            

    atomlang_hash_t          *hash;          
} atomlang_map_t;

typedef struct {
    uint32_t                *ip;            
    uint32_t                dest;           
    uint16_t                nargs;         
    atomlang_list_t          *args;        
    atomlang_closure_t       *closure;     
    atomlang_value_t         *stackstart;  
    bool                    outloop;       
} atomlang_callframe_t;

typedef enum {
    FIBER_NEVER_EXECUTED = 0,
    FIBER_ABORTED_WITH_ERROR = 1,
    FIBER_TERMINATED = 2,
    FIBER_RUNNING = 3,
    FIBER_TRYING = 4
} atomlang_fiber_status;
    
typedef struct fiber_s {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;            

    atomlang_value_t         *stack;         
    atomlang_value_t         *stacktop;      
    uint32_t                stackalloc;     

    atomlang_callframe_t     *frames;        
    uint32_t                nframes;        
    uint32_t                framesalloc;    

    atomlang_upvalue_t       *upvalues;      

    char                    *error;         
    bool                    trying;         
    struct fiber_s          *caller;        
    atomlang_value_t         result;         
    
    atomlang_fiber_status    status;         
    nanotime_t              lasttime;       
    atomlang_float_t         timewait;       
    atomlang_float_t         elapsedtime;   
} atomlang_fiber_t;

typedef struct atomlang_class_s {
    atomlang_class_t         *isa;          
    atomlang_gc_t            gc;            

    atomlang_class_t         *objclass;     
    const char              *identifier;    
    bool                    has_outer;      
    bool                    is_struct;      
    bool                    is_inited;      
    bool                    unused;         
    void                    *xdata;         
    struct atomlang_class_s  *superclass;   
    const char              *superlook;     
    atomlang_hash_t          *htable;       
    uint32_t                nivars;         
	//atomlang_value_r			inames;		
    atomlang_value_t         *ivars;        
} atomlang_class_s;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    const char              *identifier;    
    atomlang_hash_t          *htable;        
} atomlang_module_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    atomlang_class_t         *objclass;      
    void                    *xdata;         
    atomlang_value_t         *ivars;         
} atomlang_instance_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    char                    *s;             
    uint32_t                hash;           
    uint32_t                len;            
    uint32_t                alloc;          
} atomlang_string_t;

typedef struct {
    atomlang_class_t         *isa;           
    atomlang_gc_t            gc;             

    atomlang_int_t           from;           
    atomlang_int_t           to;             
} atomlang_range_t;

typedef void (*code_dump_function) (void *code);
typedef marray_t(atomlang_function_t*)   atomlang_function_r;     
typedef marray_t(atomlang_class_t*)      atomlang_class_r;        
typedef marray_t(atomlang_object_t*)     atomlang_object_r;       

ATOMLANG_API atomlang_module_t   *atomlang_module_new (atomlang_vm *vm, const char *identifier);
ATOMLANG_API void                atomlang_module_free (atomlang_vm *vm, atomlang_module_t *m);
ATOMLANG_API void                atomlang_module_blacken (atomlang_vm *vm, atomlang_module_t *m);
ATOMLANG_API uint32_t            atomlang_module_size (atomlang_vm *vm, atomlang_module_t *m);

ATOMLANG_API uint32_t           *atomlang_bytecode_deserialize (const char *buffer, size_t len, uint32_t *ninst);
ATOMLANG_API void                atomlang_function_blacken (atomlang_vm *vm, atomlang_function_t *f);
ATOMLANG_API uint16_t            atomlang_function_cpool_add (atomlang_vm *vm, atomlang_function_t *f, atomlang_value_t v);
ATOMLANG_API atomlang_value_t     atomlang_function_cpool_get (atomlang_function_t *f, uint16_t i);
ATOMLANG_API atomlang_function_t *atomlang_function_deserialize (atomlang_vm *vm, json_value *json);
ATOMLANG_API void                atomlang_function_dump (atomlang_function_t *f, code_dump_function codef);
ATOMLANG_API void                atomlang_function_free (atomlang_vm *vm, atomlang_function_t *f);
ATOMLANG_API atomlang_function_t *atomlang_function_new (atomlang_vm *vm, const char *identifier, uint16_t nparams, uint16_t nlocals, uint16_t ntemps, void *code);
ATOMLANG_API atomlang_function_t *atomlang_function_new_bridged (atomlang_vm *vm, const char *identifier, void *xdata);
ATOMLANG_API atomlang_function_t *atomlang_function_new_internal (atomlang_vm *vm, const char *identifier, atomlang_c_internal exec, uint16_t nparams);
ATOMLANG_API atomlang_function_t *atomlang_function_new_special (atomlang_vm *vm, const char *identifier, uint16_t index, void *getter, void *setter);
ATOMLANG_API atomlang_list_t     *atomlang_function_params_get (atomlang_vm *vm, atomlang_function_t *f);
ATOMLANG_API void                atomlang_function_serialize (atomlang_function_t *f, json_t *json);
ATOMLANG_API void                atomlang_function_setouter (atomlang_function_t *f, atomlang_object_t *outer);
ATOMLANG_API void                atomlang_function_setxdata (atomlang_function_t *f, void *xdata);
ATOMLANG_API uint32_t            atomlang_function_size (atomlang_vm *vm, atomlang_function_t *f);

ATOMLANG_API void                atomlang_closure_blacken (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API void                atomlang_closure_dec_refcount (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API void                atomlang_closure_inc_refcount (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API void                atomlang_closure_free (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API uint32_t            atomlang_closure_size (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API atomlang_closure_t  *atomlang_closure_new (atomlang_vm *vm, atomlang_function_t *f);

ATOMLANG_API void                atomlang_upvalue_blacken (atomlang_vm *vm, atomlang_upvalue_t *upvalue);
ATOMLANG_API void                atomlang_upvalue_free(atomlang_vm *vm, atomlang_upvalue_t *upvalue);
ATOMLANG_API atomlang_upvalue_t  *atomlang_upvalue_new (atomlang_vm *vm, atomlang_value_t *value);
ATOMLANG_API uint32_t            atomlang_upvalue_size (atomlang_vm *vm, atomlang_upvalue_t *upvalue);

ATOMLANG_API void                atomlang_class_blacken (atomlang_vm *vm, atomlang_class_t *c);
ATOMLANG_API int16_t             atomlang_class_add_ivar (atomlang_class_t *c, const char *identifier);
ATOMLANG_API void                atomlang_class_bind (atomlang_class_t *c, const char *key, atomlang_value_t value);
ATOMLANG_API uint32_t            atomlang_class_count_ivars (atomlang_class_t *c);
ATOMLANG_API atomlang_class_t    *atomlang_class_deserialize (atomlang_vm *vm, json_value *json);
ATOMLANG_API void                atomlang_class_dump (atomlang_class_t *c);
ATOMLANG_API void                atomlang_class_free (atomlang_vm *vm, atomlang_class_t *c);
ATOMLANG_API void                atomlang_class_free_core (atomlang_vm *vm, atomlang_class_t *c);
ATOMLANG_API atomlang_class_t    *atomlang_class_get_meta (atomlang_class_t *c);
ATOMLANG_API atomlang_class_t    *atomlang_class_getsuper (atomlang_class_t *c);
ATOMLANG_API bool                atomlang_class_grow (atomlang_class_t *c, uint32_t n);
ATOMLANG_API bool                atomlang_class_is_anon (atomlang_class_t *c);
ATOMLANG_API bool                atomlang_class_is_meta (atomlang_class_t *c);
ATOMLANG_API atomlang_object_t   *atomlang_class_lookup (atomlang_class_t *c, atomlang_value_t key);
ATOMLANG_API atomlang_closure_t  *atomlang_class_lookup_closure (atomlang_class_t *c, atomlang_value_t key);
ATOMLANG_API atomlang_closure_t  *atomlang_class_lookup_constructor (atomlang_class_t *c, uint32_t nparams);
ATOMLANG_API atomlang_class_t    *atomlang_class_lookup_class_identifier (atomlang_class_t *c, const char *identifier);
ATOMLANG_API atomlang_class_t    *atomlang_class_new_pair (atomlang_vm *vm, const char *identifier, atomlang_class_t *superclass, uint32_t nivar, uint32_t nsvar);
ATOMLANG_API atomlang_class_t    *atomlang_class_new_single (atomlang_vm *vm, const char *identifier, uint32_t nfields);
ATOMLANG_API void                atomlang_class_serialize (atomlang_class_t *c, json_t *json);
ATOMLANG_API bool                atomlang_class_setsuper (atomlang_class_t *subclass, atomlang_class_t *superclass);
ATOMLANG_API bool                atomlang_class_setsuper_extern (atomlang_class_t *baseclass, const char *identifier);
ATOMLANG_API void                atomlang_class_setxdata (atomlang_class_t *c, void *xdata);
ATOMLANG_API uint32_t            atomlang_class_size (atomlang_vm *vm, atomlang_class_t *c);

ATOMLANG_API void                atomlang_fiber_blacken (atomlang_vm *vm, atomlang_fiber_t *fiber);
ATOMLANG_API void                atomlang_fiber_free (atomlang_vm *vm, atomlang_fiber_t *fiber);
ATOMLANG_API atomlang_fiber_t    *atomlang_fiber_new (atomlang_vm *vm, atomlang_closure_t *closure, uint32_t nstack, uint32_t nframes);
ATOMLANG_API void                atomlang_fiber_reassign (atomlang_fiber_t *fiber, atomlang_closure_t *closure, uint16_t nargs);
ATOMLANG_API void                atomlang_fiber_reset (atomlang_fiber_t *fiber);
ATOMLANG_API void                atomlang_fiber_seterror (atomlang_fiber_t *fiber, const char *error);
ATOMLANG_API uint32_t            atomlang_fiber_size (atomlang_vm *vm, atomlang_fiber_t *fiber);

ATOMLANG_API void                atomlang_instance_blacken (atomlang_vm *vm, atomlang_instance_t *i);
ATOMLANG_API atomlang_instance_t *atomlang_instance_clone (atomlang_vm *vm, atomlang_instance_t *src_instance);
ATOMLANG_API void                atomlang_instance_deinit (atomlang_vm *vm, atomlang_instance_t *i);
ATOMLANG_API void                atomlang_instance_free (atomlang_vm *vm, atomlang_instance_t *i);
ATOMLANG_API bool                atomlang_instance_isstruct (atomlang_instance_t *i);
ATOMLANG_API atomlang_closure_t  *atomlang_instance_lookup_event (atomlang_instance_t *i, const char *name);
ATOMLANG_API atomlang_value_t     atomlang_instance_lookup_property (atomlang_vm *vm, atomlang_instance_t *i, atomlang_value_t key);
ATOMLANG_API atomlang_instance_t *atomlang_instance_new (atomlang_vm *vm, atomlang_class_t *c);
ATOMLANG_API void                atomlang_instance_serialize (atomlang_instance_t *i, json_t *json);
ATOMLANG_API void                atomlang_instance_setivar (atomlang_instance_t *instance, uint32_t idx, atomlang_value_t value);
ATOMLANG_API void                atomlang_instance_setxdata (atomlang_instance_t *i, void *xdata);
ATOMLANG_API uint32_t            atomlang_instance_size (atomlang_vm *vm, atomlang_instance_t *i);

ATOMLANG_API void                atomlang_value_blacken (atomlang_vm *vm, atomlang_value_t v);
ATOMLANG_API void                atomlang_value_dump (atomlang_vm *vm, atomlang_value_t v, char *buffer, uint16_t len);
ATOMLANG_API bool                atomlang_value_equals (atomlang_value_t v1, atomlang_value_t v2);
ATOMLANG_API void                atomlang_value_free (atomlang_vm *vm, atomlang_value_t v);
ATOMLANG_API atomlang_class_t    *atomlang_value_getclass (atomlang_value_t v);
ATOMLANG_API atomlang_class_t    *atomlang_value_getsuper (atomlang_value_t v);
ATOMLANG_API uint32_t            atomlang_value_hash (atomlang_value_t value);
ATOMLANG_API bool                atomlang_value_isobject (atomlang_value_t v);
ATOMLANG_API const char         *atomlang_value_name (atomlang_value_t value);
ATOMLANG_API void                atomlang_value_serialize (const char *key, atomlang_value_t v, json_t *json);
ATOMLANG_API uint32_t            atomlang_value_size (atomlang_vm *vm, atomlang_value_t v);
ATOMLANG_API bool                atomlang_value_vm_equals (atomlang_vm *vm, atomlang_value_t v1, atomlang_value_t v2);
ATOMLANG_API void               *atomlang_value_xdata (atomlang_value_t value);

ATOMLANG_API atomlang_value_t     atomlang_value_from_bool(bool b);
ATOMLANG_API atomlang_value_t     atomlang_value_from_error(const char* msg);
ATOMLANG_API atomlang_value_t     atomlang_value_from_float(atomlang_float_t f);
ATOMLANG_API atomlang_value_t     atomlang_value_from_int(atomlang_int_t n);
ATOMLANG_API atomlang_value_t     atomlang_value_from_null(void);
ATOMLANG_API atomlang_value_t     atomlang_value_from_object(void *obj);
ATOMLANG_API atomlang_value_t     atomlang_value_from_undefined(void);

ATOMLANG_API void                atomlang_object_blacken (atomlang_vm *vm, atomlang_object_t *obj);
ATOMLANG_API const char         *atomlang_object_debug (atomlang_object_t *obj, bool is_free);
ATOMLANG_API atomlang_object_t   *atomlang_object_deserialize (atomlang_vm *vm, json_value *entry);
ATOMLANG_API void                atomlang_object_free (atomlang_vm *vm, atomlang_object_t *obj);
ATOMLANG_API void                atomlang_object_serialize (atomlang_object_t *obj, json_t *json);
ATOMLANG_API uint32_t            atomlang_object_size (atomlang_vm *vm, atomlang_object_t *obj);

ATOMLANG_API void                atomlang_list_append_list (atomlang_vm *vm, atomlang_list_t *list1, atomlang_list_t *list2);
ATOMLANG_API void                atomlang_list_blacken (atomlang_vm *vm, atomlang_list_t *list);
ATOMLANG_API void                atomlang_list_free (atomlang_vm *vm, atomlang_list_t *list);
ATOMLANG_API atomlang_list_t     *atomlang_list_from_array (atomlang_vm *vm, uint32_t n, atomlang_value_t *p);
ATOMLANG_API atomlang_list_t     *atomlang_list_new (atomlang_vm *vm, uint32_t n);
ATOMLANG_API uint32_t            atomlang_list_size (atomlang_vm *vm, atomlang_list_t *list);

ATOMLANG_API void                atomlang_map_blacken (atomlang_vm *vm, atomlang_map_t *map);
ATOMLANG_API void                atomlang_map_append_map (atomlang_vm *vm, atomlang_map_t *map1, atomlang_map_t *map2);
ATOMLANG_API void                atomlang_map_free (atomlang_vm *vm, atomlang_map_t *map);
ATOMLANG_API void                atomlang_map_insert (atomlang_vm *vm, atomlang_map_t *map, atomlang_value_t key, atomlang_value_t value);
ATOMLANG_API atomlang_map_t      *atomlang_map_new (atomlang_vm *vm, uint32_t n);
ATOMLANG_API uint32_t            atomlang_map_size (atomlang_vm *vm, atomlang_map_t *map);

ATOMLANG_API void                atomlang_range_blacken (atomlang_vm *vm, atomlang_range_t *range);
ATOMLANG_API atomlang_range_t    *atomlang_range_deserialize (atomlang_vm *vm, json_value *json);
ATOMLANG_API void                atomlang_range_free (atomlang_vm *vm, atomlang_range_t *range);
ATOMLANG_API atomlang_range_t    *atomlang_range_new (atomlang_vm *vm, atomlang_int_t from, atomlang_int_t to, bool inclusive);
ATOMLANG_API void                atomlang_range_serialize (atomlang_range_t *r, json_t *json);
ATOMLANG_API uint32_t            atomlang_range_size (atomlang_vm *vm, atomlang_range_t *range);

ATOMLANG_API void                atomlang_string_blacken (atomlang_vm *vm, atomlang_string_t *string);
ATOMLANG_API void                atomlang_string_free (atomlang_vm *vm, atomlang_string_t *value);
ATOMLANG_API atomlang_string_t   *atomlang_string_new (atomlang_vm *vm, char *s, uint32_t len, uint32_t alloc);
ATOMLANG_API void                atomlang_string_set(atomlang_string_t *obj, char *s, uint32_t len);
ATOMLANG_API uint32_t            atomlang_string_size (atomlang_vm *vm, atomlang_string_t *string);
ATOMLANG_API atomlang_value_t     atomlang_string_to_value (atomlang_vm *vm, const char *s, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif