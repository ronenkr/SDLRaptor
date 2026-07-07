/****************************************************************************
* sfx_mixer.c - digital sound-effect voice mixer
*----------------------------------------------------------------------------
* Parses the DMX digital-patch format used by the GLB *_FX items (matches
* apodmx/DMX.C's SFX_PlayPatch: 8-byte header - type WORD (must be 3),
* rate WORD, length DWORD - followed by 8-bit unsigned mono PCM at +24,
* running (len-32) bytes) and mixes up to MAX_VOICES of them with a
* nearest-neighbour resampler into the shared render buffer built by
* audio_cb.c.
*
* Called from both threads: SFX_PlayPatch/StopPatch/Playing/SetOrigin run
* on the main thread (FX.C); SfxMixer_Render runs on the audio callback
* thread. The voice table is guarded by the same audio lock as the MIDI
* side (DisableInterrupts/RestoreInterrupts, audio_lock.c) - one lock for
* the whole audio subsystem keeps this simple.
*---------------------------------------------------------------------------*/
#include <math.h>
#include <string.h>

#include "interrupt.h"

#define MAX_VOICES     8
#define MIXER_RATE     49716

typedef struct
{
   int            active;
   const unsigned char *data;   /* GLB-locked memory; caller owns lifetime */
   unsigned long  length;       /* in samples                              */
   unsigned long  pos_frac;     /* 16.16 fixed-point read position          */
   unsigned long  step_frac;    /* 16.16 fixed-point playback step          */
   float          left_gain;
   float          right_gain;
   int            priority;
   int            generation;   /* to make handles unique across reuse      */
} sfx_voice;

static sfx_voice g_voices[ MAX_VOICES ];
static int       g_generation = 1;

static void
ComputeGains ( int sep, int vol, float *left, float *right )
{
   float gain = ( vol < 0 ) ? 0.0f : ( vol > 127 ? 1.0f : vol / 127.0f );
   float t;

   if ( sep < 1 ) sep = 1;
   if ( sep > 255 ) sep = 255;
   t = ( sep - 1 ) / 254.0f;         /* 0 = full left, 1 = full right */

   *left  = gain * ( 1.0f - t );
   *right = gain * t;
}

static unsigned long
ComputeStep ( int rate, int pitch )
{
   int    cents = ( ( pitch - 128 ) * 2400 ) / 128;
   double ratio = pow ( 2.0, cents / 1200.0 );
   double step  = ( rate * ratio / ( double ) MIXER_RATE ) * 65536.0;

   if ( step < 1.0 ) step = 1.0;
   return ( unsigned long ) step;
}

int
SfxMixer_PlayPatch ( void *vdata, int pitch, int sep, int vol, int priority )
{
   const unsigned char *data = ( const unsigned char * ) vdata;
   unsigned int   type;
   unsigned int   rate;
   unsigned long  len;
   unsigned long  flags;
   int   i;
   int   slot;
   int   lowest_pri;
   int   handle;

   type = data[0] | ( data[1] << 8 );
   if ( type != 3 )
      return -1;                    /* only the digital format is supported */

   rate = data[2] | ( data[3] << 8 );
   len  = ( unsigned long ) data[4] | ( ( unsigned long ) data[5] << 8 ) |
          ( ( unsigned long ) data[6] << 16 ) | ( ( unsigned long ) data[7] << 24 );

   if ( len <= 32 )
      return -1;
   len -= 32;

   flags = DisableInterrupts ();

   slot = -1;
   lowest_pri = 0x7fffffff;
   for ( i = 0; i < MAX_VOICES; i++ )
   {
      if ( !g_voices[i].active )
      {
         slot = i;
         break;
      }
      if ( g_voices[i].priority < lowest_pri )
      {
         lowest_pri = g_voices[i].priority;
         slot = i;
      }
   }

   if ( slot < 0 || ( g_voices[slot].active && lowest_pri > priority ) )
   {
      RestoreInterrupts ( flags );
      return -1;                    /* every voice busy with higher priority */
   }

   g_voices[slot].active    = 1;
   g_voices[slot].data      = data + 24;
   g_voices[slot].length    = len;
   g_voices[slot].pos_frac  = 0;
   g_voices[slot].step_frac = ComputeStep ( ( int ) rate, pitch );
   g_voices[slot].priority  = priority;
   g_voices[slot].generation = ++g_generation;
   ComputeGains ( sep, vol, &g_voices[slot].left_gain, &g_voices[slot].right_gain );

   handle = slot | ( g_voices[slot].generation << 8 );

   RestoreInterrupts ( flags );
   return handle;
}

static sfx_voice *
FindVoice ( int handle )
{
   int slot = handle & 0xff;

   if ( handle < 0 || slot >= MAX_VOICES )
      return NULL;
   if ( g_voices[slot].generation != ( handle >> 8 ) )
      return NULL;
   return &g_voices[slot];
}

void
SfxMixer_StopPatch ( int handle )
{
   unsigned long flags = DisableInterrupts ();
   sfx_voice *v = FindVoice ( handle );

   if ( v )
      v->active = 0;
   RestoreInterrupts ( flags );
}

int
SfxMixer_Playing ( int handle )
{
   unsigned long flags = DisableInterrupts ();
   sfx_voice *v = FindVoice ( handle );
   int active = v ? v->active : 0;

   RestoreInterrupts ( flags );
   return active;
}

void
SfxMixer_SetOrigin ( int handle, int pitch, int sep, int vol )
{
   unsigned long flags = DisableInterrupts ();
   sfx_voice *v = FindVoice ( handle );

   if ( v && v->active )
      ComputeGains ( sep, vol, &v->left_gain, &v->right_gain );
   RestoreInterrupts ( flags );
}

/*------------------------------------------------------------------------
   SfxMixer_Render () - additively mixes 'frames' stereo samples into
   'out' (interleaved float, already containing the OPL output).
   Called from the audio callback thread.
  ------------------------------------------------------------------------*/
void
SfxMixer_Render ( float *out, int frames )
{
   unsigned long flags = DisableInterrupts ();
   int i, f;

   for ( i = 0; i < MAX_VOICES; i++ )
   {
      sfx_voice *v = &g_voices[i];

      if ( !v->active )
         continue;

      for ( f = 0; f < frames; f++ )
      {
         unsigned long sample_idx = v->pos_frac >> 16;
         float sample;

         if ( sample_idx >= v->length )
         {
            v->active = 0;
            break;
         }

         sample = ( v->data[sample_idx] - 128 ) / 128.0f;

         out[f * 2 + 0] += sample * v->left_gain;
         out[f * 2 + 1] += sample * v->right_gain;

         v->pos_frac += v->step_frac;
      }
   }

   RestoreInterrupts ( flags );
}
