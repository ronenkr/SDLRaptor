/****************************************************************************
* plat_video.c - SDL3 presentation of the 320x200 8-bit screen
*----------------------------------------------------------------------------
* displayscreen (the DOS "VGA memory") is a plain 64000-byte buffer.
* Present converts it through the shadow palette into a streaming
* XRGB8888 texture and renders it stretched to 320x240 logical space
* (VGA mode 13h pixels are 1:1.2, so 320x200 -> 4:3 on a 320x240 canvas).
*---------------------------------------------------------------------------*/
#include <SDL3/SDL.h>

#include "types.h"
#include "gfxapi.h"
#include "exitapi.h"
#include "plat.h"

#define WINDOW_SCALE   3     /* default window = 960x720 */
#define LOGICAL_W      320
#define LOGICAL_H      240

PRIVATE SDL_Window   *window   = NULL;
PRIVATE SDL_Renderer *renderer = NULL;
PRIVATE SDL_Texture  *texture  = NULL;

PRIVATE BYTE  shadowpal[768];         /* 6-bit VGA values, as the game sets */
PRIVATE DWORD palette32[256];         /* expanded to XRGB8888              */
PRIVATE BOOL  screendirty = FALSE;

extern BYTE *displayscreen;

PRIVATE VOID
ExpandPalette ( VOID )
{
   INT i;

   for ( i = 0; i < 256; i++ )
   {
      /* 6-bit -> 8-bit with low-bit replication, like real RAMDACs */
      DWORD r = ( shadowpal[i * 3 + 0] & 0x3F );
      DWORD g = ( shadowpal[i * 3 + 1] & 0x3F );
      DWORD b = ( shadowpal[i * 3 + 2] & 0x3F );

      r = ( r << 2 ) | ( r >> 4 );
      g = ( g << 2 ) | ( g >> 4 );
      b = ( b << 2 ) | ( b >> 4 );

      palette32[i] = 0xFF000000u | ( r << 16 ) | ( g << 8 ) | b;
   }
}

VOID
PLAT_CreateWindow ( VOID )
{
   if ( window )
      return;

   if ( !SDL_CreateWindowAndRenderer ( "Raptor: Call of the Shadows",
            LOGICAL_W * WINDOW_SCALE, LOGICAL_H * WINDOW_SCALE,
            SDL_WINDOW_RESIZABLE, &window, &renderer ) )
   {
      EXIT_Error ( "PLAT_CreateWindow: %s", SDL_GetError () );
   }

   SDL_SetRenderLogicalPresentation ( renderer, LOGICAL_W, LOGICAL_H,
            SDL_LOGICAL_PRESENTATION_LETTERBOX );

   texture = SDL_CreateTexture ( renderer, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, SCREENWIDTH, SCREENHEIGHT );
   if ( !texture )
      EXIT_Error ( "PLAT_CreateWindow: texture: %s", SDL_GetError () );

   SDL_SetTextureScaleMode ( texture, SDL_SCALEMODE_NEAREST );

   ExpandPalette ();
   screendirty = TRUE;
   PLAT_Present ();
}

VOID
PLAT_DestroyWindow ( VOID )
{
   if ( texture )  { SDL_DestroyTexture ( texture );   texture = NULL; }
   if ( renderer ) { SDL_DestroyRenderer ( renderer ); renderer = NULL; }
   if ( window )   { SDL_DestroyWindow ( window );     window = NULL; }
}

VOID
PLAT_SetPalette ( BYTE *pal6 )
{
   memcpy ( shadowpal, pal6, 768 );
   ExpandPalette ();
   screendirty = TRUE;
}

VOID
PLAT_GetPalette ( BYTE *pal6 )
{
   memcpy ( pal6, shadowpal, 768 );
}

VOID
PLAT_MarkScreenDirty ( VOID )
{
   screendirty = TRUE;
}

BOOL
PLAT_ScreenDirty ( VOID )
{
   return screendirty;
}

VOID
PLAT_Present ( VOID )
{
   DWORD    *pixels;
   INT       pitch;
   INT       x, y;
   SDL_FRect dst = { 0.0f, 0.0f, ( float )LOGICAL_W, ( float )LOGICAL_H };

   screendirty = FALSE;

   if ( !renderer || !displayscreen )
      return;

   if ( SDL_LockTexture ( texture, NULL, ( void ** ) &pixels, &pitch ) )
   {
      BYTE *src = displayscreen;

      for ( y = 0; y < SCREENHEIGHT; y++ )
      {
         DWORD *row = ( DWORD * )( ( BYTE * ) pixels + y * pitch );

         for ( x = 0; x < SCREENWIDTH; x++ )
            row[x] = palette32[*src++];
      }
      SDL_UnlockTexture ( texture );
   }

   SDL_SetRenderDrawColor ( renderer, 0, 0, 0, 255 );
   SDL_RenderClear ( renderer );
   SDL_RenderTexture ( renderer, texture, NULL, &dst );
   SDL_RenderPresent ( renderer );
}

BOOL
PLAT_WindowToScreen ( float wx, float wy, INT *sx, INT *sy )
{
   float rx, ry;

   if ( !renderer )
      return FALSE;

   SDL_RenderCoordinatesFromWindow ( renderer, wx, wy, &rx, &ry );

   /* logical space is 320x240; the 320x200 screen fills it vertically
      stretched, so map y back from 240 to 200 */
   *sx = ( INT ) rx;
   *sy = ( INT )( ry * SCREENHEIGHT / ( float ) LOGICAL_H );

   if ( *sx < 0 ) *sx = 0;
   if ( *sx > SCREENWIDTH - 1 )  *sx = SCREENWIDTH - 1;
   if ( *sy < 0 ) *sy = 0;
   if ( *sy > SCREENHEIGHT - 1 ) *sy = SCREENHEIGHT - 1;

   return TRUE;
}
