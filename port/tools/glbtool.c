/****************************************************************************
* glbtool - headless .GLB archive inspector for the Win32/SDL3 port (M0)
*----------------------------------------------------------------------------
* Usage:
*   glbtool <datadir> list
*   glbtool <datadir> dump <ITEMNAME> <outfile>
*
* "list" parses the archives directly (validating GLB_DeCrypt and the
* KEYFILE layout); "dump" goes through the real GLB_InitSystem/GLB_GetItem
* path the game uses.
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <sys\stat.h>

#include "types.h"
#include "glbapi.h"

static CHAR *serial = "32768GLB";    /* same key GLBAPI.C uses */

static int
ListFile ( int filenum )
{
   CHAR     filename[64];
   KEYFILE  key;
   int      handle;
   int      items;
   int      j;
   long     total = 0;

   sprintf ( filename, "FILE%04u.GLB", filenum );
   handle = open ( filename, O_RDONLY | O_BINARY );
   if ( handle == -1 )
      return 0;

   read ( handle, &key, sizeof ( KEYFILE ) );
   GLB_DeCrypt ( serial, ( BYTE * ) &key, sizeof ( KEYFILE ) );
   items = ( int ) key.offset;

   printf ( "%s: %d items\n", filename, items );
   for ( j = 0; j < items; j++ )
   {
      read ( handle, &key, sizeof ( KEYFILE ) );
      GLB_DeCrypt ( serial, ( BYTE * ) &key, sizeof ( KEYFILE ) );
      printf ( "  %4d  %-16.16s  ofs=%8u  size=%8u  %s\n",
               j, key.name, key.offset, key.filesize,
               key.opt == GLB_ENCODED ? "ENC" : "   " );
      total += key.filesize;
   }
   printf ( "%s: total payload %ld bytes\n\n", filename, total );

   close ( handle );
   return 1;
}

int
main ( int argc, char *argv[] )
{
   int filenum;

   if ( argc < 3 )
   {
      printf ( "usage: glbtool <datadir> list\n"
               "       glbtool <datadir> dump <ITEMNAME> <outfile>\n" );
      return 1;
   }

   if ( _chdir ( argv[1] ) != 0 )
   {
      printf ( "glbtool: cannot chdir to %s\n", argv[1] );
      return 1;
   }

   if ( strcmp ( argv[2], "list" ) == 0 )
   {
      int opened = 0;

      for ( filenum = 0; filenum < 6; filenum++ )
         opened += ListFile ( filenum );

      printf ( "%d GLB file(s) found\n", opened );
      return opened ? 0 : 1;
   }

   if ( strcmp ( argv[2], "dump" ) == 0 && argc >= 5 )
   {
      DWORD  handle;
      INT    size;
      BYTE  *mem;

      GLB_InitSystem ( argv[0], 6, NUL );

      handle = GLB_GetItemID ( argv[3] );
      if ( handle == ( DWORD ) EMPTY )
      {
         printf ( "glbtool: item '%s' not found\n", argv[3] );
         return 1;
      }

      size = GLB_ItemSize ( handle );
      mem  = GLB_GetItem ( handle );
      GLB_SaveFile ( argv[4], mem, size );
      GLB_FreeItem ( handle );

      printf ( "dumped '%s' (%d bytes) -> %s\n", argv[3], size, argv[4] );
      return 0;
   }

   printf ( "glbtool: bad arguments\n" );
   return 1;
}
