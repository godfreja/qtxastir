/*
 * $Id: db.h,v 1.4 2002/05/25 00:07:38 we7u Exp $
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

/*
 * Database structures
 *
 */

#ifndef XASTIR_DB_H
#define XASTIR_DB_H


/* define max tracking pos */
#define MAX_TRACKS 100

/* define max tnc line size (should be from tnc.h) */
#define MAX_TNC_LINE_SIZE 300

/* define max size of info field */
#define MAX_INFO_FIELD_SIZE 256


// We should probably be using APRS_DF in extract_bearing_NRQ()
// and extract_omnidf() functions.  We aren't currently.
/* Define APRS Types */
enum APRS_Types {
    APRS_NULL,
    APRS_MSGCAP,
    APRS_FIXED,
    APRS_DOWN,
    APRS_MOBILE,
    APRS_DF,
    APRS_OBJECT,
    APRS_ITEM,
    APRS_STATUS,
    APRS_WX1,
    APRS_WX2,
    APRS_WX3,
    APRS_WX4,
    APRS_WX5,
    APRS_WX6,
    QM_WX,
    PEET_COMPLETE,
    RSWX200,
    GPS_RMC,
    GPS_GGA,
    GPS_GLL,
    STATION_CALL_DATA,
    OTHER_DATA,
    APRS_MICE,
    APRS_GRID
};


/* Define Record Types */
#define NORMAL_APRS     'N'
#define MOBILE_APRS     'M'
#define DF_APRS         'D'
#define DOWN_APRS       'Q'
#define NORMAL_GPS_RMC  'C'
#define NORMAL_GPS_GGA  'A'
#define NORMAL_GPS_GLL  'L'

/* define RECORD ACTIVES */
#define RECORD_ACTIVE    'A'
#define RECORD_NOTACTIVE 'N'
#define RECORD_CLOSED     'C'

/* define data from info type */
#define DATA_VIA_LOCAL 'L'
#define DATA_VIA_TNC   'T'
#define DATA_VIA_NET   'I'
#define DATA_VIA_FILE  'F'


/* define Heard info type */
#define VIA_TNC         'Y'
#define NOT_VIA_TNC     'N'

/* define Message types */
#define MESSAGE_MESSAGE  'M'
#define MESSAGE_BULLETIN 'B'
#define MESSAGE_NWS      'W'

// Define file info, string length are without trailing '\0'
#define MAX_CALLSIGN         20         /* 9, objects max 9 ??? */
#define MAX_TIME             20
#define MAX_PATH            100
#define MAX_LONG             12
#define MAX_LAT              11
#define MAX_ALTITUDE         10         //-32808.4 to 300000.0? feet
#define MAX_SPEED             9         /* ?? 3 in knots */
#define MAX_COURSE            7         /* ?? */
#define MAX_POWERGAIN         7
#define MAX_STATION_TIME     10         /* 6+1 */
#define MAX_SAT               4
#define MAX_COMMENTS         80
#define MAX_DISTANCE         10
#define MAX_WXSTATION        50
#define MAX_TEMP 100

#define MAX_MESSAGE_LENGTH  100
#define MAX_MESSAGE_ORDER    10


extern void Set_Del_Object(Widget w, XtPointer clientData, XtPointer calldata); // From main.c


extern char my_callsign[MAX_CALLSIGN+1];
extern char my_lat[MAX_LAT];
extern char my_long[MAX_LONG];

typedef struct {
    char active;
    char data_via;
    char type;
    char heard_via_tnc;
    time_t sec_heard;
    time_t last_ack_sent;
    char packet_time[MAX_TIME];
    char call_sign[MAX_CALLSIGN+1];
    char from_call_sign[MAX_CALLSIGN+1];
    char message_line[MAX_MESSAGE_LENGTH+1];
    char seq[MAX_MESSAGE_ORDER+1];
    char acked;
} Message;

#ifdef MSG_DEBUG
extern void msg_clear_data(Message *clear);
extern void msg_copy_data(Message *to, Message *from);
#else
#define msg_clear_data(clear) memset((Message *)clear, 0, sizeof(Message))
#define msg_copy_data(to, from) memmove((Message *)to, (Message *)from, sizeof(Message))
#endif /* MSG_DEBUG */

extern int message_update_time ();


enum AreaObjectTypes {
    AREA_OPEN_CIRCLE     = 0x0,
    AREA_LINE_RIGHT      = 0x1,
    AREA_OPEN_ELLIPSE    = 0x2,
    AREA_OPEN_TRIANGLE   = 0x3,
    AREA_OPEN_BOX        = 0x4,
    AREA_FILLED_CIRCLE   = 0x5,
    AREA_LINE_LEFT       = 0x6,
    AREA_FILLED_ELLIPSE  = 0x7,
    AREA_FILLED_TRIANGLE = 0x8,
    AREA_FILLED_BOX      = 0x9,
    AREA_MAX             = 0x9,
    AREA_NONE            = 0xF
};

