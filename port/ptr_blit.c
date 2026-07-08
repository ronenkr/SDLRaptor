/****************************************************************************
* ptr_blit.c - C rewrites of GFX/PTRAPI_A.ASM (software cursor blitters)
*----------------------------------------------------------------------------
* The cursor is 16x16; cursorstart points into displayscreen (or
* displaybuffer during PTR_FrameHook) and cursorsave is a 16x16 backing
* store. cursorloopx/y hold the clipped extents.
*---------------------------------------------------------------------------*/
#include <string.h>
#include <SDL3/SDL.h>

#include "types.h"
#include "gfxapi.h"
#include "conio.h"       /* compat_gameport_state, for inp(0x201) below */
#include "plat.h"

#define CURSORWIDTH   16
#define CURSORHEIGHT  16

/* gameport joysticks reported roughly-centered pulse-timing values, not
   raw ADC counts; PTR_JoyHandler's deadzone (joy_limit_x/yl/h, +-10) and
   ramp (JOYADD=8) are tuned against that ~0-1000-around-500 range, so SDL's
   -32768..32767 axis is rescaled down to match rather than fed in raw. */
#define JOY_CENTER  500
#define JOY_RANGE   500

extern BYTE *cursorstart;
extern BYTE *cursorsave;
extern BYTE *displaypic;
extern INT   cursorloopx;
extern INT   cursorloopy;
extern INT   joy_x, joy_y, joy_buttons;

PRIVATE SDL_Gamepad *g_pad = NULL;

PRIVATE VOID
JoyEnsureOpen ( VOID )
{
   if ( g_pad && SDL_GamepadConnected ( g_pad ) )
      return;

   if ( g_pad )
   {
      SDL_CloseGamepad ( g_pad );
      g_pad = NULL;
   }

   {
      int count = 0;
      SDL_JoystickID *ids = SDL_GetGamepads ( &count );

      if ( ids )
      {
         if ( count > 0 )
            g_pad = SDL_OpenGamepad ( ids[0] );
         SDL_free ( ids );
      }
   }
}

/*------------------------------------------------------------------------
   PTR_IsJoyPresent () - Checks to see if an SDL gamepad is connected
  ------------------------------------------------------------------------*/
BOOL
PTR_IsJoyPresent ( VOID )
{
   JoyEnsureOpen ();
   return g_pad ? TRUE : FALSE;
}

/* 4-bit gamepad face-button mask, bit set = pressed */
PRIVATE INT
JoyButtonMask ( VOID )
{
   INT mask = 0;

   if ( !g_pad )
      return 0;

   if ( SDL_GetGamepadButton ( g_pad, SDL_GAMEPAD_BUTTON_SOUTH ) ) mask |= 1;
   if ( SDL_GetGamepadButton ( g_pad, SDL_GAMEPAD_BUTTON_EAST ) )  mask |= 2;
   if ( SDL_GetGamepadButton ( g_pad, SDL_GAMEPAD_BUTTON_WEST ) )  mask |= 4;
   if ( SDL_GetGamepadButton ( g_pad, SDL_GAMEPAD_BUTTON_NORTH ) ) mask |= 8;

   return mask;
}

/*------------------------------------------------------------------------
   PTR_ReadJoyStick () - was raw gameport (0x201) pulse timing; now reads
   the first connected SDL gamepad's left stick + face buttons. Also
   refreshes compat_gameport_state, since IPT_GetButtons (INPUT.C) reads
   the buttons straight off inp(0x201) on its own TSM schedule rather than
   through here.
  ------------------------------------------------------------------------*/
VOID
PTR_ReadJoyStick ( VOID )
{
   INT mask;

   JoyEnsureOpen ();

   if ( !g_pad )
   {
      joy_x = JOY_CENTER;
      joy_y = JOY_CENTER;
      joy_buttons = 0;
      compat_gameport_state = 0xF0;
      return;
   }

   joy_x = JOY_CENTER + ( ( INT ) SDL_GetGamepadAxis ( g_pad, SDL_GAMEPAD_AXIS_LEFTX ) * JOY_RANGE ) / 32768;
   joy_y = JOY_CENTER + ( ( INT ) SDL_GetGamepadAxis ( g_pad, SDL_GAMEPAD_AXIS_LEFTY ) * JOY_RANGE ) / 32768;

   mask = JoyButtonMask ();
   joy_buttons = mask;
   /* IPT_GetButtons shifts right 4 and treats a clear bit as "pressed" */
   compat_gameport_state = ( BYTE ) ( ( ~mask & 0x0F ) << 4 );
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
