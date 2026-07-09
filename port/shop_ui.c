/****************************************************************************
* shop_ui.c - native 640x480 "Supplies" shop screen, replacing SOURCE/STORE.C
*----------------------------------------------------------------------------
* The original STORE.C drove a low-res (320x200) SWD dialog that showed one
* buyable/sellable item at a time, cycled with NEXT/PREV buttons. This
* rewrite shows every item at once in a scrollable list with a real
* scrollbar, drawn into its own native 640x480 8bpp buffer (see
* PLAT_ShopBegin/Present/End in plat_video.c) so it gets twice the
* addressable layout room of the rest of the 320x200-native game instead of
* just a bigger picture of the same grid. Existing sprite/font assets are
* reused, doubled 2x at blit time (Shop_* helpers below, twins of
* GFX_PutPic/GFX_PutMaskPic/GFX_DrawSprite/GFX_DrawChar in port/gfx_blit.c).
* Item names are plain hardcoded text (see item_names[] below) rather than
* the old per-item "name plate" GLB picture, which turned out not to be a
* real image (see the comment on item_names[]).
*
* All shop economy rules (OBJS_CanBuy/CanSell, cost/resale, buy/sell) are
* untouched, reused as-is from SOURCE/OBJECTS.C.
*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "raptor.h"
#include "plat.h"

#include "file0001.inc"

#define SHOP_W 640
#define SHOP_H 480

#define HDR_H     88
#define FOOTER_H  32
#define ROW_H     64
#define LIST_X    16
#define LIST_Y    ( HDR_H + 8 )
#define SCROLLBAR_W 40
#define LIST_W    ( SHOP_W - LIST_X * 2 - SCROLLBAR_W - 8 )
#define LIST_H    ( SHOP_H - FOOTER_H - 8 - LIST_Y )

#define TAB_W  96
#define TAB_H  32
#define TAB_Y  8
#define TAB_SELL_X ( SHOP_W - TAB_W - 16 )
#define TAB_BUY_X  ( TAB_SELL_X - TAB_W - 8 )

#define TRACK_X ( LIST_X + LIST_W + 4 )
#define TRACK_Y LIST_Y
#define TRACK_W SCROLLBAR_W
#define TRACK_H LIST_H

#define BUY_MODE  0
#define SELL_MODE 1

/* placeholder chrome colors - the real game palette lives in the external
   .GLB data files (not in this repo), so these are best-effort indices
   consistent with the few named colors the original game already hardcodes
   (RED/GREEN/YELLOW in public.h); revisit once seen against the real
   palette. */
#define COL_BG      1
#define COL_PANEL   2
#define COL_HILITE  3
#define COL_BORDER  15
#define COL_TEXT    15

typedef struct
{
   OBJ_TYPE type;
   INT      cost;
} SHOPROW;

/* Item names, hardcoded plain text (matching the OBJ_TYPE comments in
   OBJECTS.H). The ITEM00_TXT..ITEM17_TXT GLB resources STORE.C used for its
   one-item-at-a-time "name plate" turn out not to be plain GFX_PIC images -
   their payload is itself a readable label string ("TEXT_IMAGE ICON.._PIC"),
   presumably rendered by a text-field path in the original SWD dialog rather
   than blitted as a picture - so a custom list has nothing reusable to blit
   for names and just prints them as text instead. */
PRIVATE const CHAR *item_names [ S_ITEMBUY1 ] = {
      "DUAL MACHINE GUNS",
      "PLASMA GUNS",
      "SMALL WING MISSILES",
      "DUMB FIRE MISSILE",
      "AUTO TRACK MINI GUN",
      "AUTO TRACK LASER TURRET",
      "MULTIPLE MISSILE SHOTS",
      "AIR TO AIR MISSILE",
      "AIR TO GROUND MISSILE",
      "GROUND BOMB",
      "ENERGY GRAB",
      "MEGA BOMB",
      "WAVE WEAPON",
      "ALTERNATING LASER",
      "DEATH RAY",
      "SUPER SHIELD",
      "SHIELD ENERGY",
      "DAMAGE DETECTOR"
      };

