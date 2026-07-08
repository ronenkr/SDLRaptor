/****************************************************************************
* plat.h - internal API of the Win32/SDL3 platform layer
*---------------------------------------------------------------------------*/
#ifndef _PLAT_H
#define _PLAT_H

#include "types.h"

/* Set to 1 to enable the [TRACE] diagnostic prints scattered through the
   port and a few original-source call sites (added while chasing the
   name-entry hang / exit crash); compiled out by default. */
#define RAPTOR_TRACE 0

#if RAPTOR_TRACE
#include <stdio.h>
#define TRACE(...) do { fprintf ( stderr, __VA_ARGS__ ); fflush ( stderr ); } while ( 0 )
#else
#define TRACE(...) ((void)0)
#endif

/* --- plat_video.c ------------------------------------------------------ */
typedef enum
{
   PLAT_SCALER_NONE,       /* nearest-neighbor, straight to logical size */
   PLAT_SCALER_ADVMAME2X   /* Scale2x/AdvMAME2x edge-preserving 2x upscale */
} PLAT_SCALER;

VOID PLAT_CreateWindow ( VOID );          /* GFX_SetVideoMode13            */
VOID PLAT_DestroyWindow ( VOID );         /* GFX_RestoreMode               */
VOID PLAT_SetPalette ( BYTE *pal6 );      /* 768 bytes of 6-bit VGA RGB    */
VOID PLAT_GetPalette ( BYTE *pal6 );
VOID PLAT_MarkScreenDirty ( VOID );       /* displayscreen changed         */
BOOL PLAT_ScreenDirty ( VOID );
VOID PLAT_Present ( VOID );               /* convert + render + present    */
/* window x/y -> 320x200 screen coords (for the mouse); FALSE if no window */
BOOL PLAT_WindowToScreen ( float wx, float wy, INT *sx, INT *sy );
/* must be called before PLAT_CreateWindow (i.e. from main(), via --scaler) */
VOID PLAT_SetScaler ( PLAT_SCALER scaler );
VOID PLAT_ToggleFullscreen ( VOID );      /* Alt+Enter, see plat_pump.c    */

/* --- plat_pump.c -------------------------------------------------------- */
VOID PLAT_Pump ( VOID );                  /* events + services + present   */
INT  GFX_FrameCount ( VOID );             /* 70 Hz tick, pumps + anti-spin */
VOID PLAT_WaitNextTick ( VOID );          /* block until 70 Hz tick edge   */
VOID PLAT_MicroSleep ( VOID );            /* ~0.2ms yield for tight polls  */

/* --- tsm_svc.c ---------------------------------------------------------- */
VOID PLAT_RunServices ( VOID );           /* fire due TSM services         */

/* --- kbd_sdl.c / ptr_sdl.c (event sinks, called from the pump) --------- */
union SDL_Event;
VOID KBD_HandleEvent ( const union SDL_Event *ev );
VOID PTR_HandleEvent ( const union SDL_Event *ev );

/* --- ptr_blit.c: SDL gamepad, called from ptr_sdl.c's PTR_Init --------- */
BOOL PTR_IsJoyPresent ( VOID );

#endif /* _PLAT_H */
