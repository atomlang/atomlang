#define DEFAULT_OUTPUT "atomlang.g"

typedef struct {
    bool                processed;
    bool                is_fuzzy;
    
    uint32_t            ncount;
    uint32_t            nsuccess;
    uint32_t            nfailure;
    
    error_type_t        expected_error;
    atomlang_value_t     expected_value;
    int32_t             expected_row;
    int32_t             expected_col;
} unittest_data;


static const char *input_file = NULL;
static const char *output_file = DEFAULT_OUTPUT;
static const char *unittest_file = NULL;
static const char *test_folder_path = NULL;
static bool quiet_flag = false;

static void unittest_callback (atomlang_vm *vm, error_type_t error_type, const char *description, const char *notes, atomlang_value_t value, int32_t row, int32_t col, void *xdata) {
    (void) vm;
    unittest_data *data = (unittest_data *)xdata;
    data->expected_error = error_type;
    data->expected_value = value;
    data->expected_row = row;
    data->expected_col = col;
    
    if (notes) printf("\tNOTE: %s\n", notes);
    printf("\t%s\n", description);
}

static void unittest_error (atomlang_vm *vm, error_type_t error_type, const char *message, error_desc_t error_desc, void *xdata) {
    (void) vm;
    
    unittest_data *data = (unittest_data *)xdata;
    if (data->processed == true) return; // ignore 2nd error
    data->processed = true;
    
    const char *type = "NONE";
    if (error_type == ATOMLANG_ERROR_SYNTAX) type = "SYNTAX";
    else if (error_type == ATOMLANG_ERROR_SEMANTIC) type = "SEMANTIC";
    else if (error_type == ATOMLANG_ERROR_RUNTIME) type = "RUNTIME";
    else if (error_type == ATOMLANG_WARNING) type = "WARNING";
    
    if (error_type == ATOMLANG_ERROR_RUNTIME) printf("\tRUNTIME ERROR: ");
    else printf("\t%s ERROR on %d (%d,%d): ", type, error_desc.fileid, error_desc.lineno, error_desc.colno);
    printf("%s\n", message);
    
    bool same_error = (data->expected_error == error_type);
    bool same_row = (data->expected_row != -1) ? (data->expected_row == error_desc.lineno) : true;
    bool same_col = (data->expected_col != -1) ? (data->expected_col == error_desc.colno) : true;
    
    if (data->is_fuzzy) {
        ++data->nsuccess;
        printf("\tSUCCESS\n");
        return;
    }
    
    if (same_error && same_row && same_col) {
        ++data->nsuccess;
        printf("\tSUCCESS\n");
    } else {
        ++data->nfailure;
        printf("\tFAILURE\n");
    }
}

