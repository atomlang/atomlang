#include "atomlang_hash.h"
#include "atomlang_json.h"
#include "atomlang_array.h"
#include "atomlang_debug.h"
#include "atomlang_macros.h"
#include "atomlang_vm.h"
#include "atomlang_core.h"
#include "atomlang_opcodes.h"
#include "atomlang_memory.h"
#include "atomlang_vmmacros.h"
#include "atomlang_optionals.h"

static void atomlang_gc_cleanup (atomlang_vm *vm);
static void atomlang_gc_transfer (atomlang_vm *vm, atomlang_object_t *obj);
static bool vm_set_superclass (atomlang_vm *vm, atomlang_object_t *obj);
static void atomlang_gc_transform (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t *value, void *data);

static uint32_t cache_refcount = 0;
static atomlang_value_t cache[ATOMLANG_VTABLE_SIZE];

static atomlang_delegate_t empty_delegate;

struct atomlang_vm {
    atomlang_hash_t      *context;                      
    atomlang_delegate_t  *delegate;                     
    atomlang_fiber_t     *fiber;                        
    void                *data;                          
    uint32_t            pc;                             
    double              time;                           
    bool                aborted;                        
    uint32_t            maxccalls;                      
    uint32_t            nccalls;                        

    atomlang_int_t       maxrecursion;                   
    atomlang_int_t       recursioncount;                 
    
    uint32_t            nanon;                          
    char                temp[64];                       

    vm_transfer_cb      transfer;                      
    vm_cleanup_cb       cleanup;                       
    vm_filter_cb        filter;                        

    int32_t             gcenabled;                      
    atomlang_int_t       memallocated;                  
    atomlang_int_t       maxmemblock;                   
    atomlang_object_t    *gchead;                       
    atomlang_int_t       gcminthreshold;                
    atomlang_int_t       gcthreshold;                   
    atomlang_int_t       gcthreshold_original;          
    atomlang_float_t     gcratio;                       
    atomlang_int_t       gccount;                       
    atomlang_object_r    graylist;                      
    atomlang_object_r    gctemp;                        

    #if ATOMLANG_VM_STATS
    uint32_t            nfrealloc;                      
    uint32_t            nsrealloc;                      
    uint32_t            nstat[ATOMLANG_LATEST_OPCODE];   
    double              tstat[ATOMLANG_LATEST_OPCODE];   
    nanotime_t          t;                              
    #endif
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static void atomlang_stack_dump (atomlang_fiber_t *fiber) {
    uint32_t index = 0;
    for (atomlang_value_t *stack = fiber->stack; stack < fiber->stacktop; ++stack) {
        printf("[%05d]\t", index++);
        if (!stack->isa) {printf("\n"); continue;}
        atomlang_value_dump(NULL, *stack, NULL, 0);
    }
    if (index) printf("\n\n");
}

static void atomlang_callframe_dump (atomlang_fiber_t *fiber) {
    printf("===========================\n");
    printf("CALL FRAME\n");
    printf("===========================\n");
    for (uint32_t i = 0; i < fiber->nframes; ++i) {
        atomlang_callframe_t *frame = &fiber->frames[i];
        const char *fname = (frame->closure->f->identifier) ? frame->closure->f->identifier : "N/A";
        atomlang_value_t self_value = frame->stackstart[0];
        char buffer[256];
        atomlang_value_dump(NULL, self_value, buffer, sizeof(buffer));
        printf("[%03d]\t%s\t(%s)\n", i, fname, buffer);
    }
}

static uint32_t atomlang_vm_lineno (atomlang_vm *vm) {
    atomlang_fiber_t *fiber = vm->fiber;
    
    atomlang_callframe_t *frame = (fiber->nframes) ? &fiber->frames[fiber->nframes-1] : NULL;
    if (!frame) return 0;
    
    atomlang_function_t *func = (frame->closure) ? frame->closure->f : NULL;
    if (!func) return 0;
    
    if (func->tag == EXEC_TYPE_NATIVE && func->lineno) {
        uint32_t nindex = 0;
        if (frame->ip > func->bytecode) {
            nindex = (uint32_t)(frame->ip - func->bytecode) - 1;
        }
        
        return func->lineno[nindex];
    }
    
    return 0;
}


#pragma clang diagnostic pop

static void report_runtime_error (atomlang_vm *vm, error_type_t error_type, const char *format, ...) {
    char        buffer[1024];
    va_list        arg;

    if (vm->aborted) return;
    vm->aborted = true;

    if (format) {
        va_start (arg, format);
        vsnprintf(buffer, sizeof(buffer), format, arg);
        va_end (arg);
    }

    atomlang_error_callback error_cb = ((atomlang_delegate_t *)vm->delegate)->error_callback;
    if (error_cb) {
        uint32_t lineno = atomlang_vm_lineno(vm);
        error_desc_t edesc = (error_desc_t){lineno, 0, 0, 0};
        void *data = ((atomlang_delegate_t *)vm->delegate)->xdata;
		error_cb(vm, error_type, buffer, edesc, data);
    } else {
        printf("%s\n", buffer);
        fflush(stdout);
    }
    
}

static void atomlang_cache_setup (void) {
    ++cache_refcount;

    mem_check(false);
    cache[ATOMLANG_NOTFOUND_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_NOTFOUND_NAME);
    cache[ATOMLANG_ADD_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_ADD_NAME);
    cache[ATOMLANG_SUB_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_SUB_NAME);
    cache[ATOMLANG_DIV_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_DIV_NAME);
    cache[ATOMLANG_MUL_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_MUL_NAME);
    cache[ATOMLANG_REM_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_REM_NAME);
    cache[ATOMLANG_AND_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_AND_NAME);
    cache[ATOMLANG_OR_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_OR_NAME);
    cache[ATOMLANG_CMP_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_CMP_NAME);
    cache[ATOMLANG_EQQ_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_EQQ_NAME);
    cache[ATOMLANG_IS_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_IS_NAME);
    cache[ATOMLANG_MATCH_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_MATCH_NAME);
    cache[ATOMLANG_NEG_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_NEG_NAME);
    cache[ATOMLANG_NOT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_NOT_NAME);
    cache[ATOMLANG_LSHIFT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_LSHIFT_NAME);
    cache[ATOMLANG_RSHIFT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_RSHIFT_NAME);
    cache[ATOMLANG_BAND_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_BAND_NAME);
    cache[ATOMLANG_BOR_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_BOR_NAME);
    cache[ATOMLANG_BXOR_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_BXOR_NAME);
    cache[ATOMLANG_BNOT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_OPERATOR_BNOT_NAME);
    cache[ATOMLANG_LOAD_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_LOAD_NAME);
    cache[ATOMLANG_LOADS_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_LOADS_NAME);
    cache[ATOMLANG_LOADAT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_LOADAT_NAME);
    cache[ATOMLANG_STORE_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_STORE_NAME);
    cache[ATOMLANG_STOREAT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_STOREAT_NAME);
    cache[ATOMLANG_INT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_CLASS_INT_NAME);
    cache[ATOMLANG_FLOAT_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_CLASS_FLOAT_NAME);
    cache[ATOMLANG_BOOL_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_CLASS_BOOL_NAME);
    cache[ATOMLANG_STRING_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_CLASS_STRING_NAME);
    cache[ATOMLANG_EXEC_INDEX] = VALUE_FROM_CSTRING(NULL, ATOMLANG_INTERNAL_EXEC_NAME);
    mem_check(true);
}

static void atomlang_cache_free (void) {
    --cache_refcount;
    if (cache_refcount > 0) return;

    mem_check(false);
    for (uint32_t index = 0; index <ATOMLANG_VTABLE_SIZE; ++index) {
        atomlang_value_free(NULL, cache[index]);
    }
    mem_check(true);
}

atomlang_value_t atomlang_vm_keyindex (atomlang_vm *vm, uint32_t index) {
    #pragma unused (vm)
    return cache[index];
}

static inline atomlang_callframe_t *atomlang_new_callframe (atomlang_vm *vm, atomlang_fiber_t *fiber) {
    #pragma unused(vm)

    // check if there are enough slots in the call frame and optionally create new cframes
    if (fiber->framesalloc - fiber->nframes < 1) {
        uint32_t new_size = fiber->framesalloc * 2;
        void *ptr = mem_realloc(NULL, fiber->frames, sizeof(atomlang_callframe_t) * new_size);
        if (!ptr) {
            // frames reallocation failed means that there is a very high probability to be into an infinite loop
            report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "Infinite loop detected. Current execution must be aborted.");
            return NULL;
        }
        fiber->frames = (atomlang_callframe_t *)ptr;
        fiber->framesalloc = new_size;
        STAT_FRAMES_REALLOCATED(vm);
    }

    // update frames counter
    ++fiber->nframes;

    // return first available cframe (-1 because I just updated the frames counter)
    return &fiber->frames[fiber->nframes - 1];
}

static inline bool atomlang_check_stack (atomlang_vm *vm, atomlang_fiber_t *fiber, uint32_t stacktopdelta, atomlang_value_t **stackstart) {
    #pragma unused(vm)
	if (stacktopdelta == 0) return true;
	
    // update stacktop pointer before a call
    fiber->stacktop += stacktopdelta;
	
    // check stack size
	uint32_t stack_size = (uint32_t)(fiber->stacktop - fiber->stack);
    uint32_t stack_needed = MAXNUM(stack_size, DEFAULT_MINSTACK_SIZE);
    if (fiber->stackalloc >= stack_needed) return true;
    atomlang_value_t *old_stack = fiber->stack;

    // perform stack reallocation (power_of2_ceil returns 0 if argument is bigger than 2^31)
    uint32_t new_size = power_of2_ceil(fiber->stackalloc + stack_needed);
    bool size_condition = (new_size && (uint64_t)new_size >= (uint64_t)(fiber->stackalloc + stack_needed) && ((sizeof(atomlang_value_t) * new_size) < SIZE_MAX));
    void *ptr = (size_condition) ? mem_realloc(NULL, fiber->stack, sizeof(atomlang_value_t) * new_size) : NULL;
    if (!ptr) {
        // restore stacktop to previous state
        fiber->stacktop -= stacktopdelta;

        // stack reallocation failed means that there is a very high probability to be into an infinite loop
        // so return false and let the calling function (vm_exec) raise a runtime error
        return false;
    }
    
    fiber->stack = (atomlang_value_t *)ptr;
    fiber->stackalloc = new_size;
    STAT_STACK_REALLOCATED(vm);

    // check if reallocation moved the stack
    if (fiber->stack == old_stack) return true;

    // re-compute ptr offset
    ptrdiff_t offset = (ptrdiff_t)(fiber->stack - old_stack);

    // adjust stack pointer for each call frame
    for (uint32_t i=0; i < fiber->nframes; ++i) {
        fiber->frames[i].stackstart += offset;
    }

    // adjust upvalues ptr offset
    atomlang_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue) {
        upvalue->value += offset;
        upvalue = upvalue->next;
    }

    // adjust fiber stack pointer
    fiber->stacktop += offset;

    // stack is changed so update currently used stackstart
    *stackstart += offset;

    return true;
}

