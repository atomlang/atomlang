#ifndef __ATOMLANG_OPCODES__
#define __ATOMLANG_OPCODES__

typedef enum {

                          
            RET0 = 0,     
            HALT,         
            NOP,          
            RET,          
            CALL,         
            LOAD,         
            LOADS,        
            LOADAT,       
            LOADK,        
            LOADG,        
            LOADI,        
            LOADU,        
            MOVE,         
            STORE,        
            STOREAT,      
            STOREG,       
            STOREU,       
            JUMP,         
            JUMPF,        
            SWITCH,       
            ADD,          
            SUB,          
            DIV,          
            MUL,          
            REM,          
            AND,          
            OR,            
            LT,            
            GT,            
            EQ,            
            LEQ,           
            GEQ,           
            NEQ,           
            EQQ,           
            NEQQ,          
            ISA,           
            MATCH,         
            NEG,           
            NOT,           
            LSHIFT,        
            RSHIFT,        
            BAND,          
            BOR,           
            BXOR,          
            BNOT,          
            MAPNEW,         
            LISTNEW,        
            RANGENEW,       
            SETLIST,        
            CLOSURE,        
            CLOSE,        
            CHECK,        
            RESERVED2,    
            RESERVED3,    
            RESERVED4,    
            RESERVED5,    
            RESERVED6     
} opcode_t;

#define ATOMLANG_LATEST_OPCODE           RESERVED6    

typedef enum {
    ATOMLANG_NOTFOUND_INDEX              = 0,
    ATOMLANG_ADD_INDEX,
    ATOMLANG_SUB_INDEX,
    ATOMLANG_DIV_INDEX,
    ATOMLANG_MUL_INDEX,
    ATOMLANG_REM_INDEX,
    ATOMLANG_AND_INDEX,
    ATOMLANG_OR_INDEX,
    ATOMLANG_CMP_INDEX,
    ATOMLANG_EQQ_INDEX,
    ATOMLANG_IS_INDEX,
    ATOMLANG_MATCH_INDEX,
    ATOMLANG_NEG_INDEX,
    ATOMLANG_NOT_INDEX,
    ATOMLANG_LSHIFT_INDEX,
    ATOMLANG_RSHIFT_INDEX,
    ATOMLANG_BAND_INDEX,
    ATOMLANG_BOR_INDEX,
    ATOMLANG_BXOR_INDEX,
    ATOMLANG_BNOT_INDEX,
    ATOMLANG_LOAD_INDEX,
    ATOMLANG_LOADS_INDEX,
    ATOMLANG_LOADAT_INDEX,
    ATOMLANG_STORE_INDEX,
    ATOMLANG_STOREAT_INDEX,
    ATOMLANG_INT_INDEX,
    ATOMLANG_FLOAT_INDEX,
    ATOMLANG_BOOL_INDEX,
    ATOMLANG_STRING_INDEX,
    ATOMLANG_EXEC_INDEX,
    ATOMLANG_VTABLE_SIZE                 
} ATOMLANG_VTABLE_INDEX;

#define ATOMLANG_OPERATOR_ADD_NAME       "+"
#define ATOMLANG_OPERATOR_SUB_NAME       "-"
#define ATOMLANG_OPERATOR_DIV_NAME       "/"
#define ATOMLANG_OPERATOR_MUL_NAME       "*"
#define ATOMLANG_OPERATOR_REM_NAME       "%"
#define ATOMLANG_OPERATOR_AND_NAME       "&&"
#define ATOMLANG_OPERATOR_OR_NAME        "||"
#define ATOMLANG_OPERATOR_CMP_NAME       "=="
#define ATOMLANG_OPERATOR_EQQ_NAME       "==="
#define ATOMLANG_OPERATOR_NEQQ_NAME      "!=="
#define ATOMLANG_OPERATOR_IS_NAME        "is"
#define ATOMLANG_OPERATOR_MATCH_NAME     "=~"
#define ATOMLANG_OPERATOR_NEG_NAME       "neg"
#define ATOMLANG_OPERATOR_NOT_NAME        "!"
#define ATOMLANG_OPERATOR_LSHIFT_NAME    "<<"
#define ATOMLANG_OPERATOR_RSHIFT_NAME    ">>"
#define ATOMLANG_OPERATOR_BAND_NAME      "&"
#define ATOMLANG_OPERATOR_BOR_NAME       "|"
#define ATOMLANG_OPERATOR_BXOR_NAME      "^"
#define ATOMLANG_OPERATOR_BNOT_NAME      "~"
#define ATOMLANG_INTERNAL_LOAD_NAME      "load"
#define ATOMLANG_INTERNAL_LOADS_NAME     "loads"
#define ATOMLANG_INTERNAL_STORE_NAME     "store"
#define ATOMLANG_INTERNAL_LOADAT_NAME    "loadat"
#define ATOMLANG_INTERNAL_STOREAT_NAME   "storeat"
#define ATOMLANG_INTERNAL_NOTFOUND_NAME  "notfound"
#define ATOMLANG_INTERNAL_EXEC_NAME      "exec"
#define ATOMLANG_INTERNAL_LOOP_NAME      "loop"

#define ATOMLANG_CLASS_INT_NAME          "Int"
#define ATOMLANG_CLASS_FLOAT_NAME        "Float"
#define ATOMLANG_CLASS_BOOL_NAME         "Bool"
#define ATOMLANG_CLASS_STRING_NAME       "String"
#define ATOMLANG_CLASS_OBJECT_NAME       "Object"
#define ATOMLANG_CLASS_CLASS_NAME        "Class"
#define ATOMLANG_CLASS_NULL_NAME         "Null"
#define ATOMLANG_CLASS_FUNCTION_NAME     "Func"
#define ATOMLANG_CLASS_FIBER_NAME        "Fiber"
#define ATOMLANG_CLASS_INSTANCE_NAME     "Instance"
#define ATOMLANG_CLASS_CLOSURE_NAME      "Closure"
#define ATOMLANG_CLASS_LIST_NAME         "List"
#define ATOMLANG_CLASS_MAP_NAME          "Map"
#define ATOMLANG_CLASS_RANGE_NAME        "Range"
#define ATOMLANG_CLASS_UPVALUE_NAME      "Upvalue"

#define ATOMLANG_CLASS_SYSTEM_NAME       "System"
#define ATOMLANG_SYSTEM_PRINT_NAME       "print"
#define ATOMLANG_SYSTEM_PUT_NAME         "put"
#define ATOMLANG_SYSTEM_INPUT_NAME       "input"
#define ATOMLANG_SYSTEM_NANOTIME_NAME    "nanotime"

#define ATOMLANG_TOCLASS_NAME            "toClass"
#define ATOMLANG_TOSTRING_NAME           "toString"
#define ATOMLANG_TOINT_NAME              "toInt"
#define ATOMLANG_TOFLOAT_NAME            "toFloat"
#define ATOMLANG_TOBOOL_NAME             "toBool"

#endif