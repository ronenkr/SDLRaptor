/****************************************************************************
* kbd_sdl.c - replaces GFX/KBDAPI.C (INT 9 handler) with SDL3 key events
*----------------------------------------------------------------------------
* Keeps the exact public surface of KBDAPI: keyboard[256] indexed by DOS
* set-1 make codes, lastscan/lastascii/kbd_ack, paused/capslock, and the
* KBD_* functions. The DOS ISR ignored the 0xE0 prefix, so extended keys
* (arrows, right ctrl/alt, keypad enter...) collapse onto their base
* scancode - the SDL mapping table reproduces that.
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <SDL3/SDL.h>

#include "kbdapi.h"
#include "plat.h"

PUBLIC BOOL kbd_ack = FALSE;

BOOL   keyboard[256];
BOOL   paused, capslock;
INT    lastscan;
INT    lastascii;

PRIVATE VOID ( *keyboardhook )( VOID ) = ( VOID ( * ) ) 0;

PUBLIC INT        ASCIINames[] =               // Unshifted ASCII for scan codes
{
//       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
   0  ,27 ,'1','2','3','4','5','6','7','8','9','0','-','=',8  ,9  , // 0
   'q','w','e','r','t','y','u','i','o','p','[',']',13 ,0  ,'a','s', // 1
   'd','f','g','h','j','k','l',';',39 ,'`',0  ,92 ,'z','x','c','v', // 2
   'b','n','m',',','.','/',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  , // 3
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1', // 4
   '2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , // 5
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , // 6
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  // 7
};

PRIVATE INT ShiftNames[] =  // Shifted ASCII for scan codes
{
//       0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
   0  ,27 ,'!','@','#','$','%','^','&','*','(',')','_','+',8  ,9  , // 0
   'Q','W','E','R','T','Y','U','I','O','P','{','}',13 ,0  ,'A','S', // 1
   'D','F','G','H','J','K','L',':',34 ,'~',0  ,'|','Z','X','C','V', // 2
   'B','N','M','<','>','?',0  ,'*',0  ,' ',0  ,0  ,0  ,0  ,0  ,0  , // 3
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,'7','8','9','-','4','5','6','+','1', // 4
   '2','3','0',127,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , // 5
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  , // 6
   0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0    // 7
};

/*------------------------------------------------------------------------
   SDL scancode (USB HID) -> DOS set-1 make code. Extended (0xE0xx) keys
   map to their base code, matching the original ISR's behavior.
  ------------------------------------------------------------------------*/
PRIVATE BYTE sdl2dos[SDL_SCANCODE_COUNT];

