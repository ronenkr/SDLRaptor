/****************************************************************************
* plat_main.c - real entry point of the Win32/SDL3 port
*----------------------------------------------------------------------------
* Responsibilities before handing off to the game's main() (renamed to
* raptor_main via the build system):
*  - SDL init + atexit cleanup (the game exits through exit()/EXIT_*)
*  - locate the game data (FILE0000.GLB) and chdir there, so GLBAPI,
*    setup.ini, and save files all resolve in one place
*  - write a default setup.ini if missing (the DOS game demands SETUP.EXE)
*  - pad argv: RAP.C reads argv[1] (and argv[2]) without checking argc
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <limits.h>
#define _access access
#define _chdir  chdir
#define _MAX_PATH PATH_MAX
#endif
#include <SDL3/SDL.h>

#include "plat.h"

#ifndef RAPTOR_DEFAULT_DATADIR
#define RAPTOR_DEFAULT_DATADIR ""
#endif

extern int raptor_main ( int argc, char *argv[] );
extern void PLAT_InstallCrashHandler ( void );

static int
HasGameData ( const char *dir )
{
   char probe[_MAX_PATH];

   snprintf ( probe, sizeof ( probe ), "%s/FILE0000.GLB", dir );
   return _access ( probe, 0 ) == 0;
}

static void
ResolveDataDir ( const char *argv0, const char *cli_dir )
{
   char dir[_MAX_PATH];
   const char *env = getenv ( "RAPTOR_DATA" );

   if ( cli_dir && HasGameData ( cli_dir ) )
   {
      _chdir ( cli_dir );
      return;
   }

   if ( HasGameData ( "." ) )
      return;

   strncpy ( dir, argv0, sizeof ( dir ) - 1 );
   dir[sizeof ( dir ) - 1] = 0;
   {
      char *p = strrchr ( dir, '\\' );
      char *q = strrchr ( dir, '/' );

      if ( q > p )
         p = q;
      if ( p )
      {
         *p = 0;
         if ( HasGameData ( dir ) )
         {
            _chdir ( dir );
            return;
         }
      }
   }

   if ( env && HasGameData ( env ) )
   {
      _chdir ( env );
      return;
   }

   if ( HasGameData ( RAPTOR_DEFAULT_DATADIR ) )
   {
      _chdir ( RAPTOR_DEFAULT_DATADIR );
      return;
   }

   /* leave cwd alone; the game prints its own data-missing message */
}

static void
WriteDefaultSetup ( void )
{
   FILE *f;

   if ( _access ( "setup.ini", 0 ) == 0 )
      return;

   f = fopen ( "setup.ini", "wt" );
   if ( !f )
      return;

   /* CardType 5 = Sound Blaster: AdLib FM music + digital sound effects,
      exactly the paths the port's audio backend implements */
   fprintf ( f,
      "[Setup]\n"
      "Detail=1\n"
      "Control=0\n"
      "\n"
      "[Music]\n"
      "CardType=5\n"
      "Volume=127\n"
      "\n"
      "[SoundFX]\n"
      "CardType=5\n"
      "Volume=127\n"
      "Channels=8\n" );
   fclose ( f );
   printf ( "Created default setup.ini\n" );
}

static void
ApplyScalerArg ( const char *name )
{
   if ( SDL_strcasecmp ( name, "advmame2x" ) == 0 || SDL_strcasecmp ( name, "scale2x" ) == 0 )
      PLAT_SetScaler ( PLAT_SCALER_ADVMAME2X );
   else if ( SDL_strcasecmp ( name, "none" ) == 0 )
      PLAT_SetScaler ( PLAT_SCALER_NONE );
   else
      fprintf ( stderr, "--scaler: unknown scaler '%s' (expected: none, advmame2x)\n", name );
}

int
main ( int argc, char *argv[] )
{
   char *game_argv[4] = { "raptor", "", "", NULL };
   int   game_argc = 1;
   const char *cli_dir = NULL;
   int   i;

   /* consume port-specific args, forward the rest (REC/PLAY/joycal) */
   for ( i = 1; i < argc; i++ )
   {
      if ( strcmp ( argv[i], "--data" ) == 0 && i + 1 < argc )
      {
         cli_dir = argv[++i];
      }
      else if ( strcmp ( argv[i], "--scaler" ) == 0 && i + 1 < argc )
      {
         ApplyScalerArg ( argv[++i] );
      }
      else if ( game_argc < 3 )
      {
         game_argv[game_argc++] = argv[i];
      }
   }

   PLAT_InstallCrashHandler ();
   setvbuf ( stdout, NULL, _IONBF, 0 );   /* keep the printf trail exact */

   if ( !SDL_Init ( SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD ) )
   {
      fprintf ( stderr, "SDL_Init failed: %s\n", SDL_GetError () );
      return 1;
   }
   /* atexit runs LIFO: destroy the window first, quit SDL last */
   atexit ( SDL_Quit );
   atexit ( PLAT_DestroyWindow );

   ResolveDataDir ( argv[0], cli_dir );
   WriteDefaultSetup ();

   return raptor_main ( 3, game_argv );
}
