/****************************************************************************
* plat_video.c - SDL3 presentation of the 320x200 8-bit screen
*----------------------------------------------------------------------------
* displayscreen (the DOS "VGA memory") is a plain 64000-byte buffer.
* Present converts it through the shadow palette into a streaming
* XRGB8888 texture and renders it stretched to 320x240 logical space
* (VGA mode 13h pixels are 1:1.2, so 320x200 -> 4:3 on a 320x240 canvas).
*---------------------------------------------------------------------------*/
#include <string.h>
#include <SDL3/SDL.h>

#include "types.h"
#include "gfxapi.h"
#include "exitapi.h"
#include "plat.h"

#define WINDOW_SCALE   2     /* default window = 640x480 */
#define LOGICAL_W      320
#define LOGICAL_H      240

#define SHOP_W 640
#define SHOP_H 480

PRIVATE SDL_Window   *window   = NULL;
PRIVATE SDL_Renderer *renderer = NULL;
PRIVATE SDL_Texture  *texture  = NULL;
PRIVATE SDL_Texture  *shop_texture = NULL;

PRIVATE BYTE  shadowpal[768];         /* 6-bit VGA values, as the game sets */
PRIVATE DWORD palette32[256];         /* expanded to XRGB8888              */
PRIVATE BOOL  screendirty = FALSE;
PRIVATE PLAT_SCALER g_scaler = PLAT_SCALER_NONE;

/* palette-expanded 320x200 frame, built every Present before either a
   straight copy (no scaler) or Scale2x reads it */
PRIVATE DWORD scratch_rgb[SCREENWIDTH * SCREENHEIGHT];

/* same shape, but for the shop screen's native 640x480 buffer */
PRIVATE DWORD shop_scratch_rgb[SHOP_W * SHOP_H];

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

/*------------------------------------------------------------------------
   Scale2x() - AdvMAME2x/Scale2x edge-preserving 2x upscale.
   For each source pixel E with 4-neighbors B(up)/H(down)/D(left)/F(right):
      E0=D  if D==B && B!=F && D!=H   (else E)   -> top-left
      E1=F  if B==F && B!=D && F!=H   (else E)   -> top-right
      E2=D  if D==H && D!=B && H!=F   (else E)   -> bottom-left
      E3=F  if H==F && D!=H && B!=F   (else E)   -> bottom-right
   Edge pixels clamp to the source edge instead of sampling out of bounds.
  ------------------------------------------------------------------------*/
PRIVATE VOID
Scale2x (
const DWORD *src,          // INPUT : w*h source pixels
INT w,                     // INPUT : source width
INT h,                     // INPUT : source height
DWORD *dst,                // OUTPUT: 2w*2h destination pixels
INT dst_pitch_px           // INPUT : dst row stride, in pixels
)
{
   INT x, y;

   for ( y = 0; y < h; y++ )
   {
      const DWORD *srow  = src + y * w;
      const DWORD *srowU = src + ( y > 0     ? y - 1 : y ) * w;
      const DWORD *srowD = src + ( y < h - 1 ? y + 1 : y ) * w;
      DWORD       *drow0 = dst + ( y * 2 )     * dst_pitch_px;
      DWORD       *drow1 = dst + ( y * 2 + 1 ) * dst_pitch_px;

      for ( x = 0; x < w; x++ )
      {
         DWORD B = srowU[x];
         DWORD H = srowD[x];
         DWORD D = srow[ x > 0     ? x - 1 : x ];
         DWORD F = srow[ x < w - 1 ? x + 1 : x ];
         DWORD E = srow[x];
         DWORD E0 = E, E1 = E, E2 = E, E3 = E;

         if ( D == B && B != F && D != H ) E0 = D;
         if ( B == F && B != D && F != H ) E1 = F;
         if ( D == H && D != B && H != F ) E2 = D;
         if ( H == F && D != H && B != F ) E3 = F;

         drow0[x * 2]     = E0;
         drow0[x * 2 + 1] = E1;
         drow1[x * 2]     = E2;
         drow1[x * 2 + 1] = E3;
      }
   }
}

VOID
PLAT_SetScaler (
PLAT_SCALER scaler
)
{
   g_scaler = scaler;
}

VOID
PLAT_ToggleFullscreen ( VOID )
{
   if ( !window )
      return;

   SDL_SetWindowFullscreen ( window,
      ( SDL_GetWindowFlags ( window ) & SDL_WINDOW_FULLSCREEN ) ? false : true );
}

VOID
PLAT_CreateWindow ( VOID )
{
   INT tex_scale = ( g_scaler == PLAT_SCALER_ADVMAME2X ) ? 2 : 1;

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
            SDL_TEXTUREACCESS_STREAMING,
            SCREENWIDTH * tex_scale, SCREENHEIGHT * tex_scale );
   if ( !texture )
      EXIT_Error ( "PLAT_CreateWindow: texture: %s", SDL_GetError () );

   SDL_SetTextureScaleMode ( texture, SDL_SCALEMODE_NEAREST );

   /* the game draws its own cursor (PTR_DrawCursor); hide the OS one so
      they don't both show at once */
   SDL_HideCursor ();

   ExpandPalette ();
   screendirty = TRUE;
   PLAT_Present ();
}

