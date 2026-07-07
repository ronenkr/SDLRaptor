/****************************************************************************
* tsm_svc.c - replaces GFX/TSMAPI.C (timer services without the DOS PIT)
*----------------------------------------------------------------------------
* On DOS the TSM services fired from the INT 8 chain. Here PLAT_RunServices
* is called from the event pump on the main thread and fires each service
* at its registered rate with catch-up, so semantics stay close to the
* original without any threading. Registered services in this game:
*   GFX_TimeFrameRate @70 (harmless no-op now - FRAME_COUNT is clock-derived)
*   PTR_UpdateCursor  @15 (cursor draw/erase)
*   IPT_GetButtons    @26 (input latch)
*---------------------------------------------------------------------------*/
#include <string.h>
#include <SDL3/SDL.h>

#include "types.h"
#include "tsmapi.h"
#include "plat.h"

#define MAX_TASKS 8

typedef struct
{
   void   ( *callback )( void );
   int    rate;            /* calls per second            */
   int    paused;
   int    active;
   Uint64 next_due;        /* SDL_GetTicksNS timestamp    */
} task_t;

PRIVATE task_t tasks[MAX_TASKS];

void
TSM_Install ( int rate )
{
   memset ( tasks, 0, sizeof ( tasks ) );
}

int
TSM_NewService ( void ( *function )( void ), int rate, int priority, int pause )
{
   int i;

   for ( i = 0; i < MAX_TASKS; i++ )
   {
      if ( !tasks[i].active )
         break;
   }
   if ( i == MAX_TASKS || rate < 1 )
      return -1;

   tasks[i].callback = function;
   tasks[i].rate     = rate;
   tasks[i].paused   = pause;
   tasks[i].active   = 1;
   tasks[i].next_due = SDL_GetTicksNS () + SDL_NS_PER_SECOND / rate;
   return i;
}

void
TSM_DelService ( int id )
{
   if ( id >= 0 && id < MAX_TASKS )
      tasks[id].active = 0;
}

void
TSM_PauseService ( int id )
{
   if ( id >= 0 && id < MAX_TASKS )
      tasks[id].paused = 1;
}

void
TSM_ResumeService ( int id )
{
   if ( id >= 0 && id < MAX_TASKS )
   {
      tasks[id].paused   = 0;
      tasks[id].next_due = SDL_GetTicksNS () +
                           SDL_NS_PER_SECOND / tasks[id].rate;
   }
}

void
TSM_Remove ( void )
{
   memset ( tasks, 0, sizeof ( tasks ) );
}

VOID
PLAT_RunServices ( VOID )
{
   Uint64 now = SDL_GetTicksNS ();
   int    i;

   for ( i = 0; i < MAX_TASKS; i++ )
   {
      task_t *t = &tasks[i];
      Uint64  period;

      if ( !t->active || t->paused )
         continue;

      period = SDL_NS_PER_SECOND / t->rate;

      /* If we fell far behind (window drag, debugger), skip ahead
         instead of firing a burst of stale callbacks. */
      if ( now > t->next_due + 4 * period )
         t->next_due = now;

      while ( t->active && !t->paused && now >= t->next_due )
      {
         t->next_due += period;
         t->callback ();
      }
   }
}