PRIVATE const BYTE cursor_mask [16][10] = {
   {1,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0}, {1,1,1,0,0,0,0,0,0,0},
   {1,1,1,1,0,0,0,0,0,0}, {1,1,1,1,1,0,0,0,0,0}, {1,1,1,1,1,1,0,0,0,0},
   {1,1,1,1,1,1,1,0,0,0}, {1,1,1,1,1,1,1,1,0,0}, {1,1,1,1,1,1,1,1,1,0},
   {1,1,1,1,1,0,0,0,0,0}, {1,1,1,0,1,1,0,0,0,0}, {1,1,0,0,1,1,0,0,0,0},
   {1,0,0,0,0,1,1,0,0,0}, {0,0,0,0,0,1,1,0,0,0}, {0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0}
   };

PRIVATE BYTE     *shopbuf         = NULL;
PRIVATE SHOPROW   rows [ S_LAST_OBJECT ];
PRIVATE INT       row_count       = 0;
PRIVATE INT       mode            = BUY_MODE;
PRIVATE INT       scroll          = 0;
PRIVATE INT       sel             = 0;
PRIVATE INT       mouse_x         = 0;
PRIVATE INT       mouse_y         = 0;
PRIVATE BOOL      dragging_thumb  = FALSE;
PRIVATE INT       drag_dy         = 0;
PRIVATE BOOL      dirty           = TRUE;
PRIVATE CHAR      status_msg [64] = "";
PRIVATE INT       status_color    = 15;
PRIVATE INT       status_timer    = 0;

/* cheat: tap Z 5 times in a row (within CHEAT_Z_WINDOW frames of each
   other) to add 10,000,000 to the score */
#define CHEAT_Z_TAPS    5
#define CHEAT_Z_WINDOW  90
PRIVATE INT       cheat_z_count   = 0;
PRIVATE INT       cheat_z_timer   = 0;

/*--------------------------------------------------------------------------
   2x doubling blit primitives - parameterized twins of the routines in
   port/gfx_blit.c, writing into an arbitrary SHOP_W-wide buffer instead of
   the global 320-wide displaybuffer.
  --------------------------------------------------------------------------*/
PRIVATE VOID
Shop_ColorBox ( BYTE *dst, INT x, INT y, INT lx, INT ly, INT color )
{
   INT row, x0 = x, y0 = y, x1 = x + lx, y1 = y + ly;

   if ( x0 < 0 ) x0 = 0;
   if ( y0 < 0 ) y0 = 0;
   if ( x1 > SHOP_W ) x1 = SHOP_W;
   if ( y1 > SHOP_H ) y1 = SHOP_H;
   if ( x1 <= x0 || y1 <= y0 ) return;

   for ( row = y0; row < y1; row++ )
      memset ( dst + row * SHOP_W + x0, color, x1 - x0 );
}

PRIVATE VOID
Shop_Rectangle ( BYTE *dst, INT x, INT y, INT lx, INT ly, INT color )
{
   Shop_ColorBox ( dst, x,          y,          lx, 1,  color );
   Shop_ColorBox ( dst, x,          y + ly - 1, lx, 1,  color );
   Shop_ColorBox ( dst, x,          y,          1,  ly, color );
   Shop_ColorBox ( dst, x + lx - 1, y,          1,  ly, color );
}

PRIVATE VOID
Shop_PutSprite2x ( BYTE *dst, INT dx, INT dy, BYTE *pic, INT itemsize )
{
   BYTE       *end = pic + itemsize;
   GFX_SPRITE *sp  = ( GFX_SPRITE * ) ( pic + sizeof ( GFX_PIC ) );

   while ( ( BYTE * ) sp + sizeof ( GFX_SPRITE ) <= end &&
           sp->offset != ( INT ) EMPTY )
   {
      BYTE *src = ( BYTE * ) sp + sizeof ( GFX_SPRITE );
      INT   sx  = dx + sp->x * 2;
      INT   sy  = dy + sp->y * 2;
      INT   i;

      if ( sp->length < 0 || src + sp->length > end )
         break;   /* malformed/truncated resource - stop rather than run off the buffer */

      if ( sy >= 0 && sy + 1 < SHOP_H )
      {
         BYTE *d0 = dst + sy * SHOP_W;
         BYTE *d1 = d0 + SHOP_W;

         for ( i = 0; i < sp->length; i++ )
         {
            BYTE c  = src[i];
            INT  ox = sx + i * 2;

            if ( ox >= 0 && ox + 1 < SHOP_W )
            {
               d0[ox] = c; d0[ox + 1] = c;
               d1[ox] = c; d1[ox + 1] = c;
            }
         }
      }

      src += sp->length;
      sp = ( GFX_SPRITE * ) src;
   }
}

