/****************************************************************************
* dmx_sdl.c - replaces apodmx/DMX.C: DMX_Init/DeInit, card "detection"
* (our virtual hardware always succeeds as AdLib + digital), the GENMIDI.OP2
* timbre-bank conversion, and the SFX_* entry points (delegated to
* sfx_mixer.c). Music (MUS_*) lives in dmx_music.c.
*---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>

#include "dmx.h"
#include "audio_internal.h"

extern int AL_Init ( int soundcard );          /* audiolib/AL_MIDI.C */
extern void AL_RegisterTimbreBank ( unsigned char *timbres );

int
AL_Detect ( int *port, int *unk )
{
   return 0;                        /* always "found" - we emulate the chip */
}

void
AL_SetCard ( int port, void *data )
{
   /* Converts the GENMIDI_OP2 GLB item (36 bytes/instrument, id-software's
      OP2 layout) into AL_MIDI.C's native 13-bytes/instrument timbre table.
      Byte offsets copied verbatim from apodmx/DMX.C's AL_SetCard. */
   unsigned char *cdata = ( unsigned char * ) data;
   unsigned char *tmb;
   int i;

   tmb = calloc ( 13, 256 );
   if ( !tmb )
      return;

   for ( i = 0; i < 128; i++ )
   {
      tmb[i * 13 + 0]  = cdata[8 + i * 36 + 4 + 0];
      tmb[i * 13 + 1]  = cdata[8 + i * 36 + 4 + 7];
      tmb[i * 13 + 2]  = cdata[8 + i * 36 + 4 + 4] | cdata[8 + i * 36 + 4 + 5];
      tmb[i * 13 + 3]  = cdata[8 + i * 36 + 4 + 11] & 192;
      tmb[i * 13 + 4]  = cdata[8 + i * 36 + 4 + 1];
      tmb[i * 13 + 5]  = cdata[8 + i * 36 + 4 + 8];
      tmb[i * 13 + 6]  = cdata[8 + i * 36 + 4 + 2];
      tmb[i * 13 + 7]  = cdata[8 + i * 36 + 4 + 9];
      tmb[i * 13 + 8]  = cdata[8 + i * 36 + 4 + 3];
      tmb[i * 13 + 9]  = cdata[8 + i * 36 + 4 + 10];
      tmb[i * 13 + 10] = cdata[8 + i * 36 + 4 + 6];
      tmb[i * 13 + 11] = cdata[8 + i * 36 + 4 + 14] + 12;
      tmb[i * 13 + 12] = 0;
   }
   for ( i = 128; i < 175; i++ )
   {
      tmb[( i + 35 ) * 13 + 0]  = cdata[8 + i * 36 + 4 + 0];
      tmb[( i + 35 ) * 13 + 1]  = cdata[8 + i * 36 + 4 + 7];
      tmb[( i + 35 ) * 13 + 2]  = cdata[8 + i * 36 + 4 + 4] | cdata[8 + i * 36 + 4 + 5];
      tmb[( i + 35 ) * 13 + 3]  = cdata[8 + i * 36 + 4 + 11] & 192;
      tmb[( i + 35 ) * 13 + 4]  = cdata[8 + i * 36 + 4 + 1];
      tmb[( i + 35 ) * 13 + 5]  = cdata[8 + i * 36 + 4 + 8];
      tmb[( i + 35 ) * 13 + 6]  = cdata[8 + i * 36 + 4 + 2];
      tmb[( i + 35 ) * 13 + 7]  = cdata[8 + i * 36 + 4 + 9];
      tmb[( i + 35 ) * 13 + 8]  = cdata[8 + i * 36 + 4 + 3];
      tmb[( i + 35 ) * 13 + 9]  = cdata[8 + i * 36 + 4 + 10];
      tmb[( i + 35 ) * 13 + 10] = cdata[8 + i * 36 + 4 + 6];
      tmb[( i + 35 ) * 13 + 11] = cdata[8 + i * 36 + 3] + cdata[8 + i * 36 + 4 + 14] + 12;
      tmb[( i + 35 ) * 13 + 12] = 0;
   }

   AL_RegisterTimbreBank ( tmb );
   free ( tmb );
}

/* NOTE: AL_DetectFM is NOT stubbed here - AL_MIDI.C provides the real
   (hardware-probing) implementation; it's simply never called in our
   fixed "always AdLib" configuration (AL_Detect above bypasses it). */
int  MPU_Init ( int addr )                    { return -1; }
int  GUS_Init ( void )                        { return -1; }
void GUS_Shutdown ( void )                    {}

int  GF1_Detect ( void )                      { return 1; }     /* fail */
void GF1_SetMap ( void *data, int len )       {}
int  SB_Detect ( int *port, int *irq, int *dma, int *unk ) { return 0; }
void SB_SetCard ( int port, int irq, int dma ) {}
int  MPU_Detect ( int *port, int *unk )       { return 1; }     /* fail */
void MPU_SetCard ( int port )                 {}
int  CODEC_Detect ( int *a, int *b )          { return 1; }     /* fail */
int  ENS_Detect ( void )                      { return 1; }     /* fail */
int  MV_Detect ( void )                       { return 1; }     /* fail */

int
DMX_Init ( int rate, int maxsng, int mdev, int sdev )
{
   if ( !AudioCB_Init () )
      return 0;

   AL_Init ( 3 /* Adlib, see sndcards.h */ );
   DmxMusic_SetRate ( rate );      /* Raptor passes 70 - see FX.C's DMX_Init call */

   return mdev | sdev;
}

void
DMX_DeInit ( void )
{
   AudioCB_Shutdown ();
}

void
WAV_PlayMode ( int channels, int samplerate )
{
   /* the mixer already runs at a fixed internal rate/voice count */
}

int
SFX_PlayPatch ( void *vdata, int pitch, int sep, int vol, int unk1, int priority )
{
   return SfxMixer_PlayPatch ( vdata, pitch, sep, vol, 127 - priority );
}

void
SFX_StopPatch ( int handle )
{
   SfxMixer_StopPatch ( handle );
}

int
SFX_Playing ( int handle )
{
   return SfxMixer_Playing ( handle );
}

void
SFX_SetOrigin ( int handle, int pitch, int sep, int vol )
{
   SfxMixer_SetOrigin ( handle, pitch, sep, vol );
}
