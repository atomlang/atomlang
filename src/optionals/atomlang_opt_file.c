#include "atomlang_vm.h"
#include "atomlang_core.h"
#include "atomlang_hash.h"
#include "atomlang_utils.h"
#include "atomlang_macros.h"
#include "atomlang_vmmacros.h"
#include "atomlang_opt_file.h"

static atomlang_class_t              *atomlang_class_file = NULL;
static uint32_t                     refcount = 0;

typedef struct {
    atomlang_class_t         *isa;   
    atomlang_gc_t            gc;     
    FILE                    *file;  
} atomlang_file_t;

#define VALUE_AS_FILE(x)    ((atomlang_file_t *)VALUE_AS_OBJECT(x))

static uint32_t atomlang_ifile_free (atomlang_vm *vm, atomlang_object_t *object) {
    UNUSED_PARAM(vm);
    
    atomlang_file_t *instance = (atomlang_file_t *)object;
    DEBUG_FREE("FREE %s", atomlang_object_debug(object, true));
    if (instance->file) fclose(instance->file);
    mem_free((void *)object);
    
    return 0;
}

static atomlang_file_t *atomlang_ifile_new (atomlang_vm *vm, FILE *f) {
    if (!f) return NULL;
    
    atomlang_file_t *instance = (atomlang_file_t *)mem_alloc(NULL, sizeof(atomlang_file_t));
    if (!instance) return NULL;
    
    instance->isa = atomlang_class_file;
    instance->gc.free = atomlang_ifile_free;
    instance->file = f;

    if (vm) atomlang_vm_transfer(vm, (atomlang_object_t*) instance);
    return instance;
}

static bool internal_file_size (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    int64_t size = file_size((const char *)path);
    RETURN_VALUE(VALUE_FROM_INT(size), rindex);
}

