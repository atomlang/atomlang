#ifndef __ATOMLANG_DELEGATE__
#define __ATOMLANG_DELEGATE__

#include <stdint.h>
#include "atomlang_value.h"

typedef enum {
    ATOMLANG_ERROR_NONE = 0,
    ATOMLANG_ERROR_SYNTAX,
    ATOMLANG_ERROR_SEMANTIC,
    ATOMLANG_ERROR_RUNTIME,
    ATOMLANG_ERROR_IO,
    ATOMLANG_WARNING,
} error_type_t;

typedef struct {
    uint32_t        lineno;
    uint32_t        colno;
    uint32_t        fileid;
    uint32_t        offset;
} error_desc_t;

#define ERROR_DESC_NONE     (error_desc_t){0,0,0,0}

typedef void                (*atomlang_error_callback) (atomlang_vm *vm, error_type_t error_type, const char *description, error_desc_t error_desc, void *xdata);
typedef void                (*atomlang_log_callback)    (atomlang_vm *vm, const char *message, void *xdata);
typedef void                (*atomlang_log_clear) (atomlang_vm *vm, void *xdata);
typedef void                (*atomlang_unittest_callback) (atomlang_vm *vm, error_type_t error_type, const char *desc, const char *note, atomlang_value_t value, int32_t row, int32_t col, void *xdata);
typedef const char*         (*atomlang_filename_callback) (uint32_t fileid, void *xdata);
typedef const char*         (*atomlang_loadfile_callback) (const char *file, size_t *size, uint32_t *fileid, void *xdata, bool *is_static);
typedef const char**        (*atomlang_optclass_callback) (void *xdata);
typedef void                (*atomlang_parser_callback) (void *token, void *xdata);
typedef const char*         (*atomlang_precode_callback) (void *xdata);
typedef void                (*atomlang_type_callback) (void *token, const char *type, void *xdata);

typedef void                (*atomlang_bridge_blacken) (atomlang_vm *vm, void *xdata);
typedef void*               (*atomlang_bridge_clone) (atomlang_vm *vm, void *xdata);
typedef bool                (*atomlang_bridge_equals) (atomlang_vm *vm, void *obj1, void *obj2);
typedef bool                (*atomlang_bridge_execute) (atomlang_vm *vm, void *xdata, atomlang_value_t ctx, atomlang_value_t args[], int16_t nargs, uint32_t vindex);
typedef void                (*atomlang_bridge_free) (atomlang_vm *vm, atomlang_object_t *obj);
typedef bool                (*atomlang_bridge_getundef) (atomlang_vm *vm, void *xdata, atomlang_value_t target, const char *key, uint32_t vindex);
typedef bool                (*atomlang_bridge_getvalue) (atomlang_vm *vm, void *xdata, atomlang_value_t target, const char *key, uint32_t vindex);
typedef bool                (*atomlang_bridge_initinstance) (atomlang_vm *vm, void *xdata, atomlang_value_t ctx, atomlang_instance_t *instance, atomlang_value_t args[], int16_t nargs);
typedef bool                (*atomlang_bridge_setvalue) (atomlang_vm *vm, void *xdata, atomlang_value_t target, const char *key, atomlang_value_t value);
typedef bool                (*atomlang_bridge_setundef) (atomlang_vm *vm, void *xdata, atomlang_value_t target, const char *key, atomlang_value_t value);
typedef uint32_t            (*atomlang_bridge_size) (atomlang_vm *vm, atomlang_object_t *obj);
typedef const char*         (*atomlang_bridge_string) (atomlang_vm *vm, void *xdata, uint32_t *len);

typedef struct {
    void                        *xdata;                 
    bool                        report_null_errors;     
    bool                        disable_gccheck_1;      
    
    atomlang_log_callback        log_callback;           
    atomlang_log_clear           log_clear;              
    atomlang_error_callback      error_callback;         
    atomlang_unittest_callback   unittest_callback;      
    atomlang_parser_callback     parser_callback;        
    atomlang_type_callback       type_callback;          
    atomlang_precode_callback    precode_callback;       
    atomlang_loadfile_callback   loadfile_callback;      
    atomlang_filename_callback   filename_callback;      
    atomlang_optclass_callback   optional_classes;       

    atomlang_bridge_initinstance bridge_initinstance;   
    atomlang_bridge_setvalue     bridge_setvalue;       
    atomlang_bridge_getvalue     bridge_getvalue;       
    atomlang_bridge_setundef     bridge_setundef;       
    atomlang_bridge_getundef     bridge_getundef;       
    atomlang_bridge_execute      bridge_execute;        
    atomlang_bridge_blacken      bridge_blacken;        
    atomlang_bridge_string       bridge_string;         
    atomlang_bridge_equals       bridge_equals;         
    atomlang_bridge_clone        bridge_clone;          
    atomlang_bridge_size         bridge_size;           
    atomlang_bridge_free         bridge_free;         
} atomlang_delegate_t;

#endif