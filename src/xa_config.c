/*
 * $Id: xa_config.c,v 1.4 2002/02/25 20:43:47 francais1 Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <Xm/XmAll.h>

#include "xa_config.h"
#include "interface.h"
#include "xastir.h"
#include "main.h"
#include "db.h"
#include "util.h"
#include "bulletin_gui.h"
#include "list_gui.h"
#include "messages.h"
#include "draw_symbols.h"
#include "maps.h"

#define CONFIG_FILE "config/xastir.cnf"
#define CONFIG_FILE_BAK "config/xastir.bak"
#define CONFIG_FILE_TMP "config/xastir.tmp"

#define MAX_VALUE 300


void store_string(FILE * fout, char *option, char *value) {

//    if (debug_level & 1)
//        printf ("Store String Start\n");

    fprintf (fout, "%s:%s\n", option, value);

//    if (debug_level & 1)
//        printf ("Store String Stop\n");

}


void store_char(FILE * fout, char *option, char value) {
    char value_o[2];

    value_o[0] = value;
    value_o[1] = '\0';
    store_string (fout, option, value_o);
}

void store_int(FILE * fout, char *option, int value) {
    char value_o[MAX_VALUE];

    sprintf (value_o, "%d", value);
    store_string (fout, option, value_o);
}

void store_long (FILE * fout, char *option, long value) {
    char value_o[MAX_VALUE];

    sprintf (value_o, "%ld", value);
    store_string (fout, option, value_o);
}


FILE * fin;

void input_close(void)
{
    if(fin != NULL)
        (void)fclose(fin);
    fin = NULL;
}

/*
    This function will read the configuration file (xastir.cnf) until it finds
    the requested option. When the requested option is found it will return
    the value of that option.
    The return value of the function will be 1 if the option is found and 0
    if the option is not found.
*/
int get_string(char *option, char *value) {
    char config_file[MAX_VALUE];
    char config_line[256];
    char *option_in;
    char *value_in;
    short option_found;

    option_found = 0;

    if (fin == NULL) {
        strcpy (config_file, get_user_base_dir (CONFIG_FILE));
        (void)filecreate(config_file);   // Create empty file if it doesn't exist
        fin = fopen (config_file, "r");
    }

    if (fin != NULL) {
        (void)fseek(fin, 0, SEEK_SET);
        while ((fgets (&config_line[0], 256, fin) != NULL) && !option_found) {
            option_in = strtok (config_line, " \t:\r\n");
            if (strcmp (option_in, option) == 0) {
                option_found = 1;
                value_in = strtok (NULL, "\t\r\n");
                if (value_in == NULL)
                    value = "";
                else
                    strcpy (value, value_in);
            }
        }
    }
    else
        printf("Couldn't open file: %s\n", config_file);

    return (option_found);
}


int get_char(char *option, char *value) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret)
        *value = value_o[0];

    return (ret);
}

int get_int(char *option, int *value) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret)
        *value = atoi (value_o);
    return (ret);
}

int get_long(char *option, long *value) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret)
        *value = atol (value_o);

    return (ret);
}

char *get_user_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

    strcpy (base, ((env_ptr = getenv ("XASTIR_USER_BASE")) != NULL) ? env_ptr : user_dir);

    if (base[strlen (base) - 1] != '/')
        strcat (base, "/");

    strcat (base, ".xastir/");
    return strcat (base, dir);
}

char *get_data_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

    strcpy (base, ((env_ptr = getenv ("XASTIR_DATA_BASE")) != NULL) ? env_ptr : XASTIR_DATA_BASE);
    if (base[strlen (base) - 1] != '/')
        strcat (base, "/");

    return strcat (base, dir);
}



