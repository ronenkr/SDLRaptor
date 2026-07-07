/****************************************************************************
* audio_lock.c - misc link-time stubs the compiled audiolib sources need,
* plus the one REAL piece: DisableInterrupts/RestoreInterrupts.
*----------------------------------------------------------------------------
* On DOS these bracketed a critical section against the INT 8 handler.
* In the port, MIDI.C/AL_MIDI.C's shared state (Channel[], Voice[], the
* MIDI track cursors...) is touched both by the main thread (MUS_PlaySong/
* StopSong/SetMasterVolume, called from FX.C) and by the audio callback
* thread (ticking _MIDI_ServiceRoutine). DisableInterrupts/RestoreInterrupts
* are the exact choke points the original code already uses to guard that
* state, so turning them into a real (recursive) mutex is the correct,
* minimal fix - no game or audiolib source code needs to change.
*
* Everything else here is DPMI/Blaster/User-parameter surface that
* AL_MIDI.C/MIDI.C/LL_MAN.C reference but our fixed "always AdLib, always
* present" configuration never actually exercises at runtime - they just
* need to exist so the linker is happy.
*---------------------------------------------------------------------------*/
#include <SDL3/SDL.h>

#include "dpmi.h"
#include "blaster.h"
#include "user.h"

static SDL_Mutex *g_audio_mutex;

static SDL_Mutex *
GetAudioMutex ( void )
{
   static SDL_SpinLock init_lock;

   if ( !g_audio_mutex )
   {
      SDL_LockSpinlock ( &init_lock );
      if ( !g_audio_mutex )
         g_audio_mutex = SDL_CreateMutex ();
      SDL_UnlockSpinlock ( &init_lock );
   }
   return g_audio_mutex;
}

unsigned long
DisableInterrupts ( void )
{
   SDL_LockMutex ( GetAudioMutex () );
   return 0;
}

void
RestoreInterrupts ( unsigned long flags )
{
   SDL_UnlockMutex ( GetAudioMutex () );
}

int
DPMI_LockMemory ( void *address, unsigned length )
{
   return DPMI_Ok;
}

int
DPMI_UnlockMemory ( void *address, unsigned length )
{
   return DPMI_Ok;
}

int
DPMI_LockMemoryRegion ( void *start, void *end )
{
   return DPMI_Ok;
}

int
DPMI_UnlockMemoryRegion ( void *start, void *end )
{
   return DPMI_Ok;
}

int
BLASTER_GetCardSettings ( BLASTER_CONFIG *config )
{
   return BLASTER_Error;         /* never reached: AL_Init(Adlib) skips this */
}

int
BLASTER_GetEnv ( BLASTER_CONFIG *config )
{
   return BLASTER_Error;
}

int
USER_CheckParameter ( const char *parameter )
{
   return 0;
}

char *
USER_GetText ( const char *parameter )
{
   return NULL;
}
