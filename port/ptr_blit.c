/****************************************************************************
* ptr_blit.c - C rewrites of GFX/PTRAPI_A.ASM (software cursor blitters)
*----------------------------------------------------------------------------
* The cursor is 16x16; cursorstart points into displayscreen (or
* displaybuffer during PTR_FrameHook) and cursorsave is a 16x16 backing
* store. cursorloopx/y hold the clipped extents.
*---------------------------------------------------------------------------*/
#include <string.h>

#include "types.h"
#include "gfxapi.h"
#include "plat.h"

#define CURSORWIDTH   16
#define CURSORHEIGHT  16

extern BYTE *cursorstart;
extern BYTE *cursorsave;
extern BYTE *displaypic;
extern INT   cursorloopx;
extern INT   cursorloopy;
extern INT   joy_x, joy_y, joy_buttons;

/*------------------------------------------------------------------------
   PTR_ReadJoyStick () - was raw gameport (0x201) pulse timing; an SDL
   gamepad backend lands in a later pass. Until then: centered, no buttons.
  ------------------------------------------------------------------------*/
VOID
PTR_ReadJoyStick ( VOID )
{
   joy_x = 0;
   joy_y = 0;
   joy_buttons = 0;
}

VOID
PTR_Save ( VOID )
{
   BYTE *src = cursorstart;
   BYTE *dst = cursorsave;
   INT   y;

   for ( y = 0; y < CURSORHEIGHT; y++ )
   {
      memcpy ( dst, src, CURSORWIDTH );
      src += SCREENWIDTH;
      dst += CURSORWIDTH;
   }
}

VOID
PTR_ClipSave ( VOID )
{
   BYTE *src = cursorstart;
   BYTE *dst = cursorsave;
   INT   y;

   for ( y = 0; y < cursorloopy; y++ )
   {
      memcpy ( dst, src, CURSORWIDTH );
      src += SCREENWIDTH;
      dst += CURSORWIDTH;
   }
}

VOID
PTR_Erase ( VOID )
{
   BYTE *src = cursorsave;
   BYTE *dst = cursorstart;
   INT   y;

   for ( y = 0; y < CURSORHEIGHT; y++ )
   {
      memcpy ( dst, src, CURSORWIDTH );
      src += CURSORWIDTH;
      dst += SCREENWIDTH;
   }

   PLAT_MarkScreenDirty ();
}

VOID
PTR_ClipErase ( VOID )
{
   /* the save buffer always holds full 16-wide rows (PTR_ClipSave copies
      CURSORWIDTH per row); only cursorloopx bytes go back to the screen */
   BYTE *src = cursorsave;
   BYTE *dst = cursorstart;
   INT   y;

   for ( y = 0; y < cursorloopy; y++ )
   {
      memcpy ( dst, src, cursorloopx );
      src += CURSORWIDTH;
      dst += SCREENWIDTH;
   }

   PLAT_MarkScreenDirty ();
}

VOID
PTR_Draw ( VOID )
{
   BYTE *src = displaypic;
   BYTE *dst = cursorstart;
   INT   x, y;

   for ( y = 0; y < cursorloopy; y++ )
   {
      for ( x = 0; x < cursorloopx; x++ )
      {
         BYTE c = src[x];

         if ( c )
            dst[x] = c;
      }
      src += CURSORWIDTH;
      dst += SCREENWIDTH;
   }

   PLAT_MarkScreenDirty ();
}