// Care should be taken here to make sure that no out-of-range data
// is saved, as it will mess up Xastir startup from that point on.
// Also: Config file should be owned by the user, and not by root.
// If chmod 4755 is done on the executable, then the config file ends
// up being owned by root from then on.
void save_data(void)  {
    int i;
    char name_temp[20];
    char name[50];
    FILE * fout;
    char config_file[MAX_VALUE], config_file_bak[MAX_VALUE];

//    if (debug_level & 1)
//        printf ("Store String Start\n");

    strcpy (config_file, get_user_base_dir (CONFIG_FILE));
    strcpy (config_file_bak, get_user_base_dir (CONFIG_FILE_BAK));
    if ( unlink (config_file_bak) ) {
        // Problem here.  Couldn't remove the backup config file (might not exist).
        //printf("Couldn't delete file: %s, cancelling save_data()\n", config_file_bak);
        //return;
    }

    if ( rename (config_file, config_file_bak) ) {
        // Problem here.  Couldn't rename config file to config.bak.
        printf("Couldn't create backup of config file: %s, cancelling save_data()\n", config_file);
        return;
    }

    fout = fopen (config_file, "a");
    if (fout != NULL) {
        if (debug_level & 1)
            printf ("Save Data Start\n");

        /* language */
        store_string (fout, "LANGUAGE", lang_to_use);

        /* my data */
        store_string (fout, "STATION_CALLSIGN", my_callsign);

        store_string (fout, "STATION_LAT", my_lat);
        store_string (fout, "STATION_LONG", my_long);
        store_char (fout, "STATION_GROUP", my_group);
        store_char (fout, "STATION_SYMBOL", my_symbol);
        store_char (fout, "STATION_MESSAGE_TYPE", aprs_station_message_type);
        store_string (fout, "STATION_POWER", my_phg);
        store_string (fout, "STATION_COMMENTS", my_comment);
//        if (debug_level & 2)
//            printf ("Save Data 1\n");

        /* default values */
        store_long (fout, "SCREEN_WIDTH", screen_width);

        store_long (fout, "SCREEN_HEIGHT", screen_height);
        store_long (fout, "SCREEN_LAT", mid_y_lat_offset);
        store_long (fout, "SCREEN_LONG", mid_x_long_offset);
        store_int (fout, "STATION_TRANSMIT_AMB", position_amb_chars);

        if (scale_y > 0)
            store_long (fout, "SCREEN_ZOOM", scale_y);
        else
            store_long (fout, "SCREEN_ZOOM", 327680);

        store_int (fout, "MAP_BGCOLOR", map_background_color);

        store_int (fout, "MAP_DRAW_FILLED_COLORS", map_color_fill);

#ifdef HAVE_GEOTIFF
        sprintf (name, "%f", geotiff_map_intensity);
        store_string(fout, "GEOTIFF_MAP_INTENSITY", name);
#endif

        store_int (fout, "MAP_LETTERSTYLE", letter_style);
        store_int (fout, "MAP_WX_ALERT_STYLE", wx_alert_style);
        store_string(fout, "ALTNET_CALL", altnet_call);
        store_int(fout, "ALTNET", altnet);
        store_string (fout, "AUTO_MAP_DIR", AUTO_MAP_DIR);
        store_string (fout, "ALERT_MAP_DIR", ALERT_MAP_DIR);
        store_string (fout, "WIN_MAP_DIR", WIN_MAP_DIR);
        store_string (fout, "WIN_MAP_DATA", WIN_MAP_DATA);
        store_string (fout, "SYMBOLS_DIR", SYMBOLS_DIR);
        store_string (fout, "SOUND_DIR", SOUND_DIR);
        store_string (fout, "GROUP_DATA_FILE", group_data_file);
        store_string (fout, "GNIS_FILE", locate_gnis_filename);

        /* maps */
        store_int (fout, "MAPS_LONG_LAT_GRID", long_lat_grid);
        store_int (fout, "MAPS_LEVELS", map_color_levels);
        store_int (fout, "MAPS_LABELS", map_labels);
        store_int (fout, "MAPS_AUTO_MAPS", map_auto_maps);

        // display values
        store_int (fout, "DISPLAY_SYMBOL",        symbol_display);
        store_int (fout, "DISPLAY_CALLSIGN",      symbol_callsign_display);
        store_int (fout, "DISPLAY_SPEED",         symbol_speed_display);
        store_int (fout, "DISPLAY_ALTITUDE",      symbol_alt_display);
        store_int (fout, "DISPLAY_COURSE",        symbol_course_display);
        store_int (fout, "DISPLAY_DIST_COURSE",   symbol_dist_course_display);
        store_int (fout, "DISPLAY_STATION_WX",    symbol_weather_display);
        store_int (fout, "DISPLAY_STATION_PHG",   show_phg);
        store_int (fout, "DISPLAY_MOBILES_PHG",   show_phg_mobiles);
        store_int (fout, "DISPLAY_DEFAULT_PHG",   show_phg_default);
        store_int (fout, "DISPLAY_POSITION_AMB",  show_amb);
        store_int (fout, "DISPLAY_OLD_STATION_DATA", show_old_data);
        store_int (fout, "DISPLAY_DF_INFO",       show_DF);
        store_int (fout, "DISPLAY_STATION_TRAILS",station_trails);
        store_int (fout, "DISPLAY_UNITS_ENGLISH", units_english_metric);

        // Interface values
        store_int (fout, "DISABLE_TRANSMIT",      transmit_disable);
        store_int (fout, "DISABLE_POSIT_TX",      posit_tx_disable);
        store_int (fout, "DISABLE_OBJECT_TX",     object_tx_disable);


        for (i = 0; i < MAX_IFACE_DEVICES; i++) {
            sprintf (name_temp, "DEVICE%0d_", i);
            strcpy (name, name_temp);
            strcat (name, "TYPE");
            store_int (fout, name, devices[i].device_type);
            strcpy (name, name_temp);
            strcat (name, "NAME");
            store_string (fout, name, devices[i].device_name);
            strcpy (name, name_temp);
            strcat (name, "HOST");
            store_string (fout, name, devices[i].device_host_name);
            strcpy (name, name_temp);
            strcat (name, "PASSWD");
            store_string (fout, name, devices[i].device_host_pswd);
            strcpy (name, name_temp);
            strcat (name, "UNPROTO1");
            store_string (fout, name, devices[i].unproto1);
            strcpy (name, name_temp);
            strcat (name, "UNPROTO2");
            store_string (fout, name, devices[i].unproto2);
            strcpy (name, name_temp);
            strcat (name, "UNPROTO3");
            store_string (fout, name, devices[i].unproto3);
            strcpy (name, name_temp);
            strcat (name, "TNC_UP_FILE");
            store_string (fout, name, devices[i].tnc_up_file);
            strcpy (name, name_temp);
            strcat (name, "TNC_DOWN_FILE");
            store_string (fout, name, devices[i].tnc_down_file);
            strcpy (name, name_temp);
            strcat (name, "SPEED");
            store_int (fout, name, devices[i].sp);
            strcpy (name, name_temp);
            strcat (name, "STYLE");
            store_int (fout, name, devices[i].style);
            strcpy (name, name_temp);
            strcat (name, "IGATE_OPTION");
            store_int (fout, name, devices[i].igate_options);
            strcpy (name, name_temp);
            strcat (name, "TXMT");
            store_int (fout, name, devices[i].transmit_data);
            strcpy (name, name_temp);
            strcat (name, "RECONN");
            store_int (fout, name, devices[i].reconnect);
            strcpy (name, name_temp);
            strcat (name, "ONSTARTUP");
            store_int (fout, name, devices[i].connect_on_startup);
        }
        /* TNC */
        store_int (fout, "TNC_LOG_DATA", log_tnc_data);
        store_string (fout, "LOGFILE_TNC", LOGFILE_TNC);

        /* NET */
        store_int (fout, "NET_LOG_DATA", log_net_data);

        store_int (fout, "NET_RUN_AS_IGATE", operate_as_an_igate);
        store_int (fout, "NETWORK_WAITTIME", NETWORK_WAITTIME);

        // LOGGING
        store_int (fout, "LOG_IGATE", log_igate);
        store_int (fout, "LOG_WX", log_wx);
        store_string (fout, "LOGFILE_IGATE", LOGFILE_IGATE);
        store_string (fout, "LOGFILE_NET", LOGFILE_NET);
        store_string (fout, "LOGFILE_WX", LOGFILE_WX);

        // SNAPSHOTS
        store_int (fout, "SNAPSHOTS_ENABLED", snapshots_enabled);

        /* WX ALERTS */
        store_long (fout, "WX_ALERTS_REFRESH_TIME", (long)WX_ALERTS_REFRESH_TIME);

        /* GPS */
        store_long (fout, "GPS_TIME", (long)gps_time);    /* still needed */

        /* POSIT RATE */
        store_long (fout, "POSIT_RATE", (long)POSIT_rate);

        /* station broadcast type */
        store_int (fout, "BST_TYPE", output_station_type);

        store_int (fout, "BST_WX_RAW", transmit_raw_wx);
        store_int (fout, "BST_COMPRESSED_POSIT", transmit_compressed_posit);

        /* -dk7in- variable beacon interval */
        /*         mobile:   max  2 min */
        /*         fixed:    max 15 min  */
        if ((output_station_type >= 1) && (output_station_type <= 3))
            max_transmit_time = (time_t)120l;       /*  2 min @ mobile */
        else
            max_transmit_time = (time_t)900l;       /* 15 min @ fixed */
        
        /* Audio Alarms */
        store_string (fout, "SOUND_COMMAND", sound_command);

        store_int (fout, "SOUND_PLAY_ONS", sound_play_new_station);
        store_string (fout, "SOUND_ONS_FILE", sound_new_station);
        store_int (fout, "SOUND_PLAY_ONM", sound_play_new_message);
        store_string (fout, "SOUND_ONM_FILE", sound_new_message);

        store_int (fout, "SOUND_PLAY_PROX", sound_play_prox_message);
        store_string (fout, "SOUND_PROX_FILE", sound_prox_message);
        store_string (fout, "PROX_MIN", prox_min);
        store_string (fout, "PROX_MAX", prox_max);
        store_int (fout, "SOUND_PLAY_BAND", sound_play_band_open_message);
        store_string (fout, "SOUND_BAND_FILE", sound_band_open_message);
        store_string (fout, "BANDO_MIN", bando_min);
        store_string (fout, "BANDO_MAX", bando_max);
        store_int (fout, "SOUND_PLAY_WX_ALERT", sound_play_wx_alert_message);
        store_string (fout, "SOUND_WX_ALERT_FILE", sound_wx_alert_message);

#ifdef HAVE_FESTIVAL
	    /* Festival speech settings */
	store_int (fout, "SPEAK_NEW_STATION",festival_speak_new_station);
	store_int (fout, "SPEAK_PROXIMITY_ALERT",festival_speak_proximity_alert);
	store_int (fout, "SPEAK_TRACKED_ALERT",festival_speak_tracked_proximity_alert);
        store_int (fout, "SPEAK_BAND_OPENING",festival_speak_band_opening);
        store_int (fout, "SPEAK_MESSAGE_ALERT",festival_speak_new_message_alert);
        store_int (fout, "SPEAK_MESSAGE_BODY",festival_speak_new_message_body);
        store_int (fout, "SPEAK_WEATHER_ALERT",festival_speak_new_weather_alert);
#endif
        
        /* defaults */
        store_long (fout, "DEFAULT_STATION_OLD", (long)sec_old);

        store_long (fout, "DEFAULT_STATION_CLEAR", (long)sec_clear);
        store_long(fout, "DEFAULT_STATION_REMOVE", (long)sec_remove);
        store_string (fout, "HELP_DATA", HELP_FILE);
        store_int (fout, "MESSAGE_COUNTER", message_counter);
        store_string (fout, "AUTO_MSG_REPLY", auto_reply_message);
        store_int (fout, "DISPLAY_PACKET_TYPE", Display_packet_data_type);

        store_int (fout, "BULLETIN_RANGE", bulletin_range);
        store_int(fout,"VIEW_MESSAGE_RANGE",vm_range);
        store_int(fout,"VIEW_MESSAGE_LIMIT",view_message_limit);

        /* printer variables */
        store_int (fout, "PRINT_ROTATED", print_rotated);
        store_int (fout, "PRINT_AUTO_ROTATION", print_auto_rotation);
        store_int (fout, "PRINT_AUTO_SCALE", print_auto_scale);
        store_int (fout, "PRINT_IN_MONOCHROME", print_in_monochrome);
        store_int (fout, "PRINT_INVERT_COLORS", print_invert);

        /* list attributes */
        for (i = 0; i < 5; i++) {
            sprintf (name_temp, "LIST%0d_", i);
            strcpy (name, name_temp);
            strcat (name, "H");
            store_int (fout, name, list_size_h[i]);
            strcpy (name, name_temp);
            strcat (name, "W");
            store_int (fout, name, list_size_w[i]);
        }

        if (debug_level & 1)
            printf ("Save Data Stop\n");

        (void)fclose (fout);
    } else  {
        // Couldn't create new config (out of filespace?).
        printf("Couldn't open config file for appending: %s\n", config_file);

        // Continue using original config file.
        if ( rename (config_file_bak, config_file) ) {
            // Problem here, couldn't rename xastir.bak to xastir.cnf
            printf("Couldn't recover %s from %s file\n", config_file, config_file_bak);
            return;
        }
    }
}



