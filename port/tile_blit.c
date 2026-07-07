/****************************************************************************
* tile_blit.c - C rewrites of SOURCE/TILE_A.ASM (tilemap blitters)
*---------------------------------------------------------------------------*/
#include <string.h>

#include "types.h"
#include "gfxapi.h"
#include "plat.h"

#define TILEWIDTH   32
#define TILEHEIGHT  32
#define MAP_LEFT    16

extern BYTE *displaybuffer;
extern BYTE *displayscreen;
extern BYTE *tilepic;
extern BYTE *tilestart;
extern INT   tileloopy;
extern INT   g_mapleft;

VOID
TILE_Draw ( VOID )
{
   BYTE *src = tilepic;
   BYTE *dst = tilestart;
   INT   y;

   for ( y = 0; y < TILEHEIGHT; y++ )
   {
      memcpy ( dst, src, TILEWIDTH );
      src += TILEWIDTH;
      dst += SCREENWIDTH;
   }
}

VOID
TILE_ClipDraw ( VOID )
{
   BYTE *src = tilepic;
   BYTE *dst = tilestart;
   INT   y;

   for ( y = 0; y < tileloopy; y++ )
   {
      memcpy ( dst, src, TILEWIDTH );
      src += TILEWIDTH;
      dst += SCREENWIDTH;
   }
}

VOID
TILE_DisplayScreen ( VOID )
{
   /* the 288-wide play area between the side bars */
   BYTE *src = displaybuffer + MAP_LEFT;
   BYTE *dst = displayscreen + MAP_LEFT;
   INT   y;

   for ( y = 0; y < SCREENHEIGHT; y++ )
   {
      memcpy ( dst, src, 288 );
      src += SCREENWIDTH;
      dst += SCREENWIDTH;
   }

   PLAT_MarkScreenDirty ();
   PLAT_Present ();
}

VOID
TILE_ShakeScreen ( VOID )
{
   /* screen-shake: source fixed at MAP_LEFT-4, dest offset by g_mapleft-4 */
   BYTE *src = displaybuffer + MAP_LEFT - 4;
   BYTE *dst = displayscreen + g_mapleft - 4;
   INT   y;

   for ( y = 0; y < SCREENHEIGHT; y++ )
   {
      memcpy ( dst, src, 296 );
      src += SCREENWIDTH;
      dst += SCREENWIDTH;
   }

   PLAT_MarkScreenDirty ();
   PLAT_Present ();
}
