/****************************************************************************
* anim_blit.c - C rewrite of SOURCE/MOVIE_A.ASM ANIM_Render
*----------------------------------------------------------------------------
* Decodes one cutscene frame: a stream of ANIMLINE records (opt, fill,
* offset, length - all WORDs) each followed by `length` literal bytes to
* copy into displaybuffer at `offset`. A record with opt == 0 terminates.
*---------------------------------------------------------------------------*/
#include <string.h>

#include "types.h"
#include "gfxapi.h"

extern BYTE *displaybuffer;

VOID
ANIM_Render ( BYTE *inmem )
{
   for ( ;; )
   {
      ANIMLINE *ah = ( ANIMLINE * ) inmem;

      if ( ah->opt == 0 )
         break;

      inmem += sizeof ( ANIMLINE );
      memcpy ( displaybuffer + ah->offset, inmem, ah->length );
      inmem += ah->length;
   }
}
