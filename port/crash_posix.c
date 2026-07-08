/****************************************************************************
* crash_posix.c - unhandled-signal reporter for bring-up (Linux/POSIX)
*----------------------------------------------------------------------------
* POSIX counterpart to crash_win.c: prints a raw backtrace (via glibc's
* backtrace()/backtrace_symbols_fd) to stderr on a fatal signal, so access
* violations during port bring-up are diagnosable without an attached
* debugger. No line-number/symbol-demangling support (that needs addr2line
* or libbacktrace) - just addresses and, where the binary has a dynamic
* symbol table, function names.
*---------------------------------------------------------------------------*/
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_FRAMES 24

static void
CrashHandler ( int sig )
{
   void *frames[MAX_FRAMES];
   int   count = backtrace ( frames, MAX_FRAMES );

   fprintf ( stderr, "\n=== CRASH: signal %d ===\n", sig );
   fflush ( stderr );
   backtrace_symbols_fd ( frames, count, fileno ( stderr ) );

   signal ( sig, SIG_DFL );
   raise ( sig );
}

void
PLAT_InstallCrashHandler ( void )
{
   signal ( SIGSEGV, CrashHandler );
   signal ( SIGABRT, CrashHandler );
   signal ( SIGFPE,  CrashHandler );
   signal ( SIGILL,  CrashHandler );
   signal ( SIGBUS,  CrashHandler );
}

/* called from EXIT_Error so fatal game errors show where they came from */
void
PLAT_PrintBacktrace ( void )
{
   void *frames[MAX_FRAMES];
   int   count = backtrace ( frames, MAX_FRAMES );

   backtrace_symbols_fd ( frames, count, fileno ( stderr ) );
   fflush ( stderr );
}