enum AreaObjectColors {
    AREA_BLACK_HI  = 0x0,
    AREA_BLUE_HI   = 0x1,
    AREA_GREEN_HI  = 0x2,
    AREA_CYAN_HI   = 0x3,
    AREA_RED_HI    = 0x4,
    AREA_VIOLET_HI = 0x5,
    AREA_YELLOW_HI = 0x6,
    AREA_GRAY_HI   = 0x7,
    AREA_BLACK_LO  = 0x8,
    AREA_BLUE_LO   = 0x9,
    AREA_GREEN_LO  = 0xA,
    AREA_CYAN_LO   = 0xB,
    AREA_RED_LO    = 0xC,
    AREA_VIOLET_LO = 0xD,
    AREA_YELLOW_LO = 0xE,
    AREA_GRAY_LO   = 0xF
};

typedef struct {
    unsigned type : 4;
    unsigned color : 4;
    unsigned sqrt_lat_off : 8;
    unsigned sqrt_lon_off : 8;
    unsigned corridor_width : 8;
    // Will corridors bigger than 255 miles on one side of the line be needed?
} AreaObject;

typedef struct {
    char aprs_type;
    char aprs_symbol;
    char special_overlay;
    AreaObject area_object;
} APRS_Symbol;



// Struct for holding current weather data.
// This struct is pointed to by the DataRow structure.
// An empty string indicates undefined data.
typedef struct {                //                      strlen
    time_t  wx_sec_time;
    char    wx_time[MAX_TIME];
    char    wx_course[4];       // in �                     3
    char    wx_speed[4];        // in mph                   3
    time_t  wx_speed_sec_time;
    char    wx_gust[4];         // in mph                   3
    char    wx_temp[5];         // in �F                    3
    char    wx_rain[10];        // in hundredths inch/h     3
    char    wx_rain_total[10];  // in hundredths inch
    char    wx_snow[6];         // in hundredths inch/h     3
    char    wx_prec_24[10];     // in hundredths inch/day   3
    char    wx_prec_00[10];     // in hundredths inch       3
    char    wx_hum[5];          // in %                     3
    char    wx_baro[10];        // in hPa                   6
    char    wx_type;
    char    wx_station[MAX_WXSTATION];
} WeatherRow;



// Struct for holding track data.  Will keep a dynamically
// allocated list of track points (sometime in the future).
typedef struct {
    // Trail storage is organized as a ring buffer.
    // It always has at least one valid entry or is not existing!
    int     trail_color;                // trail color
    int     trail_inp;                  // ptr to next input into ring buffer
    int     trail_out;                  // ptr to first trail point
    long    trail_long_pos[MAX_TRACKS]; // coordinate of trail point
    long    trail_lat_pos[MAX_TRACKS];  // coordinate of trail point
    time_t  sec[MAX_TRACKS];            // date/time of position
    long    speed[MAX_TRACKS];          // in 0.1 km/h   undefined: -1
    int     course[MAX_TRACKS];         // in degrees    undefined: -1
    long    altitude[MAX_TRACKS];       // in 0.1 m      undefined: -99999
    char    flag[MAX_TRACKS];           // several flags, see below
} TrackRow;

// trail flag definitions
#define TR_LOCAL        0x01    // heard direct (not via digis)
#define TR_NEWTRK       0x02    // start new track



// Break DataRow into several structures.  DataRow will contain the parameters
// that are common across all types of stations.  DataRow will contain a pointer
// to TrackRow if it is a moving station, and contain a pointer to WeatherRow
// if it is a weather station.  If no weather or track data existed, the
// pointers will be NULL.  This new way of storing station data will save a LOT
// of memory.  If a station suddenly starts moving or spitting out weather data
// the new structures will be allocated, filled in, and pointers to them
// installed in DataRow.

// Station storage now is organized as an ordered linked list. We have both
// sorting by name and by time last heard

// todo: check the string length!

