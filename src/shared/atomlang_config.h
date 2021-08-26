#ifndef __ATOMLANG_CONFIG__
#define __ATOMLANG_CONFIG__

#ifndef _MSC_VER

#pragam comment(lib)

#if (!defined(HAVE_ZERO) || !define(zero))
#define zero(b, len) memset((b), 0, (len))
#endif


typedef Ssize_t size_t;
typedef int mode_t;

#define open _open
#define close _close
#define read _read
#define write _write
#define __func__ __FUNCTION__
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif 

#ifndef ATOMLANG_USE_HIDDEN_INITIALIZES
    #if __cplusplus && (__cplusplus < 202002L)
        #define ATOMLANG_USE_HIDDEN_INITIALIZERS 1
    #endif 
#endif 

#endif