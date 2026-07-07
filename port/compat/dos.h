/****************************************************************************
* dos.h - Win32/SDL3 port compatibility shim for Watcom <dos.h>
*----------------------------------------------------------------------------
* Shadows the MSVC/Watcom dos.h. Provides just enough of the Watcom DOS
* interface for the Raptor sources to compile: REGS/SREGS, int386(x),
* interrupt enable/disable, vector get/set, and _dos_getdate.
* int386/int386x are inert stubs - every real caller is either replaced by
* the port (PTRAPI, KBDAPI) or edited to not need BIOS services (RAP.C).
*---------------------------------------------------------------------------*/
#ifndef _PORT_COMPAT_DOS_H
#define _PORT_COMPAT_DOS_H

#include <errno.h>   /* Watcom's dos.h chain made errno visible; keep that */

#ifdef __cplusplus
extern "C" {
#endif

/* Watcom 32-bit register file: w.* and h.* overlay the low words/bytes
   of the x.* dword registers. */
struct DWORDREGS { unsigned int eax, ebx, ecx, edx, esi, edi, cflag; };
struct WORDREGS  { unsigned short ax, ax_hi, bx, bx_hi, cx, cx_hi,
                                  dx, dx_hi, si, si_hi, di, di_hi,
                                  cflag, cflag_hi; };
struct BYTEREGS  { unsigned char al, ah, eax_b2, eax_b3,
                                 bl, bh, ebx_b2, ebx_b3,
                                 cl, ch, ecx_b2, ecx_b3,
                                 dl, dh, edx_b2, edx_b3; };

union REGS
{
   struct DWORDREGS x;
   struct WORDREGS  w;
   struct BYTEREGS  h;
};

struct SREGS { unsigned short es, cs, ss, ds, fs, gs; };

int  int386  ( int inter_no, const union REGS *in_regs, union REGS *out_regs );
int  int386x ( int inter_no, const union REGS *in_regs, union REGS *out_regs,
               struct SREGS *seg_regs );
void segread ( struct SREGS *seg_regs );

#define _disable()   ((void)0)
#define _enable()    ((void)0)

/* Interrupt vectors do not exist here; keep the API shape for stragglers. */
typedef void (*compat_isr_t)(void);
#define _dos_getvect( num )            ((compat_isr_t)0)
#define _dos_setvect( num, handler )   ((void)0)
#define _chain_intr( handler )         ((void)0)

#define _interrupt
#define _far
#define __far
#define _near

/* NOTE: audiolib's MIDI.H/MUSIC.H use Watcom's lowercase `cdecl` keyword
   in a position MSVC's own <intrin.h> etc. also use `cdecl` in but
   expects differently ("RetType cdecl (*fn)(args)" vs MSVC's own uses) -
   redefining it here as a blanket macro breaks system headers that
   transitively follow <dos.h>. Fixed directly in those two audiolib
   headers instead (there's only one calling convention on x64 anyway). */

/* FP_OFF/FP_SEG: flat model - offset is the pointer itself */
#define FP_OFF( p )  ((unsigned int)(size_t)(p))
#define FP_SEG( p )  (0)

struct dosdate_t
{
   unsigned char  day;        /* 1-31 */
   unsigned char  month;      /* 1-12 */
   unsigned short year;       /* 1980-2099 */
   unsigned char  dayofweek;  /* 0-6 (0=Sunday) */
};

void _dos_getdate ( struct dosdate_t *date );

void delay ( unsigned int milliseconds );

/* access() mode flags (POSIX names Watcom's headers provided) */
#ifndef F_OK
#define F_OK 0
#define W_OK 2
#define R_OK 4
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PORT_COMPAT_DOS_H */