/* draws a GFX_PIC/GFX_SPRITE resource doubled 2x; color 0 is transparent.
   Bounds every read against the resource's real GLB item size - width/height
   are data-driven and not every lump turns out to be a plain rectangular
   GPIC of the size expected (crashed here once on a bad assumption). */
PRIVATE VOID
Shop_PutPic2x ( BYTE *dst, INT dx, INT dy, DWORD handle )
{
   BYTE    *pic;
   GFX_PIC *h;
   BYTE    *src;
   INT      x, y;
   INT      itemsize;

   if ( handle == ( DWORD ) EMPTY )
      return;

   pic = GLB_GetItem ( handle );
   if ( !pic )
      return;

   itemsize = GLB_ItemSize ( handle );
   if ( itemsize < ( INT ) sizeof ( GFX_PIC ) )
      return;

   h = ( GFX_PIC * ) pic;

   if ( h->type == GSPRITE )
   {
      Shop_PutSprite2x ( dst, dx, dy, pic, itemsize );
      return;
   }

   /* sane upper bound first - width/height are untrusted data, and
      width*height can silently wrap a 32-bit INT before the size
      comparison ever sees it */
   if ( h->width <= 0 || h->height <= 0 || h->width > 1024 || h->height > 1024 )
      return;

   if ( itemsize - ( INT ) sizeof ( GFX_PIC ) < h->width * h->height )
      return;   /* declared dimensions don't fit what's actually loaded */

   src = pic + sizeof ( GFX_PIC );

   for ( y = 0; y < h->height; y++ )
   {
      INT oy = dy + y * 2;

      if ( oy >= 0 && oy + 1 < SHOP_H )
      {
         BYTE *d0 = dst + oy * SHOP_W;
         BYTE *d1 = d0 + SHOP_W;

         for ( x = 0; x < h->width; x++ )
         {
            BYTE c  = src[x];
            INT  ox = dx + x * 2;

            if ( c && ox >= 0 && ox + 1 < SHOP_W )
            {
               d0[ox] = c; d0[ox + 1] = c;
               d1[ox] = c; d1[ox + 1] = c;
            }
         }
      }

      src += h->width;
   }
}

PRIVATE INT
Shop_CharAdvance ( FONT *font, BYTE ch )
{
   return ( font->width[ch] + 1 ) * 2;   /* fontspacing = 1, doubled */
}

PRIVATE VOID
Shop_DrawChar2x ( BYTE *dst, INT dx, INT dy, FONT *font, BYTE ch, INT color )
{
   BYTE *src = ( BYTE * ) font + sizeof ( FONT ) + font->charofs[ch];
   INT   w   = font->width[ch];
   INT   h   = font->height;
   INT   x, y;

   for ( y = 0; y < h; y++ )
   {
      INT oy = dy + y * 2;

      if ( oy >= 0 && oy + 1 < SHOP_H )
      {
         BYTE *d0 = dst + oy * SHOP_W;
         BYTE *d1 = d0 + SHOP_W;

         for ( x = 0; x < w; x++ )
         {
            BYTE c = src[x];

            if ( c )
            {
               BYTE v  = ( BYTE ) ( c + color );
               INT  ox = dx + x * 2;

               if ( ox >= 0 && ox + 1 < SHOP_W )
               {
                  d0[ox] = v; d0[ox + 1] = v;
                  d1[ox] = v; d1[ox + 1] = v;
               }
            }
         }
      }

      src += w;
   }
}

PRIVATE INT
Shop_Print2x ( BYTE *dst, INT x, INT y, const CHAR *str, FONT *font, INT basecolor )
{
   INT startx = x;

   basecolor--;

   for ( ; *str; str++ )
   {
      BYTE ch = ( BYTE ) *str;

      if ( font->charofs[ch] != ( SHORT ) EMPTY )
         Shop_DrawChar2x ( dst, x, y, font, ch, basecolor );

      x += Shop_CharAdvance ( font, ch );
   }

   return x - startx;
}

