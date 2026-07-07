/****************************************************************************
* audio_cb.c - owns the SDL audio device and the OPL3 chip; the callback
* is the single point where the MIDI sequencer clock, the OPL synth, and
* the digital SFX mixer come together each render sub-block.
*----------------------------------------------------------------------------
* Render order per sub-block, matching the original DOS signal path
* (INT 8 timer tick -> AL_MIDI register writes -> OPL chip -> DAC):
*   1. TaskAudio_Advance()  - fires _MIDI_ServiceRoutine as many times as
*      due; it calls straight into AL_NoteOn/Off/ControlChange/... which
*      write OPL registers via outp(0x388/0x389) -> OplWriteHook below.
*   2. MusicFade_Advance()  - steps the fade-in/out volume ramp, if any.
*   3. OPL3_GenerateStream() - synthesizes this sub-block from current
*      chip state (reflecting whatever notes just changed in step 1).
*   4. SfxMixer_Render()    - additively mixes digital sound effects on
*      top of the FM output.
* Doing this in fixed small sub-blocks (rather than one big chunk) is what
* makes MIDI timing sample-accurate instead of only accurate to the
* callback's buffer size.
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "conio.h"                 /* compat_opl_write hook (port/compat) */
#include "opl3.h"
#include "audio_internal.h"

#define MIXER_RATE       49716     /* native OPL clock / sample rate      */
#define SUBBLOCK_FRAMES  128

static opl3_chip        g_opl;
static SDL_AudioStream *g_stream      = NULL;
static SDL_AudioDeviceID g_device     = 0;
static int               g_opl_addr   = 0;
static int               g_ready      = 0;

static void
OplWriteHook ( int port, unsigned char value )
{
   if ( port == 0x388 )
      g_opl_addr = value;
   else if ( port == 0x389 )
      OPL3_WriteReg ( &g_opl, g_opl_addr, value );
}

static void SDLCALL
AudioCallback ( void *userdata, SDL_AudioStream *stream,
                int additional_amount, int total_amount )
{
   static int16_t opl_buf[ SUBBLOCK_FRAMES * 2 ];
   static float   mix_buf[ SUBBLOCK_FRAMES * 2 ];
   const int      bytes_per_frame = 2 * ( int ) sizeof ( float );
   int            frames_needed   = additional_amount / bytes_per_frame;

   while ( frames_needed > 0 )
   {
      int chunk = ( frames_needed > SUBBLOCK_FRAMES )
                  ? SUBBLOCK_FRAMES : frames_needed;
      int i;

      TaskAudio_Advance ( chunk, MIXER_RATE );
      MusicFade_Advance ( chunk, MIXER_RATE );

      OPL3_GenerateStream ( &g_opl, opl_buf, ( uint32_t ) chunk );

      for ( i = 0; i < chunk * 2; i++ )
         mix_buf[i] = opl_buf[i] / 32768.0f;

      SfxMixer_Render ( mix_buf, chunk );

      SDL_PutAudioStreamData ( stream, mix_buf, chunk * bytes_per_frame );

      frames_needed -= chunk;
   }
}

int
AudioCB_Init ( void )
{
   SDL_AudioSpec spec;

   if ( g_ready )
      return 1;

   memset ( &g_opl, 0, sizeof ( g_opl ) );
   OPL3_Reset ( &g_opl, MIXER_RATE );

   compat_opl_write = OplWriteHook;

   spec.format   = SDL_AUDIO_F32;
   spec.channels = 2;
   spec.freq     = MIXER_RATE;

   g_stream = SDL_OpenAudioDeviceStream ( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                           &spec, AudioCallback, NULL );
   if ( !g_stream )
   {
      printf ( "AudioCB_Init: SDL_OpenAudioDeviceStream failed: %s\n",
               SDL_GetError () );
      return 0;
   }

   g_device = SDL_GetAudioStreamDevice ( g_stream );
   if ( !SDL_ResumeAudioStreamDevice ( g_stream ) )
      printf ( "AudioCB_Init: SDL_ResumeAudioStreamDevice failed: %s\n",
               SDL_GetError () );

   g_ready = 1;
   return 1;
}

void
AudioCB_Shutdown ( void )
{
   if ( !g_ready )
      return;

   compat_opl_write = NULL;

   if ( g_stream )
   {
      SDL_DestroyAudioStream ( g_stream );
      g_stream = NULL;
   }

   g_ready = 0;
}
