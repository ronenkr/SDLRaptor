/****************************************************************************
* audio_internal.h - declarations shared between the audio/ port files
*---------------------------------------------------------------------------*/
#ifndef _AUDIO_INTERNAL_H
#define _AUDIO_INTERNAL_H

/* audio_cb.c - SDL audio device + OPL chip ownership */
int  AudioCB_Init ( void );          /* returns 1 on success, 0 on failure */
void AudioCB_Shutdown ( void );

/* task_audio.c - MIDI sequencer clock, ticked from the audio callback */
void TaskAudio_Advance ( int frames, int samplerate );

/* sfx_mixer.c */
int  SfxMixer_PlayPatch ( void *vdata, int pitch, int sep, int vol, int priority );
void SfxMixer_StopPatch ( int handle );
int  SfxMixer_Playing ( int handle );
void SfxMixer_SetOrigin ( int handle, int pitch, int sep, int vol );
void SfxMixer_Render ( float *out, int frames );

/* dmx_music.c - fade ramp, ticked from the audio callback */
void MusicFade_Advance ( int frames, int samplerate );

/* dmx_music.c - MUS clock rate (Hz), set from DMX_Init's own "rate" arg;
   Raptor calls DMX_Init(70, ...), so this must be 70, not the 140 that
   apodmx's own default assumed for other games (see mus2mid's 3rd arg -
   it bakes this rate into the converted MIDI file's tick division). */
void DmxMusic_SetRate ( int rate );

#endif
