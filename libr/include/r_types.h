#ifndef _INCLUDE_R_TYPES_H_
#define _INCLUDE_R_TYPES_H_

// TODO: fix this to make it crosscompile-friendly: R_SYS_OSTYPE ?
/* operating system */
#undef __BSD__
#undef __KFBSD__
#undef __UNIX__
#undef __WINDOWS__

#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__)
#define __KFBSD__ 1
#else
#define __KFBSD__ 0
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(__GNU__) || defined(__ANDROID__)
  #define __BSD__ 0
  #define __UNIX__ 1
#endif
#if __KFBSD__ || defined(__NetBSD__) || defined(__OpenBSD__)
  #define __BSD__ 1
  #define __UNIX__ 1
#endif
#if __WIN32__ || __CYGWIN__ || MINGW32
  #define __addr_t_defined
  #include <windows.h>
  #include <winsock.h>
  #undef USE_SOCKETS
  #define __WINDOWS__ 1
  #undef __UNIX__
  #undef __BSD__
#endif

#include <r_userconf.h>
#include <r_types_base.h>

#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#undef _GNU_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <fcntl.h> /* for O_RDONLY */

// TODO: FS or R_SYS_DIR ??
#undef FS
#if __WINDOWS__
#define FS "\\"
#define R_SYS_DIR "\\"
#define R_SYS_HOME "USERPROFILE"
#else
#define FS "/"
#define R_SYS_DIR "/"
#define R_SYS_HOME "HOME"
#endif

/* provide a per-module debug-enabled feature */
// TODO NOT USED. DEPREACATE
#if R_DEBUG
#define IFDBG
#else
#define IFDBG if (0)
#endif

typedef void (*PrintfCallback)(const char *str, ...);

// TODO NOT USED. DEPREACATE
#if R_RTDEBUG
#define IFRTDBG if (getenv ("LIBR_DEBUG"))
#else
#define IFRTDBG if (0)
#endif

/* compile-time introspection helpers */
#define CTO(y,z) ((size_t) &((y*)0)->z)
#define CTA(x,y,z) (x+CTO(y,z))
#define CTI(x,y,z) (*((size_t*)(CTA(x,y,z))))
#define CTS(x,y,z,t,v) {t* _=(t*)CTA(x,y,z);*_=v;}

#if R_SWIG
  #define R_API export
#elif R_INLINE
  #define R_API inline
#else
  #define R_API
#endif

#define BITS2BYTES(x) ((x/8)+((x%8)?1:0))
#define ZERO_FILL(x) memset (x, 0, sizeof (x))
#define R_NEWS(x,y) (x*)malloc(sizeof(x)*y)
#define R_NEW(x) (x*)malloc(sizeof(x))
#define R_NEW0(x) (x*)calloc(1,sizeof(x))
// TODO: Make R_NEW_COPY be 1 arg, not two
#define R_NEW_COPY(x,y) x=(y*)malloc(sizeof(y));memcpy(x,y,sizeof(y))
#define IS_PRINTABLE(x) (x>=' '&&x<='~')
#define IS_WHITESPACE(x) (x==' '||x=='\t')
#define R_MEM_ALIGN(x) ((void *)(size_t)(((ut64)(size_t)x) & 0xfffffffffffff000LL))

#define BIT_SET(x,y) (x[y>>4] |= (1<<(y&0xf)))
#define BIT_CHK(x,y) ((x[y>>4] & (1<<(y&0xf))))

#if __UNIX__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#endif
#include <unistd.h>

/* TODO: Move outside */
#define _perror(str,file,line) \
  { char buf[128];snprintf(buf,sizeof(buf),"%s:%d %s",file,line,str);perror(buf); }
#define perror(x) _perror(x,__FILE__,__LINE__)

#define eprintf(x,y...) fprintf(stderr,x,##y)

#define R_ROUND(x,y) ((x)%(y))? (x)+((y)-((x)%(y))): (x)
#define R_MAX(x,y) ((x)>(y))?x:y
#define R_MIN(x,y) ((x)>(y))?y:x
#define R_ABS(x) (((x)<0)?-(x):(x))
#define R_BTW(x,y,z) (((x)>=(y))&&((y)<=(z)))?y:x

