/****************************************************************************
* conio.h - Win32/SDL3 port compatibility shim for Watcom <conio.h>
*----------------------------------------------------------------------------
* The game and audiolib use conio.h only for port I/O (inp/outp).
* The port routes the interesting ports (0x388/0x389 AdLib -> OPL emulator,
* 0x201 gameport -> emulated joystick byte) and ignores the rest.
*---------------------------------------------------------------------------*/
#ifndef _PORT_COMPAT_CONIO_H
#define _PORT_COMPAT_CONIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* inp/outp/inpw/outpw are reserved intrinsic names under MSVC, so the
   implementations get compat_ names and macros map the DOS spellings. */
int  compat_inp   ( int port );
int  compat_inpw  ( int port );
int  compat_outp  ( int port, int value );
int  compat_outpw ( int port, int value );

#define inp    compat_inp
#define inpw   compat_inpw
#define outp   compat_outp
#define outpw  compat_outpw

/* Hook points filled in by the port's audio/input layers. */
extern void ( *compat_opl_write )( int port, unsigned char value );
extern unsigned char compat_gameport_state;   /* returned for inp(0x201) */

#ifdef __cplusplus
}
#endif

#endif /* _PORT_COMPAT_CONIO_H */
