/****************************************************************************
* task_audio.c - replaces audiolib/SOURCE/TASK_MAN.C (PIT/INT8 scheduler)
*----------------------------------------------------------------------------
* The only real consumer of this API in our build is MIDI.C's
* _MIDI_PlayRoutine (see MIDI.C: TS_ScheduleTask/TS_SetTaskRate/TS_Terminate
* around _MIDI_ServiceRoutine) - apodmx/TSMAPI.C and the rest of audiolib
* that also used TS_* are not compiled into this port (GFX's own services
* are driven by port/tsm_svc.c instead, entirely separately).
*
* TaskAudio_Advance() is called from the SDL audio callback (audio_cb.c)
* once per rendered sub-block, so task callbacks - i.e. the MIDI sequencer -
* fire sample-accurately on the audio thread instead of drifting with
* wall-clock jitter. MUS_PlaySong/StopSong (called from the main thread)
* create/destroy tasks via TS_ScheduleTask/TS_Terminate concurrently, so
* every access to the task table is guarded by the same lock as the rest
* of the audio subsystem (DisableInterrupts/RestoreInterrupts, see
* audio_lock.c) - the exact critical sections the original DOS code marked.
*---------------------------------------------------------------------------*/
#include <string.h>
#include <SDL3/SDL.h>

#include "task_man.h"
#include "interrupt.h"

#define MAX_TASKS 8

typedef struct
{
   task     pub;              /* MUST be first: TS_ScheduleTask hands out
                                  &impl->pub, and MIDI.C only ever sees it
                                  as an opaque task*, so casting that
                                  pointer back to task_impl* is safe. */
   Uint64   period_ns;
   Uint64   accum_ns;
   int      in_use;
} task_impl;

static task_impl g_tasks[ MAX_TASKS ];

volatile int TS_InInterrupt = 0;

static Uint64
HzToPeriodNs ( int rate_hz )
{
   if ( rate_hz < 1 )
      rate_hz = 1;
   return ( Uint64 ) SDL_NS_PER_SECOND / ( Uint64 ) rate_hz;
}

task *
TS_ScheduleTask ( void ( *Function )( task * ), int rate, int priority,
                   void *data )
{
   unsigned long flags = DisableInterrupts ();
   int i;

   for ( i = 0; i < MAX_TASKS; i++ )
      if ( !g_tasks[i].in_use )
         break;

   if ( i == MAX_TASKS )
   {
      RestoreInterrupts ( flags );
      return NULL;
   }

   memset ( &g_tasks[i], 0, sizeof ( task_impl ) );
   g_tasks[i].pub.TaskService = Function;
   g_tasks[i].pub.data        = data;
   g_tasks[i].pub.priority    = priority;
   g_tasks[i].pub.active      = 1;
   g_tasks[i].period_ns       = HzToPeriodNs ( rate );
   g_tasks[i].in_use          = 1;

   RestoreInterrupts ( flags );
   return &g_tasks[i].pub;
}

int
TS_Terminate ( task *ptr )
{
   unsigned long flags;
   task_impl    *impl = ( task_impl * ) ptr;

   if ( !ptr )
      return TASK_Warning;

   flags = DisableInterrupts ();
   impl->pub.active = 0;
   impl->in_use     = 0;
   RestoreInterrupts ( flags );
   return TASK_Ok;
}

void
TS_Dispatch ( void )
{
   /* Tasks are active as soon as TS_ScheduleTask creates them in this
      port, so there is nothing pending to activate here. */
}

void
TS_SetTaskRate ( task *Task, int rate )
{
   unsigned long flags;
   task_impl    *impl = ( task_impl * ) Task;

   if ( !Task )
      return;

   flags = DisableInterrupts ();
   impl->period_ns = HzToPeriodNs ( rate );
   RestoreInterrupts ( flags );
}

void
TS_Shutdown ( void )
{
   unsigned long flags = DisableInterrupts ();
   memset ( g_tasks, 0, sizeof ( g_tasks ) );
   RestoreInterrupts ( flags );
}

void
TS_UnlockMemory ( void )
{
}

int
TS_LockMemory ( void )
{
   return TASK_Ok;
}

/*------------------------------------------------------------------------
   TaskAudio_Advance () - fire due tasks for a rendered sub-block
   (called from the SDL audio callback; see audio_cb.c)
  ------------------------------------------------------------------------*/
void
TaskAudio_Advance ( int frames, int samplerate )
{
   unsigned long flags = DisableInterrupts ();
   Uint64 elapsed_ns = ( Uint64 ) frames * SDL_NS_PER_SECOND
                        / ( Uint64 ) samplerate;
   int i;

   for ( i = 0; i < MAX_TASKS; i++ )
   {
      task_impl *t = &g_tasks[i];

      if ( !t->in_use || !t->pub.active )
         continue;

      t->accum_ns += elapsed_ns;
      while ( t->in_use && t->pub.active && t->accum_ns >= t->period_ns )
      {
         t->accum_ns -= t->period_ns;
         t->pub.TaskService ( &t->pub );
      }
   }

   RestoreInterrupts ( flags );
}