PRIVATE VOID
Shop_DrawCursor ( BYTE *dst, INT mx, INT my )
{
   INT x, y;

   for ( y = 0; y < 16; y++ )
   {
      for ( x = 0; x < 10; x++ )
      {
         INT ox, oy;

         if ( !cursor_mask[y][x] )
            continue;

         ox = mx + x * 2;
         oy = my + y * 2;

         if ( ox >= 0 && ox + 1 < SHOP_W && oy >= 0 && oy + 1 < SHOP_H )
         {
            BYTE *d0 = dst + oy * SHOP_W + ox;
            BYTE *d1 = d0 + SHOP_W;

            d0[0] = COL_BORDER; d0[1] = COL_BORDER;
            d1[0] = COL_BORDER; d1[1] = COL_BORDER;
         }
      }
   }
}

/*--------------------------------------------------------------------------
   Manual palette fades against the shop's own hi-res present path (can't
   reuse GFX_FadeIn/GFX_FadeOut - those pace themselves through
   GFX_SetPalette -> PLAT_WaitNextTick -> PLAT_Pump, which would present the
   *low-res* buffer mid-fade and flash stale content over our screen).
  --------------------------------------------------------------------------*/
PRIVATE VOID
ShopFadeFromBlack ( BYTE *buf, BYTE *target, INT steps )
{
   BYTE step[768];
   INT  loop, i;

   for ( loop = 1; loop <= steps; loop++ )
   {
      for ( i = 0; i < 768; i++ )
         step[i] = ( BYTE ) ( target[i] * loop / steps );

      PLAT_SetPalette ( step );
      PLAT_ShopPresent ( buf );
      SDL_Delay ( 10 );
   }

   PLAT_SetPalette ( target );
   PLAT_ShopPresent ( buf );
}

PRIVATE VOID
ShopFadeToBlack ( BYTE *buf, INT steps )
{
   BYTE cur[768], step[768];
   INT  loop, i;

   PLAT_GetPalette ( cur );

   for ( loop = 1; loop <= steps; loop++ )
   {
      for ( i = 0; i < 768; i++ )
         step[i] = ( BYTE ) ( cur[i] - cur[i] * loop / steps );

      PLAT_SetPalette ( step );
      PLAT_ShopPresent ( buf );
      SDL_Delay ( 10 );
   }
}

/*--------------------------------------------------------------------------
   List model - reuses OBJS_CanBuy/CanSell/GetCost/GetResale as-is, only the
   *presentation* changes from one-at-a-time to "every row, scrolled".
  --------------------------------------------------------------------------*/
PRIVATE INT
ShopVisibleRows ( VOID )
{
   return LIST_H / ROW_H;
}

PRIVATE VOID
ShopEnsureVisible ( VOID )
{
   INT visible   = ShopVisibleRows ();
   INT maxscroll = row_count - visible;

   if ( maxscroll < 0 ) maxscroll = 0;
   if ( sel < scroll ) scroll = sel;
   if ( sel >= scroll + visible ) scroll = sel - visible + 1;
   if ( scroll > maxscroll ) scroll = maxscroll;
   if ( scroll < 0 ) scroll = 0;
}

PRIVATE VOID
ShopScrollBy ( INT delta )
{
   INT visible   = ShopVisibleRows ();
   INT maxscroll = row_count - visible;

   if ( maxscroll < 0 ) maxscroll = 0;

   scroll += delta;
   if ( scroll < 0 ) scroll = 0;
   if ( scroll > maxscroll ) scroll = maxscroll;
}

PRIVATE VOID
ShopMoveSel ( INT delta )
{
   sel += delta;
   if ( sel < 0 ) sel = 0;
   if ( sel >= row_count ) sel = row_count - 1;
   if ( sel < 0 ) sel = 0;
   ShopEnsureVisible ();
}