PRIVATE VOID
KBD_BuildTable ( VOID )
{
   static const struct { SDL_Scancode sdl; BYTE dos; } map[] =
   {
      { SDL_SCANCODE_ESCAPE,       0x01 },
      { SDL_SCANCODE_1,            0x02 }, { SDL_SCANCODE_2,        0x03 },
      { SDL_SCANCODE_3,            0x04 }, { SDL_SCANCODE_4,        0x05 },
      { SDL_SCANCODE_5,            0x06 }, { SDL_SCANCODE_6,        0x07 },
      { SDL_SCANCODE_7,            0x08 }, { SDL_SCANCODE_8,        0x09 },
      { SDL_SCANCODE_9,            0x0A }, { SDL_SCANCODE_0,        0x0B },
      { SDL_SCANCODE_MINUS,        0x0C }, { SDL_SCANCODE_EQUALS,   0x0D },
      { SDL_SCANCODE_BACKSPACE,    0x0E }, { SDL_SCANCODE_TAB,      0x0F },
      { SDL_SCANCODE_Q,            0x10 }, { SDL_SCANCODE_W,        0x11 },
      { SDL_SCANCODE_E,            0x12 }, { SDL_SCANCODE_R,        0x13 },
      { SDL_SCANCODE_T,            0x14 }, { SDL_SCANCODE_Y,        0x15 },
      { SDL_SCANCODE_U,            0x16 }, { SDL_SCANCODE_I,        0x17 },
      { SDL_SCANCODE_O,            0x18 }, { SDL_SCANCODE_P,        0x19 },
      { SDL_SCANCODE_LEFTBRACKET,  0x1A }, { SDL_SCANCODE_RIGHTBRACKET, 0x1B },
      { SDL_SCANCODE_RETURN,       0x1C }, { SDL_SCANCODE_KP_ENTER, 0x1C },
      { SDL_SCANCODE_LCTRL,        0x1D }, { SDL_SCANCODE_RCTRL,    0x1D },
      { SDL_SCANCODE_A,            0x1E }, { SDL_SCANCODE_S,        0x1F },
      { SDL_SCANCODE_D,            0x20 }, { SDL_SCANCODE_F,        0x21 },
      { SDL_SCANCODE_G,            0x22 }, { SDL_SCANCODE_H,        0x23 },
      { SDL_SCANCODE_J,            0x24 }, { SDL_SCANCODE_K,        0x25 },
      { SDL_SCANCODE_L,            0x26 }, { SDL_SCANCODE_SEMICOLON, 0x27 },
      { SDL_SCANCODE_APOSTROPHE,   0x28 }, { SDL_SCANCODE_GRAVE,    0x29 },
      { SDL_SCANCODE_LSHIFT,       0x2A }, { SDL_SCANCODE_BACKSLASH, 0x2B },
      { SDL_SCANCODE_Z,            0x2C }, { SDL_SCANCODE_X,        0x2D },
      { SDL_SCANCODE_C,            0x2E }, { SDL_SCANCODE_V,        0x2F },
      { SDL_SCANCODE_B,            0x30 }, { SDL_SCANCODE_N,        0x31 },
      { SDL_SCANCODE_M,            0x32 }, { SDL_SCANCODE_COMMA,    0x33 },
      { SDL_SCANCODE_PERIOD,       0x34 }, { SDL_SCANCODE_SLASH,    0x35 },
      { SDL_SCANCODE_RSHIFT,       0x36 }, { SDL_SCANCODE_KP_MULTIPLY, 0x37 },
      { SDL_SCANCODE_LALT,         0x38 }, { SDL_SCANCODE_RALT,     0x38 },
      { SDL_SCANCODE_SPACE,        0x39 }, { SDL_SCANCODE_CAPSLOCK, 0x3A },
      { SDL_SCANCODE_F1,           0x3B }, { SDL_SCANCODE_F2,       0x3C },
      { SDL_SCANCODE_F3,           0x3D }, { SDL_SCANCODE_F4,       0x3E },
      { SDL_SCANCODE_F5,           0x3F }, { SDL_SCANCODE_F6,       0x40 },
      { SDL_SCANCODE_F7,           0x41 }, { SDL_SCANCODE_F8,       0x42 },
      { SDL_SCANCODE_F9,           0x43 }, { SDL_SCANCODE_F10,      0x44 },
      { SDL_SCANCODE_NUMLOCKCLEAR, 0x45 }, { SDL_SCANCODE_SCROLLLOCK, 0x46 },
      { SDL_SCANCODE_KP_7,         0x47 }, { SDL_SCANCODE_KP_8,     0x48 },
      { SDL_SCANCODE_KP_9,         0x49 }, { SDL_SCANCODE_KP_MINUS, 0x4A },
      { SDL_SCANCODE_KP_4,         0x4B }, { SDL_SCANCODE_KP_5,     0x4C },
      { SDL_SCANCODE_KP_6,         0x4D }, { SDL_SCANCODE_KP_PLUS,  0x4E },
      { SDL_SCANCODE_KP_1,         0x4F }, { SDL_SCANCODE_KP_2,     0x50 },
      { SDL_SCANCODE_KP_3,         0x51 }, { SDL_SCANCODE_KP_0,     0x52 },
      { SDL_SCANCODE_KP_PERIOD,    0x53 },
      { SDL_SCANCODE_F11,          0x57 }, { SDL_SCANCODE_F12,      0x58 },
      /* extended keys -> base codes (0xE0 prefix was ignored on DOS) */
      { SDL_SCANCODE_UP,           0x48 }, { SDL_SCANCODE_DOWN,     0x50 },
      { SDL_SCANCODE_LEFT,         0x4B }, { SDL_SCANCODE_RIGHT,    0x4D },
      { SDL_SCANCODE_INSERT,       0x52 }, { SDL_SCANCODE_DELETE,   0x53 },
      { SDL_SCANCODE_HOME,         0x47 }, { SDL_SCANCODE_END,      0x4F },
      { SDL_SCANCODE_PAGEUP,       0x49 }, { SDL_SCANCODE_PAGEDOWN, 0x51 },
      { SDL_SCANCODE_KP_DIVIDE,    0x35 },
   };
   size_t i;

   memset ( sdl2dos, 0, sizeof ( sdl2dos ) );
   for ( i = 0; i < sizeof ( map ) / sizeof ( map[0] ); i++ )
      sdl2dos[map[i].sdl] = map[i].dos;
}