VOID
PLAT_DestroyWindow ( VOID )
{
   if ( shop_texture ) { SDL_DestroyTexture ( shop_texture ); shop_texture = NULL; }
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

   {
      BYTE *src = displayscreen;

      for ( y = 0; y < SCREENHEIGHT; y++ )
      {
         DWORD *row = scratch_rgb + y * SCREENWIDTH;

         for ( x = 0; x < SCREENWIDTH; x++ )
            row[x] = palette32[*src++];
      }
   }

   if ( SDL_LockTexture ( texture, NULL, ( void ** ) &pixels, &pitch ) )
   {
      INT pitch_px = pitch / ( INT ) sizeof ( DWORD );

      if ( g_scaler == PLAT_SCALER_ADVMAME2X )
      {
         Scale2x ( scratch_rgb, SCREENWIDTH, SCREENHEIGHT, pixels, pitch_px );
      }
      else
      {
         for ( y = 0; y < SCREENHEIGHT; y++ )
            memcpy ( ( BYTE * ) pixels + y * pitch,
                     scratch_rgb + y * SCREENWIDTH,
                     SCREENWIDTH * sizeof ( DWORD ) );
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

/*--------------------------------------------------------------------------
   Shop screen: a second, independent presentation path at a native 640x480
   (i.e. the whole game's assets are 320x200/240-native and get an exact 2x
   nearest-neighbor stretch to fill the default 640x480 window - the shop
   screen instead owns a genuine 640x480 8bpp buffer, so it gets twice the
   addressable layout room instead of just a bigger picture of the same
   320x200 grid). Both paths share the same palette state (shadowpal /
   palette32), since both buffers are just palette indices.
  --------------------------------------------------------------------------*/
VOID
PLAT_ShopBegin ( VOID )
{
   if ( !renderer )
      return;

   if ( !shop_texture )
   {
      shop_texture = SDL_CreateTexture ( renderer, SDL_PIXELFORMAT_XRGB8888,
               SDL_TEXTUREACCESS_STREAMING, SHOP_W, SHOP_H );
      if ( !shop_texture )
         EXIT_Error ( "PLAT_ShopBegin: texture: %s", SDL_GetError () );

      SDL_SetTextureScaleMode ( shop_texture, SDL_SCALEMODE_NEAREST );
   }

   SDL_SetRenderLogicalPresentation ( renderer, SHOP_W, SHOP_H,
            SDL_LOGICAL_PRESENTATION_LETTERBOX );
}

VOID
PLAT_ShopEnd ( VOID )
{
   if ( !renderer )
      return;

   SDL_SetRenderLogicalPresentation ( renderer, LOGICAL_W, LOGICAL_H,
            SDL_LOGICAL_PRESENTATION_LETTERBOX );
}

VOID
PLAT_ShopPresent ( BYTE *buf640x480 )
{
   DWORD    *pixels;
   INT       pitch;
   INT       x, y;
   SDL_FRect dst = { 0.0f, 0.0f, ( float ) SHOP_W, ( float ) SHOP_H };

   if ( !renderer || !shop_texture || !buf640x480 )
      return;

   {
      BYTE *src = buf640x480;

      for ( y = 0; y < SHOP_H; y++ )
      {
         DWORD *row = shop_scratch_rgb + y * SHOP_W;

         for ( x = 0; x < SHOP_W; x++ )
            row[x] = palette32[*src++];
      }
   }

   if ( SDL_LockTexture ( shop_texture, NULL, ( void ** ) &pixels, &pitch ) )
   {
      for ( y = 0; y < SHOP_H; y++ )
         memcpy ( ( BYTE * ) pixels + y * pitch,
                  shop_scratch_rgb + y * SHOP_W,
                  SHOP_W * sizeof ( DWORD ) );
      SDL_UnlockTexture ( shop_texture );
   }

   SDL_SetRenderDrawColor ( renderer, 0, 0, 0, 255 );
   SDL_RenderClear ( renderer );
   SDL_RenderTexture ( renderer, shop_texture, NULL, &dst );
   SDL_RenderPresent ( renderer );
}

BOOL
PLAT_ShopWindowToScreen ( float wx, float wy, INT *sx, INT *sy )
{
   float rx, ry;

   if ( !renderer )
      return FALSE;

   SDL_RenderCoordinatesFromWindow ( renderer, wx, wy, &rx, &ry );

   *sx = ( INT ) rx;
   *sy = ( INT ) ry;

   if ( *sx < 0 ) *sx = 0;
   if ( *sx > SHOP_W - 1 ) *sx = SHOP_W - 1;
   if ( *sy < 0 ) *sy = 0;
   if ( *sy > SHOP_H - 1 ) *sy = SHOP_H - 1;

   return TRUE;
}