void load_data_or_default(void) {
    int i;
    char name_temp[20];
    char name[50];
    long temp;

    /* language */
    if (!get_string ("LANGUAGE", lang_to_use))
        strcpy (lang_to_use, "English");

    /* my data */
    if (!get_string ("STATION_CALLSIGN", my_callsign))
        strcpy (my_callsign, "NOCALL");

    if (!get_string ("STATION_LAT", my_lat))
        strcpy (my_lat, "0000.000N");
    /* convert old data to high prec */

    temp = convert_lat_s2l (my_lat);
    convert_lat_l2s (temp, my_lat, sizeof(my_lat), CONVERT_HP_NOSP);

    if (!get_string ("STATION_LONG", my_long))
        strcpy (my_long, "00000.000W");
    /* convert old data to high prec */
    temp = convert_lon_s2l (my_long);
    convert_lon_l2s (temp, my_long, sizeof(my_long), CONVERT_HP_NOSP);

    if (!get_int ("STATION_TRANSMIT_AMB", &position_amb_chars))
        position_amb_chars = 0;

    if (!get_char ("STATION_GROUP", &my_group))
        my_group = '/';

    if (!get_char ("STATION_SYMBOL", &my_symbol))
        my_symbol = 'x';

    if (!get_char ("STATION_MESSAGE_TYPE", &aprs_station_message_type))
        aprs_station_message_type = '=';

    if (!get_string ("STATION_POWER", my_phg))
        strcpy (my_phg, "");

    if (!get_string ("STATION_COMMENTS", my_comment))
        sprintf (my_comment, "XASTIR-%s", XASTIR_SYSTEM);

    /* default values */
    if (!get_long ("SCREEN_WIDTH", &screen_width))
        screen_width = 640;

    if (screen_width < 640)
        screen_width = 640;

    if (!get_long ("SCREEN_HEIGHT", &screen_height))
        screen_height = 320;

    if (screen_height < 320)
        screen_height = 320;

    if (!get_long ("SCREEN_LAT", &mid_y_lat_offset))
        mid_y_lat_offset = 32400000l;

    if (!get_long ("SCREEN_LONG", &mid_x_long_offset))
        mid_x_long_offset = 64800000l;

    if (!get_long ("SCREEN_ZOOM", &scale_y))
        scale_y = 327680;
    scale_x = get_x_scale(mid_x_long_offset,mid_y_lat_offset,scale_y);

    if (!get_int ("MAP_BGCOLOR", &map_background_color))
        map_background_color = 0;

    if (!get_int ( "MAP_DRAW_FILLED_COLORS", &map_color_fill) )
        map_color_fill = 1;

#ifdef HAVE_GEOTIFF
    if (!get_string("GEOTIFF_MAP_INTENSITY", name))
        geotiff_map_intensity = 1.0;
    else
        sscanf( name, "%f", &geotiff_map_intensity);
#endif

    if (!get_int ("MAP_LETTERSTYLE", &letter_style))
        letter_style = 1;

    if (!get_int ("MAP_WX_ALERT_STYLE", &wx_alert_style))
        wx_alert_style = 1;

    if (!get_string("ALTNET_CALL", altnet_call))
        strcpy(altnet_call, "XASTIR");

    if (!get_int("ALTNET", &altnet))
        altnet=0;

    if (!get_string ("AUTO_MAP_DIR", AUTO_MAP_DIR))
        strcpy (AUTO_MAP_DIR, get_data_base_dir ("maps"));

    if (!get_string ("ALERT_MAP_DIR", ALERT_MAP_DIR))
        strcpy (ALERT_MAP_DIR, get_data_base_dir ("Counties"));

    if (!get_string ("WIN_MAP_DIR", WIN_MAP_DIR))
        strcpy (WIN_MAP_DIR, get_data_base_dir ("maps"));

    if (!get_string ("WIN_MAP_DATA", WIN_MAP_DATA))
        strcpy (WIN_MAP_DATA, get_user_base_dir ("config/winmaps.sys"));

    if (!get_string ("SYMBOLS_DIR", SYMBOLS_DIR))
        strcpy (SYMBOLS_DIR, get_data_base_dir ("symbols"));

    if (!get_string ("SOUND_DIR", SOUND_DIR))
        strcpy (SOUND_DIR, get_data_base_dir ("sounds"));

    if (!get_string ("GROUP_DATA_FILE", group_data_file))
        strcpy (group_data_file, get_user_base_dir ("config/groups"));

    if (!get_string ("GNIS_FILE", locate_gnis_filename))
        strcpy (locate_gnis_filename, get_data_base_dir ("GNIS/WA.gnis"));

    /* maps */
    if (!get_int ("MAPS_LONG_LAT_GRID", &long_lat_grid))
        long_lat_grid = 1;

    if (!get_int ("MAPS_LEVELS", &map_color_levels))
        map_color_levels = 0;

    if (!get_int ("MAPS_LABELS", &map_labels))
        map_labels = 1;

    if (!get_int ("MAPS_AUTO_MAPS", &map_auto_maps))
        map_auto_maps = 0;

    // display values
    if (!get_int ("DISPLAY_SYMBOL",        &symbol_display))
        symbol_display = 2;

    switch (symbol_display) {
        case 0: 
                symbol_display_enable = 0;
                symbol_display_rotate = 0;
                break;
        case 1:
                symbol_display_enable = 1;
                symbol_display_rotate = 0;
                break;
        case 2:
                symbol_display_enable = 1;
                symbol_display_rotate = 1;
                break;
    }
        
    if (!get_int ("DISPLAY_CALLSIGN",      &symbol_callsign_display))
        symbol_callsign_display = 1;
    if (!get_int ("DISPLAY_SPEED",         &symbol_speed_display))
        symbol_speed_display = 0;

    switch (symbol_speed_display) {
        case 0:
                speed_display_enable = 0;
                speed_display_short = 0;
                break;
        case 1:
                speed_display_enable = 1;
                speed_display_short = 1;
                break;
        case 2:
                speed_display_enable = 1;
                speed_display_short = 0;
                break;
    }

    if (!get_int ("DISPLAY_ALTITUDE",      &symbol_alt_display))
        symbol_alt_display = 0;
    if (!get_int ("DISPLAY_COURSE",        &symbol_course_display))
        symbol_course_display = 0;
    if (!get_int ("DISPLAY_DIST_COURSE",   &symbol_dist_course_display))
        symbol_dist_course_display = 0;
    if (!get_int ("DISPLAY_STATION_WX",    &symbol_weather_display))
        symbol_weather_display = 0;

    switch (symbol_weather_display) {
        case 0:
                wx_display_enable = 0;
                wx_display_short = 0;
                break;
        case 1: // Short
                wx_display_enable = 1;
                wx_display_short = 1;
                break;
        case 2: // Long
                wx_display_enable = 1;
                wx_display_short = 0;
                break;
    }

    if (!get_int ("DISPLAY_STATION_PHG",   &show_phg))
        show_phg = 0;
    if (!get_int ("DISPLAY_MOBILES_PHG",   &show_phg_mobiles))
        show_phg_mobiles = 0;
    if (!get_int ("DISPLAY_DEFAULT_PHG",   &show_phg_default))
        show_phg_default = 0;
    if (!get_int ("DISPLAY_POSITION_AMB",  &show_amb))
        show_amb = 0;
    if (!get_int ("DISPLAY_OLD_STATION_DATA", &show_old_data))
        show_old_data = 0;
    if (!get_int ("DISPLAY_DF_INFO",       &show_DF))
        show_DF = 0;
    if (!get_int ("DISPLAY_STATION_TRAILS",&station_trails))
        station_trails = 1;

    if (!get_int ("DISPLAY_UNITS_ENGLISH", &units_english_metric))
        units_english_metric = 0;

    if (!get_int ("DISABLE_TRANSMIT", &transmit_disable))
        transmit_disable = 0;

    if (!get_int ("DISABLE_POSIT_TX", &posit_tx_disable))
        posit_tx_disable = 0;

    if (!get_int ("DISABLE_OBJECT_TX", &object_tx_disable))
        object_tx_disable = 0;


    for (i = 0; i < MAX_IFACE_DEVICES; i++) {
        sprintf (name_temp, "DEVICE%0d_", i);
        strcpy (name, name_temp);
        strcat (name, "TYPE");
        if (!get_int (name, &devices[i].device_type)) {
            devices[i].device_type = DEVICE_NONE;
        }
        strcpy (name, name_temp);
        strcat (name, "NAME");
        if (!get_string (name, devices[i].device_name))
            strcpy (devices[i].device_name, "");

        strcpy (name, name_temp);
        strcat (name, "HOST");
        if (!get_string (name, devices[i].device_host_name))
            strcpy (devices[i].device_host_name, "");

        strcpy (name, name_temp);
        strcat (name, "PASSWD");
        if (!get_string (name, devices[i].device_host_pswd))
            strcpy (devices[i].device_host_pswd, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO1");
        if (!get_string (name, devices[i].unproto1))
            strcpy (devices[i].unproto1, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO2");
        if (!get_string (name, devices[i].unproto2))
            strcpy (devices[i].unproto2, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO3");
        if (!get_string (name, devices[i].unproto3))
            strcpy (devices[i].unproto3, "");

        strcpy (name, name_temp);
        strcat (name, "TNC_UP_FILE");
        if (!get_string (name, devices[i].tnc_up_file))
            strcpy (devices[i].tnc_up_file, "");

        strcpy (name, name_temp);
        strcat (name, "TNC_DOWN_FILE");
        if (!get_string (name, devices[i].tnc_down_file))
            strcpy (devices[i].tnc_down_file, "");

        strcpy (name, name_temp);
        strcat (name, "SPEED");
        if (!get_int (name, &devices[i].sp))
            devices[i].sp = 0;

        strcpy (name, name_temp);
        strcat (name, "STYLE");
        if (!get_int (name, &devices[i].style))
            devices[i].style = 0;

        strcpy (name, name_temp);
        strcat (name, "IGATE_OPTION");
        if (!get_int (name, &devices[i].igate_options))
            devices[i].igate_options = 0;

        strcpy (name, name_temp);
        strcat (name, "TXMT");
        if (!get_int (name, &devices[i].transmit_data))
            devices[i].transmit_data = 0;

        strcpy (name, name_temp);
        strcat (name, "RECONN");
        if (!get_int (name, &devices[i].reconnect))
            devices[i].reconnect = 0;

        strcpy (name, name_temp);
        strcat (name, "ONSTARTUP");
        if (!get_int (name, &devices[i].connect_on_startup))
            devices[i].connect_on_startup = 0;

    }

    /* TNC */
    if (!get_int ("TNC_LOG_DATA", &log_tnc_data))
        log_tnc_data = 0;

    if (!get_string ("LOGFILE_TNC", LOGFILE_TNC))
        strcpy (LOGFILE_TNC, get_user_base_dir ("logs/tnc.log"));

    /* NET */
    if (!get_int ("NET_LOG_DATA", &log_net_data))
        log_net_data = 0;

    if (!get_int ("NET_RUN_AS_IGATE", &operate_as_an_igate))
        operate_as_an_igate = 0;

    if (!get_int ("LOG_IGATE", &log_igate))
        log_igate = 0;

    if (!get_int ("NETWORK_WAITTIME", &NETWORK_WAITTIME))
        NETWORK_WAITTIME = 10;

    // LOGGING
    if (!get_int ("LOG_WX", &log_wx))
        log_wx = 0;

    if (!get_string ("LOGFILE_IGATE", LOGFILE_IGATE))
        strcpy (LOGFILE_IGATE, get_user_base_dir ("logs/igate.log"));

    if (!get_string ("LOGFILE_NET", LOGFILE_NET))
        strcpy (LOGFILE_NET, get_user_base_dir ("logs/net.log"));

    if (!get_string ("LOGFILE_WX", LOGFILE_WX))
        strcpy (LOGFILE_WX, get_user_base_dir ("logs/wx.log"));

    // SNAPSHOTS
    if (!get_int ("SNAPSHOTS_ENABLED", &snapshots_enabled))
        snapshots_enabled = 0;

    /* WX ALERTS */
    if (!get_long ("WX_ALERTS_REFRESH_TIME", (long *)&WX_ALERTS_REFRESH_TIME))
        WX_ALERTS_REFRESH_TIME = (time_t)30l;

    /* gps */
    if (!get_long ("GPS_TIME", (long *)&gps_time))
        gps_time = (time_t)60l;

    /* POSIT RATE */
    if (!get_long ("POSIT_RATE", (long *)&POSIT_rate))
        POSIT_rate = (time_t)30*60l;

    /* station broadcast type */
    if (!get_int ("BST_TYPE", &output_station_type))
        output_station_type = 0;

    /* raw wx transmit */
    if (!get_int ("BST_WX_RAW", &transmit_raw_wx))
        transmit_raw_wx = 0;

    /* compressed posit transmit */
    if (!get_int ("BST_COMPRESSED_POSIT", &transmit_compressed_posit))
        transmit_compressed_posit = 0;

    /* Audio Alarms*/
    if (!get_string ("SOUND_COMMAND", sound_command))
        strcpy (sound_command, "vplay -q");

    if (!get_int ("SOUND_PLAY_ONS", &sound_play_new_station))
        sound_play_new_station = 0;

    if (!get_string ("SOUND_ONS_FILE", sound_new_station))
        strcpy (sound_new_station, "newstation.wav");

    if (!get_int ("SOUND_PLAY_ONM", &sound_play_new_message))
        sound_play_new_message = 0;

    if (!get_string ("SOUND_ONM_FILE", sound_new_message))
        strcpy (sound_new_message, "newmessage.wav");

    if (!get_int ("SOUND_PLAY_PROX", &sound_play_prox_message))
        sound_play_prox_message = 0;

    if (!get_string ("SOUND_PROX_FILE", sound_prox_message))
        strcpy (sound_prox_message, "proxwarn.wav");

    if (!get_string ("PROX_MIN", prox_min))
        strcpy (prox_min, "0.01");

    if (!get_string ("PROX_MAX", prox_max))
        strcpy (prox_max, "10");

    if (!get_int ("SOUND_PLAY_BAND", &sound_play_band_open_message))
        sound_play_band_open_message = 0;

    if (!get_string ("SOUND_BAND_FILE", sound_band_open_message))
        strcpy (sound_band_open_message, "bandopen.wav");

    if (!get_string ("BANDO_MIN", bando_min))
        strcpy (bando_min, "200");

    if (!get_string ("BANDO_MAX", bando_max))
        strcpy (bando_max, "2000");

    if (!get_int ("SOUND_PLAY_WX_ALERT", &sound_play_wx_alert_message))
        sound_play_wx_alert_message = 0;

    if (!get_string ("SOUND_WX_ALERT_FILE", sound_wx_alert_message))
        strcpy (sound_wx_alert_message, "thunder.wav");

#ifdef HAVE_FESTIVAL
    /* Festival Speech defaults */

    if (!get_int ("SPEAK_NEW_STATION",&festival_speak_new_station))
        festival_speak_new_station = 0;

    if (!get_int ("SPEAK_PROXIMITY_ALERT",&festival_speak_proximity_alert))
        festival_speak_proximity_alert = 0;

    if (!get_int ("SPEAK_TRACKED_ALERT",&festival_speak_tracked_proximity_alert))
        festival_speak_tracked_proximity_alert = 0;

    if (!get_int ("SPEAK_BAND_OPENING",&festival_speak_band_opening))
        festival_speak_band_opening = 0;
                                  
    if (!get_int ("SPEAK_MESSAGE_ALERT",&festival_speak_new_message_alert))
        festival_speak_new_message_alert = 0; 

    if (!get_int ("SPEAK_MESSAGE_BODY",&festival_speak_new_message_body))
        festival_speak_new_message_body = 0;

    if (!get_int ("SPEAK_WEATHER_ALERT",&festival_speak_new_weather_alert))
        festival_speak_new_weather_alert = 0; 

#endif

    /* defaults */
    if (!get_long ("DEFAULT_STATION_OLD", (long *)&sec_old))
        sec_old = (time_t)4800l;

    if (!get_long ("DEFAULT_STATION_CLEAR", (long *)&sec_clear))
        sec_clear = (time_t)43200l;

    if (!get_long("DEFAULT_STATION_REMOVE", (long *)&sec_remove)) {
        sec_remove = sec_clear*2;
    // In the interests of keeping the memory used by Xastir down, I'm
    // commenting out the below lines.  When hooked to one or more internet
    // links it's nice to expire stations from the database more quickly.
//        if (sec_remove < (time_t)(24*3600)) // If it's less than one day,
//            sec_remove = (time_t)(24*3600); // change to a one-day expire
    }

    if (!get_int ("MESSAGE_COUNTER", &message_counter))
        message_counter = 0;

    if (!get_string ("AUTO_MSG_REPLY", auto_reply_message))
        strcpy (auto_reply_message, "Autoreply- No one is at the keyboard");

    if (!get_int ("DISPLAY_PACKET_TYPE", &Display_packet_data_type))
        Display_packet_data_type = 0;

    if (!get_int ("BULLETIN_RANGE", &bulletin_range))
        bulletin_range = 0;

    if(!get_int("VIEW_MESSAGE_RANGE", &vm_range))
        vm_range=0;

    if(!get_int("VIEW_MESSAGE_LIMIT", &view_message_limit))
        view_message_limit = 3000;


    /* printer variables */
    if (!get_int ("PRINT_ROTATED", &print_rotated))
        print_rotated = 0;

    if (!get_int ("PRINT_AUTO_ROTATION", &print_auto_rotation))
        print_auto_rotation = 1;

    if (!get_int ("PRINT_AUTO_SCALE", &print_auto_scale))
        print_auto_scale = 1;

    if (!get_int ("PRINT_IN_MONOCHROME", &print_in_monochrome))
        print_in_monochrome = 0;

    if (!get_int ("PRINT_INVERT_COLORS", &print_invert))
        print_invert = 0;


    /* list attributes */
    for (i = 0; i < 5; i++) {
        sprintf (name_temp, "LIST%0d_", i);
        strcpy (name, name_temp);
        strcat (name, "H");
        if (!get_int (name, &list_size_h[i]))
            list_size_h[i] = -1;

        strcpy (name, name_temp);
        strcat (name, "W");
        if (!get_int (name, &list_size_w[i]))
            list_size_w[i] = -1;

    }
    input_close();
}