#define R_FREE(x) { free(x); x = NULL; }

#if __WINDOWS__
#define HAVE_REGEXP 0
#else
#define HAVE_REGEXP 1
#endif

#if __WINDOWS__
#define PFMT64x "I64x"
#define PFMT64d "I64d"
#define PFMT64u "I64u"
#define PFMT64o "I64o"
#else
#define PFMT64x "llx"
#define PFMT64d "lld"
#define PFMT64u "llu"
#define PFMT64o "llo"
#endif


#if __APPLE__
# if __i386__
# define R_SYS_BASE ((ut64)0x1000)
# elif __x86_64__
# define R_SYS_BASE ((ut64)0x100000000)
# else
# define R_SYS_BASE ((ut64)0x1000)
# endif
#elif __WINDOWS__
# define R_SYS_BASE ((ut64)0x01001000)
#else // linux, bsd, ...
# if __arm__
# define R_SYS_BASE ((ut64)0x4000)
# else
# define R_SYS_BASE ((ut64)0x8048000)
# endif
#endif

/* arch */
#if __i386__
#define R_SYS_ARCH "x86"
#define R_SYS_BITS R_SYS_BITS_32
#elif __x86_64__
#define R_SYS_ARCH "x86"
#define R_SYS_BITS (R_SYS_BITS_32 | R_SYS_BITS_64)
#elif __POWERPC__
#define R_SYS_ARCH "ppc"
#define R_SYS_BITS R_SYS_BITS_32
#elif __arm__
#define R_SYS_ARCH "arm"
#define R_SYS_BITS R_SYS_BITS_32
#elif __sparc__
#define R_SYS_ARCH "sparc"
#define R_SYS_BITS R_SYS_BITS_32
#elif __mips__
#define R_SYS_ARCH "mips"
#define R_SYS_BITS R_SYS_BITS_32
#else
#define R_SYS_ARCH "unknown"
#define R_SYS_BITS R_SYS_BITS_32
#endif

enum {
	R_SYS_ARCH_NONE = 0,
	R_SYS_ARCH_X86 = 0x1,
	R_SYS_ARCH_ARM = 0x2,
	R_SYS_ARCH_PPC = 0x4,
	R_SYS_ARCH_M68K = 0x8,
	R_SYS_ARCH_JAVA = 0x10,
	R_SYS_ARCH_MIPS = 0x20,
	R_SYS_ARCH_SPARC = 0x40,
	R_SYS_ARCH_CSR = 0x80,
	R_SYS_ARCH_MSIL = 0x100,
	R_SYS_ARCH_OBJD = 0x200,
	R_SYS_ARCH_BF = 0x400,
	R_SYS_ARCH_SH = 0x800,
	R_SYS_ARCH_AVR = 0x1000,
	R_SYS_ARCH_DALVIK = 0x2000
};

/* os */
#if __APPLE__
#define R_SYS_OS "darwin"
#elif __linux__
#define R_SYS_OS "linux"
#elif __WIN32__ || __CYGWIN__ || MINGW32
#define R_SYS_OS "windows"
#elif __NetBSD__ 
#define R_SYS_OS "netbsd"
#elif __OpenBSD__
#define R_SYS_OS "openbsd"
#elif __FreeBSD__ || __FreeBSD_kernel__
#define R_SYS_OS "freebsd"
#else
#define R_SYS_OS "unknown"
#endif

/* endian */
#if LIL_ENDIAN
#define R_SYS_ENDIAN "little"
#else
#define R_SYS_ENDIAN "big"
#endif

#endif

// Usage: R_DEFINE_OBJECT(r_asm);
#if 0
#define R_DEFINE_OBJECT(type) \
 R_API struct type##_t* type##_new() { \
    return type##_init(R_NEW(struct type##_t)); \
 } \
 R_API struct type##_t* type##_free(struct type##_t *foo) { \
    return (type##_deinit(foo), free(foo), NULL); \
 }
#endif
