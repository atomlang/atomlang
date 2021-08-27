#ifndef __ATOMLANG_VM__
#define __ATOMLANG_VM__

#include "atomlang_delegate.h"
#include "atomlang_value.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATOMLANG_VM_GCENABLED            "gcEnabled"
#define ATOMLANG_VM_GCMINTHRESHOLD       "gcMinThreshold"
#define ATOMLANG_VM_GCTHRESHOLD          "gcThreshold"
#define ATOMLANG_VM_GCRATIO              "gcRatio"
#define ATOMLANG_VM_MAXCALLS             "maxCCalls"
#define ATOMLANG_VM_MAXBLOCK             "maxBlock"
#define ATOMLANG_VM_MAXRECURSION         "maxRecursionDepth"

typedef void (*vm_cleanup_cb) (atomlang_vm *vm);
typedef bool (*vm_filter_cb) (atomlang_object_t *obj);
typedef void (*vm_transfer_cb) (atomlang_vm *vm, atomlang_object_t *obj);

ATOMLANG_API atomlang_delegate_t *atomlang_vm_delegate (atomlang_vm *vm);
ATOMLANG_API atomlang_fiber_t    *atomlang_vm_fiber (atomlang_vm *vm);
ATOMLANG_API void                atomlang_vm_free (atomlang_vm *vm);
ATOMLANG_API atomlang_closure_t  *atomlang_vm_getclosure (atomlang_vm *vm);
ATOMLANG_API atomlang_value_t     atomlang_vm_getvalue (atomlang_vm *vm, const char *key, uint32_t keylen);
ATOMLANG_API atomlang_value_t     atomlang_vm_keyindex (atomlang_vm *vm, uint32_t index);
ATOMLANG_API bool                atomlang_vm_ismini (atomlang_vm *vm);
ATOMLANG_API bool                atomlang_vm_isaborted (atomlang_vm *vm);
ATOMLANG_API void                atomlang_vm_loadclosure (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API atomlang_value_t     atomlang_vm_lookup (atomlang_vm *vm, atomlang_value_t key);
ATOMLANG_API atomlang_vm         *atomlang_vm_new (atomlang_delegate_t *delegate);
ATOMLANG_API atomlang_vm         *atomlang_vm_newmini (void);
ATOMLANG_API void                atomlang_vm_reset (atomlang_vm *vm);
ATOMLANG_API atomlang_value_t     atomlang_vm_result (atomlang_vm *vm);
ATOMLANG_API bool                atomlang_vm_runclosure (atomlang_vm *vm, atomlang_closure_t *closure, atomlang_value_t sender, atomlang_value_t params[], uint16_t nparams);
ATOMLANG_API bool                atomlang_vm_runmain (atomlang_vm *vm, atomlang_closure_t *closure);
ATOMLANG_API void                atomlang_vm_set_callbacks (atomlang_vm *vm, vm_transfer_cb vm_transfer, vm_cleanup_cb vm_cleanup);
ATOMLANG_API void                atomlang_vm_setaborted (atomlang_vm *vm);
ATOMLANG_API void                atomlang_vm_seterror (atomlang_vm *vm, const char *format, ...);
ATOMLANG_API void                atomlang_vm_seterror_string (atomlang_vm* vm, const char *s);
ATOMLANG_API void                atomlang_vm_setfiber(atomlang_vm* vm, atomlang_fiber_t *fiber);
ATOMLANG_API void                atomlang_vm_setvalue (atomlang_vm *vm, const char *key, atomlang_value_t value);
ATOMLANG_API double              atomlang_vm_time (atomlang_vm *vm);

ATOMLANG_API void                atomlang_gray_object (atomlang_vm* vm, atomlang_object_t *obj);
ATOMLANG_API void                atomlang_gray_value (atomlang_vm* vm, atomlang_value_t v);
ATOMLANG_API void                atomlang_gc_setenabled (atomlang_vm* vm, bool enabled);
ATOMLANG_API void                atomlang_gc_setvalues (atomlang_vm *vm, atomlang_int_t threshold, atomlang_int_t minthreshold, atomlang_float_t ratio);
ATOMLANG_API void                atomlang_gc_start (atomlang_vm* vm);
ATOMLANG_API void                atomlang_gc_tempnull (atomlang_vm *vm, atomlang_object_t *obj);
ATOMLANG_API void                atomlang_gc_temppop (atomlang_vm *vm);
ATOMLANG_API void                atomlang_gc_temppush (atomlang_vm *vm, atomlang_object_t *obj);

ATOMLANG_API void                atomlang_vm_cleanup (atomlang_vm* vm);
ATOMLANG_API void                atomlang_vm_filter (atomlang_vm* vm, vm_filter_cb cleanup_filter);
ATOMLANG_API void                atomlang_vm_transfer (atomlang_vm* vm, atomlang_object_t *obj);

ATOMLANG_API void                atomlang_vm_initmodule (atomlang_vm *vm, atomlang_function_t *f);
ATOMLANG_API atomlang_closure_t  *atomlang_vm_loadbuffer (atomlang_vm *vm, const char *buffer, size_t len);
ATOMLANG_API atomlang_closure_t  *atomlang_vm_loadfile (atomlang_vm *vm, const char *path);

ATOMLANG_API atomlang_closure_t  *atomlang_vm_fastlookup (atomlang_vm *vm, atomlang_class_t *c, int index);
ATOMLANG_API void               *atomlang_vm_getdata (atomlang_vm *vm);
ATOMLANG_API atomlang_value_t     atomlang_vm_getslot (atomlang_vm *vm, uint32_t index);
ATOMLANG_API void                atomlang_vm_setdata (atomlang_vm *vm, void *data);
ATOMLANG_API void                atomlang_vm_setslot (atomlang_vm *vm, atomlang_value_t value, uint32_t index);
ATOMLANG_API atomlang_int_t       atomlang_vm_maxmemblock (atomlang_vm *vm);
ATOMLANG_API void                atomlang_vm_memupdate (atomlang_vm *vm, atomlang_int_t value);

ATOMLANG_API char               *atomlang_vm_anonymous (atomlang_vm *vm);
ATOMLANG_API atomlang_value_t     atomlang_vm_get (atomlang_vm *vm, const char *key);
ATOMLANG_API bool                atomlang_vm_set (atomlang_vm *vm, const char *key, atomlang_value_t value);

ATOMLANG_API bool                atomlang_isopt_class (atomlang_class_t *c);
ATOMLANG_API void                atomlang_opt_free (void);
ATOMLANG_API void                atomlang_opt_register (atomlang_vm *vm);

#ifdef __cplusplus
}
#endif

#endif