typedef struct _DataRow {
    struct _DataRow *n_next;            // pointer to next element in name ordered list
    struct _DataRow *n_prev;            // pointer to previous element in name ordered list
    struct _DataRow *t_next;            // pointer to next element in time ordered list (newer)
    struct _DataRow *t_prev;            // pointer to previous element in time ordered list (older)
    char call_sign[MAX_CALLSIGN+1];     // call sign, used also for name index
    time_t sec_heard;                   // time last heard, used also for time index
    int  time_sn;                       // serial number for making time index unique
    short flag;                         // several flags, see below
    char pos_amb;                       // Position ambiguity, 0 = none, 1 = 0.1 minute...
    long coord_lon;                     // Xastir coordinates 1/100 sec, 0 = 180�W
    long coord_lat;                     // Xastir coordinates 1/100 sec, 0 =  90�N
    TrackRow *track_data;               // Pointer to trail data or NULL if no trail data
    WeatherRow *weather_data;           // Pointer to weather data or NULL if no weather data
    char record_type;
    char data_via;                      // L local, T TNC, I internet, F file
    int  heard_via_tnc_port;
    time_t heard_via_tnc_last_time;
    int  last_heard_via_tnc;
    int  last_port_heard;
    unsigned int  num_packets;
    char packet_time[MAX_TIME];
    char origin[MAX_CALLSIGN+1];        // call sign originating an object
    APRS_Symbol aprs_symbol;
    char node_path[MAX_PATH+1];
    char pos_time[MAX_TIME];
    char altitude_time[MAX_TIME];
    char altitude[MAX_ALTITUDE];        // in meters (feet gives better resolution ???)
    char speed_time[MAX_TIME];
    char speed[MAX_SPEED+1];            // in knots
    char course[MAX_COURSE+1];
    char bearing[MAX_COURSE+1];
    char NRQ[MAX_COURSE+1];
    char power_gain[MAX_POWERGAIN+1];   // Holds the phgd values
    char signal_gain[MAX_POWERGAIN+1];  // Holds the shgd values (for DF'ing)
    char signpost[5+1];                 // Holds signpost data
    char station_time[MAX_STATION_TIME];
    char station_time_type;
    char sats_visible[MAX_SAT];
    char comments[MAX_COMMENTS+1];
    int  df_color;
} DataRow;


// station flag definitions
#define ST_OBJECT       0x01    // station is an object
#define ST_ITEM         0x02    // station is an item
#define ST_ACTIVE       0x04    // station is active (deleted objects are inactive)
#define ST_MOVING       0x08    // station is moving
#define ST_LOCAL        0x10    // heard direct (not via digis)
#define ST_VIATNC       0x20    // station heard via TNC
#define ST_3RD_PT       0x40    // third party traffic (not used yet)
#define ST_MSGCAP       0x80    // message capable (not used yet)
#define ST_STATUS       0x100   // got real status message
#define ST_INVIEW       0x200   // station is in current screen view


#ifdef DATA_DEBUG
extern void clear_data(DataRow *clear);
extern void copy_data(DataRow *to, DataRow *from);
#else
#define clear_data(clear) memset((DataRow *)clear, 0, sizeof(DataRow))
#define copy_data(to, from) memmove((DataRow *)to, (DataRow *)from, sizeof(DataRow))
#endif /* DATA_DEBUG */


extern void db_init(void);

// 
void mscan_file(char msg_type, void (*function)(Message *fill));
extern void display_file(Widget w);
extern void clean_data_file(void);
extern void read_file_line(FILE *f);
extern void mdisplay_file(char msg_type);
extern void mem_display(void);
extern long sort_input_database(char *filename, char *fill, int size);
extern void sort_display_file(char *filename, int size);
extern void clear_sort_file(char *filename);
extern int  packet_data_display;
extern void display_packet_data(void);
extern int  redraw_on_new_packet_data;
extern int decode_ax25_line(char *line, char from, int port, int dbadd);

// utilities
extern char *remove_trailing_spaces(char *data);
extern char *remove_trailing_asterisk(char *data);
extern void packet_data_add(char *from, char *line);
extern void General_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void IGate_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void WX_query(Widget w, XtPointer clientData, XtPointer calldata);
extern unsigned long max_stations;
extern int  heard_via_tnc_in_past_hour(char *call);

// messages
extern void update_messages(int force);
extern void mdelete_messages_from(char *from);
extern void mdelete_messages_to(char *to);
extern void init_message_data(void);
extern void check_message_remove(void);
extern int  new_message_data;

// stations
extern long stations;
extern DataRow *n_first;  // pointer to first element in name ordered station list
extern DataRow *n_last;   // pointer to last element in name ordered station list
extern DataRow *t_first;  // pointer to first element in time ordered station list
extern DataRow *t_last;   // pointer to last element in time ordered station list
extern void init_station_data(void);
extern int station_data_auto_update;
extern int  next_station_name(DataRow **p_curr);
extern int  prev_station_name(DataRow **p_curr);
extern int  next_station_time(DataRow **p_curr);
extern int  prev_station_time(DataRow **p_curr);
extern int  search_station_name(DataRow **p_name, char *call, int exact);
extern int  search_station_time(DataRow **p_time, time_t heard, int serial);
extern void check_station_remove(void);
extern void delete_all_stations(void);
extern void station_del(char *callsign);
extern void my_station_add(char *my_call_sign, char my_group, char my_symbol, char *my_long, char *my_lat, char *my_phg, char *my_comment, char my_amb);
extern void my_station_gps_change(char *pos_long, char *pos_lat, char *course, char *speed, char speedu, char *alt, char *sats);
extern int  locate_station(Widget w, char *call, int follow_case, int get_match, int center_map);
extern void update_station_info(Widget w);

// objects/items
extern void check_and_transmit_objects_items(time_t time);
extern int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length);

// trails
extern int  delete_trail(DataRow *fill);

// weather
extern int  get_weather_record(DataRow *fill);

extern void set_map_position(Widget w, long lat, long lon);

#endif /* XASTIR_DB_H */
