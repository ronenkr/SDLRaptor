/****************************************************************************
* dmx_music.c - replaces the MUS_* half of apodmx/DMX.C, driving MIDI.C's
* sequencer directly (skipping audiolib's MUSIC.C, which drags in every
* hardware driver). MUS format songs are converted to standard MIDI via
* apodmx/MUS2MID.C (compiled unmodified) through a pair of temp files -
* exactly the approach the original DMX_Init used, just with MIDI_PlaySong
* in place of MUSIC_PlaySong.
*
* Fades are a small linear volume ramp ticked once per audio sub-block
* (MusicFade_Advance, called from audio_cb.c) rather than a TS_* task -
* simpler than juggling a task that would need to terminate itself.
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "dmx.h"
#include "music.h"          /* defines songposition, used by midi.h */
#include "midi.h"
#include "al_midi.h"
#include "mus2mid.h"
#include "audio_internal.h"
#include "plat.h"

extern int AL_Init ( int soundcard );

/* MIDI.C's EMIDI instrument-set selection reads this global directly
   (normally set by audiolib's MUSIC.C, which we don't compile). */
int MUSIC_SoundDevice = Adlib;

static unsigned char *g_mus_data     = NULL;   /* converted MIDI bytes, owned */
static int            g_mus_loop     = 0;
static int            g_mus_active   = 0;
static int            g_mus_fadeout  = 0;
static int            g_mastervolume = 127;
static int            g_mus_rate     = 140;    /* overridden by DmxMusic_SetRate */
static int            g_funcs_ready  = 0;
static midifuncs       g_midifuncs;

typedef struct
{
   int    active;
   int    from_vol;
   int    to_vol;
   Uint64 elapsed_ns;
   Uint64 duration_ns;
} fade_state;

static fade_state g_fade;

static void
EnsureMidiFuncsInstalled ( void )
{
   if ( g_funcs_ready )
      return;

   memset ( &g_midifuncs, 0, sizeof ( g_midifuncs ) );
   g_midifuncs.NoteOff       = AL_NoteOff;
   g_midifuncs.NoteOn        = AL_NoteOn;
   g_midifuncs.ControlChange = AL_ControlChange;
   g_midifuncs.ProgramChange = AL_ProgramChange;
   g_midifuncs.PitchBend     = AL_SetPitchBend;
   /* PolyAftertouch/ChannelAftertouch/ReleasePatches/LoadPatch/SetVolume/
      GetVolume have no FM equivalent - MIDI.C null-checks each before use,
      matching audiolib MUSIC.C's own MUSIC_InitFM wiring for the Adlib
      case (Funcs->SetVolume = Funcs->GetVolume = NULL there too). */

   MIDI_SetMidiFuncs ( &g_midifuncs );
   g_funcs_ready = 1;
}

/*------------------------------------------------------------------------
   MUS_RegisterSong () - converts a MUS-format GLB item to MIDI bytes
  ------------------------------------------------------------------------*/
int
MUS_RegisterSong ( void *data )
{
   FILE *mus, *mid;
   long  midlen;

   TRACE ( "[TRACE] MUS_RegisterSong data=%p\n", data  );

   if ( g_mus_data )
   {
      free ( g_mus_data );
      g_mus_data = NULL;
   }

   if ( memcmp ( data, "MThd", 4 ) == 0 )
   {
      /* Already a MIDI file - not expected from Raptor's GLB data, but
         apodmx's original handled it, so keep parity. Caller retains
         ownership; MIDI_PlaySong only reads it while playing, and the
         game keeps the GLB item locked for that whole time. */
      g_mus_data = NULL;
      return 0;
   }

   mus = fopen ( "temp.mus", "wb" );
   if ( !mus )
      return 0;
   fwrite ( data, 1, ( ( unsigned short * ) data )[2] +
                      ( ( unsigned short * ) data )[3], mus );
   fclose ( mus );

   mus = fopen ( "temp.mus", "rb" );
   if ( !mus )
      return 0;
   mid = fopen ( "temp.mid", "wb" );
   if ( !mid )
   {
      fclose ( mus );
      return 0;
   }

   if ( mus2mid ( mus, mid, g_mus_rate, 1 /* adlibhack: always on, FM-only */ ) )
   {
      fclose ( mid );
      fclose ( mus );
      remove ( "temp.mus" );
      remove ( "temp.mid" );
      return 0;
   }
   fclose ( mid );
   fclose ( mus );

   mid = fopen ( "temp.mid", "rb" );
   if ( !mid )
   {
      remove ( "temp.mus" );
      return 0;
   }
   fseek ( mid, 0, SEEK_END );
   midlen = ftell ( mid );
   rewind ( mid );

   g_mus_data = ( unsigned char * ) malloc ( midlen );
   if ( !g_mus_data )
   {
      fclose ( mid );
      remove ( "temp.mus" );
      remove ( "temp.mid" );
      return 0;
   }
   fread ( g_mus_data, 1, midlen, mid );
   fclose ( mid );

   remove ( "temp.mus" );
   remove ( "temp.mid" );

   return 0;
}

