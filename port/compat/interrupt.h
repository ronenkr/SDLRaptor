/* interrupt.h - port compat shim for the older-ASS-revision code path in
   MIDI.C/AL_MIDI.C/TASK_MAN.C (LIBVER_ASSREV < 20021225L picks this name
   over "interrup.h"). Same two functions as audiolib's own INTERRUP.H,
   just without the #pragma aux bodies MSVC can't parse - the real
   implementation (a real mutex, since these bracket critical sections
   shared between the main thread and the audio callback thread) lives
   in audio/audio_lock.c. */
#ifndef _PORT_COMPAT_INTERRUPT_H
#define _PORT_COMPAT_INTERRUPT_H

unsigned long DisableInterrupts ( void );
void          RestoreInterrupts ( unsigned long flags );

#endif