static atomlang_upvalue_t *atomlang_capture_upvalue (atomlang_vm *vm, atomlang_fiber_t *fiber, atomlang_value_t *value) {
    // closures and upvalues implementation inspired by Lua and Wren
     // fiber->upvalues list must be ORDERED by the level of the corresponding variables in the stack starting from top

    // if upvalues is empty then create it
    if (!fiber->upvalues) {
        fiber->upvalues = atomlang_upvalue_new(vm, value);
        return fiber->upvalues;
    }

    // scan list looking for upvalue first (keeping track of the order)
    atomlang_upvalue_t *prevupvalue = NULL;
    atomlang_upvalue_t *upvalue = fiber->upvalues;
    while (upvalue && upvalue->value > value) {
        prevupvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->value == value) return upvalue;

    // upvalue not found in list so creates a new one and add it in list ORDERED
    atomlang_upvalue_t *newvalue = atomlang_upvalue_new(vm, value);
    if (prevupvalue == NULL) fiber->upvalues = newvalue;
    else prevupvalue->next = newvalue;

    // returns newly created upvalue
    newvalue->next = upvalue;
    
    return newvalue;
}

static void atomlang_close_upvalues (atomlang_fiber_t *fiber, atomlang_value_t *level) {
    while (fiber->upvalues != NULL && fiber->upvalues->value >= level) {
        atomlang_upvalue_t *upvalue = fiber->upvalues;

        // move the value into the upvalue itself and point the upvalue to it
        upvalue->closed = *upvalue->value;
        upvalue->value = &upvalue->closed;

        // remove it from the open upvalue list
        fiber->upvalues = upvalue->next;
    }
}

static void atomlang_vm_loadclass (atomlang_vm *vm, atomlang_class_t *c) {
    atomlang_hash_transform(c->htable, atomlang_gc_transform, (void *)vm);
    atomlang_class_t *meta = atomlang_class_get_meta(c);
    atomlang_hash_transform(meta->htable, atomlang_gc_transform, (void *)vm);
}

void atomlang_opt_register (atomlang_vm *vm) {
    ATOMLANG_MATH_REGISTER(vm);
    ATOMLANG_ENV_REGISTER(vm);
    ATOMLANG_JSON_REGISTER(vm);
    ATOMLANG_FILE_REGISTER(vm);
}

void atomlang_opt_free() {
    ATOMLANG_MATH_FREE();
    ATOMLANG_ENV_FREE();
    ATOMLANG_JSON_FREE();
    ATOMLANG_FILE_FREE();
}

bool atomlang_isopt_class (atomlang_class_t *c) {
    return (ATOMLANG_ISMATH_CLASS(c)) || (ATOMLANG_ISENV_CLASS(c) || (ATOMLANG_ISJSON_CLASS(c)) || (ATOMLANG_ISFILE_CLASS(c)));
}

