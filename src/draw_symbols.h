/*
 * $Id: draw_symbols.h,v 1.7 2002/11/14 19:30:25 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */


#ifndef __XASTIR_DRAW_SYMBOLS_H
#define __XASTIR_DRAW_SYMBOLS_H

#define SYMBOL_ACTIVE    'A'
#define SYMBOL_NOTACTIVE 'N'

#define MAX_SYMBOLS 400

typedef struct {
    char active;                // ??
    char table;                 // table character
    char symbol;                // symbol character
    char orient;                // orientation of the symbol, one of ' ',  'l','r','u','d'
    Pixmap pix;                 // icon picture
    Pixmap pix_mask;            // mask for transparent background
    Pixmap pix_mask_old;        // mask for ghost symbols, half transparent icons
} SymbolData;

extern int symbol_callsign_display;
extern SymbolData symbol_data[];

extern void draw_nice_string(Widget w, Pixmap where, int style, long x, long y, char *text, int bgcolor, int fgcolor, int length);
extern void clear_symbol_data(void);
extern void read_symbol_from_file(FILE *f, char *pixels);
extern void load_pixmap_symbol_file(char *filename);
extern void insert_symbol(char table, char symbol, char *pixel, int deg, char orient);
extern char symbol_orient(char *course);
extern void symbol(Widget w, int ghost,char symbol_table, char symbol_id, char symbol_overlay, Pixmap where, int mask, long x_offset, long y_offset, char rotate);

extern void draw_symbol(Widget w, char symbol_table, char symbol_id,
char symbol_overlay, long x_lon, long y_lat,char *callsign_text,
char *alt_text, char *course_text, char *speed_text, char
*my_distance, char *my_course, char *wx_temp, char* wx_wind, time_t
sec_heard, int temp_show_last_heard, Pixmap where, char rotate, char area_type);

extern void draw_pod_circle(long x_long, long y_lat, double range, int color, Pixmap where);
extern void draw_phg_rng(long x_long, long y_lat, char *phg, time_t sec_heard, Pixmap where);
extern void draw_DF_circle(long x_long, long y_lat, char *shgd, time_t sec_heard, Pixmap where);
extern void draw_wind_barb(long x_long, long y_lat, char *speed, char *course, time_t sec_heard, Pixmap where);
extern void draw_bearing(long x_long, long y_lat, char *course, char *bearing, char *NRQ, int color, time_t sec_heard, Pixmap where);
extern void draw_ambiguity(long x_long, long y_lat, char amb, time_t sec_heard, Pixmap where);
extern void draw_area(long x_long, long y_lat, char type, char color, char sqrt_lat_off, char sqrt_lon_off, char width, time_t sec_heard, Pixmap where);
extern void draw_multipoints(long x_long, long y_lat, int numpoints, long points[][2], char type, char style, time_t sec_heard, Pixmap where);  // KG4NBB
extern void Select_symbol( Widget w, XtPointer clientData, XtPointer callData);
extern int symbol_change_requested_from;
extern Widget select_symbol_dialog;
extern void Select_symbol_destroy_shell( Widget widget, XtPointer clientData, XtPointer callData);
extern void draw_symbols_init(void);
extern void draw_deadreckoning_features(DataRow *p_station, Pixmap where, Widget w);
#endif


