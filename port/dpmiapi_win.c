/****************************************************************************
* dpmiapi_win.c - replaces GFX/DPMIAPI.C for the Win32/SDL3 port
*----------------------------------------------------------------------------
* RAP_InitMem (SOURCE/RAP.C) probes DPMI for total memory, drains DOS
* conventional memory with _dpmi_dosalloc, then callocs one big block for
* VM_InitMemory. Here we report a fixed "machine size" (MAX_HMEM + MEM_KEEP,
* the most RAP_InitMem will use) and fail the conventional-memory allocs so
* its drain loop finishes immediately.
*---------------------------------------------------------------------------*/
#include "dpmiapi.h"

#define PORT_MAX_HMEM   0x400000L    /* keep in sync with MAX_HMEM (RAP.C) */
#define PORT_MEM_KEEP   0x4000L      /* keep in sync with MEM_KEEP (RAP.C) */

int
_dpmi_dosalloc ( unsigned short size, unsigned int *segment )
{
   *segment = 0;
   return 1;                          /* nonzero = failure */
}

int
_dpmi_getmemsize ( void )
{
   return PORT_MAX_HMEM + PORT_MEM_KEEP;
}

int
_dpmi_lockregion ( void *address, unsigned length )
{
   return 0;
}

int
_dpmi_unlockregion ( void *address, unsigned length )
{
   return 0;
}
