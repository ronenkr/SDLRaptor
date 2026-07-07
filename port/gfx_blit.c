/****************************************************************************
* gfx_blit.c - C rewrites of GFX/GFXAPI_A.ASM (Mode 13h rasterizers)
*----------------------------------------------------------------------------
* Faithful ports of the TASM routines. Notes carried over from the ASM:
*  - GFX_ScaleLine/GFX_CScaleLine read the scale table backwards:
*    out[j] = in[stable[tablelen-1-j]]. CScaleLine treats color 0 as
*    transparent, ScaleLine does not (the comments in GFXAPI.H have the
*    two swapped - the ASM is authoritative).
*  - The shade routines relied on 256-byte-aligned tables to index with a
*    partial-register trick; plain array indexing replaces that.
*  - GFX_DisplayScreen copies the dirty rect to displayscreen ("VGA
*    memory"), which here is just another buffer that PLAT_Present shows.
*---------------------------------------------------------------------------*/
#include <string.h>

#include "types.h"
#include "gfxapi.h"
#include "plat.h"

extern BYTE *displaybuffer;
extern BYTE *displayscreen;
extern INT   stable[];
extern INT   tablelen;
extern INT   ud_x, ud_y, ud_lx, ud_ly;
extern BOOL  update_start;
extern BYTE *gfx_inmem;
extern INT   gfx_xp, gfx_yp, gfx_lx, gfx_ly, gfx_imga;

VOID __cdecl
GFX_ScaleLine ( BYTE *outmem, BYTE *inmem )
{
   INT j;

   for ( j = 0; j < tablelen; j++ )
      outmem[j] = inmem[stable[tablelen - 1 - j]];
}

VOID __cdecl
GFX_CScaleLine ( BYTE *outmem, BYTE *inmem )
{
   INT j;

   for ( j = 0; j < tablelen; j++ )
   {
      BYTE c = inmem[stable[tablelen - 1 - j]];

      if ( c )
         outmem[j] = c;
   }
}

VOID
GFX_DrawSprite ( BYTE *dest, BYTE *inmem )
{
   GFX_SPRITE *sp = ( GFX_SPRITE * ) inmem;

   while ( sp->offset != EMPTY )
   {
      inmem += sizeof ( GFX_SPRITE );
      memcpy ( dest + ( WORD ) sp->offset, inmem, sp->length );
      inmem += sp->length;
      sp = ( GFX_SPRITE * ) inmem;
   }
}

VOID
GFX_ShadeSprite ( BYTE *dest, BYTE *inmem, BYTE *table )
{
   GFX_SPRITE *sp = ( GFX_SPRITE * ) inmem;

   while ( sp->offset != EMPTY )
   {
      BYTE *out = dest + ( WORD ) sp->offset;
      INT   len = sp->length;
      INT   i;

      for ( i = 0; i < len; i++ )
         out[i] = table[out[i]];

      inmem += sizeof ( GFX_SPRITE ) + sp->length;
      sp = ( GFX_SPRITE * ) inmem;
   }
}

VOID
GFX_Shade ( BYTE *outmem, INT maxlen, BYTE *table )
{
   INT i;

   for ( i = 0; i < maxlen; i++ )
      outmem[i] = table[outmem[i]];
}

VOID __cdecl
GFX_DrawChar ( BYTE *dest, BYTE *inmem, INT width, INT height, INT addx,
               INT color )
{
   INT x, y;

   for ( y = 0; y < height; y++ )
   {
      for ( x = 0; x < width; x++ )
      {
         BYTE c = inmem[x];

         if ( c )
            dest[x] = ( BYTE )( c + color );   /* 8-bit wrapping add */
      }
      inmem += width + addx;
      dest  += SCREENWIDTH;
   }
}

VOID
GFX_PutPic ( VOID )
{
   BYTE *src = gfx_inmem;
   BYTE *dst = displaybuffer + gfx_xp + gfx_yp * SCREENWIDTH;
   INT   y;

   for ( y = 0; y < gfx_ly; y++ )
   {
      memcpy ( dst, src, gfx_lx );
      src += gfx_lx + gfx_imga;
      dst += SCREENWIDTH;
   }
}

VOID
GFX_PutMaskPic ( VOID )
{
   /* NOTE: for the masked blit gfx_imga is the FULL source row stride
      (the caller does not subtract gfx_lx - see GFX_PutImage). */
   BYTE *src = gfx_inmem;
   BYTE *dst = displaybuffer + gfx_xp + gfx_yp * SCREENWIDTH;
   INT   x, y;

   for ( y = 0; y < gfx_ly; y++ )
   {
      for ( x = 0; x < gfx_lx; x++ )
      {
         BYTE c = src[x];

         if ( c )
            dst[x] = c;
      }
      src += gfx_imga;
      dst += SCREENWIDTH;
   }
}

VOID
GFX_DisplayScreen ( VOID )
{
   BYTE *src = displaybuffer + ud_x + ud_y * SCREENWIDTH;
   BYTE *dst = displayscreen + ud_x + ud_y * SCREENWIDTH;
   INT   y;

   for ( y = 0; y < ud_ly; y++ )
   {
      memcpy ( dst, src, ud_lx );
      src += SCREENWIDTH;
      dst += SCREENWIDTH;
   }

   update_start = FALSE;

   PLAT_MarkScreenDirty ();
   PLAT_Present ();
}