static bool atomlang_vm_exec (atomlang_vm *vm) {
    DECLARE_DISPATCH_TABLE;

    atomlang_fiber_t         *fiber = vm->fiber;         // current fiber
    atomlang_delegate_t      *delegate = vm->delegate;   // current delegate
    atomlang_callframe_t     *frame;                     // current executing frame
    atomlang_function_t      *func;                      // current executing function
    atomlang_value_t         *stackstart;                // SP => stack pointer
    register uint32_t       *ip;                        // IP => instruction pointer
    register uint32_t       inst;                       // IR => instruction register
    register opcode_t       op;                         // OP => opcode register

    // load current callframe
    LOAD_FRAME();
    DEBUG_CALL("Executing", func);

    // sanity check
    if ((ip == NULL) || (!func->bytecode) || (func->ninsts == 0)) return true;

    DEBUG_STACK();

    while (1) {
        INTERPRET_LOOP {

            CASE_CODE(NOP): {
                DEBUG_VM("NOP");
                DISPATCH();
            }

            CASE_CODE(MOVE): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("MOVE %d %d", r1, r2);

                SETVALUE(r1, STACK_GET(r2));
                DISPATCH();
            }
            
            CASE_CODE(LOADS): {
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("LOADS %d %d %d", r1, r2, r3);
                
                // r1 result
                // r2 superclass to lookup (target implicit to self)
                // r3 key
                
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);
                
                atomlang_value_t target = STACK_GET(0);
                atomlang_class_t *target_class = atomlang_value_getclass(target);
                atomlang_class_t *super_target = atomlang_class_lookup_class_identifier(target_class, VALUE_AS_CSTRING(v2));
                if (!super_target) RUNTIME_ERROR("Unable to find superclass %s in self object", VALUE_AS_CSTRING(v2));
                
                atomlang_object_t *result = atomlang_class_lookup(super_target, v3);
                if (!result) RUNTIME_ERROR("Unable to find %s in superclass %s", VALUE_AS_CSTRING(v3), (super_target->identifier) ? super_target->identifier : "N/A");
                
                SETVALUE(r1, VALUE_FROM_OBJECT(result));
                DISPATCH();
            }

            CASE_CODE(LOAD):
            CASE_CODE(LOADAT):{
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("%s %d %d %d", (op == LOAD) ? "LOAD" : ((op == LOADAT) ? "LOADAT" : "LOADS"), r1, r2, r3);

                // r1 result
                // r2 target
                // r3 key

                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, (op == LOAD) ? ATOMLANG_LOAD_INDEX : ((op == LOADAT) ? ATOMLANG_LOADAT_INDEX : ATOMLANG_LOADS_INDEX), rwin);

                // save currently executing fiber (that can change!)
                atomlang_fiber_t *current_fiber = fiber;
                
                // call closure (do not use a macro here because we want to handle both the bridged and special cases)
                STORE_FRAME();
                execute_load_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be in sync in this case
                        current_fiber = NULL;
                        PUSH_FRAME(closure, &stackstart[rwin], r1, 2);
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        atomlang_value_t r1copy = STACK_GET(r1);
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], 2, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special getter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                SETVALUE(r1, r1copy);
                                goto execute_load_function;
                            }

                            // check for special fiber error
                            fiber = vm->fiber;
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        DEBUG_ASSERT(delegate->bridge_getvalue, "bridge_getvalue delegate callback is mandatory");
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = delegate->bridge_getvalue(vm, closure->f->xdata, v2, VALUE_AS_CSTRING(v3), r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_SPECIAL: {
                        if (!closure->f->special[EXEC_TYPE_SPECIAL_GETTER]) RUNTIME_ERROR("Missing special getter function for property %s", VALUE_AS_CSTRING(v3));
                        closure = closure->f->special[EXEC_TYPE_SPECIAL_GETTER];
                        goto execute_load_function;
                    } break;
                }
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(LOADI): {
                OPCODE_GET_ONE8bit_SIGN_ONE17bit(inst, const uint32_t r1, const int32_t value);
                DEBUG_VM("LOADI %d %d", r1, value);

                SETVALUE_INT(r1, value);
                DISPATCH();
            }

            CASE_CODE(LOADK): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t index);
                DEBUG_VM("LOADK %d %d", r1, index);

                // constant pool case
                if (index < marray_size(func->cpool)) {
                    atomlang_value_t v = atomlang_function_cpool_get(func, index);
                    SETVALUE(r1, v);
                    DISPATCH();
                }

                // special value case
                switch (index) {
                    case CPOOL_VALUE_SUPER: {
                        // get super class from STACK_GET(0) which is self
                        atomlang_class_t *super = atomlang_value_getsuper(STACK_GET(0));
                        SETVALUE(r1, (super) ? VALUE_FROM_OBJECT(super) : VALUE_FROM_NULL);
                    } break;
                    case CPOOL_VALUE_ARGUMENTS: SETVALUE(r1, (frame->args) ? VALUE_FROM_OBJECT(frame->args) : VALUE_FROM_NULL); break;
                    case CPOOL_VALUE_NULL: SETVALUE(r1, VALUE_FROM_NULL); break;
                    case CPOOL_VALUE_UNDEFINED: SETVALUE(r1, VALUE_FROM_UNDEFINED); break;
                    case CPOOL_VALUE_TRUE: SETVALUE(r1, VALUE_FROM_TRUE); break;
                    case CPOOL_VALUE_FALSE: SETVALUE(r1, VALUE_FROM_FALSE); break;
                    case CPOOL_VALUE_FUNC: SETVALUE(r1, VALUE_FROM_OBJECT(frame->closure)); break;
                    default: RUNTIME_ERROR("Unknown LOADK index"); break;
                }
                DISPATCH();
            }

            CASE_CODE(LOADG): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
                DEBUG_VM("LOADG %d %d", r1, index);

                atomlang_value_t key = atomlang_function_cpool_get(func, index);
                atomlang_value_t *v = atomlang_hash_lookup(vm->context, key);
                if (!v) RUNTIME_ERROR("Unable to find object %s", VALUE_AS_CSTRING(key));

                SETVALUE(r1, *v);
                DISPATCH();
            }

            CASE_CODE(LOADU): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("LOADU %d %d", r1, r2);

                atomlang_upvalue_t *upvalue = frame->closure->upvalue[r2];
                SETVALUE(r1, *upvalue->value);
                DISPATCH();
            }

            CASE_CODE(STORE):
            CASE_CODE(STOREAT):{
                OPCODE_GET_TWO8bit_ONE10bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3);
                DEBUG_VM("%s %d %d %d", (op == STORE) ? "STORE" : "STOREAT", r1, r2,r3);

                // r1 value
                // r2 target
                // r3 key

                DEFINE_STACK_VARIABLE(v1,r1);
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_INDEX_VARIABLE(v3,r3);

                // prepare function call
                PREPARE_FUNC_CALL3(closure, v2, v3, v1, (op == STORE) ? ATOMLANG_STORE_INDEX : ATOMLANG_STOREAT_INDEX, rwin);

                // save currently executing fiber (that can change!)
                atomlang_fiber_t *current_fiber = fiber;
                
                // in case of computed property what happens is that EXEC_TYPE_INTERNAL returns false
                // and the initial closure variable is overwritten by the closure to be executed returned
                // in the r1 registed. The goto execute_store_function take cares of re-executing the code
                // with the updated closure (that contains the computed property code) but the r1 register
                // must be re-set with its initial value backed-up in the r1copy variable
                bool reset_r1 = false;
                
                // call function f (do not use a macro here because we want to handle both the bridged and special cases)
                STORE_FRAME();
                execute_store_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be in sync in this case
                        current_fiber = NULL;
                        if (reset_r1) {SETVALUE(rwin+1, v1); reset_r1 = false;}
                        // was r1 but it was incorrect, pass r3 as the destination register because I am sure it is a
                        // dummy temp (and unused) register that can be safely set to NULL
                        PUSH_FRAME(closure, &stackstart[rwin], r3, 2);
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        atomlang_value_t r1copy = STACK_GET(r1);
                        if (reset_r1) {SETVALUE(rwin+1, r1copy);  reset_r1 = false;}
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], 2, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special setter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                if (closure->f->tag == EXEC_TYPE_INTERNAL) r1copy = STACK_GET(rwin+1);
                                SETVALUE(r1, r1copy);
                                reset_r1 = true;
                                goto execute_store_function;
                            }

                            // check for special fiber error
                            fiber = vm->fiber;
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        DEBUG_ASSERT(delegate->bridge_setvalue, "bridge_setvalue delegate callback is mandatory");
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = delegate->bridge_setvalue(vm, closure->f->xdata, v2, VALUE_AS_CSTRING(v3), v1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_SPECIAL: {
                        if (!closure->f->special[EXEC_TYPE_SPECIAL_SETTER]) RUNTIME_ERROR("Missing special setter function for property %s", VALUE_AS_CSTRING(v3));
                        closure = closure->f->special[EXEC_TYPE_SPECIAL_SETTER];
                        goto execute_store_function;
                    } break;
                }
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(STOREG): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, uint32_t r1, int32_t index);
                DEBUG_VM("STOREG %d %d", r1, index);

                atomlang_value_t key = atomlang_function_cpool_get(func, index);
                atomlang_value_t v = STACK_GET(r1);
                atomlang_hash_insert(vm->context, key, v);
                DISPATCH();
            }

            CASE_CODE(STOREU): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t r2);
                DEBUG_VM("STOREU %d %d", r1, r2);

                atomlang_upvalue_t *upvalue = frame->closure->upvalue[r2];
                *upvalue->value = STACK_GET(r1);
                DISPATCH();
            }

            CASE_CODE(EQQ):
            CASE_CODE(NEQQ): {
                // decode operation
                DECODE_BINARY_OPERATION(r1,r2,r3);

                // get registers
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_EQQ_INDEX, rwin);

                // call function f
                CALL_FUNC(EQQ, closure, r1, 2, rwin);

                // get result of the EQQ selector
                register atomlang_int_t result = STACK_GET(r1).n;

                // save result (NEQQ is not EQQ)
                SETVALUE_BOOL(r1, (op == EQQ) ? result : !result);

                DISPATCH();
            }

            CASE_CODE(ISA):
            CASE_CODE(MATCH): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // get registers
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, (op == ISA) ? ATOMLANG_IS_INDEX : ATOMLANG_MATCH_INDEX, rwin);

                // call function f
                CALL_FUNC(ISA, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(LT):
            CASE_CODE(GT):
            CASE_CODE(EQ):
            CASE_CODE(LEQ):
            CASE_CODE(GEQ):
            CASE_CODE(NEQ): {

                // decode operation
                DECODE_BINARY_OPERATION(r1,r2,r3);

                // check fast comparison only if both values are boolean OR if one of them is undefined
                DEFINE_STACK_VARIABLE(v2,r2);
                DEFINE_STACK_VARIABLE(v3,r3);
                if ((VALUE_ISA_BOOL(v2) && (VALUE_ISA_BOOL(v3))) || (VALUE_ISA_UNDEFINED(v2) || (VALUE_ISA_UNDEFINED(v3)))) {
                    register atomlang_int_t eq_result = (v2.isa == v3.isa) && (v2.n == v3.n);
                    SETVALUE(r1, VALUE_FROM_BOOL((op == EQ) ? eq_result : !eq_result));
                    DISPATCH();
                } else if (VALUE_ISA_INT(v2) && VALUE_ISA_INT(v3)) {
                    // INT optimization especially useful in loops
                    if (v2.n == v3.n) SETVALUE(r1, VALUE_FROM_INT(0));
                    else SETVALUE(r1, VALUE_FROM_INT((v2.n > v3.n) ? 1 : -1));
                } else {
                    // prepare function call for binary operation
                    PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_CMP_INDEX, rwin);

                    // call function f
                    CALL_FUNC(CMP, closure, r1, 2, rwin);
                }

                // compare returns 0 if v1 and v2 are equals, 1 if v1>v2 and -1 if v1<v2
                register atomlang_int_t result = STACK_GET(r1).n;

                switch(op) {
                    case LT: SETVALUE_BOOL(r1, result < 0); break;
                    case GT: SETVALUE_BOOL(r1, result > 0); break;
                    case EQ: SETVALUE_BOOL(r1, result == 0); break;
                    case LEQ: SETVALUE_BOOL(r1, result <= 0); break;
                    case GEQ: SETVALUE_BOOL(r1, result >= 0); break;
                    case NEQ: SETVALUE_BOOL(r1, result != 0); break;
                    default: assert(0);
                }

                // optimize the case where after a comparison there is a JUMPF instruction (usually in a loop)
                uint32_t inext = *ip++;
                if ((STACK_GET(r1).n == 0) && (OPCODE_GET_OPCODE(inext) == JUMPF)) {
                    OPCODE_GET_LAST18bit(inext, int32_t value);
                    DEBUG_VM("JUMPF %d %d", (int)result, value);
                    ip = COMPUTE_JUMP(value); // JUMP is an absolute value
                    DISPATCH();
                }

                // JUMPF not executed so I need to go back in instruction pointer
                --ip;

                // continue execution
                DISPATCH();
            }

            CASE_CODE(LSHIFT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, <<);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_LSHIFT_INDEX, rwin);

                // call function f
                CALL_FUNC(LSHIFT, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(RSHIFT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, >>);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_RSHIFT_INDEX, rwin);

                // call function f
                CALL_FUNC(RSHIFT, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(BAND): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, &);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, &);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_BAND_INDEX, rwin);

                // call function f
                CALL_FUNC(BAND, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(BOR): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, |);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, |);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_BOR_INDEX, rwin);

                // call function f
                CALL_FUNC(BOR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(BXOR):{
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bit math operation first (only if both v2 and v3 are int)
                CHECK_FAST_BINARY_BIT(r1, r2, r3, v2, v3, ^);
                CHECK_FAST_BINBOOL_BIT(r1, v2, v3, ^);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_BXOR_INDEX, rwin);

                // call function f
                CALL_FUNC(BXOR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(ADD): {
                DECODE_BINARY_OPERATION(r1, r2, r3);

                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, +, NO_CHECK);

                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_ADD_INDEX, rwin);

                CALL_FUNC(ADD, closure, r1, 2, rwin);

                DISPATCH();
            }

            CASE_CODE(SUB): {
                DECODE_BINARY_OPERATION(r1, r2, r3);

                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, -, NO_CHECK);

                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_SUB_INDEX, rwin);

                CALL_FUNC(SUB, closure, r1, 2, rwin);

                DISPATCH();
            }

            CASE_CODE(DIV): {
                DECODE_BINARY_OPERATION(r1, r2, r3);

                #if defined(__clang__)
                    #pragma clang diagnostic push
                    #pragma clang diagnostic ignored "-Wdivision-by-zero"
                #elif defined(__GNUC__)
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdiv-by-zero"
				#elif defined(_MSC_VER)
					#pragma warning (push)
					#pragma warning (disable: 4723)
                #endif
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, /, CHECK_ZERO(v3));
                #if defined(__clang__)
                    #pragma clang diagnostic pop
                #elif defined(__GNUC__)
                    #pragma GCC diagnostic pop
				#elif defined(_MSC_VER)
					#pragma warning (pop)
                #endif

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_DIV_INDEX, rwin);

                // call function f
                CALL_FUNC(DIV, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(MUL): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_MATH(r1, r2, r3, v2, v3, *, NO_CHECK);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_MUL_INDEX, rwin);

                // call function f
                CALL_FUNC(MUL, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(REM): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast math operation first (only in case of int and float)
                CHECK_FAST_BINARY_REM(r1, r2, r3, v2, v3);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_REM_INDEX, rwin);

                // call function f
                CALL_FUNC(REM, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(AND): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bool operation first (only if both are bool)
                CHECK_FAST_BINARY_BOOL(r1, r2, r3, v2, v3, &&);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_AND_INDEX, rwin);

                // call function f
                CALL_FUNC(AND, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(OR): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);

                // check fast bool operation first (only if both are bool)
                CHECK_FAST_BINARY_BOOL(r1, r2, r3, v2, v3, ||);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL2(closure, v2, v3, ATOMLANG_OR_INDEX, rwin);

                // call function f
                CALL_FUNC(OR, closure, r1, 2, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(NEG): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast bool operation first (only if it is int or float)
                CHECK_FAST_UNARY_MATH(r1, r2, v2, -);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, ATOMLANG_NEG_INDEX, rwin);

                // call function f
                CALL_FUNC(NEG, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(NOT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast bool operation first  (only if it is bool)
                CHECK_FAST_UNARY_BOOL(r1, r2, v2, !);

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, ATOMLANG_NOT_INDEX, rwin);

                // call function f
                CALL_FUNC(NOT, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }

            CASE_CODE(BNOT): {
                // decode operation
                DECODE_BINARY_OPERATION(r1, r2, r3);
                #pragma unused(r3)

                // check fast int operation first
                DEFINE_STACK_VARIABLE(v2,r2);
                // ~v2.n == -v2.n-1
                if (VALUE_ISA_INT(v2)) {SETVALUE(r1, VALUE_FROM_INT(~v2.n)); DISPATCH();}

                // prepare function call for binary operation
                PREPARE_FUNC_CALL1(closure, v2, ATOMLANG_BNOT_INDEX, rwin);

                // call function f
                CALL_FUNC(BNOT, closure, r1, 1, rwin);

                // continue execution
                DISPATCH();
            }


            CASE_CODE(JUMPF): {
                // JUMPF like JUMP is an absolute value
                OPCODE_GET_ONE8bit_FLAG_ONE17bit(inst, const uint32_t r1, const uint32_t flag, const int32_t value);
                DEBUG_VM("JUMPF %d %d (flag %d)", r1, value, flag);

                if (flag) {
                    // if flag is set then ONLY boolean values must be checked (compiler must guarantee this condition)
                    // this is necessary in a FOR loop over numeric values (with an iterator) where otherwise number 0
                    // could be computed as a false condition
                    if ((VALUE_ISA_BOOL(STACK_GET(r1))) && (GETVALUE_INT(STACK_GET(r1)) == 0)) ip = COMPUTE_JUMP(value);
                    DISPATCH();
                }

                // no flag set so convert r1 to BOOL
                DEFINE_STACK_VARIABLE(v1, r1);

                // common NULL/UNDEFINED/BOOL/INT/FLOAT/STRING cases
                // NULL/UNDEFINED   =>    no check
                // BOOL/INT         =>    check n
                // FLOAT            =>    check f
                // STRING           =>    check len
                if (VALUE_ISA_NULL(v1) || (VALUE_ISA_UNDEFINED(v1))) {ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_BOOL(v1) || (VALUE_ISA_INT(v1))) {if (GETVALUE_INT(v1) == 0) ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_FLOAT(v1)) {if (GETVALUE_FLOAT(v1) == 0.0) ip = COMPUTE_JUMP(value);}
                else if (VALUE_ISA_STRING(v1)) {if (VALUE_AS_STRING(v1)->len == 0) ip = COMPUTE_JUMP(value);}
                else {
                    // no common case so check if it implements the Bool command
                    atomlang_closure_t *closure = (atomlang_closure_t *)atomlang_class_lookup_closure(atomlang_value_getclass(v1), cache[ATOMLANG_BOOL_INDEX]);

                    // if no closure is found then object is considered TRUE
                    if (closure) {
                        // prepare func call
                        uint32_t rwin = FN_COUNTREG(func, frame->nargs);
                        uint32_t _rneed = FN_COUNTREG(closure->f, 1);
						uint32_t stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - fiber->stacktop, 0);
                        if (!atomlang_check_stack(vm, fiber, stacktopdelta, &stackstart)) {
                            RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
                        }
                        SETVALUE(rwin, v1);

                        // call func and check result
                        CALL_FUNC(JUMPF, closure, rwin, 2, rwin);
                        atomlang_int_t result = STACK_GET(rwin).n;

                        // re-compute ip ONLY if false
                        if (!result) ip = COMPUTE_JUMP(value);
                    }
                }
                DISPATCH();
            }

            CASE_CODE(JUMP): {
                OPCODE_GET_ONE26bit(inst, const uint32_t value);
                DEBUG_VM("JUMP %d", value);

                ip = COMPUTE_JUMP(value); // JUMP is an absolute value
                DISPATCH();
            }

            CASE_CODE(CALL): {
                // CALL A B C => R(A) = B(B+1...B+C)
                OPCODE_GET_THREE8bit(inst, const uint32_t r1, const uint32_t r2, register uint32_t r3);
                DEBUG_VM("CALL %d %d %d", r1, r2, r3);

                DEBUG_STACK();

                // r1 is the destination register
                // r2 is the register which contains the callable object
                // r3 is the number of arguments (nparams)

                // sliding register window as in:
                // https://the-ravi-programming-language.readthedocs.io/en/latest/lua-parser.html#sliding-register-window-by-mike-pall
                const uint32_t rwin = r2 + 1;

                // object to call
                atomlang_value_t v = STACK_GET(r2);

                // closure to call
                atomlang_closure_t *closure = NULL;

                if (VALUE_ISA_CLOSURE(v)) {
                    closure = VALUE_AS_CLOSURE(v);
                } else {
                    // check for exec closure inside object
                    closure = (atomlang_closure_t *)atomlang_class_lookup_closure(atomlang_value_getclass(v), cache[ATOMLANG_EXEC_INDEX]);
                }

                // sanity check
                if (!closure) RUNTIME_ERROR("Unable to call object (in function %s)", func->identifier);

                // check stack size
                uint32_t _rneed = FN_COUNTREG(closure->f, r3);
				uint32_t stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - fiber->stacktop, 0);
                if (!atomlang_check_stack(vm, fiber, stacktopdelta, &stackstart)) {
                    RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
                }

                // if less arguments are passed then fill the holes with UNDEFINED values
                while (r3 < closure->f->nparams) {
                    SETVALUE(rwin+r3, VALUE_FROM_UNDEFINED);
                    ++r3;
                }
                
                if (VALUE_ISA_CLASS(v)) {
                    // set self as class for class_exec()
                    SETVALUE(rwin, v);
                } else if (closure->context) {
                    // check for closure autocaptured self context (or custom self set by the user)
                    SETVALUE(rwin, VALUE_FROM_OBJECT(closure->context));
                }
                
                DEBUG_STACK();

                // save currently executing fiber (that can change!)
                atomlang_fiber_t *current_fiber = fiber;
                
                // execute function
                STORE_FRAME();
                execute_call_function:
                switch(closure->f->tag) {
                    case EXEC_TYPE_NATIVE: {
                        // invalidate current_fiber because it does not need to be synced in this case
                        current_fiber = NULL;
                        // support for default arg values
                        if (marray_size(closure->f->pvalue)) {
                            uint32_t n = 1; // from 1 in order to skip self implicit argument
                            while (n < closure->f->nparams) {
                                if (VALUE_ISA_UNDEFINED(STACK_GET(rwin+n))) SETVALUE(rwin+n, marray_get(closure->f->pvalue, n-1));
                                ++n;
                            }
                        }
                        PUSH_FRAME(closure, &stackstart[rwin], r1, r3);
                        
                        // max depth recursion check
                        if (vm->maxrecursion != 0) {
                            if (func != closure->f) vm->recursioncount = 0;
                            else if (++vm->recursioncount >= vm->maxrecursion) {
                                const char *identifier = (func->identifier) ? func->identifier : "anon";
                                RUNTIME_ERROR("Max recursion depth exceeded for func %s (limit is set to %d)", identifier , vm->maxrecursion);
                                break;
                            }
                        }
                    } break;

                    case EXEC_TYPE_INTERNAL: {
                        // backup register r1 because it can be overwrite to return a closure
                        atomlang_value_t r1copy = STACK_GET(r1);
                        BEGIN_TRUST_USERCODE(vm);
                        bool result = closure->f->internal(vm, &stackstart[rwin], r3, r1);
                        END_TRUST_USERCODE(vm);
                        if (!result) {
                            if (vm->aborted) return false;

                            // check for special getter trick
                            if (VALUE_ISA_CLOSURE(STACK_GET(r1))) {
                                closure = VALUE_AS_CLOSURE(STACK_GET(r1));
                                SETVALUE(r1, r1copy);
                                goto execute_call_function;
                            }

                            // reset current fiber that could be changed during the call
                            fiber = vm->fiber;
                            
                            // check for special fiber error
                            if (fiber == NULL) return true;
                            if (fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                        }
                    } break;

                    case EXEC_TYPE_BRIDGED: {
                        bool result;
                        BEGIN_TRUST_USERCODE(vm);
                        if (VALUE_ISA_CLASS(v)) {
                            DEBUG_ASSERT(delegate->bridge_initinstance, "bridge_initinstance delegate callback is mandatory");
                            atomlang_instance_t *instance = (atomlang_instance_t *)VALUE_AS_OBJECT(stackstart[rwin]);
                            result = delegate->bridge_initinstance(vm, closure->f->xdata, STACK_GET(0), instance, &stackstart[rwin], r3);
                            SETVALUE(r1, VALUE_FROM_OBJECT(instance));
                        } else {
                            DEBUG_ASSERT(delegate->bridge_execute, "bridge_execute delegate callback is mandatory");
                            // starting from version 0.4.4 we pass context object to execute in order to give the opportunity to pass it as self parameter to closures
                            result = delegate->bridge_execute(vm, closure->f->xdata, STACK_GET(0), &stackstart[rwin], r3, r1);
                        }
                        END_TRUST_USERCODE(vm);
                        if (!result && fiber->error) RUNTIME_FIBER_ERROR(fiber->error);
                    } break;

                    case EXEC_TYPE_SPECIAL:
                        RUNTIME_ERROR("Unable to handle a special function in current context");
                        break;
                }
                
                LOAD_FRAME();
                SYNC_STACKTOP(current_fiber, fiber, stacktopdelta);

                DISPATCH();
            }

            CASE_CODE(RET0):
            CASE_CODE(RET): {
                atomlang_value_t result;

                if (op == RET0) {
                    DEBUG_VM("RET0");
                    result = VALUE_FROM_NULL;
                } else {
                    OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                    DEBUG_VM("RET %d", r1);
                    result = STACK_GET(r1);
                }

                // sanity check
                DEBUG_ASSERT(fiber->nframes > 0, "Number of active frames cannot be 0.");

                // POP frame
                --fiber->nframes;

                // close open upvalues (if any)
                atomlang_close_upvalues(fiber, stackstart);

                // check if it was a atomlang_vm_runclosure execution
                if (frame->outloop) {
                    fiber->result = result;
                    return true;
                }

                // retrieve destination register
                uint32_t dest = fiber->frames[fiber->nframes].dest;
                if (fiber->nframes == 0) {
                    if (fiber->caller == NULL) {fiber->result = result; return true;}
                    fiber = fiber->caller;
                    vm->fiber = fiber;
                } else {
                    // recompute stacktop based on last executed frame
					atomlang_callframe_t *lastframe = &fiber->frames[fiber->nframes-1];
                    fiber->stacktop = lastframe->stackstart + FN_COUNTREG(lastframe->closure->f, lastframe->nargs);
                }

                LOAD_FRAME();
                DEBUG_CALL("Resuming", func);
                SETVALUE(dest, result);

                DISPATCH();
            }

            CASE_CODE(HALT): {
                DEBUG_VM("HALT");
                return true;
            }

            CASE_CODE(SWITCH): {
                DEBUG_ASSERT(0, "To be implemented");
                DISPATCH();
            }

            CASE_CODE(MAPNEW): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
                DEBUG_VM("MAPNEW %d %d", r1, n);

                atomlang_map_t *map = atomlang_map_new(vm, n);
                SETVALUE(r1, VALUE_FROM_OBJECT(map));
                DISPATCH();
            }

            CASE_CODE(LISTNEW): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t n);
                DEBUG_VM("LISTNEW %d %d", r1, n);

                atomlang_list_t *list = atomlang_list_new(vm, n);
                SETVALUE(r1, VALUE_FROM_OBJECT(list));
                DISPATCH();
            }

            CASE_CODE(RANGENEW): {
                OPCODE_GET_THREE8bit_ONE2bit(inst, const uint32_t r1, const uint32_t r2, const uint32_t r3, const bool flag);
                DEBUG_VM("RANGENEW %d %d %d (flag %d)", r1, r2, r3, flag);

                // sanity check
                if ((!VALUE_ISA_INT(STACK_GET(r2))) || (!VALUE_ISA_INT(STACK_GET(r3))))
                    RUNTIME_ERROR("Unable to build Range from a non Int value");

                atomlang_range_t *range = atomlang_range_new(vm, VALUE_AS_INT(STACK_GET(r2)), VALUE_AS_INT(STACK_GET(r3)), !flag);
                SETVALUE(r1, VALUE_FROM_OBJECT(range));
                DISPATCH();
            }

            CASE_CODE(SETLIST): {
                OPCODE_GET_TWO8bit_ONE10bit(inst, uint32_t r1, uint32_t r2, const uint32_t r3);
                DEBUG_VM("SETLIST %d %d", r1, r2);

                atomlang_value_t v1 = STACK_GET(r1);
                bool v1_is_map = VALUE_ISA_MAP(v1);

                // r2 == 0 is an optimization, it means that list/map is composed all of literals
                // and can be filled using the constant pool (r3 in this case represents an index inside cpool)
                if (r2 == 0) {
                    atomlang_value_t v2 = atomlang_function_cpool_get(func, r3);
                    if (v1_is_map) {
                        atomlang_map_t *map = VALUE_AS_MAP(v1);
                        atomlang_map_append_map(vm, map, VALUE_AS_MAP(v2));
                    } else {
                        atomlang_list_t *list = VALUE_AS_LIST(v1);
                        atomlang_list_append_list(vm, list, VALUE_AS_LIST(v2));
                    }
                    DISPATCH();
                }

                if (v1_is_map) {
                    atomlang_map_t *map = VALUE_AS_MAP(v1);
                    while (r2) {
                        atomlang_value_t key = STACK_GET(++r1);
                        if (!VALUE_ISA_STRING(key)) RUNTIME_ERROR("Unable to build Map from a non String key");
                        atomlang_value_t value = STACK_GET(++r1);
                        atomlang_hash_insert(map->hash, key, value);
                        --r2;
                    }
                } else {
                    atomlang_list_t *list = VALUE_AS_LIST(v1);
                    while (r2) {
                        marray_push(atomlang_value_t, list->array, STACK_GET(++r1));
                        --r2;
                    }
                }
                DISPATCH();
            }

            CASE_CODE(CLOSURE): {
                OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t r1, const uint32_t index);
                DEBUG_VM("CLOSURE %d %d", r1, index);

                // get function prototype from cpool
                atomlang_value_t v = atomlang_function_cpool_get(func, index);
                if (!VALUE_ISA_FUNCTION(v)) RUNTIME_ERROR("Unable to create a closure from a non function object.");
                atomlang_function_t *f = VALUE_AS_FUNCTION(v);
                
                atomlang_gc_setenabled(vm, false);
                
                // create closure (outside GC)
                atomlang_closure_t *closure = atomlang_closure_new(vm, f);

                // save current context (only if class or instance)
                if ((VALUE_ISA_CLASS(STACK_GET(0))) || (VALUE_ISA_INSTANCE(STACK_GET(0))))
                    closure->context = VALUE_AS_OBJECT(STACK_GET(0)) ;
                
                // loop for each upvalue setup instruction
                for (uint16_t i=0; i<f->nupvalues; ++i) {
                    // code is generated by the compiler so op must be MOVE
                    inst = *ip++;
                    op = (opcode_t)OPCODE_GET_OPCODE(inst);
                    OPCODE_GET_ONE8bit_ONE18bit(inst, const uint32_t p1, const uint32_t p2);

                    // p2 can be 1 (means that upvalue is in the current call frame) or 0 (means that upvalue is in the upvalue list of the caller)
                    if (op != MOVE) RUNTIME_ERROR("Wrong OPCODE in CLOSURE statement");
                    closure->upvalue[i] = (p2) ? atomlang_capture_upvalue (vm, fiber, &stackstart[p1]) : frame->closure->upvalue[p1];
                }
                
                SETVALUE(r1, VALUE_FROM_OBJECT(closure));
                atomlang_gc_setenabled(vm, true);
                DISPATCH();
            }

            CASE_CODE(CLOSE): {
                OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                DEBUG_VM("CLOSE %d", r1);

                // close open upvalues (if any) starting from R1
                atomlang_close_upvalues(fiber, &stackstart[r1]);
                DISPATCH();
            }
            
            CASE_CODE(CHECK): {
                OPCODE_GET_ONE8bit(inst, const uint32_t r1);
                DEBUG_VM("CHECK %d", r1);
                
                atomlang_value_t value = STACK_GET(r1);
                if (VALUE_ISA_INSTANCE(value) && (atomlang_instance_isstruct(VALUE_AS_INSTANCE(value)))) {
                    atomlang_instance_t *instance = atomlang_instance_clone(vm, VALUE_AS_INSTANCE(value));
                    SETVALUE(r1, VALUE_FROM_OBJECT(instance));
                }
                
                DISPATCH();
            }

            CASE_CODE(RESERVED2):
            CASE_CODE(RESERVED3):
            CASE_CODE(RESERVED4):
            CASE_CODE(RESERVED5):
            CASE_CODE(RESERVED6):{
                RUNTIME_ERROR("Opcode not implemented in this VM version.");
                DISPATCH();
            }
        }

        INC_PC;
    };

    return true;
}

