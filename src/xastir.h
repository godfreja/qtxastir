/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id: xastir.h,v 1.18 2002/11/22 00:50:05 we7u Exp $
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

/* All of the misc entry points to be included for all packages */

#ifndef _XASTIR_H
#define _XASTIR_H

#include <X11/Intrinsic.h>

//#include "db.h"
#include "util.h"
#include "messages.h"
#include "fcc_data.h"
#include "rac_data.h"


// black
#define MY_FG_COLOR             colors[0x08]
#define MY_FOREGROUND_COLOR     XmNforeground,colors[0x08]
// gray73
#define MY_BG_COLOR             colors[0xff]
#define MY_BACKGROUND_COLOR     XmNbackground,colors[0xff]


#define CONVERT_HP_NORMAL       0
#define CONVERT_HP_NOSP         1
#define CONVERT_LP_NORMAL       2
#define CONVERT_LP_NOSP         3
#define CONVERT_DEC_DEG         4
#define CONVERT_UP_TRK          5
#define CONVERT_DMS_NORMAL      6

#ifndef M_PI                      /* if not defined in math.h */
#define M_PI 3.14159265358979323846
#endif

/* GLOBAL DEFINES */
extern GC gc;
extern Pixmap  pixmap;
extern Pixmap  pixmap_final;
extern Pixmap  pixmap_alerts;
extern Pixmap  pixmap_50pct_stipple;
extern Pixmap  pixmap_25pct_stipple;
extern Pixmap  pixmap_13pct_stipple;
extern Pixmap  pixmap_wx_stipple;


typedef struct XastirGlobal {
    Widget  top;    // top level shell
} XastirGlobal;

extern XastirGlobal Global;
extern Widget appshell;

extern int wait_to_redraw;
/*extern char my_callsign[MAX_CALLSIGN+1];*/

extern void pad_callsign(char *callsignout, char *callsignin);



extern char *to_upper(char *data);

extern void Send_message(Widget w, XtPointer clientData, XtPointer callData);

extern void create_gc(Widget w);

extern void Station_info(Widget w, XtPointer clientData, XtPointer calldata);
extern void Window_Quit(Widget w, XtPointer client, XtPointer call);

extern void fix_dialog_size(Widget w);
extern void fix_dialog_vsize(Widget w);

extern int debug_level;
extern GC gc;
extern int colors[];
extern long mid_x_long_offset;
extern long mid_y_lat_offset;
extern long x_long_offset;
extern long y_lat_offset;
extern long scale_x;            // x scaling in 1/100 sec per pixel
extern long scale_y;            // y scaling in 1/100 sec per pixel
extern long screen_width;
extern long screen_height;
extern int long_lat_grid;
//extern Pixmap  pixmap;
//extern Pixmap  pixmap_final;
//extern Pixmap  pixmap_alerts;
extern int map_color_levels;
extern int map_labels;
extern int map_auto_maps;
extern int auto_maps_skip_raster;
extern time_t sec_remove;
extern Widget da;
extern Widget text;
extern XtAppContext app_context;
extern int redraw_on_new_data;
//extern Widget hidden_shell;
extern int tiger_flag;

#ifdef HAVE_IMAGEMAGICK  //N0VH
extern int tiger_show_grid;
extern int tiger_show_counties;
extern int tiger_show_cities;
extern int tiger_show_places;
extern int tiger_show_majroads;
extern int tiger_show_streets;
extern int tiger_show_railroad;
extern int tiger_show_states;
extern int tiger_show_interstate;
extern int tiger_show_ushwy;
extern int tiger_show_statehwy;
extern int tiger_show_water;
extern int tiger_show_lakes;
extern int tiger_show_misc;
extern int tigermap_intensity;
extern int tigermap_timeout;
#endif

extern void sort_list(char *filename,int size, Widget list, int *item);
extern void redraw_symbols(Widget w);

extern Colormap cmap;



/* from messages.c */
extern char  message_counter[5+1];
extern int  auto_reply;
extern char auto_reply_message[100];
extern int  satellite_ack_mode;
extern void clear_outgoing_messages(void);
extern void reset_outgoing_messages(void);
extern void output_message(char *from, char *to, char *message, char *path);
extern void check_and_transmit_messages(time_t time);
extern Message_Window mw[MAX_MESSAGE_WINDOWS+1];
extern void clear_message_windows(void);

/* from sound.c */
extern pid_t play_sound(char *sound_cmd, char *soundfile);
extern int sound_done(void);

/* from fcc_data.c */



/* from rac_data.c */

/* from lang.c */
extern int load_language_file(char *filename);
extern char *langcode(char *code);
extern char langcode_hotkey(char *code);

/* from sound.c */
extern pid_t play_sound(char *sound_cmd, char *soundfile);

/* from location.c */
extern void set_last_position(void);
extern void map_pos_last_position(void);

/* from location_gui.c */
extern char locate_station_call[30];
extern void Last_location(Widget w, XtPointer clientData, XtPointer callData);
extern void Jump_location(Widget w, XtPointer clientData, XtPointer callData);
extern void map_pos(long mid_y, long mid_x, long sz);
extern char locate_gnis_filename[200];

// This needs to be quite long for some of the weather station
// serial data to get through ok (Peet Bros U2k Complete Record Mode
// for one).
#define MAX_LINE_SIZE 512

// from map.c
extern double calc_dscale_x(long x, long y);

/* from popup_gui.c */
extern void popup_message(char *banner, char *message);
extern void popup_ID_message(char *banner, char *message);


/* from view_messages.c */
extern void all_messages(char from, char *call_sign, char *from_call, char *message);
extern void view_all_messages(Widget w, XtPointer clientData, XtPointer callData);   

/* from db.c */
extern void setup_in_view(void);

#endif /* XASTIR_H */
