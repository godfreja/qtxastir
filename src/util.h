/*
 * $Id: util.h,v 1.30 2004/12/07 08:05:07 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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


#ifndef __XASTIR_UTIL_H
#define __XASTIR_UTIL_H

#include "database.h"
#include <pthread.h>

// Max number of WIDE digipeaters allowed
#define MAX_WIDES 3

extern char *remove_leading_spaces(char *data);
extern char *remove_trailing_spaces(char *data);
extern char *remove_trailing_asterisk(char *data);
extern int  position_amb_chars;
extern void start_timer(void);
extern void stop_timer(void);
extern void print_timer_results(void);
extern void get_timestamp(char *timestring);
extern int get_hours(void);
extern int get_minutes(void);
extern int get_seconds(void);
extern char *output_lat(char *in_lat, int comp_pos);
extern char *output_long(char *in_long, int comp_pos);
extern double phg_range(char p, char h, char g);
extern double shg_range(char s, char h, char g);
extern void phg_decode(const char *langstr, const char *phg, char *phg_decoded, int phg_decoded_length);
extern void shg_decode(const char *langstr, const char *shg, char *shg_decoded, int shg_decoded_length);
extern void bearing_decode(const char *langstr, const char *bearing_str, const char *NRQ, char *bearing_decoded, int bearing_decoded_length);
extern char *get_line(FILE *f, char *linedata, int maxline);
extern time_t time_from_aprsstring(char *timestamp);
extern char *compress_posit(const char *lat, const char group, const char *lon, const char symbol,
                const int course, const int speed, const char *phg);

extern int  position_defined(long lat, long lon, int strict);
extern void convert_screen_to_xastir_coordinates(int x, int y, long *lat, long *lon);
extern void convert_xastir_to_UTM_str(char *str, int str_len, long x, long y);
extern void convert_xastir_to_MGRS_str(char *str, int str_len, long x, long y, int nice_format);
extern void convert_xastir_to_UTM(double *easting, double *northing, char *zone, int zone_len, long x, long y);
extern void convert_UTM_to_xastir(double easting, double northing, char *zone, long *x, long *y);
extern void convert_lat_l2s(long lat, char *str, int str_len, int type);
extern void convert_lon_l2s(long lon, char *str, int str_len, int type);
extern long convert_lat_s2l(char *lat);
extern long convert_lon_s2l(char *lon);

extern double calc_distance(long lat1, long lon1, long lat2, long lon2);
extern double calc_distance_course(long lat1, long lon1, long lat2, long lon2, char *course_deg, int course_deg_length);
extern double distance_from_my_station(char *call_sign, char *course_deg);

extern char *convert_bearing_to_name(char *bearing, int opposite);

extern int  filethere(char *fn);
extern int  filecreate(char *fn);
extern time_t file_time(char *fn);
extern void log_data(char *file, char *line);
extern void disown_object_item(char *call_sign,char *new_owner);
extern void log_object_item(char *line, int disable_object, char *object_name);
extern void reload_object_item(void);
extern void log_tactical_call(char *call_sign, char *tactical_call_sign);
extern void reload_tactical_calls(void);
extern time_t sec_now(void);
extern char *get_time(char *time_here);

extern void substr(char *dest, char *src, int size);
extern int  valid_path(char *path);
extern int  valid_call(char *call);
extern int  valid_object(char *name);
extern int  valid_item(char *name);
extern int  valid_inet_name(char *name, char *info, char *origin, int origin_size);

extern char echo_digis[6][9+1];
extern void upd_echo(char *path);

extern char *to_upper(char *data);
extern int  is_num_chr(char ch);
extern int  is_num_or_sp(char ch);
extern int  is_xnum_or_dash(char *data, int max);
extern void removeCtrlCodes(char *cp);
extern void makePrintable(char *cp);
extern void spell_it_out(char *text, int max_length);

typedef struct
{
    pthread_mutex_t lock;
    pthread_t threadID;
} xastir_mutex;

extern void init_critical_section(xastir_mutex *lock);
extern int begin_critical_section(xastir_mutex *lock, char *text);
extern int end_critical_section(xastir_mutex *lock, char *text);

//#define TIMING_DEBUG
#ifdef TIMING_DEBUG
void time_mark(int start);
#endif  // TIMING_DEBUG

// dl9sau
extern char *sec_to_loc(long longitude, long latitude);

extern short checkHash(char *theCall, short theHash);

#ifdef HAVE_LIBCURL
int curl_fwrite(void *buffer, size_t size, size_t nmemb, void *stream);
int curl_getfile(char *fileimg, char *local_filename);
#endif

extern void split_string( char *data, char *cptr[], int max );
extern int check_unproto_path( char *data );

#endif // __XASTIR_UTIL_H


