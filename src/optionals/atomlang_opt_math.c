#include <math.h>
#include <time.h>
#include <inttypes.h>
#include "atomlang_vm.h"
#include "atomlang_core.h"
#include "atomlang_hash.h"
#include "atomlang_utils.h"
#include "atomlang_macros.h"
#include "atomlang_vmmacros.h"
#include "atomlang_opt_math.h"

#if ATOMLANG_ENABLE_DOUBLE
#define SIN                         sin
#define COS                         cos
#define TAN                         tan
#define ASIN                        asin
#define ACOS                        acos
#define ATAN                        atan
#define ATAN2                       atan2
#define CEIL                        ceil
#define FLOOR                       floor
#define ROUND                       round
#define LOG                         log
#define POW                         pow
#define EXP                         exp
#define SQRT                        sqrt
#define CBRT                        cbrt
#else
#define SIN                         sinf
#define COS                         cosf
#define TAN                         tanf
#define ASIN                        asinf
#define ACOS                        acosf
#define ATAN                        atanf
#define ATAN2                       atan2
#define CEIL                        ceilf
#define FLOOR                       floorf
#define ROUND                       roundf
#define LOG                         logf
#define POW                         powf
#define EXP                         expf
#define SQRT                        sqrtf
#define CBRT                        cbrtf
#endif

static atomlang_class_t              *atomlang_class_math = NULL;
static uint32_t                     refcount = 0;