atomlang_vm *atomlang_vm_new (atomlang_delegate_t *delegate) {
    atomlang_core_init();
    
    atomlang_vm *vm = mem_alloc(NULL, sizeof(atomlang_vm));
    if (!vm) return NULL;

    // setup default callbacks
    vm->transfer = atomlang_gc_transfer;
    vm->cleanup = atomlang_gc_cleanup;

    // allocate default fiber
    vm->fiber = atomlang_fiber_new(vm, NULL, 0, 0);
    vm->maxccalls = MAX_CCALLS;
    vm->maxrecursion = 0; // default is no limit

    vm->pc = 0;
    vm->delegate = (delegate) ? delegate : &empty_delegate;
    vm->context = atomlang_hash_create(DEFAULT_CONTEXT_SIZE, atomlang_value_hash, atomlang_value_equals, NULL, NULL);

    // garbage collector
    atomlang_gc_setenabled(vm, true);
    atomlang_gc_setvalues(vm, DEFAULT_CG_THRESHOLD, DEFAULT_CG_MINTHRESHOLD, DEFAULT_CG_RATIO);
    vm->memallocated = 0;
    vm->maxmemblock = MAX_MEMORY_BLOCK;
    marray_init(vm->graylist);
    marray_init(vm->gctemp);

    // init base and core
    atomlang_core_register(vm);
    atomlang_cache_setup();

    RESET_STATS(vm);
    return vm;
}