PRIVATE VOID
ShopBuildRows ( VOID )
{
   INT limit = ( mode == BUY_MODE ) ? S_ITEMBUY1 : S_LAST_OBJECT;
   INT loop, i, j;

   row_count = 0;

   for ( loop = 0; loop < limit; loop++ )
   {
      BOOL ok = ( mode == BUY_MODE ) ? OBJS_CanBuy ( loop ) : OBJS_CanSell ( loop );

      if ( ok )
      {
         rows[row_count].type = loop;
         rows[row_count].cost = ( mode == BUY_MODE )
                                 ? OBJS_GetCost ( loop )
                                 : OBJS_GetResale ( loop );
         row_count++;
      }
   }

   for ( i = 0; i < row_count - 1; i++ )
   {
      for ( j = 0; j < row_count - 1 - i; j++ )
      {
         if ( rows[j].cost > rows[j + 1].cost )
         {
            SHOPROW tmp = rows[j];
            rows[j] = rows[j + 1];
            rows[j + 1] = tmp;
         }
      }
   }

   if ( sel >= row_count ) sel = row_count - 1;
   if ( sel < 0 ) sel = 0;
   ShopEnsureVisible ();
   dirty = TRUE;
}

PRIVATE VOID
ShopBuyOrSell ( INT index )
{
   OBJ_TYPE type;

   if ( index < 0 || index >= row_count )
      return;

   type = rows[index].type;

   if ( mode == BUY_MODE )
   {
      switch ( OBJS_Buy ( type ) )
      {
         case OBJ_GOTIT:
            SND_Patch ( FX_SWEP, 127 );
            strcpy ( status_msg, "PURCHASED" );
            status_color = GREEN;
            break;

         case OBJ_NOMONEY:
            SND_Patch ( FX_WARNING, 127 );
            strcpy ( status_msg, "NOT ENOUGH MONEY" );
            status_color = RED;
            break;

         case OBJ_SHIPFULL:
            SND_Patch ( FX_WARNING, 127 );
            strcpy ( status_msg, "SHIP IS FULL" );
            status_color = RED;
            break;

         default:
            SND_Patch ( FX_WARNING, 127 );
            strcpy ( status_msg, "CANNOT BUY THAT" );
            status_color = RED;
            break;
      }
   }
   else
   {
      OBJS_Sell ( type );
      SND_Patch ( FX_SWEP, 127 );
      strcpy ( status_msg, "SOLD" );
      status_color = GREEN;
   }

   status_timer = 150;
   ShopBuildRows ();
}

/*--------------------------------------------------------------------------
   Mouse hit-testing - plain rectangle math against our own drawn widgets,
   no SWD field/hit-test reuse (SWD's hit-testing is tied to its own field
   object model, which this screen no longer has).
  --------------------------------------------------------------------------*/
PRIVATE VOID
ShopMouseDown ( VOID )
{
   INT visible = ShopVisibleRows ();

   if ( row_count > visible )
   {
      INT maxscroll = row_count - visible;
      INT thumb_h   = TRACK_H * visible / row_count;
      INT thumb_y;

      if ( thumb_h < 20 ) thumb_h = 20;
      thumb_y = ( maxscroll > 0 )
                ? TRACK_Y + ( TRACK_H - thumb_h ) * scroll / maxscroll
                : TRACK_Y;

      if ( mouse_x >= TRACK_X && mouse_x < TRACK_X + TRACK_W &&
           mouse_y >= thumb_y && mouse_y < thumb_y + thumb_h )
      {
         dragging_thumb = TRUE;
         drag_dy = mouse_y - thumb_y;
         return;
      }

      if ( mouse_x >= TRACK_X && mouse_x < TRACK_X + TRACK_W &&
           mouse_y >= TRACK_Y && mouse_y < TRACK_Y + TRACK_H )
      {
         ShopScrollBy ( mouse_y < thumb_y ? -visible : visible );
         dirty = TRUE;
         return;
      }
   }

   if ( mouse_y >= TAB_Y && mouse_y < TAB_Y + TAB_H )
   {
      if ( mouse_x >= TAB_BUY_X && mouse_x < TAB_BUY_X + TAB_W && mode != BUY_MODE )
      {
         mode = BUY_MODE;
         ShopBuildRows ();
      }
      else if ( mouse_x >= TAB_SELL_X && mouse_x < TAB_SELL_X + TAB_W && mode != SELL_MODE )
      {
         mode = SELL_MODE;
         ShopBuildRows ();
      }
   }
}

