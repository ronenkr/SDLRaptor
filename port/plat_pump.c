/****************************************************************************
* plat_pump.c - event pump, 70 Hz frame clock, and anti-spin waits
*----------------------------------------------------------------------------
* The DOS game paces itself by busy-waiting on FRAME_COUNT (a 70 Hz counter
* the PIT used to increment). Here FRAME_COUNT is the GFX_FrameCount()
* function (see GFXAPI.H): it derives the tick from the wall clock, pumps
* SDL events, runs the TSM services, and - when the game is clearly spinning
* on an unchanged tick - sleeps, so the old busy-wait loops idle at ~0% CPU.
*---------------------------------------------------------------------------*/
#include <SDL3/SDL.h>

#include "types.h"
#include "gfxapi.h"
#include "exitapi.h"
#include "plat.h"

PRIVATE Uint64 clock_start   = 0;
PRIVATE BOOL   in_pump       = FALSE;
PRIVATE Uint64 last_pump_ns  = 0;
PRIVATE INT    last_tick     = -1;
PRIVATE INT    spin_count    = 0;
PRIVATE INT    presented_tick = -1;
PRIVATE BOOL   quitting       = FALSE;

PRIVATE INT
CurrentTick ( VOID )
{
   if ( clock_start == 0 )
      clock_start = SDL_GetTicksNS ();

   return ( INT )( ( SDL_GetTicksNS () - clock_start ) * 70u
                   / SDL_NS_PER_SECOND );
}

VOID
PLAT_Pump ( VOID )
{
   SDL_Event ev;
   Uint64    now;

   if ( in_pump )
      return;
   in_pump = TRUE;

   now = SDL_GetTicksNS ();

   /* Throttle the full pump to once per millisecond - the game polls
      FRAME_COUNT tens of thousands of times per frame. */
   if ( now - last_pump_ns >= SDL_NS_PER_MS )
   {
      last_pump_ns = now;

      while ( SDL_PollEvent ( &ev ) )
      {
         switch ( ev.type )
         {
            case SDL_EVENT_QUIT:
               /* EXIT_Clean -> ShutDown -> WIN_Order shows an exit screen
                  that itself waits on input via GFX_FrameCount/PLAT_Pump,
                  so it must be reentered here (hence dropping in_pump)
                  rather than deferred - but that also means a second QUIT
                  event (closing the window again while that screen is up)
                  would recurse into EXIT_Clean while the first call is
                  still mid-teardown, corrupting window/GLB state. Only
                  ever act on the first one. */
               if ( !quitting )
               {
                  quitting = TRUE;
                  in_pump = FALSE;
                  EXIT_Clean ();
               }
               break;

            case SDL_EVENT_KEY_DOWN:
               /* Alt+Enter: fullscreen toggle, not a game key - swallow it
                  here so it never reaches KBD_HandleEvent/keyboard[]. */
               if ( ev.key.scancode == SDL_SCANCODE_RETURN
                    && ( ev.key.mod & SDL_KMOD_ALT ) && !ev.key.repeat )
               {
                  PLAT_ToggleFullscreen ();
                  break;
               }
               KBD_HandleEvent ( ( const union SDL_Event * ) &ev );
               break;

            case SDL_EVENT_KEY_UP:
               KBD_HandleEvent ( ( const union SDL_Event * ) &ev );
               break;

            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
               PTR_HandleEvent ( ( const union SDL_Event * ) &ev );
               break;

            default:
               break;
         }
      }

      PLAT_RunServices ();

      /* Catch stray writes to displayscreen (there are a few places the
         game pokes it without an explicit blit): present once per tick. */
      if ( PLAT_ScreenDirty () )
      {
         INT tick = CurrentTick ();

         if ( tick != presented_tick )
         {
            presented_tick = tick;
            PLAT_Present ();
         }
      }
   }

   in_pump = FALSE;
}

INT
GFX_FrameCount ( VOID )
{
   INT tick;

   PLAT_Pump ();

   tick = CurrentTick ();

   if ( tick == last_tick )
   {
      /* The caller is almost certainly in a "while (FRAME_COUNT == x)"
         busy-wait; yield after a few same-tick polls. */
      if ( ++spin_count >= 3 )
         SDL_DelayNS ( 500 * 1000 );      /* 0.5 ms */
   }
   else
   {
      last_tick = tick;
      spin_count = 0;
   }

   return tick;
}

VOID
PLAT_MicroSleep ( VOID )
{
   /* Used by unpaced busy-wait loops (IMS_CheckAck/IMS_IsAck have no
      FRAME_COUNT gate of their own) so they poll at ~5kHz instead of
      pegging a core while waiting on user input. */
   SDL_DelayNS ( 200 * 1000 );
}

VOID
PLAT_WaitNextTick ( VOID )
{
   INT start = CurrentTick ();

   while ( CurrentTick () == start )
   {
      PLAT_Pump ();
      SDL_DelayNS ( 500 * 1000 );
   }
}