atomlang_vm *atomlang_vm_newmini (void) {
    atomlang_core_init();
    atomlang_vm *vm = mem_alloc(NULL, sizeof(atomlang_vm));
    return vm;
}

void atomlang_vm_free (atomlang_vm *vm) {
    if (!vm) return;

    atomlang_vm_cleanup(vm);
    if (vm->context) atomlang_hash_free(vm->context);
    if (vm->context) atomlang_cache_free();
    marray_destroy(vm->gctemp);
    marray_destroy(vm->graylist);
    mem_free(vm);
}

inline atomlang_value_t atomlang_vm_lookup (atomlang_vm *vm, atomlang_value_t key) {
    atomlang_value_t *value = atomlang_hash_lookup(vm->context, key);
    return (value) ? *value : VALUE_NOT_VALID;
}

inline atomlang_closure_t *atomlang_vm_fastlookup (atomlang_vm *vm, atomlang_class_t *c, int index) {
    #pragma unused(vm)
    return (atomlang_closure_t *)atomlang_class_lookup_closure(c, cache[index]);
}

inline atomlang_value_t atomlang_vm_getvalue (atomlang_vm *vm, const char *key, uint32_t keylen) {
    STATICVALUE_FROM_STRING(k, key, keylen);
    return atomlang_vm_lookup(vm, k);
}

inline void atomlang_vm_setvalue (atomlang_vm *vm, const char *key, atomlang_value_t value) {
    atomlang_hash_insert(vm->context, VALUE_FROM_CSTRING(vm, key), value);
}

double atomlang_vm_time (atomlang_vm *vm) {
    return vm->time;
}

atomlang_value_t atomlang_vm_result (atomlang_vm *vm) {
    atomlang_value_t result = vm->fiber->result;
    vm->fiber->result = VALUE_FROM_NULL;
    return result;
}

atomlang_delegate_t *atomlang_vm_delegate (atomlang_vm *vm) {
    return vm->delegate;
}

atomlang_fiber_t *atomlang_vm_fiber (atomlang_vm* vm) {
    return vm->fiber;
}

void atomlang_vm_setfiber(atomlang_vm* vm, atomlang_fiber_t *fiber) {
    vm->fiber = fiber;
}

void atomlang_vm_seterror (atomlang_vm* vm, const char *format, ...) {
    atomlang_fiber_t *fiber = vm->fiber;

    if (fiber->error) mem_free(fiber->error);
    size_t err_size = 2048;
    fiber->error = mem_alloc(NULL, err_size);

    va_list arg;
    va_start (arg, format);
    vsnprintf(fiber->error, err_size, (format) ? format : "%s", arg);
    va_end (arg);
}