PRIVATE VOID
ShopMouseUp ( VOID )
{
   INT visible = ShopVisibleRows ();

   if ( dragging_thumb )
   {
      dragging_thumb = FALSE;
      return;
   }

   if ( mouse_x >= LIST_X && mouse_x < LIST_X + LIST_W &&
        mouse_y >= LIST_Y && mouse_y < LIST_Y + visible * ROW_H )
   {
      INT slot = ( mouse_y - LIST_Y ) / ROW_H;
      INT idx  = scroll + slot;

      if ( idx < row_count )
      {
         sel = idx;
         ShopBuyOrSell ( idx );
      }
   }

   dirty = TRUE;
}

PRIVATE VOID
ShopMouseMotion ( VOID )
{
   dirty = TRUE;

   if ( !dragging_thumb )
      return;

   {
      INT visible   = ShopVisibleRows ();
      INT maxscroll = row_count - visible;
      INT thumb_h, usable, rel;

      if ( row_count <= 0 ) return;

      thumb_h = TRACK_H * visible / row_count;
      if ( thumb_h < 20 ) thumb_h = 20;
      usable = TRACK_H - thumb_h;
      rel    = mouse_y - drag_dy - TRACK_Y;

      if ( usable > 0 && maxscroll > 0 )
         scroll = rel * maxscroll / usable;
      else
         scroll = 0;

      if ( scroll < 0 ) scroll = 0;
      if ( scroll > maxscroll ) scroll = maxscroll;
   }
}

/*--------------------------------------------------------------------------
   Frame rendering
  --------------------------------------------------------------------------*/
PRIVATE VOID
ShopDrawFrame ( FONT *font )
{
   CHAR buf[64];
   INT  visible = ShopVisibleRows ();
   INT  i;

   memset ( shopbuf, 0, SHOP_W * SHOP_H );

   /* header */
   Shop_ColorBox ( shopbuf, 0, 0, SHOP_W, HDR_H, COL_PANEL );
   Shop_Rectangle ( shopbuf, 0, 0, SHOP_W, HDR_H, COL_BORDER );
   Shop_PutPic2x ( shopbuf, 0, 0, id_pics[plr.id_pic] );

   sprintf ( buf, "%s", plr.callsign );
   Shop_Print2x ( shopbuf, 160, 14, buf, font, COL_TEXT );

   sprintf ( buf, "SCORE: %07u", ( unsigned ) plr.score );
   Shop_Print2x ( shopbuf, 160, 50, buf, font, COL_TEXT );

   /* buy/sell tabs */
   Shop_ColorBox ( shopbuf, TAB_BUY_X, TAB_Y, TAB_W, TAB_H,
                   mode == BUY_MODE ? COL_HILITE : COL_BG );
   Shop_Rectangle ( shopbuf, TAB_BUY_X, TAB_Y, TAB_W, TAB_H, COL_BORDER );
   Shop_Print2x ( shopbuf, TAB_BUY_X + 16, TAB_Y + 8, "BUY", font, COL_TEXT );

   Shop_ColorBox ( shopbuf, TAB_SELL_X, TAB_Y, TAB_W, TAB_H,
                   mode == SELL_MODE ? COL_HILITE : COL_BG );
   Shop_Rectangle ( shopbuf, TAB_SELL_X, TAB_Y, TAB_W, TAB_H, COL_BORDER );
   Shop_Print2x ( shopbuf, TAB_SELL_X + 10, TAB_Y + 8, "SELL", font, COL_TEXT );

   /* list */
   for ( i = 0; i < visible; i++ )
   {
      INT      idx = scroll + i;
      INT      ry  = LIST_Y + i * ROW_H;
      OBJ_TYPE type;
      OBJ_LIB *lib;
      INT      amt;

      if ( idx >= row_count )
         break;

      type = rows[idx].type;
      lib  = OBJS_GetLib ( type );

      /* under the "newfeatures" console command, Turret/Plasma Guns/Mini Gun stack
         onto one node (OBJECTS.C's OBJS_Add) instead of one node per
         purchase, so their owned count is num, not a node count */
      amt  = ( OBJS_IsOnly ( type ) ||
               type == S_TURRET || type == S_PLASMA_GUNS || type == S_MINI_GUN )
             ? OBJS_GetAmt ( type ) : OBJS_GetTotal ( type );

      Shop_ColorBox ( shopbuf, LIST_X, ry, LIST_W, ROW_H - 2,
                      idx == sel ? COL_HILITE : COL_BG );
      Shop_Rectangle ( shopbuf, LIST_X, ry, LIST_W, ROW_H - 2, COL_BORDER );

      if ( lib )
         Shop_PutPic2x ( shopbuf, LIST_X + 4, ry + 1, lib->item );

      if ( type < S_ITEMBUY1 )
         Shop_Print2x ( shopbuf, LIST_X + 76, ry + 22, item_names[type], font, COL_TEXT );

      sprintf ( buf, "OWN:%02d", amt );
      Shop_Print2x ( shopbuf, LIST_X + LIST_W - 210, ry + 8, buf, font, COL_TEXT );

      sprintf ( buf, "%s:%05d", mode == BUY_MODE ? "COST" : "SELL", rows[idx].cost );
      Shop_Print2x ( shopbuf, LIST_X + LIST_W - 210, ry + 34, buf, font, COL_TEXT );
   }

   /* scrollbar */
   if ( row_count > visible )
   {
      INT maxscroll = row_count - visible;
      INT thumb_h   = TRACK_H * visible / row_count;
      INT thumb_y;

      if ( thumb_h < 20 ) thumb_h = 20;
      thumb_y = ( maxscroll > 0 )
                ? TRACK_Y + ( TRACK_H - thumb_h ) * scroll / maxscroll
                : TRACK_Y;

      Shop_ColorBox ( shopbuf, TRACK_X, TRACK_Y, TRACK_W, TRACK_H, COL_BG );
      Shop_Rectangle ( shopbuf, TRACK_X, TRACK_Y, TRACK_W, TRACK_H, COL_BORDER );
      Shop_ColorBox ( shopbuf, TRACK_X + 2, thumb_y, TRACK_W - 4, thumb_h, COL_BORDER );
   }

   /* footer */
   Shop_ColorBox ( shopbuf, 0, SHOP_H - FOOTER_H, SHOP_W, FOOTER_H, COL_PANEL );
   Shop_Rectangle ( shopbuf, 0, SHOP_H - FOOTER_H, SHOP_W, FOOTER_H, COL_BORDER );

   if ( status_timer > 0 && status_msg[0] )
      Shop_Print2x ( shopbuf, 16, SHOP_H - FOOTER_H + 8, status_msg, font, status_color );
   else
      Shop_Print2x ( shopbuf, 16, SHOP_H - FOOTER_H + 8,
                     "ESC:EXIT  CLICK:BUY/SELL  WHEEL:SCROLL",
                     font, COL_TEXT );

   Shop_DrawCursor ( shopbuf, mouse_x, mouse_y );
}