static bool internal_file_exists (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool exists = file_exists((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(exists), rindex);
}

static bool internal_file_delete (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = file_delete((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_read (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    size_t len = 0;
    char *buffer = file_read((const char *)path, &len);
    if (!buffer) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    atomlang_value_t string = VALUE_FROM_STRING(vm, buffer, (uint32_t)len);
    RETURN_VALUE(string, rindex);
}

static bool internal_file_write (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3 && !VALUE_ISA_STRING(args[1]) && !VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("A path parameter of type String and a String parameter are required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    char *buffer = VALUE_AS_STRING(args[2])->s;
    size_t len = (size_t)VALUE_AS_STRING(args[2])->len;
    bool result = file_write((const char *)path, buffer, len);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_buildpath (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3 && !VALUE_ISA_STRING(args[1]) && !VALUE_ISA_STRING(args[2])) {
        RETURN_ERROR("A file and path parameters of type String are required.");
    }
    
    char *file = VALUE_AS_STRING(args[1])->s;
    char *path = VALUE_AS_STRING(args[2])->s;
    char *result = file_buildpath((const char *)file, (const char *)path);
    
    if (!result) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    atomlang_value_t string = VALUE_FROM_STRING(vm, result, (uint32_t)strlen(result));
    RETURN_VALUE(string, rindex);
}

static bool internal_file_is_directory (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = is_directory((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static bool internal_file_directory_create (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 2 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    
    char *path = VALUE_AS_STRING(args[1])->s;
    bool result = directory_create((const char *)path);
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static void scan_directory (atomlang_vm *vm, char *path, bool recursive, atomlang_closure_t *closure, atomlang_int_t *n, bool isdir) {
    DIRREF dir = directory_init ((const char *)path);
    if (!dir) return;
    
    if (isdir) {
        char *name = file_name_frompath(path);
        atomlang_value_t p1 = VALUE_FROM_CSTRING(vm, name);
        mem_free(name);
        atomlang_value_t p2 = VALUE_FROM_CSTRING(vm, path);
        atomlang_value_t p3 = VALUE_FROM_BOOL(true);
        atomlang_value_t params[] = {p1, p2, p3};
        
        atomlang_vm_runclosure(vm, closure, VALUE_FROM_NULL, params, 3);
        if (n) *n = *n + 1;
    }
    
    #ifdef WIN32
    char buffer[MAX_PATH];
    #else
    char *buffer = NULL;
    #endif

    const char *target_file;
    while ((target_file = directory_read_extend(dir, buffer))) {
        char *full_path = file_buildpath(target_file, path);
        if (recursive && (is_directory(full_path))) {
            scan_directory(vm, full_path, recursive, closure, n, true);
            continue;
        }
        
        atomlang_value_t p1 = VALUE_FROM_CSTRING(vm, target_file);
        atomlang_value_t p2 = VALUE_FROM_CSTRING(vm, full_path);
        atomlang_value_t p3 = VALUE_FROM_BOOL(false);
        atomlang_value_t params[] = {p1, p2, p3};
        mem_free(full_path);
        
        atomlang_vm_runclosure(vm, closure, VALUE_FROM_NULL, params, 3);
        if (n) *n = *n + 1;
    }
}

static bool internal_file_directory_scan (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    char *path = NULL;
    
    if (nargs < 3) {
        RETURN_ERROR("A path and a closure parameter are required.");
    }
    
    if (!VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    } else {
        path = VALUE_AS_STRING(args[1])->s;
    }
    
    int nindex = 2;
    bool recursive = true;
    if (VALUE_ISA_BOOL(args[2])) {
        recursive = VALUE_AS_BOOL(args[2]);
        nindex = 3;
    }
    
    if (!VALUE_ISA_CLOSURE(args[nindex])) {
        RETURN_ERROR("A closure parameter is required.");
    }
    
    atomlang_closure_t *closure = VALUE_AS_CLOSURE(args[nindex]);
    atomlang_int_t n = 0;
    
    scan_directory(vm, path, recursive, closure, &n, false);
    
    RETURN_VALUE(VALUE_FROM_INT(n), rindex);
}

static bool internal_file_open (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    
    if (nargs > 1 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A path parameter of type String is required.");
    }
    char *path = VALUE_AS_STRING(args[1])->s;
    
    char *mode = "r";
    if (nargs > 2 && VALUE_ISA_STRING(args[2])) {
        mode = VALUE_AS_STRING(args[2])->s;
    }
    
    FILE *file = fopen(path, mode);
    if (file == NULL) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    atomlang_file_t *instance = atomlang_ifile_new(vm, file);
    if (instance == NULL) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }
    
    RETURN_VALUE(VALUE_FROM_OBJECT((atomlang_object_t*) instance), rindex);
}

static bool internal_file_iread (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    
    if (nargs < 1 && (!VALUE_ISA_INT(args[1]) && !VALUE_ISA_STRING(args[1]))) {
        RETURN_ERROR("A parameter of type Int or String is required.");
    }
    
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    atomlang_int_t n = 256;
    atomlang_string_t *str = NULL;
    size_t nread = 0;
    
    if (VALUE_ISA_INT(args[1])) n = VALUE_AS_INT(args[1]);
    else str = VALUE_AS_STRING(args[1]);
    
    char *buffer = (char *)mem_alloc(NULL, n);
    if (!buffer) {
        RETURN_ERROR("Not enought memory to allocate required buffer.");
    }
    
    if (str == NULL) {
        nread = fread(buffer, (size_t)n, 1, instance->file);
    } else {
        
        int delimiter = (int)str->s[0];
        char *ptr, *eptr;
        
        for (ptr = buffer, eptr = buffer + n; ++nread;) {
            int c = fgetc(instance->file);
            if ((c == -1) || feof(instance->file)) break;
            
            *ptr++ = c;
            if (c == delimiter) break;
            
            if (ptr + 2 >= eptr) {
                char *nbuf;
                size_t nbufsiz = n * 2;
                ssize_t d = ptr - buffer;
                if ((nbuf = mem_realloc(NULL, buffer, nbufsiz)) == NULL) break;
                
                buffer = nbuf;
                n = nbufsiz;
                eptr = nbuf + nbufsiz;
                ptr = nbuf + d;
            }
        }
        buffer[nread] = 0;
    }
    
    atomlang_string_t *result = atomlang_string_new(vm, buffer, (uint32_t)nread, (uint32_t)n);
    RETURN_VALUE(VALUE_FROM_OBJECT(result), rindex);
}

static bool internal_file_iwrite (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {

    if (nargs < 1 && !VALUE_ISA_STRING(args[1])) {
        RETURN_ERROR("A parameter of type String is required.");
    }
    
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    atomlang_string_t *data = VALUE_AS_STRING(args[1]);
    
    size_t nwritten = fwrite(data->s, data->len, 1, instance->file);
    RETURN_VALUE(VALUE_FROM_INT(nwritten), rindex);
}

static bool internal_file_iseek (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    
    if (nargs != 3 && !VALUE_ISA_INT(args[1]) && !VALUE_ISA_INT(args[2])) {
        RETURN_ERROR("An offset parameter of type Int and a whence parameter of type Int are required.");
    }
    
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    atomlang_int_t offset = VALUE_AS_INT(args[1]);
    atomlang_int_t whence = VALUE_AS_INT(args[2]);
    
    int result = fseek(instance->file, (long)offset, (int)whence);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_ierror (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    
    UNUSED_PARAM(nargs);
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = ferror(instance->file);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_iflush (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    
    UNUSED_PARAM(nargs);
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = fflush(instance->file);
    RETURN_VALUE(VALUE_FROM_INT(result), rindex);
}

static bool internal_file_ieof (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    UNUSED_PARAM(nargs);
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    int result = feof(instance->file);
    RETURN_VALUE(VALUE_FROM_BOOL(result != 0), rindex);
}

static bool internal_file_iclose (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    UNUSED_PARAM(nargs);
    
    atomlang_file_t *instance = VALUE_AS_FILE(args[0]);
    
    bool result = false;
    if (instance->file) {
        fclose(instance->file);
        instance->file = NULL;
        result = true;
    }
    
    RETURN_VALUE(VALUE_FROM_BOOL(result), rindex);
}

static void create_optional_class (void) {
    atomlang_class_file = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_FILE_NAME, NULL, 0, 0);
    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_file);
    
    atomlang_class_bind(meta, "size", NEW_CLOSURE_VALUE(internal_file_size));
    atomlang_class_bind(meta, "exists", NEW_CLOSURE_VALUE(internal_file_exists));
    atomlang_class_bind(meta, "delete", NEW_CLOSURE_VALUE(internal_file_delete));
    atomlang_class_bind(meta, "read", NEW_CLOSURE_VALUE(internal_file_read));
    atomlang_class_bind(meta, "write", NEW_CLOSURE_VALUE(internal_file_write));
    atomlang_class_bind(meta, "buildpath", NEW_CLOSURE_VALUE(internal_file_buildpath));
    atomlang_class_bind(meta, "is_directory", NEW_CLOSURE_VALUE(internal_file_is_directory));
    atomlang_class_bind(meta, "directory_create", NEW_CLOSURE_VALUE(internal_file_directory_create));
    atomlang_class_bind(meta, "directory_scan", NEW_CLOSURE_VALUE(internal_file_directory_scan));
    
    atomlang_class_bind(meta, "open", NEW_CLOSURE_VALUE(internal_file_open));
    atomlang_class_bind(atomlang_class_file, "read", NEW_CLOSURE_VALUE(internal_file_iread));
    atomlang_class_bind(atomlang_class_file, "write", NEW_CLOSURE_VALUE(internal_file_iwrite));
    atomlang_class_bind(atomlang_class_file, "seek", NEW_CLOSURE_VALUE(internal_file_iseek));
    atomlang_class_bind(atomlang_class_file, "eof", NEW_CLOSURE_VALUE(internal_file_ieof));
    atomlang_class_bind(atomlang_class_file, "error", NEW_CLOSURE_VALUE(internal_file_ierror));
    atomlang_class_bind(atomlang_class_file, "flush", NEW_CLOSURE_VALUE(internal_file_iflush));
    atomlang_class_bind(atomlang_class_file, "close", NEW_CLOSURE_VALUE(internal_file_iclose));

    SETMETA_INITED(atomlang_class_file);
}

bool atomlang_isfile_class (atomlang_class_t *c) {
    return (c == atomlang_class_file);
}

const char *atomlang_file_name (void) {
    return ATOMLANG_CLASS_FILE_NAME;
}

void atomlang_file_register (atomlang_vm *vm) {
    if (!atomlang_class_file) create_optional_class();
    ++refcount;

    if (!vm || atomlang_vm_ismini(vm)) return;
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_FILE_NAME, VALUE_FROM_OBJECT(atomlang_class_file));
}

void atomlang_file_free (void) {
    if (!atomlang_class_file) return;
    if (--refcount) return;

    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_file);
    atomlang_class_free_core(NULL, meta);
    atomlang_class_free_core(NULL, atomlang_class_file);

    atomlang_class_file = NULL;
}