void atomlang_vm_seterror_string (atomlang_vm* vm, const char *s) {
    atomlang_fiber_t *fiber = vm->fiber;
    if (fiber->error) mem_free(fiber->error);
    fiber->error = (char *)string_dup(s);
}

#if ATOMLANG_VM_STATS
static void atomlang_vm_stats (atomlang_vm *vm) {
    printf("\n=======================================================\n");
    printf("                   ATOMLANG VM STATS\n");
    printf("=======================================================\n");
    printf("%12s %10s %10s %20s\n", "OPCODE", "USAGE", "MEAN", "MICROBENCH (ms)");
    printf("=======================================================\n");

    double total = 0.0;
    for (uint32_t i=0; i<ATOMLANG_LATEST_OPCODE; ++i) {
        if (vm->nstat[i]) {
            total += vm->tstat[i];
        }
    }

    for (uint32_t i=0; i<ATOMLANG_LATEST_OPCODE; ++i) {
        if (vm->nstat[i]) {
            uint32_t n = vm->nstat[i];
            double d = vm->tstat[i];
            double m = d / (double)n;
            double p = (d * 100) / total;
            printf("%12s %*d %*.4f %*.4f (%.2f%%)\n", opcode_name((opcode_t)i), 10, n, 11, m, 10, d, p);
        }
    }
    printf("=======================================================\n");
    printf("# Frames reallocs: %d (%d)\n", vm->nfrealloc, vm->fiber->framesalloc);
    printf("# Stack  reallocs: %d (%d)\n", vm->nsrealloc, vm->fiber->stackalloc);
    printf("=======================================================\n");
}
#endif

void atomlang_vm_loadclosure (atomlang_vm *vm, atomlang_closure_t *closure) {
    // closure MUST BE $moduleinit so sanity check here
    if (string_cmp(closure->f->identifier, INITMODULE_NAME) != 0) return;

    // re-use main fiber
    atomlang_fiber_reassign(vm->fiber, closure, 0);

    // execute $moduleinit in order to initialize VM
    atomlang_vm_exec(vm);
}

bool atomlang_vm_runclosure (atomlang_vm *vm, atomlang_closure_t *closure, atomlang_value_t sender, atomlang_value_t params[], uint16_t nparams) {
    if (!vm || !closure || vm->aborted) return false;

    // do not waste cycles on empty functions
    atomlang_function_t *f = closure->f;
    if (f && (f->tag == EXEC_TYPE_NATIVE) && ((!f->bytecode) || (f->ninsts == 0))) return true;

    // current execution fiber
    atomlang_fiber_t     *fiber = vm->fiber;
    atomlang_value_t     *stackstart = NULL;
    uint32_t            rwin = 0;
	uint32_t			stacktopdelta = 0;
    
    // current frame and current instruction pointer
    atomlang_callframe_t *frame;
    uint32_t            *ip;
	
    DEBUG_STACK();

    // self value is default to the context where the closure has been created (or set by the user)
    atomlang_value_t selfvalue;

    // MSVC bug: designated initializer was prematurely evaluated
    if (closure->context) selfvalue = VALUE_FROM_OBJECT(closure->context);
    else selfvalue = sender;
    
    // we need a way to give user the ability to access the sender value from a closure
    
    // if fiber->nframes is not zero it means that this event has been recursively called
    // from somewhere inside the main function so we need to protect and correctly setup
    // the new activation frame
    if (fiber->nframes) {
        // current call frame
        frame = &fiber->frames[fiber->nframes - 1];

        // current top of the stack
        stackstart = frame->stackstart;

        // current instruction pointer
        ip = frame->ip;

        // compute register window
        rwin = FN_COUNTREG(frame->closure->f, frame->nargs);

        // check stack size
        uint32_t _rneed = FN_COUNTREG(f,nparams+1);
		stacktopdelta = (uint32_t)MAXNUM(stackstart + rwin + _rneed - vm->fiber->stacktop, 0);
        if (!atomlang_check_stack(vm, vm->fiber, stacktopdelta, &stackstart)) {
            RUNTIME_ERROR("Infinite loop detected. Current execution must be aborted.");
        }
		
        // setup params (first param is self)
        SETVALUE(rwin, selfvalue);
        for (uint16_t i=0; i<nparams; ++i) {
            SETVALUE(rwin+i+1, params[i]);
        }

        STORE_FRAME();
        PUSH_FRAME(closure, &stackstart[rwin], rwin, nparams+1);
        SETFRAME_OUTLOOP(cframe);
    } else {
        // there are no execution frames when called outside main function
        atomlang_fiber_reassign(vm->fiber, closure, nparams+1);
        stackstart = vm->fiber->stack;
		stacktopdelta = FN_COUNTREG(closure->f, nparams+1);
		
        // setup params (first param is self)
        SETVALUE(rwin, selfvalue);
        for (uint16_t i=0; i<nparams; ++i) {
            SETVALUE(rwin+i+1, params[i]);
        }

        // check if closure uses the special _args instruction
        frame = &fiber->frames[0];
        ip = frame->ip;
        frame->args = (USE_ARGS(closure)) ? atomlang_list_from_array(vm, nparams, &stackstart[rwin]+1) : NULL;
    }

    // f can be native, internal or bridge because this function
    // is called also by convert_value2string
    // for example in Creo:
    // var mData = Data();
    // Console.write("data: " + mData);
    // mData.String is a bridged objc method
    DEBUG_STACK();

    bool result = false;
    switch (f->tag) {
        case EXEC_TYPE_NATIVE:
            ++vm->nccalls;
            if (vm->nccalls > vm->maxccalls) RUNTIME_ERROR("Maximum number of nested C calls reached (%d).", vm->maxccalls);
            result = atomlang_vm_exec(vm);
            --vm->nccalls;
            break;

        case EXEC_TYPE_INTERNAL:
            BEGIN_TRUST_USERCODE(vm);
            result = f->internal(vm, &stackstart[rwin], nparams, ATOMLANG_FIBER_REGISTER);
            END_TRUST_USERCODE(vm);
            break;

        case EXEC_TYPE_BRIDGED:
            if (vm->delegate->bridge_execute) {
                BEGIN_TRUST_USERCODE(vm);
                result = vm->delegate->bridge_execute(vm, f->xdata, selfvalue, &stackstart[rwin], nparams, ATOMLANG_FIBER_REGISTER);
                END_TRUST_USERCODE(vm);
            }
            break;

        case EXEC_TYPE_SPECIAL:
            result = false;
            break;
    }
    
    if (fiber == vm->fiber) {
        // fix pointers ONLY if fiber remains the same
        if (f->tag != EXEC_TYPE_NATIVE) --fiber->nframes;
        fiber->stacktop -= stacktopdelta;
    }
	
    DEBUG_STACK();
    return result;
}

bool atomlang_vm_runmain (atomlang_vm *vm, atomlang_closure_t *closure) {
    // first load closure into vm
    if (closure) atomlang_vm_loadclosure(vm, closure);

    // lookup main function
    atomlang_value_t main = atomlang_vm_getvalue(vm, MAIN_FUNCTION, (uint32_t)strlen(MAIN_FUNCTION));
    if (!VALUE_ISA_CLOSURE(main)) {
        report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "%s", "Unable to find main function.");
        return false;
    }

    // re-use main fiber
    atomlang_closure_t *main_closure = VALUE_AS_CLOSURE(main);
    atomlang_fiber_reassign(vm->fiber, main_closure, 0);

    // execute main function
    RESET_STATS(vm);
    nanotime_t tstart = nanotime();
    bool result = atomlang_vm_exec(vm);
    nanotime_t tend = nanotime();
    vm->time = millitime(tstart, tend);

    PRINT_STATS(vm);
    return result;
}

void atomlang_vm_reset (atomlang_vm *vm) {
    if (!vm || !vm->fiber) return;
    atomlang_fiber_reset(vm->fiber);
}

atomlang_closure_t *atomlang_vm_getclosure (atomlang_vm *vm) {
    if (!vm || !vm->fiber) return NULL;
    if (!vm->fiber->nframes) return NULL;
    if (vm->aborted) return NULL;

    atomlang_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    return frame->closure;
}

void atomlang_vm_setslot (atomlang_vm *vm, atomlang_value_t value, uint32_t index) {
    if (vm->aborted) return;
    if (index == ATOMLANG_FIBER_REGISTER) {
        vm->fiber->result = value;
        return;
    }

    atomlang_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    frame->stackstart[index] = value;
}

atomlang_value_t atomlang_vm_getslot (atomlang_vm *vm, uint32_t index) {
    atomlang_callframe_t *frame = &(vm->fiber->frames[vm->fiber->nframes-1]);
    return frame->stackstart[index];
}

void atomlang_vm_setdata (atomlang_vm *vm, void *data) {
    vm->data = data;
}

void *atomlang_vm_getdata (atomlang_vm *vm) {
    return vm->data;
}

void atomlang_vm_set_callbacks (atomlang_vm *vm, vm_transfer_cb vm_transfer, vm_cleanup_cb vm_cleanup) {
    vm->transfer = vm_transfer;
    vm->cleanup = vm_cleanup;
}

void atomlang_vm_transfer (atomlang_vm *vm, atomlang_object_t *obj) {
    if (vm->transfer) vm->transfer(vm, obj);
}

void atomlang_vm_cleanup (atomlang_vm *vm) {
    if (vm->cleanup) vm->cleanup(vm);
}

void atomlang_vm_filter (atomlang_vm *vm, vm_filter_cb cleanup_filter) {
    vm->filter = cleanup_filter;
}

bool atomlang_vm_ismini (atomlang_vm *vm) {
    return (vm->context == NULL);
}

bool atomlang_vm_isaborted (atomlang_vm *vm) {
    if (!vm) return true;
    return vm->aborted;
}

void atomlang_vm_setaborted (atomlang_vm *vm) {
    vm->aborted = true;
}

char *atomlang_vm_anonymous (atomlang_vm *vm) {
    snprintf(vm->temp, sizeof(vm->temp), "%sanon%d", ATOMLANG_VM_ANONYMOUS_PREFIX, ++vm->nanon);
    return vm->temp;
}

void atomlang_vm_memupdate (atomlang_vm *vm, atomlang_int_t value) {
    vm->memallocated += value;
}

atomlang_int_t atomlang_vm_maxmemblock (atomlang_vm *vm) {
    return vm->maxmemblock;
}