/*------------------------------------------------------------------------
   KBD_HandleEvent () - SDL key event -> DOS keyboard state
   (the port's replacement for the INT 9 ISR; called from PLAT_Pump)
  ------------------------------------------------------------------------*/
VOID
KBD_HandleEvent ( const union SDL_Event *uev )
{
   const SDL_Event *ev = ( const SDL_Event * ) uev;
   BYTE key;

   if ( ev->key.scancode == SDL_SCANCODE_PAUSE )
   {
      if ( ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat )
         paused ^= TRUE;
      return;
   }

   if ( ev->key.scancode >= SDL_SCANCODE_COUNT )
      return;

   key = sdl2dos[ev->key.scancode];
   if ( key == 0 )
      return;

   if ( ev->type == SDL_EVENT_KEY_UP )
   {
      fprintf ( stderr, "[TRACE] KBD_HandleEvent: KEY_UP sdl=%d dos=0x%02X\n",
                ev->key.scancode, key );
      fflush ( stderr );

      keyboard[key] = FALSE;

      if ( key == SC_CAPS_LOCK )
         capslock = FALSE;
   }
   else
   {
      fprintf ( stderr, "[TRACE] KBD_HandleEvent: KEY_DOWN sdl=%d dos=0x%02X repeat=%d\n",
                ev->key.scancode, key, ev->key.repeat );
      fflush ( stderr );

      kbd_ack = TRUE;
      lastscan = key;
      keyboard[key] = TRUE;
      if ( lastscan && KBD_ISCAPS )
         lastascii = ShiftNames[key];
      else
         lastascii = ASCIINames[key];
      if ( key == SC_CAPS_LOCK )
         capslock = TRUE;

      fprintf ( stderr, "[TRACE] KBD_HandleEvent: lastscan=0x%02X lastascii=%d\n",
                lastscan, lastascii );
      fflush ( stderr );
   }
}

/***************************************************************************
   KBD_Clear() - Resets all flags
 ***************************************************************************/
VOID
KBD_Clear (
VOID
)
{
   lastscan = SC_NONE;
   memset ( ( VOID * ) keyboard, 0, sizeof ( keyboard ) );
}

/***************************************************************************
 KBD_SetKeyboardHook() - Sets User function to call from keyboard handler
 ***************************************************************************/
VOID
KBD_SetKeyboardHook (
VOID ( *hook )( VOID )
)
{
   keyboardhook = hook;
}

/***************************************************************************
   KBD_Ascii2Scan () - converts most ASCII chars to keyboard scan code
 ***************************************************************************/
INT
KBD_Ascii2Scan (
INT ascii
)
{
   INT loop;

   ascii = tolower ( ascii );

   for ( loop = 0; loop < 100; loop++ )
      if ( ASCIINames[loop] == ascii )
         return ( loop );

   return ( 0 );
}

/***************************************************************************
KBD_Wait() - Waits for Key to be released
 ***************************************************************************/
VOID
KBD_Wait (
INT scancode
)
{
   volatile BOOL *ky = &keyboard[scancode];

   while ( *ky )
   {
      PLAT_Pump ();
      SDL_Delay ( 1 );
   }

   lastscan = SC_NONE;
   lastascii = SC_NONE;
}

/***************************************************************************
KBD_IsKey() - Tests to see if key is down if so waits for release
 ***************************************************************************/
BOOL
KBD_IsKey (
INT scancode
)
{
   if ( KBD_Key ( scancode ) )
   {
      KBD_Wait ( scancode );
      return ( TRUE );
   }

   return ( FALSE );
}

/***************************************************************************
   KBD_Install() - Sets up keyboard system
 ***************************************************************************/
VOID
KBD_Install (
VOID
)
{
   KBD_BuildTable ();
   memset ( keyboard, 0, sizeof ( keyboard ) );
}

/***************************************************************************
   KBD_End() - Shuts down KBD system
 ***************************************************************************/
VOID
KBD_End (
VOID
)
{
}
