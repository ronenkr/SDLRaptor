#ifndef _TYPES_H
#define _TYPES_H

#include <stdlib.h>   /* must precede the random() macro below: on glibc,
                          <stdlib.h> declares its own random(); including it
                          first lets that declaration resolve normally, then
                          the macro shadows only later call sites */

/* x86_64 has one calling convention; MSVC's __cdecl is a no-op keyword
   there, but GCC/Clang don't know it at all. */
#if !defined(_MSC_VER) && !defined(__cdecl)
#define __cdecl
#endif

/* O_BINARY is an MSVC-ism; POSIX has no text/binary distinction, so treat
   it as a no-op flag there (GLBAPI.C/LOADSAVE.C open GLB/save files with it). */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* _MAX_PATH is MSVC's <stdlib.h> constant; POSIX's equivalent is PATH_MAX
   from <limits.h> (GLBAPI.C sizes path buffers with it). */
#ifndef _MAX_PATH
#include <limits.h>
#ifdef PATH_MAX
#define _MAX_PATH PATH_MAX
#else
#define _MAX_PATH 4096
#endif
#endif

typedef enum
{
	FALSE,
	TRUE
}BOOL;

#define LOCAL static
#define PRIVATE static
#define PUBLIC
#define TSMCALL
#define SPECIAL
#define MACRO
#define NUL (VOID *)0
#define EMPTY ~0
#define ASIZE(a) (sizeof(a)/sizeof((a)[0]))
#define FMUL32(a) ( ( a ) << 5 )

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef void VOID;
typedef char CHAR;
typedef short SHORT;
typedef unsigned short USHORT;
typedef int	INT;
typedef unsigned int UINT;

#define random( x ) ( rand() % x )

#endif