atomlang_value_t atomlang_vm_get (atomlang_vm *vm, const char *key) {
    if (key) {
        if (strcmp(key, ATOMLANG_VM_GCENABLED) == 0) return VALUE_FROM_INT(vm->gcenabled);
        if (strcmp(key, ATOMLANG_VM_GCMINTHRESHOLD) == 0) return VALUE_FROM_INT(vm->gcminthreshold);
        if (strcmp(key, ATOMLANG_VM_GCTHRESHOLD) == 0) return VALUE_FROM_INT(vm->gcthreshold);
        if (strcmp(key, ATOMLANG_VM_GCRATIO) == 0) return VALUE_FROM_FLOAT(vm->gcratio);
        if (strcmp(key, ATOMLANG_VM_MAXCALLS) == 0) return VALUE_FROM_INT(vm->maxccalls);
        if (strcmp(key, ATOMLANG_VM_MAXBLOCK) == 0) return VALUE_FROM_INT(vm->maxmemblock);
        if (strcmp(key, ATOMLANG_VM_MAXRECURSION) == 0) return VALUE_FROM_INT(vm->maxrecursion);
    }
    return VALUE_FROM_NULL;
}

bool atomlang_vm_set (atomlang_vm *vm, const char *key, atomlang_value_t value) {
    if (key) {
        if ((strcmp(key, ATOMLANG_VM_GCENABLED) == 0) && VALUE_ISA_BOOL(value)) {VALUE_AS_BOOL(value) ? ++vm->gcenabled : --vm->gcenabled ; return true;}
        if ((strcmp(key, ATOMLANG_VM_GCMINTHRESHOLD) == 0) && VALUE_ISA_INT(value)) {vm->gcminthreshold = VALUE_AS_INT(value); return true;}
        if ((strcmp(key, ATOMLANG_VM_GCTHRESHOLD) == 0) && VALUE_ISA_INT(value)) {vm->gcthreshold = VALUE_AS_INT(value); return true;}
        if ((strcmp(key, ATOMLANG_VM_GCRATIO) == 0) && VALUE_ISA_FLOAT(value)) {vm->gcratio = VALUE_AS_FLOAT(value); return true;}
        if ((strcmp(key, ATOMLANG_VM_MAXCALLS) == 0) && VALUE_ISA_INT(value)) {vm->maxccalls = (uint32_t)VALUE_AS_INT(value); return true;}
        if ((strcmp(key, ATOMLANG_VM_MAXBLOCK) == 0) && VALUE_ISA_INT(value)) {vm->maxmemblock = (uint32_t)VALUE_AS_INT(value); return true;}
        if ((strcmp(key, ATOMLANG_VM_MAXRECURSION) == 0) && VALUE_ISA_INT(value)) {vm->maxrecursion = (uint32_t)VALUE_AS_INT(value); return true;}
    }
    return false;
}

