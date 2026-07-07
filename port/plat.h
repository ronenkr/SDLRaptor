/****************************************************************************
* plat.h - internal API of the Win32/SDL3 platform layer
*---------------------------------------------------------------------------*/
#ifndef _PLAT_H
#define _PLAT_H

#include "types.h"

/* --- plat_video.c ------------------------------------------------------ */
VOID PLAT_CreateWindow ( VOID );          /* GFX_SetVideoMode13            */
VOID PLAT_DestroyWindow ( VOID );         /* GFX_RestoreMode               */
VOID PLAT_SetPalette ( BYTE *pal6 );      /* 768 bytes of 6-bit VGA RGB    */
VOID PLAT_GetPalette ( BYTE *pal6 );
VOID PLAT_MarkScreenDirty ( VOID );       /* displayscreen changed         */
BOOL PLAT_ScreenDirty ( VOID );
VOID PLAT_Present ( VOID );               /* convert + render + present    */
/* window x/y -> 320x200 screen coords (for the mouse); FALSE if no window */
BOOL PLAT_WindowToScreen ( float wx, float wy, INT *sx, INT *sy );

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

#endif /* _PLAT_H */