/***************************************************************************
STORE_Enter () - Lets User go in store and buy and sell things
 ***************************************************************************/
VOID
STORE_Enter (
VOID
)
{
   SDL_Event ev;
   FONT     *font;
   BOOL      running  = TRUE;
   BOOL      alt_down = FALSE;

   PTR_DrawCursor ( FALSE );
   KBD_Clear ();
   GFX_FadeOut ( 0, 0, 0, 5 );

   shopbuf = ( BYTE * ) calloc ( SHOP_W * SHOP_H, 1 );
   if ( !shopbuf )
      EXIT_Error ( "STORE_Enter: out of memory for shop buffer" );

   font = ( FONT * ) GLB_GetItem ( FONT2_FNT );

   mode           = BUY_MODE;
   scroll         = 0;
   sel            = 0;
   dragging_thumb = FALSE;
   status_msg[0]  = '\0';
   status_timer   = 0;
   cheat_z_count  = 0;
   cheat_z_timer  = 0;

   ShopBuildRows ();

   PLAT_ShopBegin ();
   ShopDrawFrame ( font );
   ShopFadeFromBlack ( shopbuf, palette, 16 );
   dirty = FALSE;

   while ( running )
   {
      while ( SDL_PollEvent ( &ev ) )
      {
         switch ( ev.type )
         {
            case SDL_EVENT_QUIT:
               EXIT_Clean ();
               break;

            case SDL_EVENT_KEY_DOWN:
            {
               SDL_Scancode sc = ev.key.scancode;

               if ( sc == SDL_SCANCODE_RETURN &&
                    ( ev.key.mod & SDL_KMOD_ALT ) && !ev.key.repeat )
               {
                  PLAT_ToggleFullscreen ();
                  break;
               }

               if ( sc == SDL_SCANCODE_LALT || sc == SDL_SCANCODE_RALT )
                  alt_down = TRUE;

               if ( !ev.key.repeat )
               {
                  if ( sc == SDL_SCANCODE_X && alt_down )
                  {
                     PLAT_ShopEnd ();
                     WIN_AskExit ();
                     PLAT_ShopBegin ();
                     dirty = TRUE;
                     break;
                  }

                  if ( sc == SDL_SCANCODE_ESCAPE )
                  {
                     running = FALSE;
                     break;
                  }

                  if ( sc == SDL_SCANCODE_SPACE )
                  {
                     mode ^= SELL_MODE;
                     ShopBuildRows ();
                     break;
                  }

                  if ( sc == SDL_SCANCODE_RETURN )
                  {
                     ShopBuyOrSell ( sel );
                     break;
                  }

                  if ( sc == SDL_SCANCODE_Z )
                  {
                     cheat_z_count++;
                     cheat_z_timer = CHEAT_Z_WINDOW;

                     if ( cheat_z_count >= CHEAT_Z_TAPS )
                     {
                        plr.score += 10000000;
                        cheat_z_count = 0;
                        cheat_z_timer = 0;
                        SND_Patch ( FX_SWEP, 127 );
                        strcpy ( status_msg, "CHEAT ACTIVATED" );
                        status_color = GREEN;
                        status_timer = 150;
                     }

                     dirty = TRUE;
                     break;
                  }
               }

               if ( sc == SDL_SCANCODE_UP )       { ShopMoveSel ( -1 ); dirty = TRUE; }
               if ( sc == SDL_SCANCODE_DOWN )      { ShopMoveSel ( 1 );  dirty = TRUE; }
               if ( sc == SDL_SCANCODE_PAGEUP )    { ShopScrollBy ( -ShopVisibleRows () ); dirty = TRUE; }
               if ( sc == SDL_SCANCODE_PAGEDOWN )  { ShopScrollBy ( ShopVisibleRows () );  dirty = TRUE; }
               if ( sc == SDL_SCANCODE_HOME )      { scroll = 0; sel = 0; dirty = TRUE; }
               if ( sc == SDL_SCANCODE_END )        { sel = row_count - 1; ShopEnsureVisible (); dirty = TRUE; }
               break;
            }

            case SDL_EVENT_KEY_UP:
               if ( ev.key.scancode == SDL_SCANCODE_LALT || ev.key.scancode == SDL_SCANCODE_RALT )
                  alt_down = FALSE;
               break;

            case SDL_EVENT_MOUSE_MOTION:
            {
               INT sx, sy;

               if ( PLAT_ShopWindowToScreen ( ev.motion.x, ev.motion.y, &sx, &sy ) )
               {
                  mouse_x = sx;
                  mouse_y = sy;
                  ShopMouseMotion ();
               }
               break;
            }

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
               if ( ev.button.button == SDL_BUTTON_LEFT )
                  ShopMouseDown ();
               break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
               if ( ev.button.button == SDL_BUTTON_LEFT )
                  ShopMouseUp ();
               break;

            case SDL_EVENT_MOUSE_WHEEL:
               ShopScrollBy ( - ( INT ) ( ev.wheel.y * 2 ) );
               dirty = TRUE;
               break;

            default:
               break;
         }
      }

      if ( !running )
         break;

      if ( status_timer > 0 )
      {
         status_timer--;
         if ( status_timer == 0 )
            dirty = TRUE;
      }

      if ( cheat_z_timer > 0 )
      {
         cheat_z_timer--;
         if ( cheat_z_timer == 0 )
            cheat_z_count = 0;
      }

      if ( dirty )
      {
         ShopDrawFrame ( font );
         PLAT_ShopPresent ( shopbuf );
         dirty = FALSE;
      }

      SDL_Delay ( 8 );
   }

   ShopFadeToBlack ( shopbuf, 16 );

   free ( shopbuf );
   shopbuf = NULL;

   PLAT_ShopEnd ();

   memset ( displaybuffer, 0, 64000 );
   GFX_DisplayUpdate ();
   GFX_SetPalette ( palette, 0 );
}
