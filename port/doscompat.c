/****************************************************************************
* doscompat.c - implementations for the port's <dos.h>/<conio.h> shims
*---------------------------------------------------------------------------*/
#include <time.h>
#include <dos.h>
#include <conio.h>
#ifndef _WIN32
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#endif

void ( *compat_opl_write )( int port, unsigned char value ) = 0;
unsigned char compat_gameport_state = 0xF0;   /* all buttons released */

#ifndef _WIN32
char *
strupr ( char *s )
{
   char *p = s;

   while ( *p )
   {
      *p = ( char ) toupper ( ( unsigned char ) *p );
      p++;
   }
   return s;
}

int
stricmp ( const char *a, const char *b )
{
   return strcasecmp ( a, b );
}

int
strcmpi ( const char *a, const char *b )
{
   return strcasecmp ( a, b );
}

long
filelength ( int handle )
{
   long cur = lseek ( handle, 0, SEEK_CUR );
   long len = lseek ( handle, 0, SEEK_END );

   lseek ( handle, cur, SEEK_SET );
   return len;
}

int
chsize ( int handle, long size )
{
   return ftruncate ( handle, size );
}

char *
ltoa ( long value, char *str, int radix )
{
   char   buf[8 * sizeof ( long ) + 2];   /* worst case: base 2 + sign + NUL */
   char  *p = buf + sizeof ( buf ) - 1;
   int    negative = value < 0 && radix == 10;
   unsigned long uval = negative ? ( unsigned long ) ( -value )
                                  : ( unsigned long ) value;

   *p = 0;
   do
   {
      int digit = ( int ) ( uval % ( unsigned long ) radix );

      *--p = ( char ) ( digit < 10 ? '0' + digit : 'a' + digit - 10 );
      uval /= ( unsigned long ) radix;
   } while ( uval );

   if ( negative )
      *--p = '-';

   memmove ( str, p, ( size_t ) ( buf + sizeof ( buf ) - p ) );
   return str;
}
#endif

int
int386 ( int inter_no, const union REGS *in_regs, union REGS *out_regs )
{
   if ( in_regs != out_regs )
      *out_regs = *in_regs;
   out_regs->x.cflag = 1;     /* "failed" - no BIOS/DOS services here */
   return 0;
}

int
int386x ( int inter_no, const union REGS *in_regs, union REGS *out_regs,
          struct SREGS *seg_regs )
{
   return int386 ( inter_no, in_regs, out_regs );
}

void
segread ( struct SREGS *seg_regs )
{
   seg_regs->es = seg_regs->cs = seg_regs->ss = 0;
   seg_regs->ds = seg_regs->fs = seg_regs->gs = 0;
}

void
_dos_getdate ( struct dosdate_t *date )
{
   time_t     now  = time ( 0 );
   struct tm *info = localtime ( &now );

   date->day       = ( unsigned char )  info->tm_mday;
   date->month     = ( unsigned char )( info->tm_mon + 1 );
   date->year      = ( unsigned short )( info->tm_year + 1900 );
   date->dayofweek = ( unsigned char )  info->tm_wday;
}

void
delay ( unsigned int milliseconds )
{
   /* Replaced by an SDL_Delay-backed version once SDL is linked (plat_pump);
      a coarse spin keeps headless tools self-contained. */
   clock_t end = clock () + ( clock_t )milliseconds * CLOCKS_PER_SEC / 1000;
   while ( clock () < end )
      ;
}

int
compat_inp ( int port )
{
   switch ( port )
   {
      case 0x201:                      /* gameport */
         return compat_gameport_state;

      default:
         return 0;
   }
}

int
compat_inpw ( int port )
{
   return 0;
}

int
compat_outp ( int port, int value )
{
   switch ( port )
   {
      case 0x388:                      /* AdLib address / data ports */
      case 0x389:
         if ( compat_opl_write )
            compat_opl_write ( port, ( unsigned char )value );
         break;

      default:
         break;
   }
   return value;
}

int
compat_outpw ( int port, int value )
{
   return value;
}