static bool math_abs (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_int_t computed_value;
        #if ATOMLANG_ENABLE_INT64
        computed_value = (atomlang_int_t)llabs((long long)value.n);
        #else
        computed_value = (atomlang_int_t)labs((long)value.n);
        #endif
        RETURN_VALUE(VALUE_FROM_INT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value;
        #if ATOMLANG_ENABLE_DOUBLE
        computed_value = (atomlang_float_t)fabs((double)value.f);
        #else
        computed_value = (atomlang_float_t)fabsf((float)value.f);
        #endif
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_acos (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ACOS((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ACOS((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_asin (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ASIN((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ASIN((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_atan (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ATAN((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ATAN((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_atan2 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)

    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    atomlang_value_t value = GET_VALUE(1);
    atomlang_value_t value2 = GET_VALUE(2);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    atomlang_float_t n2;
    if (VALUE_ISA_INT(value2)) n2 = (atomlang_float_t)value2.n;
    else if (VALUE_ISA_FLOAT(value2)) n2 = (atomlang_float_t)value2.f;
    else RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ATAN2((atomlang_float_t)value.n, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ATAN2((atomlang_float_t)value.f, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_cbrt (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)CBRT((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)CBRT((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_xrt (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    atomlang_value_t base = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);

    if (VALUE_ISA_NULL(value) || VALUE_ISA_NULL(base)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_INT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)pow((atomlang_float_t)value.n, 1.0/base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_FLOAT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)pow((atomlang_float_t)value.n, 1.0/base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)pow((atomlang_float_t)value.f, 1.0/base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)pow((atomlang_float_t)value.f, 1.0/base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_ceil (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)CEIL((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)CEIL((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_cos (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)COS((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)COS((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }


    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_exp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)EXP((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)EXP((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_floor (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)FLOOR((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)FLOOR((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static int gcf(int x, int y) {
    if (x == 0) {
        return y;
    }
    while (y != 0) {
        if (x > y) {
            x = x - y;
        }
        else {
            y = y - x;
        }
    }
    return x;
}

static bool math_gcf (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (vm, rindex)
    if (nargs < 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    for (int i = 1; i < nargs; ++i) {
        if (!VALUE_ISA_INT(GET_VALUE(i))) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    }

    int gcf_value = (int)GET_VALUE(1).n;

    for (int i = 1; i < nargs-1; ++i) {
        gcf_value = gcf(gcf_value, (int)GET_VALUE(i+1).n);
    }

    RETURN_VALUE(VALUE_FROM_INT(gcf_value), rindex);
}

static int lcm(int x, int y) {
    return x*y/gcf(x,y);
}

static bool math_lcm (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused (vm, rindex)
    if (nargs < 3) RETURN_ERROR("2 or more arguments expected");

    for (int i = 1; i < nargs; ++i) {
        if (!VALUE_ISA_INT(GET_VALUE(i))) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
    }

    int lcm_value = (int)GET_VALUE(1).n;

    for (int i = 1; i < nargs-1; ++i) {
        lcm_value = lcm(lcm_value, (int)GET_VALUE(i+1).n);
    }

    RETURN_VALUE(VALUE_FROM_INT(lcm_value), rindex);
}

static bool math_lerp (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
#pragma unused(vm, nargs)

    if (nargs < 4) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    atomlang_float_t a, b, t;

    atomlang_value_t value = GET_VALUE(1);
    if (VALUE_ISA_INT(value)) {
        a = (atomlang_float_t)value.n;
    } else {
        if (VALUE_ISA_FLOAT(value)) {
            a = value.f;
        } else {
            RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
        }
    }

    value = GET_VALUE(2);
    if (VALUE_ISA_INT(value)) {
        b = (atomlang_float_t)value.n;
    } else {
        if (VALUE_ISA_FLOAT(value)) {
            b = value.f;
        } else {
            RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
        }
    }

    value = GET_VALUE(3);
    if (VALUE_ISA_INT(value)) {
        t = (atomlang_float_t)value.n;
    } else {
        if (VALUE_ISA_FLOAT(value)) {
            t = value.f;
        } else {
            RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
        }
    }

    atomlang_float_t lerp = a+(b-a)*t;
    RETURN_VALUE(VALUE_FROM_FLOAT(lerp), rindex);
}

static bool math_log (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_log10 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.n)/(atomlang_float_t)LOG((atomlang_float_t)10);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.f)/(atomlang_float_t)LOG((atomlang_float_t)10);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_logx (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t base = GET_VALUE(1);
    atomlang_value_t value = GET_VALUE(2);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_INT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.n)/(atomlang_float_t)LOG((atomlang_float_t)base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_INT(value) && VALUE_ISA_FLOAT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.n)/(atomlang_float_t)LOG((atomlang_float_t)base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_INT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.f)/(atomlang_float_t)LOG((atomlang_float_t)base.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value) && VALUE_ISA_FLOAT(base)) {
        atomlang_float_t computed_value = (atomlang_float_t)LOG((atomlang_float_t)value.f)/(atomlang_float_t)LOG((atomlang_float_t)base.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_max (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs == 1) RETURN_VALUE(VALUE_FROM_NULL, rindex);

    atomlang_float_t n = -ATOMLANG_FLOAT_MAX;
    uint16_t maxindex = 1;
    bool found = false;

    for (uint16_t i = 1; i<nargs; ++i) {
        atomlang_value_t value = GET_VALUE(i);
        if (VALUE_ISA_INT(value)) {
            found = true;
            if ((atomlang_float_t)value.n > n) {
                n = (atomlang_float_t)value.n;
                maxindex = i;
            }
        } else if (VALUE_ISA_FLOAT(value)) {
            found = true;
            if (value.f > n) {
                n = value.f;
                maxindex = i;
            }
        } else continue;
    }

    if (!found) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }

    RETURN_VALUE(GET_VALUE(maxindex), rindex);
}

static bool math_min (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    if (nargs == 1) RETURN_VALUE(VALUE_FROM_NULL, rindex);

    atomlang_float_t n = ATOMLANG_FLOAT_MAX;
    uint16_t minindex = 1;
    bool found = false;

    for (uint16_t i = 1; i<nargs; ++i) {
        atomlang_value_t value = GET_VALUE(i);
        if (VALUE_ISA_INT(value)) {
            found = true;
            if ((atomlang_float_t)value.n < n) {
                n = (atomlang_float_t)value.n;
                minindex = i;
            }
        } else if (VALUE_ISA_FLOAT(value)) {
            found = true;
            if (value.f < n) {
                n = value.f;
                minindex = i;
            }
        } else continue;
    }

    if (!found) {
        RETURN_VALUE(VALUE_FROM_NULL, rindex);
    }

    RETURN_VALUE(GET_VALUE(minindex), rindex);
}

static bool math_pow (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)

    if (nargs != 3) RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    atomlang_value_t value = GET_VALUE(1);
    atomlang_value_t value2 = GET_VALUE(2);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    atomlang_float_t n2;
    if (VALUE_ISA_INT(value2)) n2 = (atomlang_float_t)value2.n;
    else if (VALUE_ISA_FLOAT(value2)) n2 = (atomlang_float_t)value2.f;
    else RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)POW((atomlang_float_t)value.n, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)POW((atomlang_float_t)value.f, n2);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_round (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)ROUND((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_int_t ndigits = 0;
        bool toString = false;
        
        if (nargs >= 3 && VALUE_ISA_INT(GET_VALUE(2))) {
            atomlang_value_t extra = GET_VALUE(2);
            if (VALUE_AS_INT(extra) > 0) ndigits = VALUE_AS_INT(extra);
            if (ndigits > FLOAT_MAX_DECIMALS) ndigits = FLOAT_MAX_DECIMALS;
        }
        
        if (nargs >= 4 && VALUE_ISA_BOOL(GET_VALUE(3))) {
            toString = VALUE_AS_BOOL(GET_VALUE(3));
        }
        
        if (ndigits) {
            double d = pow(10.0, (double)ndigits);
            atomlang_float_t f = (atomlang_float_t)(ROUND((atomlang_float_t)value.f * (atomlang_float_t)d)) / (atomlang_float_t)d;
            
            char buffer[512];
            snprintf(buffer, sizeof(buffer), "%f", f);
            
            char *p = buffer;
            while (p) {
                if (p[0] == '.') {
                    ++p;
                    atomlang_int_t n = 0;
                    while (p && n < ndigits) {
                        ++p;
                        ++n;
                    }
                    if (p) p[0] = 0;
                    break;
                }
                ++p;
            }
            
            if (toString) {
                RETURN_VALUE(VALUE_FROM_CSTRING(vm, buffer), rindex);
            }
            
            RETURN_VALUE(VALUE_FROM_FLOAT(strtod(buffer, NULL)), rindex);
        }
        
        atomlang_float_t computed_value = (atomlang_float_t)ROUND((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_sin (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)SIN((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)SIN((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_sqrt (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)SQRT((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)SQRT((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_tan (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, nargs)
    atomlang_value_t value = GET_VALUE(1);

    if (VALUE_ISA_NULL(value)) {
        RETURN_VALUE(VALUE_FROM_INT(0), rindex);
    }

    if (VALUE_ISA_INT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)TAN((atomlang_float_t)value.n);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    if (VALUE_ISA_FLOAT(value)) {
        atomlang_float_t computed_value = (atomlang_float_t)TAN((atomlang_float_t)value.f);
        RETURN_VALUE(VALUE_FROM_FLOAT(computed_value), rindex);
    }

    RETURN_VALUE(VALUE_FROM_UNDEFINED, rindex);
}

static bool math_PI (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(3.141592653589793), rindex);
}

static bool math_E (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(2.718281828459045), rindex);
}

static bool math_LN2 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.6931471805599453), rindex);
}

static bool math_LN10 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(2.302585092994046), rindex);
}

static bool math_LOG2E (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(1.4426950408889634), rindex);
}

static bool math_LOG10E (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.4342944819032518), rindex);
}

static bool math_SQRT2 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(1.4142135623730951), rindex);
}

static bool math_SQRT1_2 (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm, args, nargs)
    RETURN_VALUE(VALUE_FROM_FLOAT(0.7071067811865476), rindex);
}

#if ATOMLANG_ENABLE_INT64

#define LFSR_GERME 123456789123456789ULL

static uint64_t lfsr258_y1 = LFSR_GERME, lfsr258_y2 = LFSR_GERME, lfsr258_y3 = LFSR_GERME, lfsr258_y4 = LFSR_GERME, lfsr258_y5 = LFSR_GERME;

static void lfsr258_init (uint64_t n) {
    static int lfsr258_inited = 0;
    if (lfsr258_inited) return;
    lfsr258_inited = 1;
    if (n == 0) n = LFSR_GERME;
    
    lfsr258_y1 = n; lfsr258_y2 = n; lfsr258_y3 = n; lfsr258_y4 = n; lfsr258_y5 = n;
}

static double lfsr258 (void) {
    uint64_t b;
    
    b = ((lfsr258_y1 << 1) ^ lfsr258_y1) >> 53;
    lfsr258_y1 = ((lfsr258_y1 & 18446744073709551614UL) << 10) ^ b;
    b = ((lfsr258_y2 << 24) ^ lfsr258_y2) >> 50;
    lfsr258_y2 = ((lfsr258_y2 & 18446744073709551104UL) << 5) ^ b;
    b = ((lfsr258_y3 << 3) ^ lfsr258_y3) >> 23;
    lfsr258_y3 = ((lfsr258_y3 & 18446744073709547520UL) << 29) ^ b;
    b = ((lfsr258_y4 << 5) ^ lfsr258_y4) >> 24;
    lfsr258_y4 = ((lfsr258_y4 & 18446744073709420544UL) << 23) ^ b;
    b = ((lfsr258_y5 << 3) ^ lfsr258_y5) >> 33;
    lfsr258_y5 = ((lfsr258_y5 & 18446744073701163008UL) << 8) ^ b;
    return (lfsr258_y1 ^ lfsr258_y2 ^ lfsr258_y3 ^ lfsr258_y4 ^ lfsr258_y5) * 5.421010862427522170037264e-20;
}
#else

#define LFSR_SEED 987654321

static uint32_t lfsr113_z1 = LFSR_SEED, lfsr113_z2 = LFSR_SEED, lfsr113_z3 = LFSR_SEED, lfsr113_z4 = LFSR_SEED;

static void lfsr113_init (uint32_t n) {
    static int lfsr113_inited = 0;
    if (lfsr113_inited) return;
    lfsr113_inited = 1;
    if (n == 0) n = LFSR_SEED;
    
    lfsr113_z1 = n; lfsr113_z2 = n; lfsr113_z3 = n; lfsr113_z4 = n;
}

static double lfsr113 (void) {
    uint32_t b;
    b  = ((lfsr113_z1 << 6) ^ lfsr113_z1) >> 13;
    lfsr113_z1 = ((lfsr113_z1 & 4294967294U) << 18) ^ b;
    b  = ((lfsr113_z2 << 2) ^ lfsr113_z2) >> 27;
    lfsr113_z2 = ((lfsr113_z2 & 4294967288U) << 2) ^ b;
    b  = ((lfsr113_z3 << 13) ^ lfsr113_z3) >> 21;
    lfsr113_z3 = ((lfsr113_z3 & 4294967280U) << 7) ^ b;
    b  = ((lfsr113_z4 << 3) ^ lfsr113_z4) >> 12;
    lfsr113_z4 = ((lfsr113_z4 & 4294967168U) << 13) ^ b;
    return (lfsr113_z1 ^ lfsr113_z2 ^ lfsr113_z3 ^ lfsr113_z4) * 2.3283064365386963e-10;
}
#endif

static bool math_random (atomlang_vm *vm, atomlang_value_t *args, uint16_t nargs, uint32_t rindex) {
    #pragma unused(vm)
    
    #if ATOMLANG_ENABLE_INT64
    lfsr258_init(nanotime());
    atomlang_float_t rnd = lfsr258();
    #else
    lfsr113_init((uint32_t)time(NULL));
    atomlang_float_t rnd = lfsr113();
    #endif
    
    if (nargs > 1) {
        atomlang_value_t value1 = VALUE_FROM_UNDEFINED;
        atomlang_value_t value2 = VALUE_FROM_UNDEFINED;
        
        if (nargs == 2) {
            value2 = GET_VALUE(1);
            if (VALUE_ISA_INT(value2)) value1 = VALUE_FROM_INT(0);
            if (VALUE_ISA_FLOAT(value2)) value1 = VALUE_FROM_FLOAT(0.0);
        }
        
        if (nargs == 3) {
            value1 = GET_VALUE(1);
            value2 = GET_VALUE(2);
        }
        
        if ((value1.isa == value2.isa) && (VALUE_ISA_INT(value1))) {
            atomlang_int_t n1 = VALUE_AS_INT(value1); 
            atomlang_int_t n2 = VALUE_AS_INT(value2); 
            if (n1 == n2) RETURN_VALUE(VALUE_FROM_INT(n1), rindex);
            
			atomlang_int_t n0 = (atomlang_int_t)(rnd * (atomlang_float_t)ATOMLANG_INT_MAX);
            if (n1 > n2) {atomlang_int_t temp = n1; n1 = n2; n2 = temp;} 
            atomlang_int_t n = (atomlang_int_t)(n0 % (n2 + 1 - n1) + n1);
            RETURN_VALUE(VALUE_FROM_INT(n), rindex);
        }
        
        if ((value1.isa == value2.isa) && (VALUE_ISA_FLOAT(value1))) {
            atomlang_float_t n1 = VALUE_AS_FLOAT(value1); 
            atomlang_float_t n2 = VALUE_AS_FLOAT(value2); 
            if (n1 == n2) RETURN_VALUE(VALUE_FROM_FLOAT(n1), rindex);
            
            if (n1 > n2) {atomlang_float_t temp = n1; n1 = n2; n2 = temp;}  
            atomlang_float_t diff = n2 - n1;
            atomlang_float_t r = rnd * diff;
            RETURN_VALUE(VALUE_FROM_FLOAT(r + n1), rindex);
        }
    }
    
    RETURN_VALUE(VALUE_FROM_FLOAT(rnd), rindex);
}

static void create_optional_class (void) {
    atomlang_class_math = atomlang_class_new_pair(NULL, ATOMLANG_CLASS_MATH_NAME, NULL, 0, 0);
    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_math);

    atomlang_class_bind(meta, "abs", NEW_CLOSURE_VALUE(math_abs));
    atomlang_class_bind(meta, "acos", NEW_CLOSURE_VALUE(math_acos));
    atomlang_class_bind(meta, "asin", NEW_CLOSURE_VALUE(math_asin));
    atomlang_class_bind(meta, "atan", NEW_CLOSURE_VALUE(math_atan));
    atomlang_class_bind(meta, "atan2", NEW_CLOSURE_VALUE(math_atan2));
    atomlang_class_bind(meta, "cbrt", NEW_CLOSURE_VALUE(math_cbrt));
    atomlang_class_bind(meta, "xrt", NEW_CLOSURE_VALUE(math_xrt));
    atomlang_class_bind(meta, "ceil", NEW_CLOSURE_VALUE(math_ceil));
    atomlang_class_bind(meta, "cos", NEW_CLOSURE_VALUE(math_cos));
    atomlang_class_bind(meta, "exp", NEW_CLOSURE_VALUE(math_exp));
    atomlang_class_bind(meta, "floor", NEW_CLOSURE_VALUE(math_floor));
    atomlang_class_bind(meta, "gcf", NEW_CLOSURE_VALUE(math_gcf));
    atomlang_class_bind(meta, "lcm", NEW_CLOSURE_VALUE(math_lcm));
    atomlang_class_bind(meta, "lerp", NEW_CLOSURE_VALUE(math_lerp));
    atomlang_class_bind(meta, "log", NEW_CLOSURE_VALUE(math_log));
    atomlang_class_bind(meta, "log10", NEW_CLOSURE_VALUE(math_log10));
    atomlang_class_bind(meta, "logx", NEW_CLOSURE_VALUE(math_logx));
    atomlang_class_bind(meta, "max", NEW_CLOSURE_VALUE(math_max));
    atomlang_class_bind(meta, "min", NEW_CLOSURE_VALUE(math_min));
    atomlang_class_bind(meta, "pow", NEW_CLOSURE_VALUE(math_pow));
    atomlang_class_bind(meta, "random", NEW_CLOSURE_VALUE(math_random));
    atomlang_class_bind(meta, "round", NEW_CLOSURE_VALUE(math_round));
    atomlang_class_bind(meta, "sin", NEW_CLOSURE_VALUE(math_sin));
    atomlang_class_bind(meta, "sqrt", NEW_CLOSURE_VALUE(math_sqrt));
    atomlang_class_bind(meta, "tan", NEW_CLOSURE_VALUE(math_tan));

    atomlang_closure_t *closure = NULL;
    closure = computed_property_create(NULL, NEW_FUNCTION(math_PI), NULL);
    atomlang_class_bind(meta, "PI", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_E), NULL);
    atomlang_class_bind(meta, "E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LN2), NULL);
    atomlang_class_bind(meta, "LN2", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LN10), NULL);
    atomlang_class_bind(meta, "LN10", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LOG2E), NULL);
    atomlang_class_bind(meta, "LOG2E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_LOG10E), NULL);
    atomlang_class_bind(meta, "LOG10E", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_SQRT2), NULL);
    atomlang_class_bind(meta, "SQRT2", VALUE_FROM_OBJECT(closure));
    closure = computed_property_create(NULL, NEW_FUNCTION(math_SQRT1_2), NULL);
    atomlang_class_bind(meta, "SQRT1_2", VALUE_FROM_OBJECT(closure));

    SETMETA_INITED(atomlang_class_math);
}

bool atomlang_ismath_class (atomlang_class_t *c) {
    return (c == atomlang_class_math);
}

const char *atomlang_math_name (void) {
    return ATOMLANG_CLASS_MATH_NAME;
}

void atomlang_math_register (atomlang_vm *vm) {
    if (!atomlang_class_math) create_optional_class();
    ++refcount;

    if (!vm || atomlang_vm_ismini(vm)) return;
    atomlang_vm_setvalue(vm, ATOMLANG_CLASS_MATH_NAME, VALUE_FROM_OBJECT(atomlang_class_math));
}

void atomlang_math_free (void) {
    if (!atomlang_class_math) return;
    if (--refcount) return;

    atomlang_class_t *meta = atomlang_class_get_meta(atomlang_class_math);
    computed_property_free(meta, "PI", true);
    computed_property_free(meta, "E", true);
    computed_property_free(meta, "LN2", true);
    computed_property_free(meta, "LN10", true);
    computed_property_free(meta, "LOG2E", true);
    computed_property_free(meta, "LOG10E", true);
    computed_property_free(meta, "SQRT2", true);
    computed_property_free(meta, "SQRT1_2", true);
    atomlang_class_free_core(NULL, meta);
    atomlang_class_free_core(NULL, atomlang_class_math);

    atomlang_class_math = NULL;
}