int
MUS_UnregisterSong ( int handle )
{
   TRACE ( "[TRACE] MUS_UnregisterSong data=%p\n", g_mus_data  );

   if ( g_mus_data )
   {
      free ( g_mus_data );
      g_mus_data = NULL;
   }
   return 0;
}

int
MUS_ChainSong ( int handle, int next )
{
   g_mus_loop = ( next == handle );
   return 0;
}

int
MUS_PlaySong ( int handle, int volume )
{
   int status;

   TRACE ( "[TRACE] MUS_PlaySong data=%p\n", g_mus_data  );

   if ( !g_mus_data )
      return 1;

   EnsureMidiFuncsInstalled ();
   status = MIDI_PlaySong ( g_mus_data, g_mus_loop );
   TRACE ( "[TRACE] MIDI_PlaySong returned %d\n", status  );
   if ( status == MIDI_Ok )
   {
      g_mus_active  = 1;
      g_mus_fadeout = 0;
      g_fade.active = 0;
      MIDI_SetVolume ( g_mastervolume * 2 );
   }
   return ( status != MIDI_Ok );
}

int
MUS_FadeInSong ( int handle, int ms )
{
   TRACE ( "[TRACE] MUS_FadeInSong data=%p ms=%d\n", g_mus_data, ms  );

   if ( !g_mus_data )
      return 1;

   EnsureMidiFuncsInstalled ();
   MIDI_SetVolume ( 0 );

   g_fade.active      = 1;
   g_fade.from_vol    = 0;
   g_fade.to_vol      = g_mastervolume * 2;
   g_fade.elapsed_ns   = 0;
   g_fade.duration_ns  = ( Uint64 ) ms * 1000000ull;

   {
      int status = MIDI_PlaySong ( g_mus_data, g_mus_loop );

      if ( status == MIDI_Ok )
      {
         g_mus_active  = 1;
         g_mus_fadeout = 0;
      }
      return ( status != MIDI_Ok );
   }
}

int
MUS_FadeOutSong ( int handle, int ms )
{
   if ( !g_mus_active )
      return 1;

   g_fade.active     = 1;
   g_fade.from_vol   = MIDI_GetVolume ();
   g_fade.to_vol     = 0;
   g_fade.elapsed_ns  = 0;
   g_fade.duration_ns = ( Uint64 ) ms * 1000000ull;

   g_mus_fadeout = 1;
   return 0;
}

int
MUS_QrySongPlaying ( int handle )
{
   if ( g_mus_active )
   {
      if ( g_mus_fadeout && !g_fade.active )
      {
         MIDI_StopSong ();
         g_mus_active  = 0;
         g_mus_fadeout = 0;
      }
      else if ( !MIDI_SongPlaying () )
      {
         g_mus_active  = 0;
         g_mus_fadeout = 0;
      }
   }
   return g_mus_active;
}

int
MUS_StopSong ( int handle )
{
   TRACE ( "[TRACE] MUS_StopSong data=%p\n", g_mus_data  );
   MIDI_StopSong ();
   TRACE ( "[TRACE] MIDI_StopSong done\n"  );
   g_mus_active  = 0;
   g_mus_fadeout = 0;
   g_fade.active = 0;
   return 0;
}

void
MUS_PauseSong ( int handle )
{
   MIDI_PauseSong ();
}

void
MUS_ResumeSong ( int handle )
{
   MIDI_ContinueSong ();
}

void
DmxMusic_SetRate ( int rate )
{
   if ( rate > 0 )
      g_mus_rate = rate;
}

void
MUS_SetMasterVolume ( int volume )
{
   g_mastervolume = volume;
   if ( g_funcs_ready )
      MIDI_SetVolume ( volume * 2 );
}

/*------------------------------------------------------------------------
   MusicFade_Advance () - linear volume ramp, ticked from the audio
   callback thread once per rendered sub-block.
  ------------------------------------------------------------------------*/
void
MusicFade_Advance ( int frames, int samplerate )
{
   if ( !g_fade.active )
      return;

   g_fade.elapsed_ns += ( Uint64 ) frames * SDL_NS_PER_SECOND
                         / ( Uint64 ) samplerate;

   if ( g_fade.elapsed_ns >= g_fade.duration_ns )
   {
      MIDI_SetVolume ( g_fade.to_vol );
      g_fade.active = 0;
   }
   else
   {
      double t = ( double ) g_fade.elapsed_ns / ( double ) g_fade.duration_ns;
      int    vol = g_fade.from_vol +
                   ( int ) ( ( g_fade.to_vol - g_fade.from_vol ) * t );

      MIDI_SetVolume ( vol );
   }
}