static bool real_set_superclass (atomlang_vm *vm, atomlang_class_t *c, atomlang_value_t key, const char *supername) {
    // 1. LOOKUP in current stack hierarchy
    STATICVALUE_FROM_STRING(superkey, supername, strlen(supername));
    void_r *stack = (void_r *)vm->data;
    size_t n = marray_size(*stack);
    for (size_t i=0; i<n; i++) {
        atomlang_object_t *obj = marray_get(*stack, i);
        // if object is a CLASS then lookup hash table
        if (OBJECT_ISA_CLASS(obj)) {
            atomlang_class_t *c2 = (atomlang_class_t *)atomlang_class_lookup((atomlang_class_t *)obj, superkey);
            if ((c2) && (OBJECT_ISA_CLASS(c2))) {
                mem_free(supername);
                if (!atomlang_class_setsuper(c, c2)) goto error_max_ivar;
                return true;
            }
        }
        // if object is a FUNCTION then iterate the constant pool
        else if (OBJECT_ISA_FUNCTION(obj)) {
            atomlang_function_t *f = (atomlang_function_t *)obj;
            if (f->tag == EXEC_TYPE_NATIVE) {
                size_t count = marray_size(f->cpool);
                for (size_t j=0; j<count; j++) {
                    atomlang_value_t v = marray_get(f->cpool, j);
                    if (VALUE_ISA_CLASS(v)) {
                        atomlang_class_t *c2 = VALUE_AS_CLASS(v);
                        if (strcmp(c2->identifier, supername) == 0) {
                            mem_free(supername);
                            if (!atomlang_class_setsuper(c, c2)) goto error_max_ivar;
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 2. not found in stack hierarchy so LOOKUP in VM
    atomlang_value_t v = atomlang_vm_lookup(vm, superkey);
    if (VALUE_ISA_CLASS(v)) {
        mem_free(supername);
        if (!atomlang_class_setsuper(c, VALUE_AS_CLASS(v))) goto error_max_ivar;
        return true;
    }

    report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "Unable to find superclass %s of class %s.", supername, VALUE_AS_CSTRING(key));
    mem_free(supername);
    return false;

error_max_ivar:
    report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "Maximum number of allowed ivars (%d) reached for class %s.", MAX_IVARS, VALUE_AS_CSTRING(key));
    return false;
}

static void vm_set_superclass_callback (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused(hashtable)
    atomlang_vm *vm = (atomlang_vm *)data;

    if (VALUE_ISA_FUNCTION(value)) vm_set_superclass(vm, VALUE_AS_OBJECT(value));
    if (!VALUE_ISA_CLASS(value)) return;

    // process class
    atomlang_class_t *c = VALUE_AS_CLASS(value);
    DEBUG_DESERIALIZE("set_superclass_callback for class %s", c->identifier);

    const char *supername = c->xdata;
    c->xdata = NULL;
    if (supername) {
        if (!real_set_superclass(vm, c, key, supername)) return;
    }

    atomlang_hash_iterate(c->htable, vm_set_superclass_callback, vm);
}

static bool vm_set_superclass (atomlang_vm *vm, atomlang_object_t *obj) {
    void_r *stack = (void_r *)vm->data;
    marray_push(void*, *stack, obj);

    if (OBJECT_ISA_CLASS(obj)) {
        // CLASS case: process class and its hash table
        atomlang_class_t *c = (atomlang_class_t *)obj;
        DEBUG_DESERIALIZE("set_superclass for class %s", c->identifier);

        STATICVALUE_FROM_STRING(key, c->identifier, strlen(c->identifier));
        const char *supername = c->xdata;
        c->xdata = NULL;
        if (supername) real_set_superclass(vm, c, key, supername);
        atomlang_hash_iterate(c->htable, vm_set_superclass_callback, vm);

    } else if (OBJECT_ISA_FUNCTION(obj)) {
        // FUNCTION case: scan constant pool and recursively call fixsuper

        atomlang_function_t *f = (atomlang_function_t *)obj;
        if (f->tag == EXEC_TYPE_NATIVE) {
            size_t n = marray_size(f->cpool);
            for (size_t i=0; i<n; i++) {
                atomlang_value_t v = marray_get(f->cpool, i);
                if (VALUE_ISA_FUNCTION(v)) vm_set_superclass(vm, (atomlang_object_t *)VALUE_AS_FUNCTION(v));
                else if (VALUE_ISA_CLASS(v)) vm_set_superclass(vm, (atomlang_object_t *)VALUE_AS_CLASS(v));
            }
        }
    } else {
        report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "%s", "Unable to recognize object type.");
        return false;
    }

    marray_pop(*stack);
    return true;
}

atomlang_closure_t *atomlang_vm_loadfile (atomlang_vm *vm, const char *path) {
    size_t len;
    const char *buffer = file_read(path, &len);
    if (!buffer) return NULL;

    atomlang_closure_t *closure = atomlang_vm_loadbuffer(vm, buffer, len);

    mem_free(buffer);
    return closure;
}

atomlang_closure_t *atomlang_vm_loadbuffer (atomlang_vm *vm, const char *buffer, size_t len) {
    // state buffer for further processing super classes
    void_r objects;
    marray_init(objects);

    // start json parsing
    json_value *json = json_parse (buffer, len);
    if (!json) goto abort_load;
    if (json->type != json_object) goto abort_load;

    // disable GC while deserializing objects
    atomlang_gc_setenabled(vm, false);

    // scan loop
    atomlang_closure_t *closure = NULL;
    uint32_t n = json->u.object.length;
    for (uint32_t i=0; i<n; ++i) {
        json_value *entry = json->u.object.values[i].value;
        if (entry->u.object.length == 0) continue;

        // each entry must be an object
        if (entry->type != json_object) goto abort_load;

        atomlang_object_t *obj = atomlang_object_deserialize(vm, entry);
        if (!obj) goto abort_load;

        // save object for further processing
        marray_push(void*, objects, obj);

        // add a sanity check here: obj can be a function or a class, nothing else

        // transform every function to a closure
        if (OBJECT_ISA_FUNCTION(obj)) {
            atomlang_function_t *f = (atomlang_function_t *)obj;
            const char *identifier = f->identifier;
            atomlang_closure_t *cl = atomlang_closure_new(vm, f);
            if (string_casencmp(identifier, INITMODULE_NAME, strlen(identifier)) == 0) {
                closure = cl;
            } else {
                atomlang_vm_setvalue(vm, identifier, VALUE_FROM_OBJECT(cl));
            }
        }
    }
    json_value_free(json);
    json = NULL;

    // fix superclass(es)
    size_t count = marray_size(objects);
    if (count) {
        void *saved = vm->data;

        // prepare stack to help resolve nested super classes
        void_r stack;
        marray_init(stack);
        vm->data = (void *)&stack;

        // loop of each processed object
        for (size_t i=0; i<count; ++i) {
            atomlang_object_t *obj = (atomlang_object_t *)marray_get(objects, i);
            if (!vm_set_superclass(vm, obj)) goto abort_super;
        }

        marray_destroy(stack);
        marray_destroy(objects);
        vm->data = saved;
    }

    atomlang_gc_setenabled(vm, true);
    return closure;

abort_load:
    report_runtime_error(vm, ATOMLANG_ERROR_RUNTIME, "%s", "Unable to parse JSON executable file.");

abort_super:
    marray_destroy(objects);
    if (json) json_value_free(json);
    atomlang_gc_setenabled(vm, true);
    return NULL;
}

void atomlang_gray_object (atomlang_vm *vm, atomlang_object_t *obj) {
    if (!obj) return;

    // avoid recursion if object has already been visited
    if (obj->gc.isdark) return;
    DEBUG_GC("GRAY %s", atomlang_object_debug(obj, false));

    // object has been reached
    obj->gc.isdark = true;

    marray_push(atomlang_object_t *, vm->graylist, obj);
}

void atomlang_gray_value (atomlang_vm *vm, atomlang_value_t v) {
    if (atomlang_value_isobject(v)) atomlang_gray_object(vm, (atomlang_object_t *)v.p);
}

static void atomlang_gray_hash (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t value, void *data) {
    #pragma unused (hashtable)
    atomlang_vm *vm = (atomlang_vm*)data;
    atomlang_gray_value(vm, key);
    atomlang_gray_value(vm, value);
}

void atomlang_gc_setvalues (atomlang_vm *vm, atomlang_int_t threshold, atomlang_int_t minthreshold, atomlang_float_t ratio) {
    vm->gcminthreshold = (minthreshold) ? minthreshold : DEFAULT_CG_MINTHRESHOLD;
    vm->gcthreshold = (threshold) ? threshold : DEFAULT_CG_THRESHOLD;
    vm->gcratio = (ratio) ? ratio : DEFAULT_CG_RATIO;
    vm->gcthreshold_original = vm->gcthreshold;
}

static void atomlang_gc_transform (atomlang_hash_t *hashtable, atomlang_value_t key, atomlang_value_t *value, void *data) {
    #pragma unused (hashtable)

    atomlang_vm *vm = (atomlang_vm *)data;
    atomlang_object_t *obj = VALUE_AS_OBJECT(*value);

    if (OBJECT_ISA_FUNCTION(obj)) {
        atomlang_function_t *f = (atomlang_function_t *)obj;
        if (f->tag == EXEC_TYPE_SPECIAL) {
            if (f->special[0]) {
                atomlang_gc_transfer(vm, (atomlang_object_t *)f->special[0]);
                f->special[0] = atomlang_closure_new(vm, f->special[0]);
            }
            if (f->special[1]) {
                atomlang_gc_transfer(vm, (atomlang_object_t *)f->special[1]);
                f->special[1] = atomlang_closure_new(vm, f->special[1]);
            }
        } else if (f->tag == EXEC_TYPE_NATIVE) {
            atomlang_vm_initmodule(vm, f);
        }

        bool is_super_function = false;
        if (VALUE_ISA_STRING(key)) {
            atomlang_string_t *s = VALUE_AS_STRING(key);
            // looking for a string that begins with $init AND it is longer than strlen($init)
            is_super_function = ((s->len > 5) && (string_casencmp(s->s, CLASS_INTERNAL_INIT_NAME, 5) == 0));
        }

        atomlang_closure_t *closure = atomlang_closure_new(vm, f);
        *value = VALUE_FROM_OBJECT((atomlang_object_t *)closure);
        if (!is_super_function) atomlang_gc_transfer(vm, obj);
    } else if (OBJECT_ISA_CLASS(obj)) {
        atomlang_class_t *c = (atomlang_class_t *)obj;
        atomlang_vm_loadclass(vm, c);
    } else {
        // should never reach this point
        assert(0);
    }
}

void atomlang_vm_initmodule (atomlang_vm *vm, atomlang_function_t *f) {
    size_t n = marray_size(f->cpool);
    for (size_t i=0; i<n; i++) {
        atomlang_value_t v = marray_get(f->cpool, i);
        if (VALUE_ISA_CLASS(v)) {
            atomlang_class_t *c =  VALUE_AS_CLASS(v);
            atomlang_vm_loadclass(vm, c);
        }
        else if (VALUE_ISA_FUNCTION(v)) {
            atomlang_vm_initmodule(vm, VALUE_AS_FUNCTION(v));
        }
    }
}

static void atomlang_gc_transfer_object (atomlang_vm *vm, atomlang_object_t *obj) {
    DEBUG_GC("GC TRANSFER %s", atomlang_object_debug(obj, false));
    ++vm->gccount;
    obj->gc.next = vm->gchead;
    vm->gchead = obj;
}

static void atomlang_gc_check (atomlang_vm *vm) {
	if (vm->memallocated >= vm->gcthreshold) atomlang_gc_start(vm);
}

static void atomlang_gc_transfer (atomlang_vm *vm, atomlang_object_t *obj) {
    if (vm->gcenabled > 0) {
        #if ATOMLANG_GC_STRESSTEST
        #if 0
        // check if ptr is already in the list
        atomlang_object_t **ptr = &vm->gchead;
        while (*ptr) {
            if (obj == *ptr) {
                printf("Object %s already GC!\n", atomlang_object_debug(obj, false));
                assert(0);
            }
            ptr = &(*ptr)->gc.next;
        }
        #endif
        atomlang_gc_start(vm);
        #else
        if (vm->memallocated >= vm->gcthreshold) atomlang_gc_start(vm);
        #endif
    }

    atomlang_gc_transfer_object(vm, obj);
    atomlang_vm_memupdate(vm, atomlang_object_size(vm, obj));
}

static void atomlang_gc_sweep (atomlang_vm *vm) {
    atomlang_object_t **obj = &vm->gchead;
    while (*obj) {
        if (!(*obj)->gc.isdark) {
            // object is unreachable so remove from the list and free it
            atomlang_object_t *unreached = *obj;
            *obj = unreached->gc.next;
            atomlang_object_free(vm, unreached);
            --vm->gccount;
        } else {
            (*obj)->gc.isdark = false;
            obj = &(*obj)->gc.next;
        }
    }
}

void atomlang_gc_start (atomlang_vm *vm) {
    if (!vm->fiber) return;

    #if ATOMLANG_GC_STATS
    atomlang_int_t membefore = vm->memallocated;
    nanotime_t tstart = nanotime();
    #endif

    // reset memory counter
    vm->memallocated = 0;

    for (uint32_t i=0; i<marray_size(vm->gctemp); ++i) {
        atomlang_object_t *obj = marray_get(vm->gctemp, i);
        atomlang_gray_object(vm, obj);
    }

    atomlang_gray_object(vm, (atomlang_object_t *)vm->fiber);

    atomlang_hash_iterate(vm->context, atomlang_gray_hash, (void*)vm);

    // root has been grayed so recursively scan reachable objects
    while (marray_size(vm->graylist)) {
        atomlang_object_t *obj = marray_pop(vm->graylist);
        atomlang_object_blacken(vm, obj);
    }

    // check unreachable objects (collect white objects)
    atomlang_gc_sweep(vm);

    // dynamically update gcthreshold
    vm->gcthreshold = (atomlang_int_t)(vm->memallocated + (vm->memallocated * vm->gcratio / 100));
    if (vm->gcthreshold < vm->gcminthreshold) vm->gcthreshold = vm->gcminthreshold;
    
    // this line prevents GC to run more than needed
    if (vm->gcthreshold < vm->gcthreshold_original) vm->gcthreshold = vm->gcthreshold_original;
    
    #if ATOMLANG_GC_STATS
    nanotime_t tend = nanotime();
    double gctime = millitime(tstart, tend);
    printf("GC %lu before, %lu after (%lu collected - %lu objects), next at %lu. Took %.2fms.\n",
           (unsigned long)membefore,
           (unsigned long)vm->memallocated,
           (membefore > vm->memallocated) ? (unsigned long)(membefore - vm->memallocated) : 0,
           (unsigned long)vm->gccount,
           (unsigned long)vm->gcthreshold,
           gctime);
    #endif
}

static void atomlang_gc_cleanup (atomlang_vm *vm) {
    if (!vm->gchead) return;

    if (vm->filter) {
        // filter case
        vm_filter_cb filter = vm->filter;

        // we need to remove freed objects from the linked list
        // so we need a pointer to the previous object in order
        // to be able to point it to obj's next ptr
        //
        //         +--------+      +--------+      +--------+
        //     --> |  prev  |  --> |   obj  |  --> |  next  |  -->
        //         +--------+      +--------+      +--------+
        //             |                               ^
        //             |                               |
        //             +-------------------------------+

        atomlang_object_t *obj = vm->gchead;
        atomlang_object_t *prev = NULL;

        while (obj) {
            if (!filter(obj)) {
                prev = obj;
                obj = obj->gc.next;
                continue;
            }

            // REMOVE obj from the linked list
            atomlang_object_t *next = obj->gc.next;
            if (!prev) vm->gchead = next;
            else prev->gc.next = next;

            if (OBJECT_ISA_INSTANCE(obj)) atomlang_instance_deinit(vm, (atomlang_instance_t *)obj);
            atomlang_object_free(vm, obj);
            --vm->gccount;
            obj = next;
        }
        return;
    }
    
    // no filter so free all GC objects
    // step 1:
    // execute all deinit methods (if any)
    atomlang_object_t *obj = vm->gchead;
    while (obj) {
        atomlang_object_t *next = obj->gc.next;
        if (OBJECT_ISA_INSTANCE(obj)) atomlang_instance_deinit(vm, (atomlang_instance_t *)obj);
        obj = next;
    }
    
    // step2:
    // free all memory
    obj = vm->gchead;
    while (obj) {
        atomlang_object_t *next = obj->gc.next;
        atomlang_object_free(vm, obj);
        --vm->gccount;
        obj = next;
    }
    vm->gchead = NULL;

    // free all temporary allocated objects
    while (marray_size(vm->gctemp)) {
        atomlang_object_t *tobj = marray_pop(vm->gctemp);
        atomlang_object_free(vm, tobj);
    }
}

void atomlang_gc_setenabled (atomlang_vm *vm, bool enabled) {
    if (!vm) return;
    (enabled) ? ++vm->gcenabled : --vm->gcenabled ;
    
	if (vm->gcenabled > 0 && (!vm->delegate->disable_gccheck_1)) atomlang_gc_check(vm);
}

void atomlang_gc_temppush (atomlang_vm *vm, atomlang_object_t *obj) {
    marray_push(atomlang_object_t *, vm->gctemp, obj);
}

void atomlang_gc_temppop (atomlang_vm *vm) {
    marray_pop(vm->gctemp);
}

void atomlang_gc_tempnull (atomlang_vm *vm, atomlang_object_t *obj) {
    for (uint32_t i=0; i<marray_size(vm->gctemp); ++i) {
        atomlang_object_t *tobj = marray_get(vm->gctemp, i);
        if (tobj == obj) {
            marray_setnull(vm->gctemp, i);
            break;
        }
    }
}