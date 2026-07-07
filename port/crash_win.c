/****************************************************************************
* crash_win.c - unhandled-exception reporter for bring-up
*----------------------------------------------------------------------------
* Prints the faulting address, symbol (via dbghelp + PDB), and a short
* stack walk to stderr, so access violations during the port bring-up are
* diagnosable without an attached debugger.
*---------------------------------------------------------------------------*/
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

static void
PrintFrame ( HANDLE process, DWORD64 addr )
{
   char             buf[sizeof ( SYMBOL_INFO ) + 256];
   SYMBOL_INFO     *sym = ( SYMBOL_INFO * ) buf;
   IMAGEHLP_LINE64  line;
   DWORD64          disp64 = 0;
   DWORD            disp32 = 0;

   sym->SizeOfStruct = sizeof ( SYMBOL_INFO );
   sym->MaxNameLen = 255;
   line.SizeOfStruct = sizeof ( line );

   if ( SymFromAddr ( process, addr, &disp64, sym ) )
   {
      if ( SymGetLineFromAddr64 ( process, addr, &disp32, &line ) )
         fprintf ( stderr, "  %p  %s+0x%llx  (%s:%lu)\n", ( void * ) addr,
                   sym->Name, disp64, line.FileName, line.LineNumber );
      else
         fprintf ( stderr, "  %p  %s+0x%llx\n", ( void * ) addr,
                   sym->Name, disp64 );
   }
   else
      fprintf ( stderr, "  %p  <no symbol>\n", ( void * ) addr );
}

static LONG WINAPI
CrashFilter ( EXCEPTION_POINTERS *ep )
{
   HANDLE       process = GetCurrentProcess ();
   CONTEXT     *ctx = ep->ContextRecord;
   STACKFRAME64 frame;
   int          i;

   fprintf ( stderr, "\n=== CRASH: exception 0x%08lX at address %p ===\n",
             ep->ExceptionRecord->ExceptionCode,
             ep->ExceptionRecord->ExceptionAddress );
   if ( ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION )
      fprintf ( stderr, "  %s address %p\n",
                ep->ExceptionRecord->ExceptionInformation[0] ? "writing"
                                                             : "reading",
                ( void * ) ep->ExceptionRecord->ExceptionInformation[1] );

   SymSetOptions ( SYMOPT_LOAD_LINES | SYMOPT_UNDNAME );
   SymInitialize ( process, NULL, TRUE );

   memset ( &frame, 0, sizeof ( frame ) );
   frame.AddrPC.Offset = ctx->Rip;
   frame.AddrPC.Mode = AddrModeFlat;
   frame.AddrFrame.Offset = ctx->Rbp;
   frame.AddrFrame.Mode = AddrModeFlat;
   frame.AddrStack.Offset = ctx->Rsp;
   frame.AddrStack.Mode = AddrModeFlat;

   for ( i = 0; i < 24; i++ )
   {
      if ( !StackWalk64 ( IMAGE_FILE_MACHINE_AMD64, process,
                          GetCurrentThread (), &frame, ctx, NULL,
                          SymFunctionTableAccess64, SymGetModuleBase64,
                          NULL ) )
         break;
      if ( frame.AddrPC.Offset == 0 )
         break;
      PrintFrame ( process, frame.AddrPC.Offset );
   }

   fflush ( stderr );
   return EXCEPTION_EXECUTE_HANDLER;
}

void
PLAT_InstallCrashHandler ( void )
{
   SetUnhandledExceptionFilter ( CrashFilter );
}

/* called from EXIT_Error so fatal game errors show where they came from */
void
PLAT_PrintBacktrace ( void )
{
   HANDLE process = GetCurrentProcess ();
   void  *frames[24];
   USHORT count, i;

   SymSetOptions ( SYMOPT_LOAD_LINES | SYMOPT_UNDNAME );
   SymInitialize ( process, NULL, TRUE );

   count = CaptureStackBackTrace ( 1, 24, frames, NULL );
   for ( i = 0; i < count; i++ )
      PrintFrame ( process, ( DWORD64 ) frames[i] );
   fflush ( stderr );
}
