/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id: db.c,v 1.48 2002/06/06 17:03:37 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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

#include "config.h"
#include "snprintf.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "draw_symbols.h"
#include "alert.h"
#include "util.h"
#include "bulletin_gui.h"
#include "fcc_data.h"
#include "rac_data.h"
#include "interface.h"
#include "wx.h"
#include "igate.h"
#include "list_gui.h"
#include "track_gui.h"
#include "xa_config.h"

#ifdef  WITH_DMALLOC
#include <dmalloc.h>
#endif


#define STATION_REMOVE_CYCLE 60         /* check station remove in seconds (every minute) */
#define MESSAGE_REMOVE_CYCLE 60         /* check message remove in seconds (every minute) */
#define MAX_TRAIL_SEG_LEN    60l        /* max length of displayed trail segments in minutes (1 deg) */
#define IN_VIEW_MIN         600l        /* margin for off-screen stations, with possible trails on screen, in minutes */
#define TRAIL_POINT_MARGIN   30l        /* margin for off-screen trails points, for segment to be drawn, in minutes */
#define TRAIL_MAX_SPEED     900         /* max. acceptible speed for drawing trails, in mph */
#define MY_TRAIL_COLOR      0x16        /* trail color index reserved for my station */
#define MY_TRAIL_DIFF_COLOR   0         /* all my calls (SSIDs) use different colors (0/1) */
#define TRAIL_ECHO_TIME      30         /* check for delayed echos of last 30 minutes */
#define TRAIL_ECHO_COUNT    100         /* check max. 100 points for delayed echos, 0 for OFF */

// Station Info
Widget  db_station_popup = (Widget)NULL;
char *db_station_info_callsign = NULL;
Pixmap  SiS_icon0, SiS_icon;
Widget  SiS_symb;
Widget  station_list;
Widget  button_store_track;
int station_data_auto_update = 0;

Widget si_text;
Widget db_station_info  = (Widget)NULL;

static xastir_mutex db_station_info_lock;
static xastir_mutex db_station_popup_lock;

void redraw_symbols(Widget w);
int  delete_weather(DataRow *fill);
void Station_data_destroy_track(Widget widget, XtPointer clientData, XtPointer callData);
void my_station_gps_change(char *pos_long, char *pos_lat, char *course, char *speed, char speedu, char *alt, char *sats);

int  extract_speed_course(char *info, char *speed, char *course);
int  extract_bearing_NRQ(char *info, char *bearing, char *nrq);

int  tracked_stations = 0;       // A count variable used in debug code only
void track_station(Widget w, char *call_tracked, DataRow *p_station);

int  new_message_data;
time_t last_message_remove;     // last time we did a check for message removing

char packet_data[16][300];
int  packet_data_display;
int  redraw_on_new_packet_data;

long stations;                  // number of stored stations
DataRow *n_first;               // pointer to first element in name sorted station list
DataRow *n_last;                // pointer to last  element in name sorted station list
DataRow *t_first;               // pointer to first element in time sorted station list (oldest)
DataRow *t_last;                // pointer to last  element in time sorted station list (newest)
time_t last_station_remove;     // last time we did a check for station removing
time_t last_sec,curr_sec;       // for comparing if seconds in time have changed
int next_time_sn;               // time serial number for unique time index

void draw_trail(Widget w, DataRow *fill, int solid);
void export_trail(DataRow *p_station);

int decoration_offset_x = 0;
int decoration_offset_y = 0;
int last_station_info_x = 0;
int last_station_info_y = 0;
int fcc_lookup_pushed = 0;
int rac_lookup_pushed = 0;

time_t last_object_check = 0;   // Used to determine when to re-transmit objects/items





void db_init(void)
{
    init_critical_section( &db_station_info_lock );
    init_critical_section( &db_station_popup_lock );
}





///////////////////////////////////  Utilities  ////////////////////////////////////////////////////





/*
 *  Look if call is mine, exact looks on SSID too
 */
int is_my_call(char *call, int exact) {
    char *p_del;
    int ok;

    if (exact) {
        ok = (int)( !strcmp(call,my_callsign) );
    } else {
        int len1,len2;
        p_del = index(call,'-');
        if (p_del == NULL)
            len1 = (int)strlen(call);
        else
            len1 = p_del - call;
        p_del = index(my_callsign,'-');
        if (p_del == NULL)
            len2 = (int)strlen(my_callsign);
        else
            len2 = p_del - my_callsign;
        ok = (int)(len1 == len2 && !strncmp(call,my_callsign,(size_t)len1));
    }
    return(ok);
}
        




char *remove_leading_spaces(char *data) {
    int i,j;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

    j = 0;
    for( i = 0; i < strlen(data); i++ ) {
        if(data[i] != ' ') {
            data[j++] = data[i];
        }
        else {
            break;
        }
    }

    data[j] = '\0'; // Terminate the string

    return(data);
}





char *remove_trailing_spaces(char *data) {
    int i;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

    for(i=strlen(data)-1;i>=0;i--)
        if(data[i] == ' ')
            data[i] = '\0';
        else
            break;

        return(data);
}





char *remove_trailing_asterisk(char *data) {
    int i;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

    for(i=strlen(data)-1;i>0;i--) {
        if(data[i] == '*')
            data[i] = '\0';
    }
    return(data);
}





/////////////////////////////////////////// Messages ///////////////////////////////////////////

static long *msg_index;
static long msg_index_end;
static long msg_index_max;
static Message *msg_data;       // All messages, including ones we've transmitted (via loopback in the code)
time_t last_message_update = 0;
ack_record *ack_list_head = NULL;  // Head of linked list storing most recent ack's


// How often update_messages() will run, in seconds.
// This is necessary because routines like UpdateTime()
// call update_messages() VERY OFTEN.
//
// Actually, we just changed the code around so that we only call
// update_messages() with the force option, and only when we receive a
// message.  message_update_delay is no longer used, and we don't call
// update_messages() from UpdateTime() anymore.
static int message_update_delay = 300;





//WE7U
// Saves latest ack in a linked list.  We need this value in order
// to use Reply/Ack protocol when sending out messages.
void store_most_recent_ack(char *callsign, char *ack) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];
    char new_ack[5+1];

    strncpy(call,callsign,sizeof(call));
    remove_trailing_spaces(call);

    // Get a copy of "ack".  We might need to change it.
    strcpy(new_ack,ack);

    // If it's more than 2 characters long, we can't use it for
    // Reply/Ack protocol as there's only space enough for two.
    // In this case we need to make sure that we blank out any
    // former ack that was 1 or 2 characters, so that communications
    // doesn't stop.
    if ( strlen(new_ack) > 2 ) {
        // It's too long, blank it out so that gets saved as "",
        // which will overwrite any previously saved ack's that were
        // short enough to use.
        new_ack[0] = '\0';
    }

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Update the ack field.
        //printf("Found callsign %s on recent ack list, Old:%s, New:%s\n",call,p->ack,new_ack);
        xastir_snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
    }
    else {  // Not found.  Add a new record to the beginning of the
            // list.
        //printf("New callsign %s, adding to list.  Ack: %s\n",call,new_ack);
        p = (ack_record *)malloc(sizeof(ack_record));
        xastir_snprintf(p->callsign,sizeof(p->callsign),"%s",call);
        xastir_snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
        p->next = ack_list_head;
        ack_list_head = p;
    }
}





// Gets latest ack by callsign
char *get_most_recent_ack(char *callsign) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];

    strncpy(call,callsign,sizeof(call));
    remove_trailing_spaces(call);

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Return pointer to ack string.
        //printf("Found callsign %s on linked list, returning ack: %s\n",call,p->ack);
        return(&p->ack[0]);
    }
    else {
        //printf("Callsign %s not found\n",call);
        return(NULL);
    }
}





void init_message_data(void) {  // called at start of main

    new_message_data = 0;
//    message_counter = 0;  // Now read in from config file instead
    last_message_remove = sec_now();
}





#ifdef MSG_DEBUG
void msg_clear_data(Message *clear) {
    int size;
    int i;
    unsigned char *data_ptr;

    data_ptr = (unsigned char *)clear;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = 0;
}





void msg_copy_data(Message *to, Message *from) {
    int size;
    int i;
    unsigned char *data_ptr;
    unsigned char *data_ptr_from;

    data_ptr = (unsigned char *)to;
    data_ptr_from = (unsigned char *)from;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = *data_ptr_from++;
}
#endif /* MSG_DEBUG */





// Returns 1 if it's time to update the messages again
int message_update_time () {
    if ( sec_now() > (last_message_update + message_update_delay) )
        return(1);
    else
        return(0);
}





int msg_comp_active(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];

    xastir_snprintf(temp_a, sizeof(temp_a), "%c%s%s%s",
            ((Message*)a)->active, ((Message*)a)->call_sign,
            ((Message*)a)->from_call_sign,
            ((Message*)a)->seq);
    xastir_snprintf(temp_b, sizeof(temp_b), "%c%s%s%s",
            ((Message*)b)->active, ((Message*)b)->call_sign,
            ((Message*)b)->from_call_sign,
            ((Message*)b)->seq);

    return(strcmp(temp_a, temp_b));
}





int msg_comp_data(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];

    xastir_snprintf(temp_a, sizeof(temp_a), "%s%s%s",
            msg_data[*(long*)a].call_sign, msg_data[*(long *)a].from_call_sign,
            msg_data[*(long *)a].seq);
    xastir_snprintf(temp_b, sizeof(temp_b), "%s%s%s", msg_data[*(long*)b].call_sign,
            msg_data[*(long *)b].from_call_sign, msg_data[*(long *)b].seq);

    return(strcmp(temp_a, temp_b));
}





void msg_input_database(Message *m_fill) {
#define MSG_INCREMENT 10
    void *m_ptr;
    long i;

    if (msg_index_end == msg_index_max) {
        for (i = 0; i < msg_index_end; i++) {
            if (msg_data[msg_index[i]].active == RECORD_NOTACTIVE) {
                memcpy(&msg_data[msg_index[i]], m_fill, sizeof(Message));
                qsort(msg_data, (size_t)msg_index_end, sizeof(Message), msg_comp_active);
                for (i = 0; i < msg_index_end; i++) {
                    msg_index[i] = i;
                    if (msg_data[i].active == RECORD_NOTACTIVE) {
                        msg_index_end = i;
                        break;
                    }
                }
                qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);
                return;
            }
        }
        m_ptr = realloc(msg_data, (msg_index_max+MSG_INCREMENT)*sizeof(Message));
        if (m_ptr) {
            msg_data = m_ptr;
            m_ptr = realloc(msg_index, (msg_index_max+MSG_INCREMENT)*sizeof(Message *));
            if (m_ptr) {
                msg_index = m_ptr;
                msg_index_max += MSG_INCREMENT;
            } else {
                XtWarning("Unable to allocate message index.\n");
            }
        } else {
            XtWarning("Unable to allocate message database.\n");
        }
    }
    if (msg_index_end < msg_index_max) {
        msg_index[msg_index_end] = msg_index_end;
        memcpy(&msg_data[msg_index_end++], m_fill, sizeof(Message));
        qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);
    }
}





// Does a binary search through a sorted message database looking
// for a string match.
//
// If two or more messages match, this routine _should_ return the
// message with the latest timestamp.  This will ensure that earlier
// messages don't get mistaken for current messages, for the case
// where the remote station did a restart and is using the same
// sequence numbers over again.
//
long msg_find_data(Message *m_fill) {
    long record_start, record_mid, record_end, return_record, done;
    char tempfile[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char tempfill[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];


    xastir_snprintf(tempfill, sizeof(tempfill), "%s%s%s",
            m_fill->call_sign,
            m_fill->from_call_sign,
            m_fill->seq);

    return_record = -1L;
    if (msg_index && msg_index_end >= 1) {
        /* more than one record */
         record_start=0L;
         record_end = (msg_index_end - 1);
         record_mid=(record_end-record_start)/2;

         done=0;
         while (!done) {

            /* get data for record start */
            xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                    msg_data[msg_index[record_start]].call_sign,
                    msg_data[msg_index[record_start]].from_call_sign,
                    msg_data[msg_index[record_start]].seq);

            if (strcmp(tempfill, tempfile) < 0) {
                /* filename comes before */
                /*printf("Before No data found!!\n");*/
                done=1;
                break;
            } else { /* get data for record end */

                xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_end]].call_sign,
                        msg_data[msg_index[record_end]].from_call_sign,
                        msg_data[msg_index[record_end]].seq);

                if (strcmp(tempfill,tempfile)>=0) { /* at end or beyond */
                    if (strcmp(tempfill, tempfile) == 0) {
                        return_record = record_end;
                    }

                    done=1;
                    break;
                } else if ((record_mid == record_start) || (record_mid == record_end)) {
                    /* no mid for compare check to see if in the middle */
                    done=1;
                    xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                            msg_data[msg_index[record_mid]].call_sign,
                            msg_data[msg_index[record_mid]].from_call_sign,
                            msg_data[msg_index[record_mid]].seq);
                    if (strcmp(tempfill,tempfile)==0) {
                        return_record = record_mid;
                    }
                }
            }
            if (!done) { /* get data for record mid */
                xastir_snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_mid]].call_sign,
                        msg_data[msg_index[record_mid]].from_call_sign,
                        msg_data[msg_index[record_mid]].seq);

                if (strcmp(tempfill, tempfile) == 0) {
                    return_record = record_mid;
                    done = 1;
                    break;
                }

                if(strcmp(tempfill, tempfile)<0)
                    record_end = record_mid;
                else
                    record_start = record_mid;

                record_mid = record_start+(record_end-record_start)/2;
            }
        }
    }
    return(return_record);
}





void msg_replace_data(Message *m_fill, long record_num) {
    memcpy(&msg_data[msg_index[record_num]], m_fill, sizeof(Message));
}





void msg_get_data(Message *m_fill, long record_num) {
    memcpy(m_fill, &msg_data[msg_index[record_num]], sizeof(Message));
}





void msg_update_ack_stamp(long record_num) {

    //printf("Attempting to update ack stamp: %ld\n",record_num);
    if ( (record_num >= 0) && (record_num < msg_index_end) ) {
        msg_data[msg_index[record_num]].last_ack_sent = sec_now();
        //printf("Ack stamp: %ld\n",msg_data[msg_index[record_num]].last_ack_sent);
    }
    //printf("\n\n\n*** Record: %ld ***\n\n\n",record_num);
}





// WE7U
// Called when we receive an ACK.  Sets the "acked" field in a
// Message which gets rid of the highlighting in the Send Message
// dialog for that line.  This lets us know which messages have
// been acked and which have not.  If timeout is non-zero, then
// set acked to 2.  We use this in the update_messages() to flag
// that "TIMEOUT:" should prefix the string.
//
void msg_record_ack(char *to_call_sign, char *my_call, char *seq, int timeout) {
    Message m_fill;
    long record;
    int do_update = 0;

    if (debug_level & 1) {
        printf("Recording ack for message to: %s, seq: %s\n",
            to_call_sign,
            seq);
    }

    // Find the corresponding message in msg_data[i], set the
    // "acked" field to one.

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);
    if(record != -1L) {     // Found a match!
        if (debug_level & 1) {
            printf("Found in msg db, updating acked field %d -> 1, seq %s, record %ld\n",
                msg_data[msg_index[record]].acked,
                seq,
                record);
        }
        // Only cause an update if this is the first ack.  This
        // reduces dialog "flashing" a great deal
        if ( msg_data[msg_index[record]].acked == 0 ) {

            // Check for my callsign.  If found, update any open message
            // dialogs
            if (is_my_call(msg_data[msg_index[record]].from_call_sign, 1) ) {

                //printf("From: %s\tTo: %s\n",
                //    msg_data[msg_index[record]].from_call_sign,
                //    msg_data[msg_index[record]].call_sign);

                do_update++;
            }
        }
        else {  // This message has already been acked.
        }

        if (timeout)
            msg_data[msg_index[record]].acked = (char)2;
        else
            msg_data[msg_index[record]].acked = (char)1;

        if (debug_level & 1) {
            printf("Found in msg db, updating acked field %d -> 1, seq %s, record %ld\n\n",
                msg_data[msg_index[record]].acked,
                seq,
                record);
        }
    }
    else {
        if (debug_level & 1)
            printf("Matching message not found\n");
    }

    if (do_update) {
        update_messages(1); // Force an update
    }
}





// Returns the time_t for last_ack_sent, or 0.
// Also returns the record number found if not passed a NULL pointer
// in record_out.
time_t msg_data_add(char *call_sign, char *from_call, char *data,
        char *seq, char type, char from, long *record_out) {
    Message m_fill;
    long record;
    char time_data[MAX_TIME];
    int do_update = 0;
    time_t last_ack_sent;

    if (debug_level & 1)
        printf("msg_data_add start\n");

    if ( (data != NULL) && (strlen(data) > MAX_MESSAGE_LENGTH) ) {
        if (debug_level & 2)
            printf("msg_data_add:  Message length too long\n");

        *record_out = -1L;
        return((time_t)0l);
    }

    substr(m_fill.call_sign, call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, from_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same call_sign, from_call_sign,
    // and seq number
    record = msg_find_data(&m_fill);
    msg_clear_data(&m_fill);
    if(record != -1L) { /* fill old data */
        msg_get_data(&m_fill, record);
        last_ack_sent = m_fill.last_ack_sent;
        //printf("Found: last_ack_sent: %ld\n",m_fill.last_ack_sent);

        //printf("Found a duplicate message.  Updating fields, seq %s\n",seq);

        // If message is different this time, do an update to the
        // send message window and update the sec_heard field.  The
        // remote station must have restarted and is re-using the
        // sequence numbers.  What a pain!
        if (strcmp(m_fill.message_line,data) != 0) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
            do_update++;
            //printf("Message is different this time: Setting last_ack_sent to 0\n");
        }

        // If message is the same, but the sec_heard field is quite
        // old (more than 8 hours), the remote station must have
        // restarted, is re-using the sequence numbers, and just
        // happened to send the same message with the same sequence
        // number.  Again, what a pain!  Either that, or we
        // connected to a spigot with a _really_ long queue!
        if (m_fill.sec_heard < (sec_now() - (8 * 60 * 60) )) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
            do_update++;
            //printf("Found >8hrs old: Setting last_ack_sent to 0\n");
        }

        // Check for zero time
        if (m_fill.sec_heard == (time_t)0) {
            m_fill.sec_heard = sec_now();
            printf("Zero time on a previous message.\n");
        }
    }
    else {
        // Only do this if it's a new message.  This keeps things
        // more in sequence by not updating the time stamps
        // constantly on old messages that don't get ack'ed.
        m_fill.sec_heard = sec_now();
        last_ack_sent = (time_t)0;

        do_update++;    // Always do an update to the message window
                        // for new messages
        //printf("New msg: Setting last_ack_sent to 0\n");
    }

    /* FROM */
    m_fill.data_via=from;
    m_fill.active=RECORD_ACTIVE;
    m_fill.type=type;
    if (m_fill.heard_via_tnc != VIA_TNC)
        m_fill.heard_via_tnc = (from == 'T') ? VIA_TNC : NOT_VIA_TNC;

    substr(m_fill.call_sign,call_sign,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign,from_call,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    // Update the message field
    substr(m_fill.message_line,data,MAX_MESSAGE_LENGTH);

    substr(m_fill.seq,seq,MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    strcpy(m_fill.packet_time,get_time(time_data));

    if(record == -1L) {     // No old record found
        m_fill.acked = 0;   // We can't have been acked yet

        // We'll be sending an ack right away if this is a new
        // message, so might as well set the time now so that we
        // don't care about failing to set it in
        // msg_update_ack_stamp due to the record number being -1.
        m_fill.last_ack_sent = sec_now();

        msg_input_database(&m_fill);    // Create a new entry
        //printf("No record found: Setting last_ack_sent to sec_now()00\n");
    }
    else {  // Old record found
        //printf("Replacing the message in the database, seq %s\n",seq);
        msg_replace_data(&m_fill, record);  // Copy fields from m_fill to record
    }

    /* display messages */
    if (type == MESSAGE_MESSAGE)
        all_messages(from,call_sign,from_call,data);

    // Check for my callsign.  If found, update any open message
    // dialogs
    if (       is_my_call(m_fill.from_call_sign, 1)
            || is_my_call(m_fill.call_sign, 1) ) {

        if (do_update) {
            update_messages(1); // Force an update
        }
    }
 
    if (debug_level & 1)
        printf("msg_data_add end\n");

    // Return the important variables we'll need
    *record_out = record;
    return(last_ack_sent);
}
 




// WE7U:  What I'd like to do for the following routine:  Use
// XmTextGetInsertionPosition() or XmTextGetCursorPosition() to
// find the last of the text.  Could also save the position for
// each SendMessage window.  Compare the timestamps of messages
// found with the last update time.  If newer, then add them to
// the end.  This should stop the incessant scrolling.

// Another idea, easier method:  Create a buffer.  Snag out the
// messages from the array and sort by time.  Put them into a
// buffer.  Figure out the length of the text widget, and append
// the extra length of the buffer onto the end of the text widget.
// Once the message data is turned into a linked list, it might
// be sorted already by time, so this window will look better
// anyway.

// Calling update_messages with force == 1 will cause an update
// no matter what message_update_time() says.
void update_messages(int force) {
    static XmTextPosition pos;
    char temp1[MAX_CALLSIGN+1];
    char temp2[500];
    char stemp[20];
    long i;
    int mw_p;

    if ( message_update_time() || force) {

        //printf("Um %d\n",(int)sec_now() );

        /* go through all mw_p's! */

        // Perform this for each message window
        for (mw_p=0; msg_index && mw_p < MAX_MESSAGE_WINDOWS; mw_p++) {
            //pos=0;

begin_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

            if (mw[mw_p].send_message_dialog!=NULL/* && mw[mw_p].message_group==1*/) {

//printf("\n");

                // Clear the text from message window
                XmTextReplace(mw[mw_p].send_message_text,
                    (XmTextPosition) 0,
                    XmTextGetLastPosition(mw[mw_p].send_message_text),
                    "");

                // Snag the callsign you're dealing with from the message dialogue
                if (mw[mw_p].send_message_call_data != NULL) {
                    strcpy(temp1,XmTextFieldGetString(mw[mw_p].send_message_call_data));

                    new_message_data--;
                    if (new_message_data<0)
                        new_message_data=0;

                    if(strlen(temp1)>0) {   // We got a callsign from the dialog so
                        // create a linked list of the message indexes in time-sorted order

                        typedef struct _index_record {
                            int index;
                            time_t sec_heard;
                            struct _index_record *next;
                        } index_record;
                        index_record *head = NULL;
                        index_record *p_prev = NULL;
                        index_record *p_next = NULL;
//WE7U

                        // Allocate the first record (a dummy record)
                        head = (index_record *)malloc(sizeof(index_record));
                        head->index = -1;
                        head->sec_heard = (time_t)0;
                        head->next = NULL;

                        (void)remove_trailing_spaces(temp1);
                        (void)to_upper(temp1);

                        pos = 0;
                        // Loop through looking for messages to/from that callsign
                        for (i = 0; i < msg_index_end; i++) {
                            if (msg_data[msg_index[i]].active == RECORD_ACTIVE
                                    && (strcmp(temp1, msg_data[msg_index[i]].from_call_sign) == 0
                                        || strcmp(temp1,msg_data[msg_index[i]].call_sign) == 0)
                                    && (is_my_call(msg_data[msg_index[i]].from_call_sign, 1)
                                        || is_my_call(msg_data[msg_index[i]].call_sign, 1)
                                        || mw[mw_p].message_group ) ) {
                                int done = 0;

                                // Message matches our parameters so
                                // save the relevant data about the
                                // message in our linked list.  Compare
                                // the sec_heard field to see whether
                                // we're higher or lower, and insert the
                                // record at the correct spot in the
                                // list.  We end up with a time-sorted
                                // list.
                                p_prev  = head;
                                p_next = p_prev->next;
                                while (!done && (p_next != NULL)) {  // Loop until end of list or record inserted

                                    //printf("Looping, looking for insertion spot\n");

                                    if (p_next->sec_heard <= msg_data[msg_index[i]].sec_heard) {
                                        // Advance one record
                                        p_prev = p_next;
                                        p_next = p_prev->next;
                                    }
                                    else {  // We found the correct insertion spot
                                        done++;
                                    }
                                }

                                //printf("Inserting\n");

                                // Add the record in between p_prev and
                                // p_next, even if we're at the end of
                                // the list (in that case p_next will be
                                // NULL.
                                p_prev->next = (index_record *)malloc(sizeof(index_record));
                                p_prev->next->next = p_next; // Link to rest of records or NULL
                                p_prev->next->index = i;
                                p_prev->next->sec_heard = msg_data[msg_index[i]].sec_heard;
// Remember to free this entire linked list before exiting the loop for
// this message window!
                            }
                        }
                        // Done processing the entire list for this
                        // message window.

                        //printf("Done inserting/looping\n");

                        if (head->next != NULL) {   // We have messages to display
                            int done = 0;

                            //printf("We have messages to display\n");

                            // Run through the linked list and dump the
                            // info out.  It's now in time-sorted order.

// Another optimization would be to keep a count of records added, then
// later when we were dumping it out to the window, only dump the last
// XX records out.

                            p_prev = head->next;    // Skip the first dummy record
                            p_next = p_prev->next;
                            while (!done && (p_prev != NULL)) {  // Loop until end of list
                                int j = p_prev->index;  // Snag the index out of the record

                                //printf("\nLooping through, reading messages\n");
 
//printf("acked: %d\n",msg_data[msg_index[j]].acked);
 
                                // Message matches so snag the important pieces into a string
                                xastir_snprintf(stemp, sizeof(stemp),
                                    "%c%c/%c%c %c%c:%c%c",
                                    msg_data[msg_index[j]].packet_time[0],
                                    msg_data[msg_index[j]].packet_time[1],
                                    msg_data[msg_index[j]].packet_time[2],
                                    msg_data[msg_index[j]].packet_time[3],
                                    msg_data[msg_index[j]].packet_time[8],
                                    msg_data[msg_index[j]].packet_time[9],
                                    msg_data[msg_index[j]].packet_time[10],
                                    msg_data[msg_index[j]].packet_time[11]
                                );

// Somewhere in here we appear to be losing the first message.  It
// doesn't get written to the window later in the QSO.  Same for
// closing the window and re-opening it, putting the same callsign
// in and pressing "New Call" button.  First message is missing.

                                // Label the message line with who sent it.
                                // If acked = 2 a timeout has occurred
                                xastir_snprintf(temp2, sizeof(temp2),
                                    "%s  %-9s>%s%s\n",
                                    // Debug code.  Trying to find sorting error
                                    //"%ld  %s  %-9s>%s\n",
                                    //msg_data[msg_index[j]].sec_heard,
                                    stemp,
                                    msg_data[msg_index[j]].from_call_sign,
                                    (msg_data[msg_index[j]].acked == 2) ? "*TIMEOUT* " : "",
                                    msg_data[msg_index[j]].message_line);

//printf("update_message: %s|%s", temp1, temp2);
 
                                if (debug_level & 2) printf("update_message: %s|%s\n", temp1, temp2);
                                // Replace the text from pos to pos+strlen(temp2) by the string "temp2"
                                if (mw[mw_p].send_message_text != NULL) {

                                    // Insert the text at the end
//                                    XmTextReplace(mw[mw_p].send_message_text,
//                                            pos,
//                                            pos+strlen(temp2),
//                                            temp2);

                                    XmTextInsert(mw[mw_p].send_message_text,
                                            pos,
                                            temp2);
 
                                    // Set highlighting based on the "acked" field
//printf("acked: %d\t",msg_data[msg_index[j]].acked);
                                    if ( (msg_data[msg_index[j]].acked == 0)    // Not acked yet
                                            && ( is_my_call(msg_data[msg_index[j]].from_call_sign, 1)) ) {
//printf("Setting underline\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+23,
                                            pos+strlen(temp2),
                                            //XmHIGHLIGHT_SECONDARY_SELECTED); // Underlining
                                            XmHIGHLIGHT_SELECTED);         // Reverse Video
                                    }
                                    else {  // Message was acked, get rid of highlighting
//printf("Setting normal\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+23,
                                            pos+strlen(temp2),
                                            XmHIGHLIGHT_NORMAL);
                                    }

//printf("Text: %s\n",temp2); 

                                    pos += strlen(temp2);

                                }

                                // Advance to the next record in the list
                                p_prev = p_next;
                                if (p_next != NULL)
                                    p_next = p_prev->next;

                            }   // End of while
                        }   // End of if
                        else {  // No messages matched, list is empty
                        }

// What does this do?  Move all of the text?
//                        if (pos > 0) {
//                            if (mw[mw_p].send_message_text != NULL) {
//                                XmTextReplace(mw[mw_p].send_message_text,
//                                        --pos,
//                                        XmTextGetLastPosition(mw[mw_p].send_message_text),
//                                        "");
//                            }
//                        }

                        //printf("Free'ing list\n");

                        // De-allocate the linked list
                        p_prev = head;
                        while (p_prev != NULL) {

                            //printf("You're free!\n");

                            p_next = p_prev->next;
                            free(p_prev);
                            p_prev = p_next;
                        }

                        // Show the last added message in the window
                        XmTextShowPosition(mw[mw_p].send_message_text,
                            pos);
                    }
                }
            }

end_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

        }
        last_message_update = sec_now();

//printf("Message index end: %ld\n",msg_index_end);
 
    }
}





void mdelete_messages_from(char *from) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, my_callsign) == 0 && strcmp(msg_data[i].from_call_sign, from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages_to(char *to) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages(char *to_from) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to_from) == 0 || strcmp(msg_data[i].from_call_sign, to_from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdata_delete_type(const char msg_type, const time_t reference_time) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if ((msg_type == '\0' || msg_type == msg_data[i].type) &&
                msg_data[i].active == RECORD_ACTIVE && msg_data[i].sec_heard < reference_time)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void check_message_remove(void) {       // called in timing loop

    if (last_message_remove < sec_now() - MESSAGE_REMOVE_CYCLE) {
        mdata_delete_type('\0', sec_now()-sec_remove);
        last_message_remove = sec_now();
    }
}





void mscan_file(char msg_type, void (*function)(Message *)) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if ((msg_type == '\0' || msg_type == msg_data[msg_index[i]].type) &&
                msg_data[msg_index[i]].active == RECORD_ACTIVE)
            function(&msg_data[msg_index[i]]);
}





void mprint_record(Message *m_fill) {
    printf("%-9s>%-9s seq:%5s type:%c :%s\n", m_fill->from_call_sign, m_fill->call_sign,
        m_fill->seq, m_fill->type, m_fill->message_line);
}





void mdisplay_file(char msg_type) {
    printf("\n\n");
    mscan_file(msg_type, mprint_record);
    printf("\tmsg_index_end %ld, msg_index_max %ld\n", msg_index_end, msg_index_max);
}





/////////////////////////////////////// Station Data ///////////////////////////////////////////





void pad_callsign(char *callsignout, char *callsignin) {
    int i,l;

    l=(int)strlen(callsignin);
    for(i=0; i<9;i++) {
        if(i<l) {
            if(isalnum((int)callsignin[i]) || callsignin[i]=='-') {
                callsignout[i]=callsignin[i];
            }
            else {
                callsignout[i] = ' ';
            }
        }
        else {
            callsignout[i] = ' ';
        }
    }
    callsignout[i] = '\0';
}





void overlay_symbol(char symbol, char data, DataRow *fill) {
    if ( data != '/' && data !='\\') {
        /* Symbol overlay */
        fill->aprs_symbol.aprs_type = '\\';
        fill->aprs_symbol.special_overlay = data;
    } else {
        fill->aprs_symbol.aprs_type = data;
        fill->aprs_symbol.special_overlay=' ';
    }
    fill->aprs_symbol.aprs_symbol = symbol;
}





APRS_Symbol *id_callsign(char *call_sign, char * to_call) {
    char *ptr;
    char *id = "/aUfbYX's><OjRkv";
    char hold[MAX_CALLSIGN+1];
    int index;
    static APRS_Symbol symbol;

    symbol.aprs_symbol = '/';
    symbol.special_overlay = '\0';
    symbol.aprs_type ='/';
    ptr=strchr(call_sign,'-');
    if(ptr!=NULL)                      /* get symbol from SSID */
        if((index=atoi(ptr+1))<= 15)
            symbol.aprs_symbol = id[index];

    if (strncmp(to_call, "GPS", 3) == 0 || strncmp(to_call, "SPC", 3) == 0 || strncmp(to_call, "SYM", 3) == 0) {
        substr(hold, to_call+3, 3);
        if ((ptr = strpbrk(hold, "->,")) != NULL)
            *ptr = '\0';

        if (strlen(hold) >= 2) {
            switch (hold[0]) {
                case 'A':
                    symbol.aprs_type = '\\';

                case 'P':
                    if (('0' <= hold[1] && hold[1] <= '9') || ('A' <= hold[1] && hold[1] <= 'Z'))
                        symbol.aprs_symbol = hold[1];

                    break;

                case 'O':
                    symbol.aprs_type = '\\';

                case 'B':
                    switch (hold[1]) {
                        case 'B':
                            symbol.aprs_symbol = '!';
                            break;
                        case 'C':
                            symbol.aprs_symbol = '"';
                            break;
                        case 'D':
                            symbol.aprs_symbol = '#';
                            break;
                        case 'E':
                            symbol.aprs_symbol = '$';
                            break;
                        case 'F':
                            symbol.aprs_symbol = '%';
                            break;
                        case 'G':
                            symbol.aprs_symbol = '&';
                            break;
                        case 'H':
                            symbol.aprs_symbol = '\'';
                            break;
                        case 'I':
                            symbol.aprs_symbol = '(';
                            break;
                        case 'J':
                            symbol.aprs_symbol = ')';
                            break;
                        case 'K':
                            symbol.aprs_symbol = '*';
                            break;
                        case 'L':
                            symbol.aprs_symbol = '+';
                            break;
                        case 'M':
                            symbol.aprs_symbol = ',';
                            break;
                        case 'N':
                            symbol.aprs_symbol = '-';
                            break;
                        case 'O':
                            symbol.aprs_symbol = '.';
                            break;
                        case 'P':
                            symbol.aprs_symbol = '/';
                            break;
                    }
                    break;

                case 'D':
                    symbol.aprs_type = '\\';

                case 'H':
                    switch (hold[1]) {
                        case 'S':
                            symbol.aprs_symbol = '[';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '\\';
                            break;
                        case 'U':
                            symbol.aprs_symbol = ']';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '^';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '_';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '`';
                            break;
                    }
                    break;

                case 'N':
                    symbol.aprs_type = '\\';

                case 'M':
                    switch (hold[1]) {
                        case 'R':
                            symbol.aprs_symbol = ':';
                            break;
                        case 'S':
                            symbol.aprs_symbol = ';';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '<';
                            break;
                        case 'U':
                            symbol.aprs_symbol = '=';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '>';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '?';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '@';
                            break;
                    }
                    break;

                case 'Q':
                    symbol.aprs_type = '\\';

                case 'J':
                    switch (hold[1]) {
                        case '1':
                            symbol.aprs_symbol = '{';
                            break;
                        case '2':
                            symbol.aprs_symbol = '|';
                            break;
                        case '3':
                            symbol.aprs_symbol = '}';
                            break;
                        case '4':
                            symbol.aprs_symbol = '~';
                            break;
                    }
                    break;

                case 'S':
                    symbol.aprs_type = '\\';

                case 'L':
                    if ('A' <= hold[1] && hold[1] <= 'Z')
                        symbol.aprs_symbol = tolower((int)hold[1]);

                    break;
            }
            if (hold[2])
                symbol.special_overlay = hold[2];
        }
    }
    return(&symbol);
}





/******************************** Sort begin *************************** ****/





void  clear_sort_file(char *filename) {
    char ptr_filename[400];

    xastir_snprintf(ptr_filename, sizeof(ptr_filename), "%s-ptr", filename);
    (void)unlink(filename);
    (void)unlink(ptr_filename);
}





void sort_reset_pointers(FILE *pointer,long new_data_ptr,long records, int type, long start_ptr) {
    long cp;
    long temp[13000];
    long buffn,start_buffn;
    long cp_records;
    long max_buffer;
    int my_size;

    my_size=(int)sizeof(new_data_ptr);
    max_buffer=13000l;
    if(type==0) {
        /* before start_ptr */
        /* copy back pointers */
        cp=start_ptr;
        for(buffn=records; buffn > start_ptr; buffn-=max_buffer) {
            start_buffn=buffn-max_buffer;
            if(start_buffn<start_ptr)
                start_buffn=start_ptr;

            cp_records=buffn-start_buffn;
            (void)fseek(pointer,(my_size*start_buffn),SEEK_SET);
            if(fread(&temp,(my_size*cp_records),1,pointer)==1) {
                (void)fseek(pointer,(my_size*(start_buffn+1)),SEEK_SET);
                (void)fwrite(&temp,(my_size*cp_records),1,pointer);
            }
        }
        /* copy new pointer in */
        (void)fseek(pointer,(my_size*start_ptr),SEEK_SET);
        (void)fwrite(&new_data_ptr,(size_t)my_size,1,pointer);
    }
}





long sort_input_database(char *filename, char *fill, int size) {
    FILE *my_data;
    FILE *pointer;
    char file_data[2000];

    char ptr_filename[400];

    char tempfile[2000];
    char tempfill[2000];

    int ptr_size;
    long data_ptr;
    long new_data_ptr;
    long return_records;
    long records;
    long record_start;
    long record_end;
    long record_mid;
    int done;

    ptr_size=(int)sizeof(new_data_ptr);
    xastir_snprintf(ptr_filename, sizeof(ptr_filename), "%s-ptr", filename);
    /* get first string to sort on */
    (void)sscanf(fill,"%1999s",tempfill);

    data_ptr=0l;
    my_data=NULL;
    return_records=0l;
    pointer = fopen(ptr_filename,"r+");
    /* check if file is there */
    if(pointer == NULL)
      pointer = fopen(ptr_filename,"a+");

    if(pointer!=NULL) {
        my_data = fopen(filename,"a+");
        if(my_data!=NULL) {

            // Next statement needed for Solaris 7, as the fopen above
            // doesn't put the filepointer at the end of the file.
            (void) fseek(my_data,0l,SEEK_END);  //KD6ZWR

            new_data_ptr = data_ptr = ftell(my_data);
            (void)fwrite(fill,(size_t)size,1,my_data);
            records = (data_ptr/size);
            return_records=records+1;
            if(records<1) {
                /* no data yet */
                (void)fseek(pointer,0l,SEEK_SET);
                (void)fwrite(&data_ptr,(size_t)ptr_size,1,pointer);
            } else {
                /* more than one record*/
                (void)fseek(pointer,(ptr_size*records),SEEK_SET);
                (void)fwrite(&data_ptr,(size_t)ptr_size,1,pointer);
                record_start=0l;
                record_end=records;
                record_mid=(record_end-record_start)/2;
                done=0;
                while(!done) {
                    /*printf("Records Start %ld, Mid %ld, End %ld\n",record_start,record_mid,record_end);*/
                    /* get data for record start */
                    (void)fseek(pointer,(ptr_size*record_start),SEEK_SET);
                    (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                    (void)fseek(my_data,data_ptr,SEEK_SET);
                    if(fread(file_data,(size_t)size,1,my_data)==1) {
                        /* COMPARE HERE */
                        (void)sscanf(file_data,"%1999s",tempfile);
                        if(strcasecmp(tempfill,tempfile)<0) {
                            /* file name comes before */
                            /*printf("END - Before start\n");*/
                            done=1;
                            /* now place pointer before start*/
                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_start);
                        } else {
                            /* get data for record end */
                            (void)fseek(pointer,(ptr_size*record_end),SEEK_SET);
                            (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                            (void)fseek(my_data,data_ptr,SEEK_SET);
                            if(fread(file_data,(size_t)size,1,my_data)==1) {
                                /* COMPARE HERE */
                                (void)sscanf(file_data,"%1999s",tempfile);
                                if(strcasecmp(tempfill,tempfile)>0) {
                                    /* file name comes after */
                                    /*printf("END - After end\n");*/
                                    done=1;
                                    /* now place pointer after end */
                                } else {
                                    if((record_mid==record_start) || (record_mid==record_end)) {
                                        /* no mid for compare check to see if in the middle */
                                        /*printf("END - NO Middle\n");*/
                                        done=1;
                                        /* now place pointer before start*/
                                        if (record_mid==record_start)
                                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_mid+1);
                                        else
                                            sort_reset_pointers(pointer,new_data_ptr,records,0,record_mid-1);
                                    } else {
                                        /* get data for record mid */
                                        (void)fseek(pointer,(ptr_size*record_mid),SEEK_SET);
                                        (void)fread(&data_ptr,(size_t)ptr_size,1,pointer);
                                        (void)fseek(my_data,data_ptr,SEEK_SET);
                                        if(fread(file_data,(size_t)size,1,my_data)==1) {
                                            /* COMPARE HERE */
                                            (void)sscanf(file_data,"%1999s",tempfile);
                                            if(strcasecmp(tempfill,tempfile)<0) {
                                                /* checking comes before */
                                                /*record_start=0l;*/
                                                record_end=record_mid;
                                                record_mid=record_start+(record_end-record_start)/2;
                                                /*printf("TOP %ld, mid %ld\n",record_mid,record_end);*/
                                            } else {
                                                /* checking comes after*/
                                                record_start=record_mid;
                                                /*record_end=end*/
                                                record_mid=record_start+(record_end-record_start)/2;
                                                /*printf("BOTTOM start %ld, mid %ld\n",record_start,record_mid);*/
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } else
            printf("Could not open file %s\n",filename);
    } else
        printf("Could not open file %s\n",filename);

    if(my_data!=NULL)
        (void)fclose(my_data);

    if(pointer!=NULL)
        (void)fclose(pointer);

    return(return_records);
}





/******************** sort end **********************/



// is_altnet()
//
// Returns true if station fits the current altnet description.
//
int is_altnet(DataRow *p_station) {
    char temp_altnet_call[20+1];
    char temp2[20+1];
    char *net_ptr;
    int  altnet_match;
    int  result;


    // Snag a possible altnet call out of the record for later use
    substr(temp_altnet_call, p_station->node_path, MAX_CALL);
    strcpy(temp2,temp_altnet_call); // Save for later

    if ((net_ptr = strchr(temp_altnet_call, ',')))
        *net_ptr = '\0';    // Chop the string at the first ',' character

    for (altnet_match = (int)strlen(altnet_call); altnet && altnet_call[altnet_match-1] == '*'; altnet_match--);

    result = (!strncmp(temp_altnet_call, altnet_call, (size_t)altnet_match)
                 || !strcmp(temp_altnet_call, "local")
                 || !strncmp(temp_altnet_call, "SPC", 3)
                 || !strcmp(temp_altnet_call, "SPECL")
                 || is_my_call(p_station->call_sign,1));

    if ( (debug_level & 1) && result )
        printf( "%s  %-9s  %s\n", altnet_call, temp_altnet_call, temp2 );

    return(result);
}





// display_station
//
// single is 1 if the calling station wants to update only a
// single station.  If updating multiple stations in a row, then
// "single" will be passed to us as a zero.
//
void display_station(Widget w, DataRow *p_station, int single) {
    char temp_altitude[20];
    char temp_course[20];
    char temp_speed[20];
    char temp_call[20+1];
    char wx_tm[50];
    char temp_wx_temp[30];
    char temp_wx_wind[40];
    char temp_my_distance[20];
    char temp_my_course[20];
    char temp1_my_course[20];
    time_t temp_sec_heard;
    long l_lon, l_lat;
    WeatherRow *weather;
    char orient;
    float value;
    char tmp[7+1];


    if (debug_level & 128)
        printf("Display station called for Single=%d.\n", single);

    if (!symbol_display)
        return;

    // setup call string for display
    if (symbol_callsign_display)
        strcpy(temp_call,p_station->call_sign);
    else
        strcpy(temp_call,"");

    // setup altitude string for display
    if (symbol_alt_display && strlen(p_station->altitude)>0) {
        xastir_snprintf(temp_altitude, sizeof(temp_altitude), "%.0f%s",
            atof(p_station->altitude)*cvt_m2len,un_alt);
    } else
        strcpy(temp_altitude,"");

    // setup speed and course strings for display
    strcpy(temp_speed,"");
    strcpy(temp_course,"");

    if (!(strlen(p_station->course)>0 && strlen(p_station->speed)>0 && atof(p_station->course) == 0 && atof(p_station->speed) == 0)) {
        // don't display 'fixed' stations speed and course
        if (symbol_speed_display && strlen(p_station->speed)>0) {
            strncpy(tmp, un_spd, sizeof(tmp));
            tmp[sizeof(tmp)-1] = '\0';     // Terminate the string
            if (symbol_speed_display == 1)
                tmp[0] = '\0';          // without unit
 
            xastir_snprintf(temp_speed, sizeof(temp_speed), "%.0f%s",
                    atof(p_station->speed)*cvt_kn2len,tmp);
        }

        if(symbol_course_display && strlen(p_station->course)>0) {
            xastir_snprintf(temp_course, sizeof(temp_course), "%.0f�",
                    atof(p_station->course));
        }
    }

    // setup distance and direction strings for display
    if (symbol_dist_course_display && strcmp(p_station->call_sign,my_callsign)!=0) {
        l_lat = convert_lat_s2l(my_lat);
        l_lon = convert_lon_s2l(my_long);

        // Get distance in nautical miles, convert to current measurement standard
        value = cvt_kn2len * calc_distance_course(l_lat,l_lon,
                p_station->coord_lat,p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

        if (value < 5.0)
            sprintf(temp_my_distance,"%0.1f%s",value,un_dst);
        else
            sprintf(temp_my_distance,"%0.0f%s",value,un_dst);

        xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%.0f�",
                atof(temp1_my_course));
    } else {
        strcpy(temp_my_distance,"");
        strcpy(temp_my_course,"");
    }

    // setup weather strings for display
    strcpy(temp_wx_temp,"");
    strcpy(temp_wx_wind,"");
    if (symbol_weather_display && p_station->weather_data != NULL) {
        weather = p_station->weather_data;
        if (strlen(weather->wx_temp) > 0) {
            strcpy(tmp,"T:");
            if (symbol_weather_display != 2)
                tmp[0] = '\0';

            if (units_english_metric)
                xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f�F ",
                        tmp, atof(weather->wx_temp));
            else
                xastir_snprintf(temp_wx_temp, sizeof(temp_wx_temp), "%s%.0f�C ",
                        tmp,((atof(weather->wx_temp)-32.0)*5.0)/9.0);
        }
        if (symbol_weather_display == 2) {
            if (strlen(weather->wx_hum) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "H:%.0f%%", atof(weather->wx_hum));
                strcat(temp_wx_temp,wx_tm);
            }
            if (strlen(weather->wx_speed) > 0)
                xastir_snprintf(temp_wx_wind, sizeof(temp_wx_wind), "S:%.0f%s ",
                        atof(weather->wx_speed)*cvt_mi2len,un_spd);

            if (strlen(weather->wx_gust) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "G:%.0f%s ",
                        atof(weather->wx_gust)*cvt_mi2len,un_spd);
                strcat(temp_wx_wind,wx_tm);
            }
            if (strlen(weather->wx_course) > 0) {
                xastir_snprintf(wx_tm, sizeof(wx_tm), "C:%.0f�", atof(weather->wx_course));
                strcat(temp_wx_wind,wx_tm);
            }
            if (temp_wx_wind[strlen(temp_wx_wind)-1] == ' ')
                temp_wx_wind[strlen(temp_wx_wind)-1] = '\0';  // delete blank at EOL
        }
        if (temp_wx_temp[strlen(temp_wx_temp)-1] == ' ')
            temp_wx_temp[strlen(temp_wx_temp)-1] = '\0';  // delete blank at EOL
    }

    (void)remove_trailing_asterisk(p_station->call_sign);  // DK7IN: is this needed here?

    if (symbol_display == 2)
        orient = symbol_orient(p_station->course);   // rotate symbol
    else
        orient = ' ';

    // Prevents my own call from "ghosting"?
    temp_sec_heard = (strcmp(p_station->call_sign, my_callsign) == 0) ? sec_now(): p_station->sec_heard;


//WE7U6
    // Check whether it's a locally-owned object/item
    if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
            && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
              || ((p_station->flag & ST_ITEM  ) != 0) ) ) { // or an item
        temp_sec_heard = sec_now(); // We don't want our own objects/items to "ghost"
    }


//------------------------------------------------------------------------------------------

    // If we're only planning on updating a single station at this time.
    // The section here draws directly onto the screen instead of a pixmap.
    if (single) {
        if (show_amb && p_station->pos_amb)
            draw_ambiguity(p_station->coord_lon, p_station->coord_lat,
                    p_station->pos_amb,temp_sec_heard,XtWindow(da));

//WE7U
        // Check for DF'ing data, draw DF circles if present and enabled
        if (show_DF && strlen(p_station->signal_gain) == 7) {  // There's an SHGD defined
            //printf("SHGD:%s\n",p_station->signal_gain);
            draw_DF_circle(p_station->coord_lon,
                    p_station->coord_lat,
                    p_station->signal_gain,
                    temp_sec_heard,
                    XtWindow(da));
        }

//WE7U
        // Check for DF'ing beam heading/NRQ data
        if (show_DF && (strlen(p_station->bearing) == 3) && (strlen(p_station->NRQ) == 3) ) {
            //printf("Bearing: %s\n",p_station->signal_gain,NRQ);
            if (p_station->df_color == -1)
                p_station->df_color = rand() % 32;
            draw_bearing(p_station->coord_lon,
                    p_station->coord_lat,
                    p_station->course,
                    p_station->bearing,
                    p_station->NRQ,
                    trail_colors[p_station->df_color],
                    temp_sec_heard,
                    XtWindow(da));
        }

        if (p_station->aprs_symbol.area_object.type != AREA_NONE)
            draw_area(p_station->coord_lon, p_station->coord_lat,
                p_station->aprs_symbol.area_object.type,
                p_station->aprs_symbol.area_object.color,
                p_station->aprs_symbol.area_object.sqrt_lat_off,
                p_station->aprs_symbol.area_object.sqrt_lon_off,
                p_station->aprs_symbol.area_object.corridor_width,
                temp_sec_heard,
                XtWindow(da));

        draw_symbol(w,
            p_station->aprs_symbol.aprs_type,
            p_station->aprs_symbol.aprs_symbol,
            p_station->aprs_symbol.special_overlay,
            p_station->coord_lon,
            p_station->coord_lat,
            temp_call,
            temp_altitude,
            temp_course,
            temp_speed,
            temp_my_distance,
            temp_my_course,
            temp_wx_temp,
            temp_wx_wind,
            temp_sec_heard,
            XtWindow(da),
            orient,
            p_station->aprs_symbol.area_object.type);

        temp_sec_heard = p_station->sec_heard;    // DK7IN: ???
    }   // End of "if (single)" portion

//------------------------------------------------------------------------------------------

    // Now we draw the same things into pixmaps instead of the screen.
    // Then when we update from pixmap_final to the screen all of the items
    // will still be there.


//WE7U6
    // Check whether it's a locally-owned object/item
    if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
            && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
              || ((p_station->flag & ST_ITEM  ) != 0) ) ) { // or an item
        temp_sec_heard = sec_now(); // We don't want our own objects/items to "ghost"
    }


    if (show_amb && p_station->pos_amb)
            draw_ambiguity(p_station->coord_lon,p_station->coord_lat,p_station->pos_amb,temp_sec_heard,pixmap_final);

//WE7U
    // Check for DF'ing data, draw DF circles if present and enabled
    if (show_DF && strlen(p_station->signal_gain) == 7) {  // There's an SHGD defined
        //printf("SHGD:%s\n",p_station->signal_gain);
        draw_DF_circle(p_station->coord_lon,p_station->coord_lat,p_station->signal_gain,temp_sec_heard,pixmap_final);
    }

//WE7U
    // Check for DF'ing beam heading/NRQ data
    if (show_DF && (strlen(p_station->bearing) == 3) && (strlen(p_station->NRQ) == 3) ) {
        //printf("Bearing: %s\n",p_station->signal_gain,NRQ);
        if (p_station->df_color == -1)
            p_station->df_color = rand() % 32;
        draw_bearing(p_station->coord_lon,
                p_station->coord_lat,
                p_station->course,
                p_station->bearing,
                p_station->NRQ,
                trail_colors[p_station->df_color],
                temp_sec_heard,
                pixmap_final);
    }


    if (p_station->aprs_symbol.area_object.type != AREA_NONE)
            draw_area(p_station->coord_lon, p_station->coord_lat,
            p_station->aprs_symbol.area_object.type,
            p_station->aprs_symbol.area_object.color,
            p_station->aprs_symbol.area_object.sqrt_lat_off,
            p_station->aprs_symbol.area_object.sqrt_lon_off,
            p_station->aprs_symbol.area_object.corridor_width,
            temp_sec_heard,
            pixmap_final);

    draw_symbol(w,
        p_station->aprs_symbol.aprs_type,
        p_station->aprs_symbol.aprs_symbol,
        p_station->aprs_symbol.special_overlay,
        p_station->coord_lon,
        p_station->coord_lat,
        temp_call,
        temp_altitude,
        temp_course,
        temp_speed,
        temp_my_distance,
        temp_my_course,
        temp_wx_temp,
        temp_wx_wind,
        temp_sec_heard,
        pixmap_final,
        orient,
        p_station->aprs_symbol.area_object.type);

    if (show_phg && strlen(p_station->power_gain) == 7) {   // There's a PHG defined
        /*printf("PHG:%s\n",p_station->power_gain);*/

        if ( !(p_station->flag & ST_MOVING) || show_phg_mobiles ) { // Not-moving, or mobile PHG flag turned on
            draw_phg_rng(p_station->coord_lon,p_station->coord_lat,p_station->power_gain,temp_sec_heard,pixmap_final);
            if (single) { // data_add       ????
                draw_phg_rng(p_station->coord_lon,p_station->coord_lat,p_station->power_gain,temp_sec_heard,XtWindow(w));
            }
        }
    }
    else if (show_phg && show_phg_default && !(p_station->flag & (ST_OBJECT|ST_ITEM))) {
        // No PHG defined and not an object/item.  Display a PHG of 3130 as default
        // as specified in the spec:  9W, 3dB omni at 20 feet = 6.2 mile PHG radius.
        if ( !(p_station->flag & ST_MOVING) || show_phg_mobiles ) { // Not-moving, or mobile PHG flag turned on
            draw_phg_rng(p_station->coord_lon,p_station->coord_lat,"PHG3130",temp_sec_heard,pixmap_final);
        }
    }
}





// draw line relativ
void draw_test_line(Widget w, long x, long y, long dx, long dy, long ofs) {

    x += screen_width  - 10 - ofs;
    y += screen_height - 10;
    (void)XDrawLine(XtDisplay(w),pixmap_final,gc,x,y,x+dx,y+dy);
}





// draw text
void draw_ruler_text(Widget w, char * text, long ofs) {
    int x,y;
    int len;

    len = (int)strlen(text);
    x = screen_width  - 10 - ofs / 2;
    y = screen_height - 10;
    x -= len * 3;
    y -= 3;
    draw_nice_string(w,pixmap_final,0,x,y,text,0x10,0x20,len);
}





/*
 *  Calculate and draw ruler on right bottom of screen
 */
void draw_ruler(Widget w) {
    int ruler_pix;      // min size of ruler in pixel
    char unit[5+1];     // units
    char text[20];      // ruler text
    double ruler_siz;   // len of ruler in meters etc.
    int mag;
    int i;
    int dx, dy;

    ruler_pix = (int)(screen_width / 9);        // ruler size (in pixels)
    ruler_siz = ruler_pix * scale_x * calc_dscale_x(mid_x_long_offset,mid_y_lat_offset); // size in meter

    if(units_english_metric) {
        if (ruler_siz > 1609.3/2) {
            strcpy(unit,"mi");
            ruler_siz /= 1609.3;
        } else {
            strcpy(unit,"ft");
            ruler_siz /= 0.3048;
        }
    } else {
        strcpy(unit,"m");
        if (ruler_siz > 1000/2) {
            strcpy(unit,"km");
            ruler_siz /= 1000.0;
        }
    }

    mag = 1;
    while (ruler_siz > 5.0) {             // get magnitude
        ruler_siz /= 10.0;
        mag *= 10;
    }
    // select best value and adjust ruler length
    if (ruler_siz > 2.0) {
        ruler_pix = (int)(ruler_pix * 5.0 / ruler_siz +0.5);
        ruler_siz = 5.0 * mag;
    } else {
        if (ruler_siz > 1.0) {
            ruler_pix = (int)(ruler_pix * 2.0 / ruler_siz +0.5);
            ruler_siz = 2.0 * mag;
        } else {
            ruler_pix = (int)(ruler_pix * 1.0 / ruler_siz +0.5);
            ruler_siz = 1.0 * mag;
        }
    }
    xastir_snprintf(text, sizeof(text), "%.0f %s",ruler_siz,unit);      // setup string
    //printf("Ruler: %s, %d\n",text,ruler_pix);

    (void)XSetLineAttributes(XtDisplay(w),gc,1,LineSolid,CapRound,JoinRound);
    (void)XSetForeground(XtDisplay(w),gc,colors[0x20]);         // white
    for (i = 8; i >= 0; i--) {
        dx = (((i / 3)+1) % 3)-1;         // looks complicated...
        dy = (((i % 3)+1) % 3)-1;         // I want 0 / 0 as last entry

        if (i == 0)
            (void)XSetForeground(XtDisplay(w),gc,colors[0x10]);         // black

        draw_test_line(w,dx,dy,          ruler_pix,0,ruler_pix);        // hor line
        draw_test_line(w,dx,dy,              0,5,    ruler_pix);        // ver left
        draw_test_line(w,dx+ruler_pix,dy,    0,5,    ruler_pix);        // ver right
        if (text[0] == '2')
            draw_test_line(w,dx+0.5*ruler_pix,dy,0,3,ruler_pix);        // ver middle

        if (text[0] == '5') {
            draw_test_line(w,dx+0.2*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.4*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.6*ruler_pix,dy,0,3,ruler_pix);        // ver middle
            draw_test_line(w,dx+0.8*ruler_pix,dy,0,3,ruler_pix);        // ver middle
        }
    }
    draw_ruler_text(w,text,ruler_pix);
}





/*
 *  Display all stations on screen (trail, symbol, info text)
 */
void display_file(Widget w) {
    DataRow *p_station;         // pointer to station data
    time_t temp_sec_heard;      // time last heard
    time_t t_clr, t_old;

    if(debug_level & 1)
        printf("Display File Start\n");

    t_old = sec_now() - sec_old;        // precalc compare times
    t_clr = sec_now() - sec_clear;
    temp_sec_heard = 0l;
    p_station = t_first;                // start with oldest station, have newest on top at t_last
    while (p_station != NULL) {
        if (debug_level & 64) {
            printf("display_file: Examining %s\n", p_station->call_sign);
        }
        if ((p_station->flag & ST_ACTIVE) != 0) {       // ignore deleted objects

            temp_sec_heard = (is_my_call(p_station->call_sign,1))? sec_now(): p_station->sec_heard;

            if ((p_station->flag & ST_INVIEW) != 0) {  // skip far away stations...
                // we make better use of the In View flag in the future
                if ( !altnet || is_altnet(p_station) ) {
                    if (debug_level & 256) {
                        printf("display_file:  Inview, check for trail\n");
                    }
                    if (station_trails && p_station->track_data != NULL) {
                        // ????????????   what is the difference? :
                        if (debug_level & 256) {
                            printf( "%s:    Trails on and track_data\n",
                                    "display_file");
                        }
                        if (temp_sec_heard > t_clr) {
                            // Not too old, so draw trail
                            if (temp_sec_heard > t_old) {
                                // New trail, so draw solid trail
                                if (debug_level & 256) {
                                    printf("Drawing Solid trail for %s\n",
                                    p_station->call_sign);
                                }
                                draw_trail(w,p_station,1);
                            }
                            else {
                                if (debug_level & 256) {
                                    printf("Drawing trail for %s\n",
                                        p_station->call_sign);
                                }
                                draw_trail(w,p_station,0);
                            }
                        }
                    }
                    else if (debug_level & 256) {
                        printf("Station trails %d, track_data %x\n",
                            station_trails, (int)p_station->track_data);
                    }
                    display_station(w,p_station,0);
                }
                else if (debug_level & 64) {
                    printf("display_file: Station %s skipped altnet\n",
                        p_station->call_sign);
                }
            }
        }
        else if (debug_level & 64) {
            printf("display_file: ignored deleted %s\n", p_station->call_sign);
        }
        p_station = p_station->t_next;  // next station
    }
    if (debug_level & 1)
        printf("Display File Stop\n");
    draw_ruler(w);
}





//////////////////////////////  Station Info  /////////////////////////////////////





/*
 *  Delete Station Info PopUp
 */
void Station_data_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

    XtDestroyWidget(shell);
    db_station_info = (Widget)NULL;

end_critical_section(&db_station_info_lock, "db.c:Station_data_destroy_shell" );

}





/*
 *  Store track data for current station
 */
void Station_data_store_track(Widget w, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station = clientData;

    //busy_cursor(XtParent(w));
    busy_cursor(appshell);
    XtSetSensitive(button_store_track,FALSE);
    export_trail(p_station);            // store trail to file
}





/*
 *  Delete tracklog for current station
 */
void Station_data_destroy_track( /*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    DataRow *p_station = clientData;

    if (delete_trail(p_station))
        redraw_on_new_data = 2;         // redraw immediately
}





void Station_data_add_fcc(Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char temp[500];
    FccAppl my_data;
    char *station = (char *) clientData;

    (void)check_fcc_data();
    //busy_cursor(XtParent(w));
    busy_cursor(appshell);
    if (search_fcc_data_appl(station, &my_data)==1) {
        /*printf("FCC call %s\n",station);*/
        xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s %s %s\n%s %s, %s %s, %s %s\n\n",
            langcode("STIFCC0001"),
            langcode("STIFCC0003"),my_data.name_licensee,
            langcode("STIFCC0004"),my_data.text_street,my_data.text_pobox,
            langcode("STIFCC0005"),my_data.city,
            langcode("STIFCC0006"),my_data.state,
            langcode("STIFCC0007"),my_data.zipcode);
            XmTextInsert(si_text,0,temp);
            XmTextShowPosition(si_text,0);

        fcc_lookup_pushed = 1;
    }
}





void Station_data_add_rac(Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char temp[512];
    char club[512];
    rac_record my_data;
    char *station = (char *) clientData;

    strncpy(temp," ",sizeof(temp));
    (void)check_rac_data();
    //busy_cursor(XtParent(w));
    busy_cursor(appshell);
    if (search_rac_data(station, &my_data)==1) {
        /*printf("IC call %s\n",station);*/
        /*   sprintf(temp,"RAC Database Lookup\n%15.15s %20.20s\n%31.31s\n%20.20s\n%2.2s %7.7s\n", */
        xastir_snprintf(temp, sizeof(temp), "%s\n%s %s\n%s\n%s, %s\n%s\n",
                langcode("STIFCC0002"),my_data.first_name,my_data.last_name,my_data.address,
                my_data.city,my_data.province,my_data.postal_code);

        if (my_data.qual_a[0] == 'A')
            strcat(temp,langcode("STIFCC0008"));

        if (my_data.qual_d[0] == 'D')
            strcat(temp,langcode("STIFCC0009"));

        if (my_data.qual_b[0] == 'B' && my_data.qual_c[0] != 'C')
            strcat(temp,langcode("STIFCC0010"));

        if (my_data.qual_c[0] == 'C')
            strcat(temp,langcode("STIFCC0011"));

        strcat(temp,"\n");
        if (strlen(my_data.club_name) > 1){
            xastir_snprintf(club, sizeof(club), "%s\n%s\n%s, %s\n%s\n",
                    my_data.club_name, my_data.club_address,
                    my_data.club_city, my_data.club_province, my_data.club_postal_code);
            strcat(temp,club);
        }
        strcat(temp,"\n");
        XmTextInsert(si_text,0,temp);
        XmTextShowPosition(si_text,0);

        rac_lookup_pushed = 1;
    }
}





void Station_query_trace(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRST", call);
    transmit_message_data(station,temp);
}





void Station_query_messages(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRSM", call);
    transmit_message_data(station,temp);
}





void Station_query_direct(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?APRSD", call);
    transmit_message_data(station,temp);
}





void Station_query_version(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *station = (char *) clientData;
    char temp[50];
    char call[25];

    pad_callsign(call,station);
    xastir_snprintf(temp, sizeof(temp), ":%s:?VER", call);
    transmit_message_data(station,temp);
}





void General_query(/*@unused@*/ Widget w, XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    char *location = (char *) clientData;
    char temp[50];

    xastir_snprintf(temp, sizeof(temp), "?APRS?%s", location);
    output_my_data(temp,-1,0,0);
}





void IGate_query(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    output_my_data("?IGATE?",-1,0,0);
}





void WX_query(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer calldata) {
    output_my_data("?WX?",-1,0,0);
}





/*
 *  Change data for current object
 *  If calldata = 2, we're doing a move object operation.  Pass the
 *  value on through to Set_Del_Object.
 */
void Modify_object( Widget w, XtPointer clientData, XtPointer calldata) {
    DataRow *p_station = clientData;

    //if (calldata != NULL)
    //    printf("Modify_object:  calldata:  %s\n", calldata);

    //printf("Object Name: %s\n", p_station->call_sign);
    Set_Del_Object( w, p_station, calldata );
}





static void PosTestExpose(Widget parent, XtPointer clientData, XEvent *event, Boolean * continueToDispatch) {
    Position x, y;

    XtVaGetValues(parent, XmNx, &x, XmNy, &y, NULL);

    if (debug_level & 1)
        printf("Window Decoration Offsets:  X:%d\tY:%d\n", x, y);

    // Store the new-found offets in global variables
    decoration_offset_x = (int)x;
    decoration_offset_y = (int)y;

    // Get rid of the event handler and the test dialog
    XtRemoveEventHandler(parent, ExposureMask, True, (XtEventHandler) PosTestExpose, (XtPointer)NULL);
    XtRemoveGrab(XtParent(parent));
    XtDestroyWidget(XtParent(parent));
}





// Here's a stupid trick that we have to do in order to find out how big
// window decorations are.  We need to know this information in order to
// be able to kill/recreate dialogs in the same place each time.  If we
// were to just get and set the X/Y values of the dialog, we would creep
// across the screen by the size of the decorations each time.
// I've seen it.  It's ugly.
//
void compute_decorations( void ) {
    Widget cdtest = (Widget)NULL;
    Widget cdform = (Widget)NULL;
    Cardinal n = 0;
    Arg args[20];
 

    // We'll create a dummy dialog at 0,0, then query it's
    // position.  That'll give us back the position of the
    // widget.  Subtract 0,0 from it (easy huh?) and we get
    // the size of the window decorations.  Store these values
    // in global variables for later use.

    n = 0;
    XtSetArg(args[n], XmNx, 0); n++;
    XtSetArg(args[n], XmNy, 0); n++;

    cdtest = (Widget) XtVaCreatePopupShell("compute_decorations test",
                        xmDialogShellWidgetClass,Global.top,
                        args, n,
                        NULL);
 
    n = 0;
    XtSetArg(args[n], XmNwidth, 0); n++;    // Make it tiny
    XtSetArg(args[n], XmNheight, 0); n++;   // Make it tiny
    cdform = XmCreateForm(cdtest, "compute_decorations test form", args, n);

    XtAddEventHandler(cdform, ExposureMask, True, (XtEventHandler) PosTestExpose,
        (XtPointer)NULL);

    XtManageChild(cdform);
    XtManageChild(cdtest);
}





// Enable/disable auto-update of Station_data dialog
void station_data_auto_update_toggle ( /*@unused@*/ Widget widget, /*@unused@*/ XtPointer clientData, XtPointer callData) {
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set)
        station_data_auto_update = 1;
    else
        station_data_auto_update = 0;
}





// WE7U
// Fill in the station data window with real data
void station_data_fill_in ( /*@unused@*/ Widget w, XtPointer clientData, XtPointer calldata ) {
    DataRow *p_station;
    char *station = (char *) clientData;
    char temp[300];
    int pos, last_pos;
    int last;
    char temp_my_distance[20];
    char temp_my_course[20];
    char temp1_my_course[20];
    float temp_out_C, e, humidex;
    long l_lat, l_lon;
    float value;
    WeatherRow *weather;
    time_t sec;
    struct tm *time;
    int i;


    db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog
    temp_out_C=0;
    pos=0;

begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

    if (db_station_info == NULL) {  // We don't have a dialog to write to

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

        return;
    }

    if (!search_station_name(&p_station,station,1)  // Can't find call,
            || (p_station->flag & ST_ACTIVE) == 0) {  // or found deleted objects

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

        return;
    }


    // Clear the text
    XmTextSetString(si_text,NULL);

    // Packets received ... 
    //sprintf(temp,langcode("WPUPSTI005"),p_station->num_packets);
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI005"),p_station->num_packets);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp, p_station->packet_time, 2);
    temp[2]='/';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp,p_station->packet_time+2,2);
    temp[2]='/';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp,p_station->packet_time+4,4);
    temp[4]=' ';
    temp[5]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp,p_station->packet_time+8,2);
    temp[2]=':';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp,p_station->packet_time+10,2);
    temp[2]=':';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    strncpy(temp,p_station->packet_time+12,2);
    temp[2]='\n';
    temp[3]='\0';
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Object
    if (strlen(p_station->origin) > 0) {
        //sprintf(temp,langcode("WPUPSTI000"),p_station->origin);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI000"),p_station->origin);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp, "\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Heard via TNC ...
    if ((p_station->flag & ST_VIATNC) != 0) {        // test "via TNC" flag
        //sprintf(temp,langcode("WPUPSTI006"),p_station->heard_via_tnc_port);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI006"),p_station->heard_via_tnc_port);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    } else {
        //sprintf(temp,langcode("WPUPSTI007"));
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI007"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    switch(p_station->data_via) {
        case('L'):
            //sprintf(temp,langcode("WPUPSTI008"));
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI008"));
            break;

        case('T'):
            //sprintf(temp,langcode("WPUPSTI009"),p_station->last_port_heard);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI009"),p_station->last_port_heard);
            break;

        case('I'):
            //sprintf(temp,langcode("WPUPSTI010"),p_station->last_port_heard);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI010"),p_station->last_port_heard);
            break;

        case('F'):
            //sprintf(temp,langcode("WPUPSTI011"));
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI011"));
            break;

        default:
            //sprintf(temp,langcode("WPUPSTI012"));
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI012"));
            break;
    }
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->track_data != NULL) {
        //sprintf(temp,langcode("WPUPSTI013"));
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI013"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    //sprintf(temp, "\n");
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Echoed from: ...
    if (is_my_call(p_station->call_sign,1)) {
        //sprintf(temp,langcode("WPUPSTI055"));
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI055"));
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        for (i=0;i<6;i++) {
            if (echo_digis[i][0] == '\0')
                break;

            //sprintf(temp," %s",echo_digis[i]);
            xastir_snprintf(temp, sizeof(temp), " %s",echo_digis[i]);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }
        //sprintf(temp, "\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Data Path ...
    //sprintf(temp,langcode("WPUPSTI043"),p_station->node_path);
    xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI043"),p_station->node_path);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    //sprintf(temp,"\n");
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Comments ...
    if(strlen(p_station->comments)>0) {
        //sprintf(temp,langcode("WPUPSTI044"),p_station->comments);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI044"),p_station->comments);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Current Power Gain ...
    if (strlen(p_station->power_gain) == 7)
        phg_decode(langcode("WPUPSTI014"), p_station->power_gain, temp, sizeof(temp) );
    else if (p_station->flag & (ST_OBJECT|ST_ITEM))
        //sprintf(temp, langcode("WPUPSTI014"), "none");
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "none");
    else if (units_english_metric)
        //sprintf(temp, langcode("WPUPSTI014"), "default (9W @ 20ft HAAT, 3dB omni, range 6.2mi)");
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "default (9W @ 20ft HAAT, 3dB omni, range 6.2mi)");
    else
        //sprintf(temp, langcode("WPUPSTI014"), "default (9W @ 6.1m HAAT, 3dB omni, range 10.0km)");
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI014"), "default (9W @ 6.1m HAAT, 3dB omni, range 10.0km)");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);
    //sprintf(temp,"\n");
    xastir_snprintf(temp, sizeof(temp), "\n");
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // Current DF Info ...
    if (strlen(p_station->signal_gain) == 7) {
        shg_decode(langcode("WPUPSTI057"), p_station->signal_gain, temp, sizeof(temp) );
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    if (strlen(p_station->bearing) == 3) {
        bearing_decode(langcode("WPUPSTI058"), p_station->bearing, p_station->NRQ, temp, sizeof(temp) );
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Signpost Data
    if (strlen(p_station->signpost) > 0) {
        //sprintf(temp,"%s: %s",langcode("POPUPOB029"), p_station->signpost);
        xastir_snprintf(temp, sizeof(temp), "%s: %s",langcode("POPUPOB029"), p_station->signpost);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Altitude ...
    last_pos=pos;
    if (strlen(p_station->altitude) > 0) {
        if (units_english_metric)
            //sprintf(temp,langcode("WPUPSTI016"),atof(p_station->altitude)*3.28084,"ft");
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude)*3.28084,"ft");
        else
            //sprintf(temp,langcode("WPUPSTI016"),atof(p_station->altitude),"m");
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI016"),atof(p_station->altitude),"m");

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Course ...
    if (strlen(p_station->course) > 0) {
        //sprintf(temp,langcode("WPUPSTI017"),p_station->course);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI017"),p_station->course);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Speed ...
    if (strlen(p_station->speed) > 0) {
        if (units_english_metric)
            //sprintf(temp,langcode("WPUPSTI019"),atof(p_station->speed)*1.1508);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI019"),atof(p_station->speed)*1.1508);

        else
            //sprintf(temp,langcode("WPUPSTI018"),atof(p_station->speed)*1.852);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI018"),atof(p_station->speed)*1.852);

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if (last_pos!=pos) {
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Distance ...
    last_pos = pos;
    // do my course
    if (!is_my_call(p_station->call_sign,1)) {
        l_lat = convert_lat_s2l(my_lat);
        l_lon = convert_lon_s2l(my_long);

        // Get distance in nautical miles.
        value = (float)calc_distance_course(l_lat,l_lon,p_station->coord_lat,
            p_station->coord_lon,temp1_my_course,sizeof(temp1_my_course));

        // n7tap: This is a quick hack to get some more useful values for
        //        distance to near ojects.  I will translate the strings someday.
        if (units_english_metric) {
            if (value*1.15078 < 0.99)
                //sprintf(temp_my_distance,"%d yards",(int)(value*1.15078*1760));
                xastir_snprintf(temp_my_distance, sizeof(temp_my_distance), "%d yards",(int)(value*1.15078*1760));

            else
                //sprintf(temp_my_distance,langcode("WPUPSTI020"),value*1.15078);
                xastir_snprintf(temp_my_distance, sizeof(temp_my_distance), langcode("WPUPSTI020"),value*1.15078);
        }
        else {
            if (value*1.852 < 0.99)
                //sprintf(temp_my_distance,"%d meters",(int)(value*1.852*1000));
                xastir_snprintf(temp_my_distance, sizeof(temp_my_distance), "%d meters",(int)(value*1.852*1000));

            else
                //sprintf(temp_my_distance,langcode("WPUPSTI021"),value*1.852);
                xastir_snprintf(temp_my_distance, sizeof(temp_my_distance), langcode("WPUPSTI021"),value*1.852);
        }
        //sprintf(temp_my_course,"%s�",temp1_my_course);
        xastir_snprintf(temp_my_course, sizeof(temp_my_course), "%s�",temp1_my_course);
        //sprintf(temp,langcode("WPUPSTI022"),temp_my_distance,temp_my_course);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI022"),temp_my_distance,temp_my_course);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if(last_pos!=pos) {
        //sprintf(temp,"\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    // Last Position
    sec  = p_station->sec_heard;
    time = localtime(&sec);
    //sprintf(temp,"%s%02d/%02d  %02d:%02d   ",langcode("WPUPSTI023"),
    //        time->tm_mon + 1, time->tm_mday,time->tm_hour,time->tm_min);
    xastir_snprintf(temp, sizeof(temp), "%s%02d/%02d  %02d:%02d   ",langcode("WPUPSTI023"),
        time->tm_mon + 1, time->tm_mday,time->tm_hour,time->tm_min);
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (coordinate_system == USE_UTM) {
        convert_xastir_to_UTM_str(temp, sizeof(temp),
            p_station->coord_lon, p_station->coord_lat);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }
    else {
        if (coordinate_system == USE_DDDDDD) {
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS) {
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_DMS_NORMAL);
        }
        else {  // Assume coordinate_system == USE_DDMMMM
            convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        //sprintf(temp,"  ");
        xastir_snprintf(temp, sizeof(temp), "  ");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        if (coordinate_system == USE_DDDDDD) {
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DEC_DEG);
        }
        else if (coordinate_system == USE_DDMMSS) {
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_DMS_NORMAL);
        }
        else {  // Assume coordinate_system == USE_DDMMMM
            convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_HP_NORMAL);
        }
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
    }

    if (p_station->altitude[0] != '\0')
        //sprintf(temp," %5.0f%s",atof(p_station->altitude)*cvt_m2len,un_alt);
        xastir_snprintf(temp, sizeof(temp), " %5.0f%s", atof(p_station->altitude)*cvt_m2len, un_alt);
    else
        substr(temp,"        ",1+5+strlen(un_alt));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->speed[0] != '\0')
        //sprintf(temp," %4.0f%s",atof(p_station->speed)*cvt_kn2len,un_spd);
        xastir_snprintf(temp, sizeof(temp), " %4.0f%s",atof(p_station->speed)*cvt_kn2len,un_spd);
    else
        substr(temp,"         ",1+4+strlen(un_spd));
    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if (p_station->course[0] != '\0')
        //sprintf(temp," %3d�",atoi(p_station->course));
        xastir_snprintf(temp, sizeof(temp), " %3d�",atoi(p_station->course));
    else
        //sprintf(temp, "     ");
        xastir_snprintf(temp, sizeof(temp), "     ");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    if ((p_station->flag & ST_LOCAL) != 0)
        //sprintf(temp, " *\n");
        xastir_snprintf(temp, sizeof(temp), " *\n");

    else
        //sprintf(temp, "  \n");
        xastir_snprintf(temp, sizeof(temp), "  \n");

    XmTextInsert(si_text,pos,temp);
    pos += strlen(temp);

    // list rest of trail data
    if (p_station->track_data != NULL) {
        if ((p_station->track_data->trail_inp - p_station->track_data->trail_out-1+MAX_TRACKS)
                %MAX_TRACKS +1 > 1) {       // we need at least a second point
            for (last=(p_station->track_data->trail_inp-2+MAX_TRACKS)%MAX_TRACKS;
                    (last+1-p_station->track_data->trail_out+MAX_TRACKS)%MAX_TRACKS > 0;
                    last = (last-1+MAX_TRACKS)%MAX_TRACKS) {
                sec  = p_station->track_data->sec[last];
                time = localtime(&sec);
                if ((p_station->track_data->flag[last] & TR_NEWTRK) != '\0')
                    //sprintf(temp,"            +  %02d/%02d  %02d:%02d   ",
                    //        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);
                    xastir_snprintf(temp, sizeof(temp), "            +  %02d/%02d  %02d:%02d   ",
                        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);
                else
                    //sprintf(temp,"               %02d/%02d  %02d:%02d   ",
                    //        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);
                    xastir_snprintf(temp, sizeof(temp), "               %02d/%02d  %02d:%02d   ",
                        time->tm_mon + 1,time->tm_mday,time->tm_hour,time->tm_min);

                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                if (coordinate_system == USE_UTM) {
                    convert_xastir_to_UTM_str(temp, sizeof(temp),
                        p_station->track_data->trail_long_pos[last],
                        p_station->track_data->trail_lat_pos[last]);
                    XmTextInsert(si_text,pos,temp);
                    pos += strlen(temp);
                }
                else {
                    if (coordinate_system == USE_DDDDDD) {
                        convert_lat_l2s(p_station->track_data->trail_lat_pos[last], temp, sizeof(temp), CONVERT_DEC_DEG);
                    }
                    else if (coordinate_system == USE_DDMMSS) {
                        convert_lat_l2s(p_station->track_data->trail_lat_pos[last], temp, sizeof(temp), CONVERT_DMS_NORMAL);
                    }
                    else {  // Assume coordinate_system == USE_DDMMMM
                        convert_lat_l2s(p_station->track_data->trail_lat_pos[last], temp, sizeof(temp), CONVERT_HP_NORMAL);
                    }
                    XmTextInsert(si_text,pos,temp);
                    pos += strlen(temp);

                    //sprintf(temp,"  ");
                    xastir_snprintf(temp, sizeof(temp), "  ");
                    XmTextInsert(si_text,pos,temp);
                    pos += strlen(temp);

                    if (coordinate_system == USE_DDDDDD) {
                        convert_lon_l2s(p_station->track_data->trail_long_pos[last], temp, sizeof(temp), CONVERT_DEC_DEG);
                    }
                    else if (coordinate_system == USE_DDMMSS) {
                        convert_lon_l2s(p_station->track_data->trail_long_pos[last], temp, sizeof(temp), CONVERT_DMS_NORMAL);
                    }
                    else {  // Assume coordinate_system == USE_DDMMMM
                        convert_lon_l2s(p_station->track_data->trail_long_pos[last], temp, sizeof(temp), CONVERT_HP_NORMAL);
                    }
                    XmTextInsert(si_text,pos,temp);
                    pos += strlen(temp);
                }

                if (p_station->track_data->altitude[last] > -99999)
                    //sprintf(temp," %5.0f%s",p_station->track_data->altitude[last]*cvt_dm2len,un_alt);
                    xastir_snprintf(temp, sizeof(temp), " %5.0f%s",p_station->track_data->altitude[last]*cvt_dm2len,un_alt);
                else
                    substr(temp,"         ",1+5+strlen(un_alt));

                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                if (p_station->track_data->speed[last] >= 0)
                    //sprintf(temp," %4.0f%s",p_station->track_data->speed[last]*cvt_hm2len,un_spd);
                    xastir_snprintf(temp, sizeof(temp), " %4.0f%s",p_station->track_data->speed[last]*cvt_hm2len,un_spd);
                else
                    substr(temp,"         ",1+4+strlen(un_spd));

                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                if (p_station->track_data->course[last] >= 0)
                    //sprintf(temp," %3d�",p_station->track_data->course[last]);
                    xastir_snprintf(temp, sizeof(temp), " %3d�",p_station->track_data->course[last]);
                else
                    //sprintf(temp, "     ");
                    xastir_snprintf(temp, sizeof(temp), "     ");

                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);

                if ((p_station->track_data->flag[last] & TR_LOCAL) != '\0')
                    //sprintf(temp, " *\n");
                    xastir_snprintf(temp, sizeof(temp), " *\n");
                else
                    //sprintf(temp, "  \n");
                    xastir_snprintf(temp, sizeof(temp), "  \n");

                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }
        }
    }

    // Weather Data ...
    if (p_station->weather_data != NULL) {
        weather = p_station->weather_data;
        //sprintf(temp, "\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        //sprintf(temp,langcode("WPUPSTI024"),weather->wx_type,weather->wx_station);
        xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI024"),weather->wx_type,weather->wx_station);
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        sprintf(temp, "\n");
        xastir_snprintf(temp, sizeof(temp), "\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);
        if (units_english_metric)
            //sprintf(temp,langcode("WPUPSTI026"),weather->wx_course,weather->wx_speed);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI026"),weather->wx_course,weather->wx_speed);
        else
            //sprintf(temp,langcode("WPUPSTI025"),weather->wx_course,(int)(atof(weather->wx_speed)*1.6094));
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI025"),weather->wx_course,(int)(atof(weather->wx_speed)*1.6094));

        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

        if (strlen(weather->wx_gust) > 0) {
            if (units_english_metric)
                //sprintf(temp,langcode("WPUPSTI028"),weather->wx_gust);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI028"),weather->wx_gust);
            else
                //sprintf(temp,langcode("WPUPSTI027"),(int)(atof(weather->wx_gust)*1.6094));
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI027"),(int)(atof(weather->wx_gust)*1.6094));

            strcat(temp, "\n");
        } else
            //sprintf(temp, "\n");
            xastir_snprintf(temp, sizeof(temp), "\n");

        XmTextInsert(si_text, pos, temp);
        pos += strlen(temp);

        if (strlen(weather->wx_temp) > 0) {
            if (units_english_metric)
                //sprintf(temp,langcode("WPUPSTI030"),weather->wx_temp);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI030"),weather->wx_temp);
            else {
                temp_out_C =(((atof(weather->wx_temp)-32)*5.0)/9.0);
                //sprintf(temp,langcode("WPUPSTI029"),temp_out_C);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI029"),temp_out_C);
            }
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_hum) > 0) {
            //sprintf(temp,langcode("WPUPSTI031"),weather->wx_hum);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI031"),weather->wx_hum);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

// NOTE:  The below (Humidex) is not coded for english units, only for metric.
// What is Humidex anyway?  Heat Index?  Wind Chill? --we7u

        // DK7IN: ??? units_english ???
        if (strlen(weather->wx_hum) > 0 && strlen(weather->wx_temp) > 0 && (!units_english_metric) &&
                (atof(weather->wx_hum) > 0.0) ) {

            e = (float)(6.112 * pow(10,(7.5 * temp_out_C)/(237.7 + temp_out_C)) * atof(weather->wx_hum) / 100.0);
            humidex = (temp_out_C + ((5.0/9.0) * (e-10.0)));

            //sprintf(temp,langcode("WPUPSTI032"),humidex);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI032"),humidex);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_baro) > 0) {
            //sprintf(temp,langcode("WPUPSTI033"),weather->wx_baro);
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI033"),weather->wx_baro);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            //sprintf(temp, "\n");
            xastir_snprintf(temp, sizeof(temp), "\n");
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        } else {
            if(last_pos!=pos) {
                //sprintf(temp, "\n");
                xastir_snprintf(temp, sizeof(temp), "\n");
                XmTextInsert(si_text,pos,temp);
                pos += strlen(temp);
            }
        }

        if (strlen(weather->wx_snow) > 0) {
            if(units_english_metric)
                //sprintf(temp,langcode("WPUPSTI035"),atof(weather->wx_snow)/100.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI035"),atof(weather->wx_snow)/100.0);
            else
                //sprintf(temp,langcode("WPUPSTI034"),atof(weather->wx_snow)*2.54);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI034"),atof(weather->wx_snow)*2.54);
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            //sprintf(temp, "\n");
            xastir_snprintf(temp, sizeof(temp), "\n");
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain) > 0 || strlen(weather->wx_prec_00) > 0
                || strlen(weather->wx_prec_24) > 0) {
            //sprintf(temp,langcode("WPUPSTI036"));
            xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI036"));
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain) > 0) {
            if (units_english_metric)
                //sprintf(temp,langcode("WPUPSTI038"),atof(weather->wx_rain)/100.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI038"),atof(weather->wx_rain)/100.0);
            else
                //sprintf(temp,langcode("WPUPSTI037"),atof(weather->wx_rain)*.254);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI037"),atof(weather->wx_rain)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_prec_24) > 0) {
            if(units_english_metric)
                //sprintf(temp,langcode("WPUPSTI040"),atof(weather->wx_prec_24)/100.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI040"),atof(weather->wx_prec_24)/100.0);
            else
                //sprintf(temp,langcode("WPUPSTI039"),atof(weather->wx_prec_24)*.254);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI039"),atof(weather->wx_prec_24)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_prec_00) > 0) {
            if (units_english_metric)
                //sprintf(temp,langcode("WPUPSTI042"),atof(weather->wx_prec_00)/100.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI042"),atof(weather->wx_prec_00)/100.0);
            else
                //sprintf(temp,langcode("WPUPSTI041"),atof(weather->wx_prec_00)*.254);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI041"),atof(weather->wx_prec_00)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }

        if (strlen(weather->wx_rain_total) > 0) {
            //sprintf(temp,"\n%s",langcode("WPUPSTI046"));
            xastir_snprintf(temp, sizeof(temp), "\n%s",langcode("WPUPSTI046"));
            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
            if (units_english_metric)
                //sprintf(temp,langcode("WPUPSTI048"),atof(weather->wx_rain_total)/100.0);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI048"),atof(weather->wx_rain_total)/100.0);
            else
                //sprintf(temp,langcode("WPUPSTI047"),atof(weather->wx_rain_total)*.254);
                xastir_snprintf(temp, sizeof(temp), langcode("WPUPSTI047"),atof(weather->wx_rain_total)*.254);

            XmTextInsert(si_text,pos,temp);
            pos += strlen(temp);
        }
        //sprintf(temp, "\n\n");
        xastir_snprintf(temp, sizeof(temp), "\n\n");
        XmTextInsert(si_text,pos,temp);
        pos += strlen(temp);

    }

    if (fcc_lookup_pushed) {
        Station_data_add_fcc(w, clientData, calldata);
    }
    else if (rac_lookup_pushed) {
        Station_data_add_rac(w, clientData, calldata);
    }

    XmTextShowPosition(si_text,0);

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

}





//WE7U
/*
 *  List station info and trail
 *  If calldata is non-NULL, then we drop straight through to the
 *  Modify->Object dialog.
 */
void Station_data(/*@unused@*/ Widget w, XtPointer clientData, XtPointer calldata) {
    DataRow *p_station;
    char *station = (char *) clientData;
    char temp[300];
    unsigned int n;
    Atom delw;
    static Widget  pane, form, button_cancel, button_message, button_fcc,
      button_rac, button_clear_track, button_trace, button_messages, button_object_modify,
      button_direct, button_version, station_icon, station_call, station_type,
      station_data_auto_update_w;
    Arg args[20];
    Pixmap icon;
    Position x,y;    // For saving current dialog position
    int restore_position = 0;


//WE7U7
    busy_cursor(appshell);

    db_station_info_callsign = (char *) clientData; // Used for auto-updating this dialog

    // If we haven't calculated our decoration offsets yet, do so now
    if ( (decoration_offset_x == 0) && (decoration_offset_y == 0) ) {
        compute_decorations();
    }
 
    if (db_station_info != NULL) {  // We already have a dialog
        restore_position = 1;

        // This is a pain.  We can get the X/Y position, but when
        // we restore the new dialog to the same position we're
        // off by the width/height of our window decorations.  Call
        // above was added to pre-compute the offsets that we'll need.
        XtVaGetValues(db_station_info, XmNx, &x, XmNy, &y, NULL);

        // This call doesn't work.  It returns the widget location,
        // just like the XtVaGetValues call does.  I need the window
        // decoration location instead.
        //XtTranslateCoords(db_station_info, 0, 0, &xnew, &ynew);
        //printf("%d:%d\t%d:%d\n", x, xnew, y, ynew);

        if (last_station_info_x == 0)
            last_station_info_x = x - decoration_offset_x;

        if (last_station_info_y == 0)
            last_station_info_y = y - decoration_offset_y;

        // Now get rid of the old dialog
       Station_data_destroy_shell(db_station_info, db_station_info, NULL);
    }
    else {
        // Clear the global state variables
        fcc_lookup_pushed = 0;
        rac_lookup_pushed = 0;
    }


begin_critical_section(&db_station_info_lock, "db.c:Station_data" );


    if (db_station_info == NULL) {

        db_station_info = XtVaCreatePopupShell(langcode("WPUPSTI001"),xmDialogShellWidgetClass,Global.top,
                        XmNdeleteResponse,XmDESTROY,
                        XmNdefaultPosition, FALSE,
                        NULL);

        pane = XtVaCreateWidget("Station Data pane",xmPanedWindowWidgetClass, db_station_info,
                        XmNbackground, colors[0xff],
                        NULL);

        form =  XtVaCreateWidget("Station Data form",xmFormWidgetClass, pane,
                        XmNfractionBase, 4,
                        XmNbackground, colors[0xff],
                        XmNautoUnmanage, FALSE,
                        XmNshadowThickness, 1,
                        NULL);

        if (search_station_name(&p_station,station,1)           // find call
                  && (p_station->flag & ST_ACTIVE) != 0) {   // ignore deleted objects
            icon = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),
                    20,20,DefaultDepthOfScreen(XtScreen(appshell)));

            symbol(db_station_info,0,p_station->aprs_symbol.aprs_type,
                p_station->aprs_symbol.aprs_symbol,
                p_station->aprs_symbol.special_overlay,icon,0,0,0,' ');

            station_icon = XtVaCreateManagedWidget("Station Data icon", xmLabelWidgetClass, form,
                                XmNtopAttachment, XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_FORM,
                                XmNleftOffset, 5,
                                XmNrightAttachment, XmATTACH_NONE,
                                XmNlabelType, XmPIXMAP,
                                XmNlabelPixmap,icon,
                                XmNbackground, colors[0xff],
                                NULL);

            station_type = XtVaCreateManagedWidget("Station Data type", xmTextFieldWidgetClass, form,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNshadowThickness,       0,
                                XmNcolumns,5,
                                XmNwidth,((5*7)+2),
                                XmNbackground, colors[0xff],
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget,station_icon,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                NULL);

            //sprintf(temp, "%c%c%c", p_station->aprs_symbol.aprs_type,
            //        p_station->aprs_symbol.aprs_symbol,
            //        p_station->aprs_symbol.special_overlay);
            xastir_snprintf(temp, sizeof(temp), "%c%c%c", p_station->aprs_symbol.aprs_type,
                p_station->aprs_symbol.aprs_symbol,
                p_station->aprs_symbol.special_overlay);

            XmTextFieldSetString(station_type, temp);
            XtManageChild(station_type);

            station_call = XtVaCreateManagedWidget("Station Data call", xmTextFieldWidgetClass, form,
                                XmNeditable,   FALSE,
                                XmNcursorPositionVisible, FALSE,
                                XmNtraversalOn, FALSE,
                                XmNshadowThickness,       0,
                                XmNcolumns,15,
                                XmNwidth,((15*7)+2),
                                XmNbackground, colors[0xff],
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget, station_type,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                NULL);

            XmTextFieldSetString(station_call,p_station->call_sign);
            XtManageChild(station_call);

//WE7U
            station_data_auto_update_w = XtVaCreateManagedWidget(langcode("WPUPSTI056"),
                                xmToggleButtonGadgetClass, form,
                                XmNtopAttachment,XmATTACH_FORM,
                                XmNtopOffset, 2,
                                XmNbottomAttachment,XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_WIDGET,
                                XmNleftWidget, station_call,
                                XmNleftOffset,10,
                                XmNrightAttachment,XmATTACH_NONE,
                                XmNbackground,colors[0xff],
                                NULL);
            XtAddCallback(station_data_auto_update_w,XmNvalueChangedCallback,station_data_auto_update_toggle,"1");

            n=0;
            XtSetArg(args[n], XmNrows, 15); n++;
            XtSetArg(args[n], XmNcolumns, 100); n++;
            XtSetArg(args[n], XmNeditable, FALSE); n++;
            XtSetArg(args[n], XmNtraversalOn, FALSE); n++;
            XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
            XtSetArg(args[n], XmNwordWrap, TRUE); n++;
            XtSetArg(args[n], XmNbackground, colors[0xff]); n++;
            XtSetArg(args[n], XmNscrollHorizontal, FALSE); n++;
            XtSetArg(args[n], XmNcursorPositionVisible, FALSE); n++;
            XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
            XtSetArg(args[n], XmNtopWidget, station_icon); n++;
            XtSetArg(args[n], XmNtopOffset, 5); n++;
            XtSetArg(args[n], XmNbottomAttachment, XmATTACH_NONE); n++;
            XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNleftOffset, 5); n++;
            XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
            XtSetArg(args[n], XmNrightOffset, 5); n++;

            si_text = NULL;
            si_text = XmCreateScrolledText(form,"Station_data",args,n);


end_critical_section(&db_station_info_lock, "db.c:Station_data" );

            // Fill in the si_text widget with real data
            station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);
 
begin_critical_section(&db_station_info_lock, "db.c:Station_data" );

        }


        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget,XtParent(si_text),
                            XmNtopOffset,10,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_POSITION,
                            XmNleftPosition, 3,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 4,
                            XmNrightOffset, 5,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
        XtAddCallback(button_cancel, XmNactivateCallback, Station_data_destroy_shell, db_station_info);

        // [ Store Track ] or single Position
        button_store_track = XtVaCreateManagedWidget(langcode("WPUPSTI054"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget,XtParent(si_text),
                            XmNtopOffset,10,
                            XmNbottomAttachment, XmATTACH_NONE,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
        XtAddCallback(button_store_track,   XmNactivateCallback, Station_data_store_track ,(XtPointer)p_station);

        if (p_station->track_data != NULL) {
            // [ Clear Track ]
            button_clear_track = XtVaCreateManagedWidget(langcode("WPUPSTI045"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget,button_store_track,
                            XmNtopOffset,1,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset,5,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_clear_track, XmNactivateCallback, Station_data_destroy_track ,(XtPointer)p_station);

        } else {
            // DK7IN: I drop the version button for mobile stations
            // we just have too much buttons...
            // and should find another solution
            // [ Station Version Query ]
            button_version = XtVaCreateManagedWidget(langcode("WPUPSTI052"),xmPushButtonGadgetClass, form,
                            XmNtopAttachment, XmATTACH_WIDGET,
                            XmNtopWidget,button_store_track,
                            XmNtopOffset,1,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNbottomOffset,5,
                            XmNleftAttachment, XmATTACH_FORM,
                            XmNleftOffset,5,
                            XmNrightAttachment, XmATTACH_POSITION,
                            XmNrightPosition, 1,
                            XmNbackground, colors[0xff],
                            XmNnavigationType, XmTAB_GROUP,
                            NULL);
            XtAddCallback(button_version, XmNactivateCallback, Station_query_version ,(XtPointer)p_station->call_sign);
        }            

    if ( ((p_station->flag & ST_OBJECT) == 0) && ((p_station->flag & ST_ITEM) == 0) ) { // Not an object/
        // printf("Not an object or item...\n");
        // [Send Message]
        button_message = XtVaCreateManagedWidget(langcode("WPUPSTI002"),xmPushButtonGadgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget,XtParent(si_text),
                XmNtopOffset,10,
                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_message, XmNactivateCallback, Send_message_call ,(XtPointer)p_station->call_sign);
    } else {
        // printf("Found an object or item...\n");
        button_object_modify = XtVaCreateManagedWidget(langcode("WPUPSTI053"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,XtParent(si_text),
                                XmNtopOffset,10,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_object_modify, XmNactivateCallback, Modify_object, (XtPointer)p_station);
    }

    // Add FCC button only if probable match
        if ((! strncmp(station,"A",1)) || (! strncmp(station,"K",1)) ||
            (! strncmp(station,"N",1)) || (! strncmp(station,"W",1))  ) {
            button_fcc = XtVaCreateManagedWidget(langcode("WPUPSTI003"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,XtParent(si_text),
                                XmNtopOffset,10,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 2,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 3,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
            XtAddCallback(button_fcc, XmNactivateCallback, Station_data_add_fcc,(XtPointer)p_station->call_sign);

            if ( ! check_fcc_data())
                XtSetSensitive(button_fcc,FALSE);
        }

    // Add RAC button only if probable match
        if (!strncmp(station,"VE",2) || !strncmp(station,"VA",2)) {
            button_rac = XtVaCreateManagedWidget(langcode("WPUPSTI004"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,XtParent(si_text),
                                XmNtopOffset,10,
                                XmNbottomAttachment, XmATTACH_NONE,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 2,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 3,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
            XtAddCallback(button_rac, XmNactivateCallback, Station_data_add_rac,(XtPointer)p_station->call_sign);

            if ( ! check_rac_data())
                XtSetSensitive(button_rac,FALSE);
        }

        // [ Trace Query ]
        button_trace = XtVaCreateManagedWidget(langcode("WPUPSTI049"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,button_store_track,
                                XmNtopOffset,1,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_trace, XmNactivateCallback, Station_query_trace ,(XtPointer)p_station->call_sign);

        // [ Un-Acked Messages Query ]
        button_messages = XtVaCreateManagedWidget(langcode("WPUPSTI050"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,button_store_track,
                                XmNtopOffset,1,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 2,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 3,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_messages, XmNactivateCallback, Station_query_messages ,(XtPointer)p_station->call_sign);

        // [ Direct Stations Query ]
        button_direct = XtVaCreateManagedWidget(langcode("WPUPSTI051"),xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,button_store_track,
                                XmNtopOffset,1,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset,5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 3,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 4,
                                XmNbackground, colors[0xff],
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);
        XtAddCallback(button_direct, XmNactivateCallback, Station_query_direct ,(XtPointer)p_station->call_sign);

        if (restore_position) {
            XtVaSetValues(db_station_info,XmNx,x - decoration_offset_x,XmNy,y - decoration_offset_y,NULL);
        }
        else {
            pos_dialog(db_station_info);
        }

        delw = XmInternAtom(XtDisplay(db_station_info),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(db_station_info, delw, Station_data_destroy_shell, (XtPointer)db_station_info);

        XtManageChild(form);
        XtManageChild(si_text);
        XtVaSetValues(si_text, XmNbackground, colors[0x0f], NULL);
        XtManageChild(pane);

//WE7U
        if (station_data_auto_update)
            XmToggleButtonSetState(station_data_auto_update_w,TRUE,FALSE);

        if (calldata == NULL) { // We're not going straight to the Modify dialog
                                // and will actually use the dialog we just drew

            XtPopup(db_station_info,XtGrabNone);

            fix_dialog_size(db_station_info);
            XmTextShowPosition(si_text,0);

            // Move focus to the Cancel button.  This appears to highlight t
            // button fine, but we're not able to hit the <Enter> key to
            // have that default function happen.  Note:  We _can_ hit the
            // <SPACE> key, and that activates the option.
            XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);
        }
    }

end_critical_section(&db_station_info_lock, "db.c:Station_data" );

    if (calldata != NULL) {     // We were called from the Object->Modify menu item
        Modify_object(w, (XtPointer)p_station, calldata);
        // We just drew all of the above, but now we wish to destroy it and
        // just leave the modify dialog in place.
        Station_data_destroy_shell(w, (XtPointer)db_station_info, NULL);
    }
}





// Used for auto-refreshing the Station_info dialog.  Called from
// main.c:UpdateTime() every 30 seconds.
//
void update_station_info(Widget w) {

begin_critical_section(&db_station_info_lock, "db.c:update_station_info" );

    // If we have a dialog to update and a callsign to pass to it
    if (( db_station_info != NULL)
            && (db_station_info_callsign != NULL)
            && (strlen(db_station_info_callsign) != 0) ) {

end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

        // Fill in the si_text widget with real data
        station_data_fill_in( w, (XtPointer)db_station_info_callsign, NULL);
    }
    else {

end_critical_section(&db_station_info_lock, "db.c:update_station_info" );

    }
}





/*
 *  Station Info Selection PopUp window: Canceled
 */
void Station_info_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;

    if (db_station_info!=NULL)
        Station_data_destroy_shell(db_station_info, db_station_info, NULL);

    XtPopdown(shell);
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);  
    (void)XFreePixmap(XtDisplay(appshell),SiS_icon);  

begin_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

    XtDestroyWidget(shell);
    db_station_popup = (Widget)NULL;

end_critical_section(&db_station_popup_lock, "db.c:Station_info_destroy_shell" );

}





/*
 *  Station Info Selection PopUp window: Quit with selected station
 */
void Station_info_select_destroy_shell(Widget widget, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    int i,x;
    char *temp;
    char temp2[50];
    XmString *list;
    int found;
    //Widget shell = (Widget) clientData;

    found=0;

begin_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

    if (db_station_popup) {
        XtVaGetValues(station_list,XmNitemCount,&i,XmNitems,&list,NULL);

        for (x=1; x<=i;x++) {
            if (XmListPosSelected(station_list,x)) {
                found=1;
                if (XmStringGetLtoR(list[(x-1)],XmFONTLIST_DEFAULT_TAG,&temp))
                    x=i+1;
            }
        }

        // DK7IN ?? should we not first close the PopUp, then call Station_data ??
        if (found) {
            //sprintf(temp2,"%s",temp);
            xastir_snprintf(temp2, sizeof(temp2), "%s", temp);
            Station_data(widget,temp2,NULL);
            XtFree(temp);
        }
/*
        // This section of code gets rid of the Station Chooser.  Frank wanted to
        // be able to leave the Station Chooser up after selection so that other
        // stations could be selected, therefore I commented this out.
        XtPopdown(shell);                   // Get rid of the station chooser popup here
        (void)XFreePixmap(XtDisplay(appshell),SiS_icon0);
        (void)XFreePixmap(XtDisplay(appshell),SiS_icon);
        XtDestroyWidget(shell);             // and here
        db_station_popup = (Widget)NULL;    // and here
*/
    }

end_critical_section(&db_station_popup_lock, "db.c:Station_info_select_destroy_shell" );

}





/*
 *  Station Info PopUp
 *  if only one station in view it shows the data with Station_data()
 *  otherwise we get a selection box
 *  clientData will be non-null if we wish to drop through to the object->modify
 *  dialog.
 */
void Station_info(Widget w, /*@unused@*/ XtPointer clientData, XtPointer calldata) {
    DataRow *p_station;
    DataRow *p_found;
    int num_found;
    unsigned long min_diff,diff;
    XmString str_ptr;
    unsigned int n;
    Atom delw;
    static Widget pane, form, button_ok, button_cancel;
    Arg al[20];                    /* Arg List */
    register unsigned int ac = 0;           /* Arg Count */


//WE7U7
    busy_cursor(appshell);

    num_found=0;
    min_diff=32000000ul;
    p_found = NULL;
    p_station = n_first;
    while (p_station != NULL) {    // search through database for nearby stations
        if ((p_station->flag & ST_INVIEW) != 0) {       // only test stations in view
            if (!altnet || is_altnet(p_station)) {
                diff = (unsigned long)( labs((x_long_offset+(menu_x*scale_x)) - p_station->coord_lon)
                    + labs((y_lat_offset+(menu_y*scale_y))  - p_station->coord_lat) );
                if (diff < min_diff) {
                    min_diff  = diff;
                    p_found   = p_station;
                    num_found = 1;
                } else {
                    if (diff <= min_diff+(scale_x+scale_y))
                        num_found++;
                }
            }
        }
        p_station = p_station->n_next;
    }

    if (p_found != NULL && (min_diff < (unsigned)(125*(scale_x+scale_y)))) {
        if (num_found == 1) { // We only found one station, so it's easy
            Station_data(w,p_found->call_sign,clientData);
        } else {            // We found more: create dialog to choose a station
            if (db_station_popup != NULL)
                Station_info_destroy_shell(db_station_popup, db_station_popup, NULL);

begin_critical_section(&db_station_popup_lock, "db.c:Station_info" );

            if (db_station_popup == NULL) {
                // setup a selection box:
                db_station_popup = XtVaCreatePopupShell(langcode("STCHO00001"),xmDialogShellWidgetClass,Global.top,
                                    XmNdeleteResponse,XmDESTROY,
                                    XmNdefaultPosition, FALSE,
                                    XmNbackground, colors[0xff],
                                    NULL);

                pane = XtVaCreateWidget("Station_info pane",xmPanedWindowWidgetClass, db_station_popup,
                            XmNbackground, colors[0xff],
                            NULL);

                form =  XtVaCreateWidget("Station_info form",xmFormWidgetClass, pane,
                            XmNfractionBase, 5,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);

                /*set args for color */
                ac = 0;
                XtSetArg(al[ac], XmNbackground, colors[0xff]); ac++;
                XtSetArg(al[ac], XmNvisibleItemCount, 6); ac++;
                XtSetArg(al[ac], XmNtraversalOn, TRUE); ac++;
                XtSetArg(al[ac], XmNshadowThickness, 3); ac++;
                XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
                XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT); ac++;
                XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNtopOffset, 5); ac++;
                XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
                XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNrightOffset, 5); ac++;
                XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
                XtSetArg(al[ac], XmNleftOffset, 5); ac++;

                station_list = XmCreateScrolledList(form,"Station_info list",al,ac);

// DK7IN: I want to add the symbol in front of the call...
        // icon
        SiS_icon0 = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
        SiS_icon  = XCreatePixmap(XtDisplay(appshell),RootWindowOfScreen(XtScreen(appshell)),20,20,
                    DefaultDepthOfScreen(XtScreen(appshell)));
/*      SiS_symb  = XtVaCreateManagedWidget("Station_info icon", xmLabelWidgetClass, ob_form1,
                            XmNlabelType,               XmPIXMAP,
                            XmNlabelPixmap,             SiS_icon,
                            XmNbackground,              colors[0xff],
                            XmNleftAttachment,          XmATTACH_FORM,
                            XmNtopAttachment,           XmATTACH_FORM,
                            XmNbottomAttachment,        XmATTACH_NONE,
                            XmNrightAttachment,         XmATTACH_NONE,
                            NULL);
*/

                /*printf("What station\n");*/
                n = 1;
                p_station = n_first;
                while (p_station != NULL) {    // search through database for nearby stations
                    if ((p_station->flag & ST_INVIEW) != 0) {       // only test stations in view
                        if (!altnet || is_altnet(p_station)) {
                            diff = (unsigned long)( labs((x_long_offset+(menu_x*scale_x)) - p_station->coord_lon)
                                + labs((y_lat_offset+(menu_y*scale_y))  - p_station->coord_lat) );
                            if (diff <= min_diff+(scale_x+scale_y)) {
                                /*printf("Station %s\n",p_station->call_sign);*/
                                XmListAddItem(station_list, str_ptr = XmStringCreateLtoR(p_station->call_sign,
                                        XmFONTLIST_DEFAULT_TAG), (int)n++);
                                XmStringFree(str_ptr);
                            }
                        }
                    }
                    p_station = p_station->n_next;
                }
                button_ok = XtVaCreateManagedWidget("Info",xmPushButtonGadgetClass, form,
                                XmNtopAttachment, XmATTACH_WIDGET,
                                XmNtopWidget,XtParent(station_list),
                                XmNtopOffset,5,
                                XmNbottomAttachment, XmATTACH_FORM,
                                XmNbottomOffset, 5,
                                XmNleftAttachment, XmATTACH_POSITION,
                                XmNleftPosition, 1,
                                XmNrightAttachment, XmATTACH_POSITION,
                                XmNrightPosition, 2,
                                XmNnavigationType, XmTAB_GROUP,
                                NULL);

                button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00003"),xmPushButtonGadgetClass, form,
                                    XmNtopAttachment, XmATTACH_WIDGET,
                                    XmNtopWidget,XtParent(station_list),
                                    XmNtopOffset,5,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 5,
                                    XmNleftAttachment, XmATTACH_POSITION,
                                    XmNleftPosition, 3,
                                    XmNrightAttachment, XmATTACH_POSITION,
                                    XmNrightPosition, 4,
                                    XmNnavigationType, XmTAB_GROUP,
                                    NULL);

                XtAddCallback(button_cancel, XmNactivateCallback, Station_info_destroy_shell, db_station_popup);
                XtAddCallback(button_ok, XmNactivateCallback, Station_info_select_destroy_shell, db_station_popup);

                pos_dialog(db_station_popup);

                delw = XmInternAtom(XtDisplay(db_station_popup),"WM_DELETE_WINDOW", FALSE);
                XmAddWMProtocolCallback(db_station_popup, delw, Station_info_destroy_shell, (XtPointer)db_station_popup);

                XtManageChild(form);
                XtManageChild(station_list);
                XtVaSetValues(station_list, XmNbackground, colors[0x0f], NULL);
                XtManageChild(pane);

                XtPopup(db_station_popup,XtGrabNone);

                // Move focus to the Cancel button.  This appears to highlight t
                // button fine, but we're not able to hit the <Enter> key to
                // have that default function happen.  Note:  We _can_ hit the
                // <SPACE> key, and that activates the option.
                XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);

            }

end_critical_section(&db_station_popup_lock, "db.c:Station_info" );

        }
    }
}





int heard_via_tnc_in_past_hour(char *call) {
    DataRow *p_station;
    int in_hour;

    in_hour=0;
    if (search_station_name(&p_station,call,1)) {  // find call
        if((p_station->flag & ST_VIATNC) != 0) {        // test "via TNC" flag
            /* check to see if the last packet was message capable? */
            /* ok look to see if it is within the hour */
            in_hour = (int)((p_station->heard_via_tnc_last_time+3600l) > sec_now());
            if(debug_level & 2)
                printf("Call %s: %ld %ld ok %d\n",call,(long)(p_station->heard_via_tnc_last_time),(long)sec_now(),in_hour);
        } else {
            if (debug_level & 2)
                printf("Call %s Not heard via tnc\n",call);
        }
    } else {
        if (debug_level & 2)
            printf("IG:station not found\n");
    }
    return(in_hour);
}



//////////////////////////////////// Weather Data //////////////////////////////////////////////////



/* valid characters for APRS weather data fields */
int is_aprs_chr(char ch) {

    if (isdigit((int)ch) || ch==' ' || ch=='.' || ch=='-')
    return(1);
    else
    return(0);
}



/* check data format    123 ___ ... */
int is_weather_data(char *data, int len) {
    int ok = 1;
    int i;

    for (i=0;ok && i<len;i++)
        if (!is_aprs_chr(data[i]))
            ok = 0;
    return(ok);
}



/* extract single weather data item from information field */
int extract_weather_item(char *data, char type, int datalen, char *temp) {
    int i,ofs,found,len;

    found=0;
    len = (int)strlen(data);
    for(ofs=0; !found && ofs<len-datalen; ofs++)      // search for start sequence
        if (data[ofs]==type) {
            found=1;
            if (!is_weather_data(data+ofs+1, datalen))
                found=0;
        }
    if (found) {   // ofs now points after type character
        substr(temp,data+ofs,datalen);
        for (i=ofs-1;i<len-datalen;i++)        // delete item from info field
            data[i] = data[i+datalen+1];
    } else
        temp[0] = '\0';
    return(found);
}


// DK7IN 77
// raw weather report            in information field
// positionless weather report   in information field
// complete weather report       with lat/lon
//  see APRS Reference page 62ff

int extract_weather(DataRow *p_station, char *data, int compr) {
    char time_data[MAX_TIME];
    char temp[5];
    int  ok = 1;
    WeatherRow *weather;
    char course[4];
    char speed[4];

    if (compr) {        // compressed position report
        if (strlen(data) >= 8
                     && data[0] =='g' && is_weather_data(&data[1],3)
                     && data[4] =='t' && is_weather_data(&data[5],3)) {
            strcpy(speed, p_station->speed);    // we find WX course/speed
            strcpy(course,p_station->course);   // in compressed position data
        } else {
            ok = 0;     // no weather data found
        }
    } else {
        if (strlen(data)>=15 && data[3]=='/'
                     && is_weather_data(data,3) && is_weather_data(&data[4],3)
                     && data[7] =='g' && is_weather_data(&data[8], 3)
                     && data[11]=='t' && is_weather_data(&data[12],3)) {    // Complete Weather Report
            (void)extract_speed_course(data,speed,course);
            //    printf("found Complete Weather Report\n");
        } else
            // need date/timestamp
            if (strlen(data)>=16
                    && data[0] =='c' && is_weather_data(&data[1], 3)
                    && data[4] =='s' && is_weather_data(&data[5], 3)
                    && data[8] =='g' && is_weather_data(&data[9], 3)
                    && data[12]=='t' && is_weather_data(&data[13],3)) { // Positionless Weather Data
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degress)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                //        printf("found weather2\n");
        } else
            ok = 0;
    }

    if (ok) {
        ok = get_weather_record(p_station);     // get existing or create new weather record
    }
    if (ok) {
        weather = p_station->weather_data;
        strcpy(weather->wx_speed, speed);
        strcpy(weather->wx_course,course);
        if (compr) {        // course/speed was taken from normal data, delete that
            // fix me: we delete a potential real speed/course now
            // we should differentiate between normal and weather data in compressed position decoding...
            p_station->speed_time[0]     = '\0';
            p_station->speed[0]          = '\0';
            p_station->course[0]         = '\0';
        }
        (void)extract_weather_item(data,'g',3,weather->wx_gust);      // gust (peak wind speed in mph in the last 5 minutes)

        (void)extract_weather_item(data,'t',3,weather->wx_temp);      // temperature (in deg Fahrenheit), could be negative

        (void)extract_weather_item(data,'r',3,weather->wx_rain);      // rainfall (1/100 inch) in the last hour

        (void)extract_weather_item(data,'p',3,weather->wx_prec_24);   // rainfall (1/100 inch) in the last 24 hours

        (void)extract_weather_item(data,'P',3,weather->wx_prec_00);   // rainfall (1/100 inch) since midnight

        if (extract_weather_item(data,'h',2,weather->wx_hum))   // humidity (in %, 00 = 100%)
            //sprintf(weather->wx_hum,"%03d",(atoi(weather->wx_hum)+99)%100+1);
            xastir_snprintf(weather->wx_hum, sizeof(weather->wx_hum), "%03d",(atoi(weather->wx_hum)+99)%100+1);

        if (extract_weather_item(data,'b',5,weather->wx_baro))  // barometric pressure (1/10 mbar / 1/10 hPascal)
            //sprintf(weather->wx_baro,"%0.1f",(float)(atoi(weather->wx_baro)/10.0));
            xastir_snprintf(weather->wx_baro, sizeof(weather->wx_baro), "%0.1f",(float)(atoi(weather->wx_baro)/10.0));

        (void)extract_weather_item(data,'s',3,weather->wx_snow);      // snowfall (in inches) in the last 24 hours
                                                                      // was 1/100 inch, APRS reference says inch! ??

        (void)extract_weather_item(data,'L',3,temp);                  // luminosity (in watts per square meter) 999 and below

        (void)extract_weather_item(data,'l',3,temp);                  // luminosity (in watts per square meter) 1000 and above

        (void)extract_weather_item(data,'#',3,temp);                  // raw rain counter

//    extract_weather_item(data,'w',3,temp);                          // ?? text wUII

    // now there should be the name of the weather station...

        strcpy(weather->wx_time,get_time(time_data));
        weather->wx_sec_time=sec_now();
//        weather->wx_data=1;  // we don't need this

//        case ('.'):/* skip */
//            wx_strpos+=4;
//            break;

//        default:
//            wx_done=1;
//            weather->wx_type=data[wx_strpos];
//            if(strlen(data)>wx_strpos+1)
//                strncpy(weather->wx_station,data+wx_strpos+1,MAX_WXSTATION);
//            break;
    }
    return(ok);
}


////////////////////////////////////////////////////////////////////////////////////////////////////



void init_weather(WeatherRow *weather) {    // clear weather data

    weather->wx_sec_time      = 0; // ?? is 0 ok ??
    weather->wx_time[0]       = '\0';
    weather->wx_course[0]     = '\0';
    weather->wx_speed[0]      = '\0';
    weather->wx_speed_sec_time = 0; // ??
    weather->wx_gust[0]       = '\0';
    weather->wx_temp[0]       = '\0';
    weather->wx_rain[0]       = '\0';
    weather->wx_rain_total[0] = '\0';
    weather->wx_snow[0]       = '\0';
    weather->wx_prec_24[0]    = '\0';
    weather->wx_prec_00[0]    = '\0';
    weather->wx_hum[0]        = '\0';
    weather->wx_baro[0]       = '\0';
    weather->wx_type          = '\0';
    weather->wx_station[0]    = '\0';
}



int get_weather_record(DataRow *fill) {    // get or create weather storage
    int ok=1;

    if (fill->weather_data == NULL) {      // new weather data, allocate storage and init
        fill->weather_data = malloc(sizeof(WeatherRow));
        if (fill->weather_data == NULL)
            ok = 0;
        else
            init_weather(fill->weather_data);
    }
    return(ok);
}



int delete_weather(DataRow *fill) {    // delete weather storage, if allocated

    if (fill->weather_data != NULL) {
        free(fill->weather_data);
        fill->weather_data = NULL;
        return(1);
    }
    return(0);
}



////////////////////////////////////////// Trails //////////////////////////////////////////////////


/*
 *  See if current color is defined as active trail color
 */
int trail_color_active(int color_index) {

    // this should be made configurable...
    // to select trail colors to use
    
    return(1);          // accept this color
}


/*
 *  Get new trail color for a call
 */
int new_trail_color(char *call) {
    int color, found, i;

    if (is_my_call(call,MY_TRAIL_DIFF_COLOR)) {
        color = MY_TRAIL_COLOR;    // It's my call, so use special color
    } else {
        // all other callsigns get some other color out of the color table
        color = current_trail_color;
        for(i=0,found=0;!found && i<max_trail_colors;i++) {
            color = (color + 1) % max_trail_colors; // try next color in list
            // skip special and or inactive colors.
            if (color != MY_TRAIL_COLOR && trail_color_active(color))
                found = 1;
        }
        if (found)
            current_trail_color = color;        // save it for next time
        else
            color = current_trail_color;        // keep old color
    }
    return(color);
}


// Trail storage in ring buffer (1 is the oldest point)
// inp         v                   
//      [.][1][.][.][.][.][.][.][.]   at least 1 point in buffer,
// out      v                         else no allocated trail memory

// inp                  v
//      [.][1][2][3][4][.][.][.][.]   half full
// out      v                      

// inp      v  
//      [9][1][2][3][4][5][6][7][8]   buffer full (wrap around)
// out      v


/*
 *  Store one trail point, allocate trail storage if necessary
 */
int store_trail_point(DataRow *p_station, long lon, long lat, time_t sec, char *alt, char *speed, char *course, short stn_flag) {
    int ok = 1;
    int trail_inp, trail_prev;
    char flag;

    if (debug_level & 256) {
        printf("store_trail_point: for %s\n", p_station->call_sign);
    }
    if (p_station->track_data == NULL) {       // new trail, allocate storage and do initialisation
        if (debug_level & 256) {
            printf("Creating new trail.\n");
        }
        p_station->track_data = malloc(sizeof(TrackRow));
        if (p_station->track_data == NULL) {
            if (debug_level & 256) {
                printf("store_trail_point: MALLOC failed for trail.\n");
            }
            ok = 0;
        }
        else {
            p_station->track_data->trail_out   = p_station->track_data->trail_inp = 0;
            p_station->track_data->trail_color = new_trail_color(p_station->call_sign);
            tracked_stations++;
        }
    }
    else if (p_station->track_data->trail_out ==
                p_station->track_data->trail_inp) {   // ring buffer is full
        p_station->track_data->trail_out =   // increment and keep inside buffer
            (p_station->track_data->trail_out+1) % MAX_TRACKS;
    }
    if (ok) {
        if (debug_level & 256) {
            printf("store_trail_point: Storing data for %s[%d]n",
                p_station->call_sign, p_station->track_data->trail_inp);
        }
        trail_inp = p_station->track_data->trail_inp;
        p_station->track_data->trail_long_pos[trail_inp] = lon;
        p_station->track_data->trail_lat_pos[trail_inp]  = lat;
        p_station->track_data->sec[trail_inp]            = sec;
        if (alt[0] != '\0')
            p_station->track_data->altitude[trail_inp] = atoi(alt)*10;
        else            
            p_station->track_data->altitude[trail_inp] = -99999;
        if (speed[0] != '\0')
            p_station->track_data->speed[trail_inp]  = (long)(atof(speed)*18.52);
        else
            p_station->track_data->speed[trail_inp]  = -1;
        if (course[0] != '\0')
            p_station->track_data->course[trail_inp] = atoi(course);
        else
            p_station->track_data->course[trail_inp] = -1;
        flag = '\0';                    // init flags
        if ((stn_flag & ST_LOCAL) != 0)
            flag |= TR_LOCAL;           // set "local" flag
        if ((trail_inp-p_station->track_data->trail_out+MAX_TRACKS)%MAX_TRACKS +1 > 1) {
            // we have at least two points...
            trail_prev = (trail_inp - 1 + MAX_TRACKS) % MAX_TRACKS;
            if (abs(lon-p_station->track_data->trail_long_pos[trail_prev]) > MAX_TRAIL_SEG_LEN*60*100 ||
                    abs(lat-p_station->track_data->trail_lat_pos[trail_prev]) > MAX_TRAIL_SEG_LEN*60*100)
                flag |= TR_NEWTRK;      // set "new track" flag for long segments
            else
                if (abs(sec - p_station->track_data->sec[trail_prev]) > 2700)
                    flag |= TR_NEWTRK;  // set "new track" flag for long pauses
        } else {
            flag |= TR_NEWTRK;          // set "new track" flag for first point
        }
        p_station->track_data->flag[trail_inp] = flag;
        p_station->track_data->trail_inp = (trail_inp+1) % MAX_TRACKS;
    }
    return(ok);
}





/*
 *  Check if current packet is a delayed echo
 */
int is_trailpoint_echo(DataRow *p_station) {
    int i, j;
    time_t checktime;
    char temp[50];

    if (p_station->track_data == NULL)
        return(0);                      // first point couldn't be an echo
    checktime = p_station->sec_heard - TRAIL_ECHO_TIME*60;
    for (i = (p_station->track_data->trail_inp -1 +MAX_TRACKS)%MAX_TRACKS,j=0;
            j < TRAIL_ECHO_COUNT; i = (--i+MAX_TRACKS)%MAX_TRACKS ,j++) {
        if (p_station->track_data->sec[i] < checktime)
            return(0);              // outside time frame
        if ((p_station->coord_lon == p_station->track_data->trail_long_pos[i])
                && (p_station->coord_lat == p_station->track_data->trail_lat_pos[i])
                && (p_station->speed == '\0' || p_station->track_data->speed[i] < 0
                        || (long)(atof(p_station->speed)*18.52) == p_station->track_data->speed[i])
                        // current: char knots, trail: long 0.1m (-1 is undef)
                && (p_station->course == '\0' || p_station->track_data->course[i] <= 0
                        || atoi(p_station->course) == p_station->track_data->course[i])
                        // current: char, trail: int (-1 is undef)
                && (p_station->altitude == '\0' || p_station->track_data->altitude[i] <= -99999
                        || atoi(p_station->altitude)*10 == p_station->track_data->altitude[i])) {
                        // current: char, trail: int (-99999 is undef)
            if (debug_level & 1) {
                if (j > 0) {
                    printf("delayed echo for %s",p_station->call_sign);
                    convert_lat_l2s(p_station->coord_lat, temp, sizeof(temp), CONVERT_HP_NORMAL);
                    printf(" at %s",temp);
                    convert_lon_l2s(p_station->coord_lon, temp, sizeof(temp), CONVERT_HP_NORMAL);
                    printf(" %s, already heard %d packets ago\n",temp,j+1);
                }
            }
            return(1);              // we found a delayed echo
        }
        if (i == p_station->track_data->trail_out) {
            return(0);                  // that was the last available point!
        }
    }
    return(0);                          // no echo found
}





/*
 *  Delete trail and free memory
 */
int delete_trail(DataRow *fill) {
    if (fill->track_data != NULL) {
        free(fill->track_data);
        fill->track_data = NULL;
        tracked_stations--;
        return(1);
    }
        return(0);
}


/*
 *  Draw trail on screen
 */
void draw_trail(Widget w, DataRow *fill, int solid) {
    int i;
    char short_dashed[2]  = {(char)1,(char)5};
    char medium_dashed[2] = {(char)5,(char)5};
    long lat0, lon0, lat1, lon1;        // trail segment points
    long marg_lat, marg_lon;
    int col_trail, col_dot;
    XColor rgb;
/*    Colormap cmap;  KD6ZWR - Now set in main() */
    long brightness;
    char flag1;

    if (fill->track_data != NULL) {
        // trail should have at least two points:
        if ( (fill->track_data->trail_inp - fill->track_data->trail_out +
                MAX_TRACKS) % MAX_TRACKS != 1) {
            if (debug_level & 256) {
                printf("draw_trail called for %s with %s.\n",
                    fill->call_sign, (solid? "Solid" : "Non-Solid"));
            }
            col_trail = trail_colors[fill->track_data->trail_color];

            // define color of position dots in trail
            rgb.pixel = col_trail;
/*            cmap = DefaultColormap(XtDisplay(w), DefaultScreen(XtDisplay(w)));  KD6ZWR - Now set in main() */
            XQueryColor(XtDisplay(w),cmap,&rgb);
            brightness = (long)(0.3*rgb.red + 0.55*rgb.green + 0.15*rgb.blue);
            if (brightness > 32000) {
                col_dot = trail_colors[0x05];   // black dot on light trails
            } else {
                col_dot = trail_colors[0x06];   // white dot on dark trail
            }

            if (solid)
                // Used to be "JoinMiter" and "CapButt" below
                (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineSolid, CapRound, JoinRound);
            else {
                // Another choice is LineDoubleDash
                (void)XSetLineAttributes(XtDisplay(w), gc, 3, LineOnOffDash, CapRound, JoinRound);
                (void)XSetDashes(XtDisplay(w), gc, 0, short_dashed , 2);
            }

            marg_lat = screen_height * scale_y;                 // setup area for acceptable trail points
            if (marg_lat < TRAIL_POINT_MARGIN*60*100)           // on-screen plus 50% margin
                marg_lat = TRAIL_POINT_MARGIN*60*100;           // but at least a minimum margin
            marg_lon = screen_width  * scale_x;
            if (marg_lon < TRAIL_POINT_MARGIN*60*100)
                marg_lon = TRAIL_POINT_MARGIN*60*100;

            for (i = fill->track_data->trail_out;
                       (fill->track_data->trail_inp -1 - i +MAX_TRACKS)%MAX_TRACKS > 0;
                       i = ++i%MAX_TRACKS) {                                    // loop over trail points
                lon0 = fill->track_data->trail_long_pos[(i)];                   // Trail segment start
                lat0 = fill->track_data->trail_lat_pos[(i)];
                lon1 = fill->track_data->trail_long_pos[(i+1)%MAX_TRACKS];      // Trail segment end
                lat1 = fill->track_data->trail_lat_pos[(i+1)%MAX_TRACKS];
                flag1 = fill->track_data->flag[(i+1)%MAX_TRACKS];

                if ( (abs(lon0-mid_x_long_offset) < marg_lon) &&                // trail points have to
                     (abs(lon1-mid_x_long_offset) < marg_lon) &&                // be in margin area
                     (abs(lat0-mid_y_lat_offset)  < marg_lat) &&
                     (abs(lat1-mid_y_lat_offset)  < marg_lat) ) {

                    if ((flag1 & TR_NEWTRK) == '\0') {
                        // WE7U: possible drawing problems, if numbers too big ????
                        lon0 = (lon0-x_long_offset)/scale_x;                       // check arguments for
                        lat0 = (lat0-y_lat_offset)/scale_y;                        // XDrawLine()
                        lon1 = (lon1-x_long_offset)/scale_x;
                        lat1 = (lat1-y_lat_offset)/scale_y;

                        if (abs(lon0) < 32700 && abs(lon1) < 32700 &&
                            abs(lat0) < 32700 && abs(lat1) < 32700) {
                            // draw trail segment
                            (void)XSetForeground(XtDisplay(w),gc,col_trail);
                            (void)XDrawLine(XtDisplay(w),pixmap_final,gc,lon0,lat0,lon1,lat1);
                            // draw position point itself
                            (void)XSetForeground(XtDisplay(w),gc,col_dot);
                            (void)XDrawPoint(XtDisplay(w),pixmap_final,gc,lon0,lat0);
                        }
/*                      (void)XDrawLine(XtDisplay(w), pixmap_final, gc,         // draw trail segment
                            (lon0-x_long_offset)/scale_x,(lat0-y_lat_offset)/scale_y,
                            (lon1-x_long_offset)/scale_x,(lat1-y_lat_offset)/scale_y);
*/
                    }
                }
            }
            (void)XSetDashes(XtDisplay(w), gc, 0, medium_dashed , 2);
        }
        else if (debug_level & 256) {
            printf("Trail for %s does not contain 2 or more points.\n",
                fill->call_sign);
        }
    }
}





// DK7IN: there should be some library functions for the next two,
//        but I don't have any documentation while being in holidays...
void month2str(int month, char *str) {

    switch (month) {
        case 0:         strcpy(str,"Jan"); break;
        case 1:         strcpy(str,"Feb"); break;
        case 2:         strcpy(str,"Mar"); break;
        case 3:         strcpy(str,"Apr"); break;
        case 4:         strcpy(str,"May"); break;
        case 5:         strcpy(str,"Jun"); break;
        case 6:         strcpy(str,"Jul"); break;
        case 7:         strcpy(str,"Aug"); break;
        case 8:         strcpy(str,"Sep"); break;
        case 9:         strcpy(str,"Oct"); break;
        case 10:        strcpy(str,"Nov"); break;
        case 11:        strcpy(str,"Dec"); break;
        default:        strcpy(str,"   "); break;
    }
}

void wday2str(int wday, char *str) {

    switch (wday) {
        case 0:         strcpy(str,"Sun"); break;
        case 1:         strcpy(str,"Mon"); break;
        case 2:         strcpy(str,"Tue"); break;
        case 3:         strcpy(str,"Wed"); break;
        case 4:         strcpy(str,"Thu"); break;
        case 5:         strcpy(str,"Fri"); break;
        case 6:         strcpy(str,"Sat"); break;
        default:        strcpy(str,"   "); break;
    }
}



/*
 *  Export trail point to file
 */
void exp_trailpos(FILE *f,long lat,long lon,time_t sec,long speed,int course,long alt,int newtrk) {
    struct tm *time;
    char temp[12+1];
    char month[3+1];
    char wday[3+1];

    if (newtrk)
        fprintf(f,"\nN  New Track Start\n");
    // DK7IN: The format may change in the near future !
    //        Are there any standards? I want to be able to be compatible to
    //        GPS data formats (e.g. G7TO) for easy interchange from/to GPS
    //        How should we present undefined data? (speed/course/altitude)
    convert_lat_l2s(lat, temp, sizeof(temp), CONVERT_UP_TRK);
    fprintf(f,"T  %s",temp);
    convert_lon_l2s(lon, temp, sizeof(temp), CONVERT_UP_TRK);
    fprintf(f," %s",temp);
    time  = gmtime(&sec);
    month2str(time->tm_mon,month);
    wday2str(time->tm_wday,wday);
    fprintf(f," %s %s %02d %02d:%02d:%02d %04d",wday,month,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec,time->tm_year+1900);
    if (alt > -99999)
        fprintf(f,"  %5.0fm",(float)(alt/10.0));
    else        // undefined
        fprintf(f,"        ");
    if (speed >= 0)
        fprintf(f," %4.0fkm/h",(float)(speed/10.0));
    else        // undefined
        fprintf(f,"          ");
    if (course >= 0)                    // DK7IN: is 0 undefined ?? 1..360 ?
        fprintf(f," %3d�\n",course);
    else        // undefined
        fprintf(f,"     \n");
}


/*
 *  Export trail for one station to file
 */
void exp_trailstation(FILE *f, DataRow *p_station) {
    int i;
    long lat0, lon0, lat1, lon1;
    int newtrk;
    time_t sec, sec1;
    long speed;         // 0.1km/h
    int  course;        // degrees
    long alt;           // 0.1m

    if (p_station->origin == NULL || p_station->origin[0] == '\0')
        fprintf(f,"\n#C %s\n",p_station->call_sign);
    else
        fprintf(f,"\n#O %s %s\n",p_station->call_sign,p_station->origin);
    newtrk = 1;

    if (p_station->track_data != NULL) {
        // trail should have at least two points
        if ((p_station->track_data->trail_inp - p_station->track_data->trail_out+MAX_TRACKS)%MAX_TRACKS != 1) {
            for (i = p_station->track_data->trail_out;
                   (p_station->track_data->trail_inp -1 - i +MAX_TRACKS)%MAX_TRACKS > 0;
                   i = ++i%MAX_TRACKS) {                                    // loop over trail points
                lon0   = p_station->track_data->trail_long_pos[(i)];                   // Trail segment start
                lat0   = p_station->track_data->trail_lat_pos[(i)];
                lon1   = p_station->track_data->trail_long_pos[(i+1)%MAX_TRACKS];      // Trail segment end
                lat1   = p_station->track_data->trail_lat_pos[(i+1)%MAX_TRACKS];
                sec    = p_station->track_data->sec[(i)];
                sec1   = p_station->track_data->sec[(i+1)%MAX_TRACKS];
                speed  = p_station->track_data->speed[(i)];
                course = p_station->track_data->course[(i)];
                alt    = p_station->track_data->altitude[(i)];
                if ((p_station->track_data->flag[(i)] & TR_LOCAL) != '\0')
                    newtrk = 1;
                exp_trailpos(f,lat0,lon0,sec,speed,course,alt,newtrk);
                newtrk = 0;
            }
            // last trail point
            lon0   = p_station->track_data->trail_long_pos[(i)];                   // Trail segment start
            lat0   = p_station->track_data->trail_lat_pos[(i)];
            sec    = p_station->track_data->sec[(i)];
            speed  = p_station->track_data->speed[(i)];
            course = p_station->track_data->course[(i)];
            alt    = p_station->track_data->altitude[(i)];
            if ((p_station->flag & ST_LOCAL) != 0)
                newtrk = 1;
            exp_trailpos(f,lat0,lon0,sec,speed,course,alt,newtrk);
        }    
    } else {        // single position
        if (p_station->altitude[0] != '\0')
            alt = atoi(p_station->altitude)*10;
        else            
            alt = -99999l;
        if (p_station->speed[0] != '\0')
            speed = (long)(atof(p_station->speed)*18.52);
        else
            speed = -1;
        if (p_station->course[0] != '\0')
            course = atoi(p_station->course);
        else
            course = -1;
        exp_trailpos(f,p_station->coord_lat,p_station->coord_lon,p_station->sec_heard,speed,course,alt,newtrk);
    }
    fprintf(f,"\n");
}





/*
 *  Export trail data for one or all stations to file
 */
void export_trail(DataRow *p_station) {
    char file[420];
    FILE *f;
    time_t sec;
    struct tm *time;
    int storeall;

    sec = sec_now();
    time  = gmtime(&sec);
    if (p_station == NULL)
        storeall = 1;
    else
        storeall = 0;
    if (storeall) {
        // define filename for storing all station
        //sprintf(file,"%s/%04d%02d%02d-%02d%02d%02d.trk",get_user_base_dir("tracklogs"),time->tm_year+1900,time->tm_mon+1,time->tm_mday,time->tm_hour,time->tm_min,time->tm_sec);
        xastir_snprintf(file, sizeof(file),
            "%s/%04d%02d%02d-%02d%02d%02d.trk",
            get_user_base_dir("tracklogs"),
            time->tm_year+1900,
            time->tm_mon+1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec);
    } else {
        // define filename for current station
        //sprintf(file,"%s/%s.trk", get_user_base_dir("tracklogs"), p_station->call_sign);
        xastir_snprintf(file, sizeof(file), "%s/%s.trk", get_user_base_dir("tracklogs"), p_station->call_sign);
    }

    // create or open file
    (void)filecreate(file);     // create empty file if it doesn't exist
    // DK7IN: owner should better be set to user, it is now root with kernel AX.25!
    f=fopen(file,"a");          // open file for append
    if (f != NULL) {
        fprintf(f,"# WGS-84 tracklog created by Xastir %04d/%02d/%02d %02d:%02d\n",time->tm_year+1900,time->tm_mon+1,time->tm_mday,time->tm_hour,time->tm_min);
        if (storeall) {
            p_station = n_first;
            while (p_station != NULL) {
                exp_trailstation(f,p_station);
                p_station = p_station->n_next;
            }
        } else {
            exp_trailstation(f,p_station);
        }
        (void)fclose(f);
    } else
        printf("Couldn't create or open tracklog file %s\n",file);
}




//////////////////////////////////////  Station storage  ///////////////////////////////////////////

// Station storage is done in a double-linked list. In fact there are two such
// pointer structures, one for sorting by name and one for sorting by time.
// We store both the pointers to the next and to the previous elements.  DK7IN

/*
 *  Setup station storage structure
 */
void init_station_data(void) {

    stations = 0;                       // empty station list
    n_first = NULL;                     // pointer to next element in name sorted list
    n_last  = NULL;                     // pointer to previous element in name sorted list
    t_first = NULL;                     // pointer to oldest element in time sorted list
    t_last  = NULL;                     // pointer to newest element in time sorted list
    last_sec = sec_now();               // check value for detecting changed seconds in time
    next_time_sn = 0;                   // serial number for unique time index
    current_trail_color = 0x00;         // first trail color used will be 0x01
    last_station_remove = sec_now();    // last time we checked for stations to remove
    track_station_on = 0;               // no tracking at startup
}





/*
 *  Initialize station data
 */        
void init_station(DataRow *p_station) {
    // the list pointers should already be set
    p_station->track_data       = NULL;         // no trail
    p_station->weather_data     = NULL;         // no weather
    p_station->coord_lat        = 0l;           //  90�N  \ undefined
    p_station->coord_lon        = 0l;           // 180�W  / position
    p_station->pos_amb          = 0;            // No ambiguity
    p_station->call_sign[0]     = '\0';         // ?????
    p_station->sec_heard        = 0;
    p_station->time_sn          = 0;
    p_station->flag             = 0;            // set all flags to inactive
    p_station->record_type      = '\0';
    p_station->data_via         = '\0';         // L local, T TNC, I internet, F file
    p_station->heard_via_tnc_port = 0;
    p_station->heard_via_tnc_last_time = 0;
    p_station->last_heard_via_tnc = 0;
    p_station->last_port_heard   = 0;
    p_station->num_packets       = 0;
    p_station->aprs_symbol.aprs_type = '\0';
    p_station->aprs_symbol.aprs_symbol = '\0';
    p_station->aprs_symbol.special_overlay = '\0';
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
    p_station->station_time_type = '\0';
    p_station->origin[0]         = '\0';        // no object
    p_station->packet_time[0]    = '\0';
    p_station->node_path[0]      = '\0';
    p_station->pos_time[0]       = '\0';
    p_station->altitude_time[0]  = '\0';
    p_station->altitude[0]       = '\0';
    p_station->speed_time[0]     = '\0';
    p_station->speed[0]          = '\0';
    p_station->course[0]         = '\0';
    p_station->bearing[0]        = '\0';
    p_station->NRQ[0]            = '\0';
    p_station->power_gain[0]     = '\0';
    p_station->signal_gain[0]    = '\0';
    p_station->signpost[0]       = '\0';
    p_station->station_time[0]   = '\0';
    p_station->sats_visible[0]   = '\0';
    p_station->comments[0]       = '\0';
    p_station->df_color          = -1;
}





/*
 *  Remove element from name ordered list
 */
void remove_name(DataRow *p_rem) {      // todo: return pointer to next element
    if (p_rem->n_prev == NULL)          // first element
        n_first = p_rem->n_next;
    else
        p_rem->n_prev->n_next = p_rem->n_next;

    if (p_rem->n_next == NULL)          // last element
        n_last = p_rem->n_prev;
    else
        p_rem->n_next->n_prev = p_rem->n_prev;
}





/*
 *  Remove element from time ordered list
 */
void remove_time(DataRow *p_rem) {      // todo: return pointer to next element
    if (p_rem->t_prev == NULL)          // first element
        t_first = p_rem->t_next;
    else
        p_rem->t_prev->t_next = p_rem->t_next;

    if (p_rem->t_next == NULL)          // last element
        t_last = p_rem->t_prev;
    else
        p_rem->t_next->t_prev = p_rem->t_prev;
}





/*
 *  Insert existing element into name ordered list before p_name
 */
void insert_name(DataRow *p_new, DataRow *p_name) {
    p_new->n_next = p_name;
    if (p_name == NULL) {               // add to end of list
        p_new->n_prev = n_last;
        if (n_last == NULL)             // add to empty list
            n_first = p_new;
        else
            n_last->n_next = p_new;
        n_last = p_new;
    } else {
        p_new->n_prev = p_name->n_prev;
        if (p_name->n_prev == NULL)     // add to begin of list
            n_first = p_new;
        else
            p_name->n_prev->n_next = p_new;
        p_name->n_prev = p_new;
    }
}





/*
 *  Insert existing element into time ordered list before p_time
 *  The p_new record ends up being on the "older" side of p_time when
 *  all done inserting (closer in the list to the t_first pointer).
 */
void insert_time(DataRow *p_new, DataRow *p_time) {
    p_new->t_next = p_time;
    if (p_time == NULL) {               // add to end of list (becomes newest station)
        p_new->t_prev = t_last;         // connect to previous end of list
        if (t_last == NULL)             // if list empty, create list
            t_first = p_new;            // it's now our only station on the list
        else
            t_last->t_next = p_new;     // list not empty, link original last record to our new one
        t_last = p_new;                 // end of list (newest record pointer) points to our new record
    } else {                            // Else we're inserting into the middle of the list somewhere
        p_new->t_prev = p_time->t_prev;
        if (p_time->t_prev == NULL)     // add to begin of list (new record becomes oldest station)
            t_first = p_new;
        else
            p_time->t_prev->t_next = p_new; // else 
        p_time->t_prev = p_new;
    }
}





/*
 *  Free station memory for one entry
 */
void delete_station_memory(DataRow *p_del) {
    remove_name(p_del);
    remove_time(p_del);
    free(p_del);
    stations--;
}





/*
 *  Create new uninitialized element in station list
 *  and insert it before p_name and p_time entries
 */
/*@null@*/ DataRow *insert_new_station(DataRow *p_name, DataRow *p_time) {
    DataRow *p_new;

    p_new = (DataRow *)malloc(sizeof(DataRow));
    if (p_new != NULL) {                // we really got the memory
        p_new->call_sign[0] = '\0';     // just to be sure
        insert_name(p_new,p_name);      // insert element into name ordered list
        insert_time(p_new,p_time);      // insert element into time ordered list
    }
    if (p_new == NULL)
        printf("ERROR: we got no memory for station storage\n");
    return(p_new);                      // return pointer to new element
}





/*
 *  Create new initialized element for call in station list
 *  and insert it before p_name and p_time entries
 */
/*@null@*/ DataRow *add_new_station(DataRow *p_name, DataRow *p_time, char *call) {
    DataRow *p_new;
    char station_num[30];

    p_new = insert_new_station(p_name,p_time);  // allocate memory
    if (p_new != NULL) {
        init_station(p_new);                    // initialize new station record
        strcpy(p_new->call_sign,call);
        stations++;

        // this should not be here...  ??
        if (!wait_to_redraw) {          // show number of stations in status line
            //sprintf(station_num, langcode("BBARSTH001"), stations);
            xastir_snprintf(station_num, sizeof(station_num), langcode("BBARSTH001"), stations);
            XmTextFieldSetString(text3, station_num);
        }
    }
    return(p_new);                      // return pointer to new element
}





/*
 *  Move station record before p_time in time ordered list
 */
void move_station_time(DataRow *p_curr, DataRow *p_time) {

    if (p_curr != NULL) {               // need a valid record
        remove_time(p_curr);
        insert_time(p_curr,p_time);
    }
}





/*
 *  Move station record before p_name in name ordered list
 */
void move_station_name(DataRow *p_curr, DataRow *p_name) {

    if (p_curr != NULL) {               // need a valid record
        remove_name(p_curr);
        insert_name(p_curr,p_name);
    }
}





/*
 *  Search station record by callsign
 *  Returns a station with a call equal or after the searched one
 */
int search_station_name(DataRow **p_name, char *call, int exact) {
    // DK7IN: we do a linear search here.
    // Maybe I setup a tree storage too, to see what is better,
    // tree should be faster in search, list faster at display time.
    // I don't look at case, objects and internet names could have lower case
    int i,j;
    int ok = 1;
    char ch0,ch1;

    (*p_name) = n_first;                                // start of alphabet
    ch0 = call[0];
    if (ch0 == '\0')
        ok = 0;
    else {
        while(1) {                                      // check first char
            if ((*p_name) == NULL)
                break;                                  // reached end of list
            if ((*p_name)->call_sign[0] >= ch0)         // we also catch the '\0'
                break;                                  // finished search
            (*p_name) = (*p_name)->n_next;              // next element
        }
        // we now probably have found the first char
        if ((*p_name) == NULL || (*p_name)->call_sign[0] != ch0)
            ok = 0;                                     // nothing found!
    }

    for (i=1;ok && i<(int)strlen(call);i++) {           // check rest of string
        ch1 = call[i];
        ch0 = call[i-1];
        while (ok) {
            if ((*p_name)->call_sign[i] >= ch1)         // we also catch the '\0'
                break;                                  // finished search
            (*p_name) = (*p_name)->n_next;              // next element
            if ((*p_name) == NULL)
                break;                                  // reached end of list
            for (j=0;ok && j<i;j++) {                   // unchanged first part?
                if ((*p_name)->call_sign[j] != call[j])
                    ok = 0;
            }
        }
        // we now probably have found the next char
        if ((*p_name) == NULL || (*p_name)->call_sign[i] != ch1)
            ok = 0;                                     // nothing found!
    }  // check next char in call

    if (exact && ok && strlen((*p_name)->call_sign) != strlen(call))
        ok = 0;
    return(ok);         // if not ok: p_name points to correct insert position in name list
}





/*    
 *  Search station record by time and time serial number, serial ignored if -1
 *  Returns a station that is equal or older than the search criterium
 */
int search_station_time(DataRow **p_time, time_t heard, int serial) {
    int ok = 1;

    (*p_time) = t_last;                                 // newest station
    if (heard == 0) {                                   // we want the newest station
        if (t_last == 0)
            ok = 0;                                     // empty list
    }
    else {
        while((*p_time) != NULL) {                      // check time
            if ((*p_time)->sec_heard <= heard)          // compare
                break;                                  // found time or earlier
            (*p_time) = (*p_time)->t_prev;              // next element
        }
        // we now probably have found the entry
        if ((*p_time) != NULL && (*p_time)->sec_heard == heard) {
            // we got a match, but there may be more of them
            if (serial >= 0) {                          // check serial number, ignored if -1
                while((*p_time) != NULL) {              // for unique time index
                    if ((*p_time)->sec_heard == heard && (*p_time)->time_sn <= serial)  // compare
                        break;                          // found it (same time, maybe earlier SN)
                    if ((*p_time)->sec_heard < heard)   // compare
                        break;                          // found it (earlier time)
                    (*p_time) = (*p_time)->t_prev;      // consider next element
                }
                if ((*p_time) == NULL || (*p_time)->sec_heard != heard || (*p_time)->time_sn != serial)
                    ok = 0;                             // no perfect match
            }
        } else
            ok = 0;                                     // no perfect match
    }
    return(ok);
}





/*
 *  Get pointer to next station in name sorted list
 */
int next_station_name(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = n_first;
    else
        (*p_curr) = (*p_curr)->n_next;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Get pointer to previous station in name sorted list
 */
int prev_station_name(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = n_last;
    else
        (*p_curr) = (*p_curr)->n_prev;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}



    

/*
 *  Get pointer to newer station in time sorted list
 */
int next_station_time(DataRow **p_curr) {

    if ((*p_curr) == NULL)
        (*p_curr) = t_first;    // Grab oldest station if NULL passed to us???
    else
        (*p_curr) = (*p_curr)->t_next;  // Else grab newer station
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Get pointer to older station in time sorted list
 */
int prev_station_time(DataRow **p_curr) {
    
    if ((*p_curr) == NULL)
        (*p_curr) = t_last; // Grab newest station if NULL passed to us???
    else
        (*p_curr) = (*p_curr)->t_prev;
    if ((*p_curr) != NULL)
        return(1);
    else
        return(0);
}





/*
 *  Set flag for all stations in current view area or a margin area around it
 *  That are the stations we look at, if we want to draw symbols or trails
 */
void setup_in_view(void) {
    DataRow *p_station;
    long min_lat, max_lat;                      // screen borders plus space
    long min_lon, max_lon;                      // for trails from off-screen stations
    long marg_lat, marg_lon;                    // margin around screen

    marg_lat = (long)(3 * screen_height * scale_y/2);
    marg_lon = (long)(3 * screen_width  * scale_x/2);
    if (marg_lat < IN_VIEW_MIN*60*100)          // allow a minimum area, 
        marg_lat = IN_VIEW_MIN*60*100;          // there could be outside stations
    if (marg_lon < IN_VIEW_MIN*60*100)          // with trail parts on screen
        marg_lon = IN_VIEW_MIN*60*100;

    // Only screen view
    // min_lat = y_lat_offset  + (long)(screen_height * scale_y);
    // max_lat = y_lat_offset;
    // min_lon = x_long_offset;
    // max_lon = x_long_offset + (long)(screen_width  * scale_x);

    // Screen view plus one screen wide margin
    // There could be stations off screen with on screen trails
    // See also the use of position_on_extd_screen()
    min_lat = mid_y_lat_offset  - marg_lat;
    max_lat = mid_y_lat_offset  + marg_lat;
    min_lon = mid_x_long_offset - marg_lon;
    max_lon = mid_x_long_offset + marg_lon;

    p_station = n_first;
    while (p_station != NULL) {
        if ((p_station->flag & ST_ACTIVE) == 0        // ignore deleted objects
                  || p_station->coord_lon < min_lon || p_station->coord_lon > max_lon
                  || p_station->coord_lat < min_lat || p_station->coord_lat > max_lat
                  || (p_station->coord_lat == 0 && p_station->coord_lon == 0)) {
            // outside view and undefined stations:
            p_station->flag &= (~ST_INVIEW);        // clear "In View" flag
        } else
            p_station->flag |= ST_INVIEW;           // set "In View" flag
        p_station = p_station->n_next;
    }
}





/*
 *  Check if position is inside screen borders
 */
int position_on_screen(long lat, long lon) {

    if (   lon > x_long_offset && lon < (x_long_offset+(long)(screen_width *scale_x))
        && lat > y_lat_offset  && lat < (y_lat_offset +(long)(screen_height*scale_y)) 
        && !(lat == 0 && lon == 0))     // discard undef positions from screen
        return(1);                      // position is inside the screen
    else
        return(0);
}





/*
 *  Check if position is inside extended screen borders
 *  (real screen + one screen margin for trails)
 *  used for station "In View" flag
 */
int position_on_extd_screen(long lat, long lon) {
    long marg_lat, marg_lon;                    // margin around screen

    marg_lat = (long)(3 * screen_height * scale_y/2);
    marg_lon = (long)(3 * screen_width  * scale_x/2);
    if (marg_lat < IN_VIEW_MIN*60*100)          // allow a minimum area, 
        marg_lat = IN_VIEW_MIN*60*100;          // there could be outside stations
    if (marg_lon < IN_VIEW_MIN*60*100)          // with trail parts on screen
        marg_lon = IN_VIEW_MIN*60*100;

    if (    abs(lon - mid_x_long_offset) < marg_lon
         && abs(lat - mid_y_lat_offset)  < marg_lat
         && !(lat == 0 && lon == 0))    // discard undef positions from screen
        return(1);                      // position is inside the area
    else
        return(0);
}





/*
 *  Check if position is inside inner screen area
 *  (real screen + minus 1/6 screen margin)
 *  used for station tracking
 */
int position_on_inner_screen(long lat, long lon) {

    if (    lon > mid_x_long_offset-(long)(screen_width *scale_x/3)
         && lon < mid_x_long_offset+(long)(screen_width *scale_x/3)
         && lat > mid_y_lat_offset -(long)(screen_height*scale_y/3)
         && lat < mid_y_lat_offset +(long)(screen_height*scale_y/3)
         && !(lat == 0 && lon == 0))    // discard undef positions from screen
        return(1);                      // position is inside the area
    else
        return(0);
}





/*
 *  Delete single station with all its data    ?? delete messages ??
 *  This function is called with a callsign parameter.  Only used for
 *  my callsign, not for any other.
 */
void station_del(char *call) {
    DataRow *p_name;                    // DK7IN: do it with move... ?

    if (search_station_name(&p_name, call, 1)) {
        (void)delete_trail(p_name);     // Free track storage if it exists.
        (void)delete_weather(p_name);   // free weather memory, if allocated
        delete_station_memory(p_name);  // free memory
    }
}





/*
 *  Delete single station with all its data    ?? delete messages ??
 *  This function is called with a pointer instead of a callsign.
 */
void station_del_ptr(DataRow *p_name) {

    if (p_name != NULL) {

//WE7U7
        //printf("Removing: %s heard %d seconds ago\n",p_name->call_sign, (int)(sec_now() - p_name->sec_heard));

        (void)delete_trail(p_name);     // Free track storage if it exists.
        (void)delete_weather(p_name);   // free weather memory, if allocated
        delete_station_memory(p_name);  // free memory
    }
}





/*
 *  Delete all stations             ?? delete messages ??
 */
void delete_all_stations(void) {
    DataRow *p_name;
    DataRow *p_curr;
    
    p_name = n_first;
    while (p_name != NULL) {
        p_curr = p_name;
        p_name = p_name->n_next;
        station_del_ptr(p_curr);
        //(void)delete_trail(p_curr);     // free trail memory, if allocated
        //(void)delete_weather(p_curr);   // free weather memory, if allocated
        //delete_station_memory(p_curr);  // free station memory
    }
    if (stations != 0) {
        printf("ERROR: stations should be 0 after stations delete, is %ld\n",stations);
        stations = 0;
    }    
}





/*
 *  Check if we have to delete old stations
 */
void check_station_remove(void) {
    DataRow *p_station;
    time_t t_rem;
    int done = 0;

    if (last_station_remove < (sec_now() - STATION_REMOVE_CYCLE)) {
        t_rem = sec_now() - sec_remove;

//WE7U7
        //t_rem = sec_now() - (5 * 60);     // Useful for debug only

        p_station = t_first;    // Oldest station in our list
        while (p_station != NULL && !done) {
            if (p_station->sec_heard < t_rem) {
                if (!is_my_call(p_station->call_sign,1)) {
                    mdelete_messages(p_station->call_sign);     // delete messages
                    station_del_ptr(p_station);
                    //(void)delete_trail(p_station);              // Free track storage if it exists.
                    //(void)delete_weather(p_station);            // free weather memory, if allocated
                    //delete_station_memory(p_station);           // free memory
                }
            } else
                done++;                                         // all other stations are newer...
            p_station = p_station->t_next;
        }
        last_station_remove = sec_now();
    }
}





/*
 *  Delete an object (mark it as deleted)
 */
void delete_object(char *name) {
    DataRow *p_station;

    p_station = NULL;
    if (search_station_name(&p_station,name,1)) {       // find object name
        p_station->flag &= (~ST_ACTIVE);                // clear flag
        p_station->flag &= (~ST_INVIEW);                // clear "In View" flag
        if (position_on_screen(p_station->coord_lat,p_station->coord_lon))
            redraw_on_new_data = 2;                     // redraw now
            // there is some problem...  it is not redrawn immediately! ????
            // but deleted from list immediatetly
        redo_list = (int)TRUE;                          // and update lists
    }
}





///////////////////////////////////////  APRS Decoding  ////////////////////////////////////////////


/*
 *  Extract Uncompressed Position Report from begin of line
 */
int extract_position(DataRow *p_station, char **info, int type) {
    int ok;
    char temp_lat[8+1];
    char temp_lon[9+1];
    char temp_grid[8+1];
    char *my_data;
    float gridlat;
    float gridlon;
    my_data = (*info);

    if (type != APRS_GRID){
        ok = (int)(strlen(my_data) >= 19);
        ok = (int)(ok && my_data[4]=='.' && my_data[14]=='.'
                    && (my_data[7] =='N' || my_data[7] =='S')
                    && (my_data[17]=='W' || my_data[17]=='E'));
        // errors found:  [4]: X   [7]: n s   [17]: w e
        if (ok) {
            ok =             is_num_chr(my_data[0]);           // 5230.31N/01316.88E>
            ok = (int)(ok && is_num_chr(my_data[1]));          // 0123456789012345678
            ok = (int)(ok && is_num_or_sp(my_data[2]));
            ok = (int)(ok && is_num_or_sp(my_data[3]));
            ok = (int)(ok && is_num_or_sp(my_data[5]));
            ok = (int)(ok && is_num_or_sp(my_data[6]));
            ok = (int)(ok && is_num_chr(my_data[9]));
            ok = (int)(ok && is_num_chr(my_data[10]));
            ok = (int)(ok && is_num_chr(my_data[11]));
            ok = (int)(ok && is_num_or_sp(my_data[12]));
            ok = (int)(ok && is_num_or_sp(my_data[13]));
            ok = (int)(ok && is_num_or_sp(my_data[15]));
            ok = (int)(ok && is_num_or_sp(my_data[16]));
        }
                                            
        if (ok) {
            overlay_symbol(my_data[18], my_data[8], p_station);
            p_station->pos_amb = 0;
            // spaces in latitude set position ambiguity, spaces in longitude do not matter
            // we will adjust the lat/long to the center of the rectangle of ambiguity
            if (my_data[2] == ' ') {      // nearest degree
                p_station->pos_amb = 4;
                my_data[2]  = my_data[12] = '3';
                my_data[3]  = my_data[5]  = my_data[6]  = '0';
                my_data[13] = my_data[15] = my_data[16] = '0';
            }
            else if (my_data[3] == ' ') { // nearest 10 minutes
                p_station->pos_amb = 3;
                my_data[3]  = my_data[13] = '5';
                my_data[5]  = my_data[6]  = '0';
                my_data[15] = my_data[16] = '0';
            }
            else if (my_data[5] == ' ') { // nearest minute
                p_station->pos_amb = 2;
                my_data[5]  = my_data[15] = '5';
                my_data[6]  = '0';
                my_data[16] = '0';
            }
            else if (my_data[6] == ' ') { // nearest 1/10th minute
                p_station->pos_amb = 1;
                my_data[6]  = my_data[16] = '5';
            }

            strncpy(temp_lat,my_data,7);
            temp_lat[7] = my_data[7];
            temp_lat[8] = '\0';

            strncpy(temp_lon,my_data+9,8);
            temp_lon[8] = my_data[17];
            temp_lon[9] = '\0';

            if (!is_my_call(p_station->call_sign,1)) {      // don't change my position, I know it better...
                p_station->coord_lat = convert_lat_s2l(temp_lat);   // ...in case of position ambiguity
                p_station->coord_lon = convert_lon_s2l(temp_lon);
            }

            (*info) += 19;                  // delete position from comment
        }
    } else { // is it a grid 
        // first sanity checks, need more
        ok = (int)(is_num_chr(my_data[2]));
        ok = (int)(ok && is_num_chr(my_data[3]));
        ok = (int)(ok && ((my_data[0]>='A')&&(my_data[0]<='R')));
        ok = (int)(ok && ((my_data[1]>='A')&&(my_data[1]<='R')));
        if (ok) {
            strncpy(temp_grid,my_data,8);
            // this test treats >6 digit grids as 4 digit grids; >6 are uncommon.
            // the spec mentioned 4 or 6, I'm not sure >6 is even allowed.
            if ( (temp_grid[6] != ']') || (temp_grid[4] == 0) || (temp_grid[5] == 0)){
                p_station->pos_amb = 6; // 1deg lat x 2deg lon 
                temp_grid[4] = 'L';
                temp_grid[5] = 'L';
            } else {
                p_station->pos_amb = 5; // 2.5min lat x 5min lon
                temp_grid[4] = toupper(temp_grid[4]); 
                temp_grid[5] = toupper(temp_grid[5]);
            }
            // These equations came from what I read in the qgrid source code and
            // various mailing list archives.
            gridlon= (20.*((float)temp_grid[0]-65.) + 2.*((float)temp_grid[2]-48.) + 5.*((float)temp_grid[4]-65.)/60.) - 180.;
            gridlat= (10.*((float)temp_grid[1]-65.) + ((float)temp_grid[3]-48.) + 5.*(temp_grid[5]-65.)/120.) - 90.;
            // could check for my callsign here, and avoid changing it...
            p_station->coord_lat = (unsigned long)(32400000l + (360000.0 * (-gridlat)));
            p_station->coord_lon = (unsigned long)(64800000l + (360000.0 * gridlon));
            p_station->aprs_symbol.aprs_type = '/';
            p_station->aprs_symbol.aprs_symbol = 'G';
        }        // is it valid grid or not - "ok"
        // could cut off the grid square from the comment here, but why bother?
    } // is it grid or not
    return(ok);
}




// DK7IN 99
/*
 *  Extract Compressed Position Report Data Formats from begin of line
 *    [APRS Reference, chapter 9]
 */
int extract_comp_position(DataRow *p_station, char **info, /*@unused@*/ int type) {
    int ok;
    char x1, x2, x3, x4, y1, y2, y3, y4;
    int c = 0;
    int s = 0;
    int T = 0;
    char *my_data;
    float lon = 0;
    float lat = 0;
    float range;

    // compressed data format  /YYYYXXXX$csT  is a fixed 13-character field
    // used for ! / @ = data IDs
    //   /     Symbol Table ID
    //   YYYY  compressed latitude
    //   XXXX  compressed longitude
    //   $     Symbol Code
    //   cs    compressed
    //            course/speed
    //            radio range
    //            altitude
    //   T     compression type ID

    my_data = (*info);
    ok = (int)(strlen(my_data) >= 13);
    if (ok) {
        y1 = my_data[1] - '!';  y2 = my_data[2] - '!';  y3 = my_data[3] - '!';  y4 = my_data[4] - '!';
        x1 = my_data[5] - '!';  x2 = my_data[6] - '!';  x3 = my_data[7] - '!';  x4 = my_data[8] - '!';

        c = (int)(my_data[10] - '!');
        s = (int)(my_data[11] - '!');
        T = (int)(my_data[12] - '!');

        if ((int)x1 == -1) x1 = '\0';   if ((int)x2 == -1) x2 = '\0';   // convert ' ' to '0'
        if ((int)x3 == -1) x3 = '\0';   if ((int)x4 == -1) x4 = '\0';   // not specified in
        if ((int)y1 == -1) y1 = '\0';   if ((int)y2 == -1) y2 = '\0';   // APRS Reference!
        if ((int)y3 == -1) y3 = '\0';   if ((int)y4 == -1) y4 = '\0';   // do we need it ???

        ok = (int)(ok && (x1 >= '\0' && x1 < (char)91));                //  /YYYYXXXX$csT
        ok = (int)(ok && (x2 >= '\0' && x2 < (char)91));                //  0123456789012
        ok = (int)(ok && (x3 >= '\0' && x3 < (char)91));
        ok = (int)(ok && (x4 >= '\0' && x4 < (char)91));
        ok = (int)(ok && (y1 >= '\0' && y1 < (char)91));
        ok = (int)(ok && (y2 >= '\0' && y2 < (char)91));
        ok = (int)(ok && (y3 >= '\0' && y3 < (char)91));
        ok = (int)(ok && (y4 >= '\0' && y4 < (char)91));
        T &= 0x3F;      // DK7IN: force Compression Byte to valid format
                        // mask off upper two unused bits, they should be zero!?
        ok = (int)(ok && (c == -1 || ((c >=0 && c < 91) && (s >= 0 && s < 91) && (T >= 0 && T < 64))));

        if (ok) {
            lat = ((((int)y1 * 91 + (int)y2) * 91 + (int)y3) * 91 + (int)y4 ) / 380926.0; // in deg, 0:  90�N
            lon = ((((int)x1 * 91 + (int)x2) * 91 + (int)x3) * 91 + (int)x4 ) / 190463.0; // in deg, 0: 180�W
            lat *= 60 * 60 * 100;                       // in 1/100 sec
            lon *= 60 * 60 * 100;                       // in 1/100 sec
            if ((((long)(lat+4) % 60) > 8) || (((long)(lon+4) % 60) > 8))
                ok = 0;                                 // check max resolution 0.01 min
        }                                               // to catch even more errors
    }
    if (ok) {
        overlay_symbol(my_data[9], my_data[0], p_station);      // Symbol / Table
        if (!is_my_call(p_station->call_sign,1)) {      // don't change my position, I know it better...
            p_station->coord_lat = (long)((lat));               // in 1/100 sec
            p_station->coord_lon = (long)((lon));               // in 1/100 sec
        }

        if (c >= 0) {                                   // ignore csT if c = ' '
            if (c < 90) {
                if ((T & 0x18) == 0x10) {               // check for GGA (with altitude)
                    //sprintf(p_station->altitude,"%06.0f",pow(1.002,(double)(c*91+s))*0.3048);  // in meters
                    xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%06.0f",pow(1.002,(double)(c*91+s))*0.3048);
                } else {                                // compressed course/speed
                    //sprintf(p_station->course,"%03d",c*4);                           // deg
                    xastir_snprintf(p_station->course, sizeof(p_station->course), "%03d",c*4);
                    //sprintf(p_station->speed, "%03.0f",pow(1.08,(double)s)-1);       // knots
                    xastir_snprintf(p_station->speed, sizeof(p_station->speed), "%03.0f",pow(1.08,(double)s)-1);
                }
            } else {
                if (c == 90) {
                    // pre-calculated radio range
                    range = 2 * pow(1.08,(double)s);    // miles

                    // DK7IN: dirty hack...  but better than nothing
                    if (s <= 5)                         // 2.9387 mi
                        //sprintf(p_station->power_gain, "PHG%s0", "000");
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "000");
                    else if (s <= 17)                   // 7.40 mi
                        //sprintf(p_station->power_gain, "PHG%s0", "111");
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "111");
                    else if (s <= 36)                   // 31.936 mi
                        //sprintf(p_station->power_gain, "PHG%s0", "222");
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "222");
                    else if (s <= 75)                   // 642.41 mi
                        //sprintf(p_station->power_gain, "PHG%s0", "333");
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "333");
                    else                       // max 90:  2037.8 mi
                        //sprintf(p_station->power_gain, "PHG%s0", "444");
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "444");
                }
            }
        }
        (*info) += 13;                  // delete position from comment
    }
    return(ok);
}





/*
 *  Extract speed and course from beginning of info field
 */
int extract_speed_course(char *info, char *speed, char *course) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 7) {
        found = 1;
        for(i=0; found && i<7; i++) {           // check data format
            if (i==3) {                         // check separator
                if (info[i]!='/')
                    found = 0;
            } else {
                if( !( isdigit((int)info[i])
                        || (info[i] == ' ')     // Spaces and periods are allowed.  Need these
                        || (info[i] == '.') ) ) // here so that we can get the field deleted
                    found = 0;
            }
        }
    }
    if (found) {
        substr(course,info,3);
        substr(speed,info+4,3);
        for (i=0;i<=len-7;i++)        // delete speed/course from info field
            info[i] = info[i+7];
    }
    if (!found || atoi(course) < 1) {   // course 0 means undefined
        speed[0] ='\0';
        course[0]='\0';
    }
    for(i=0; i<2; i++) {        // recheck data format looking for undefined fields
        if( !(isdigit((int)speed[i]) ) )
            speed[0] = '\0';
        if( !(isdigit((int)course[i]) ) )
            course[0] = '\0';
    }

    return(found);
}





//WE7U
/*
 *  Extract bearing and number/range/quality from beginning of info field
 */
int extract_bearing_NRQ(char *info, char *bearing, char *nrq) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 8) {
        found = 1;
        for(i=1; found && i<8; i++)         // check data format
            if(!(isdigit((int)info[i]) || (i==4 && info[i]=='/')))
                found=0;
    }
    if (found) {
        substr(bearing,info+1,3);
        substr(nrq,info+5,3);

        //printf("Bearing: %s\tNRQ: %s\n", bearing, nrq);

        for (i=0;i<=len-8;i++)        // delete bearing/nrq from info field
            info[i] = info[i+8];
    }
    if (!found || nrq[2] == '0') {   // Q of 0 means useless bearing
        bearing[0] ='\0';
        nrq[0]='\0';
    }
    return(found);
}





/*
 *  Extract altitude from APRS info field          "/A=012345"    in feet
 */
int extract_altitude(char *info, char *altitude) {
    int i,ofs,found,len;

    found=0;
    len = (int)strlen(info);
    for(ofs=0; !found && ofs<len-8; ofs++)      // search for start sequence
        if (strncmp(info+ofs,"/A=",3)==0) {
            found=1;
            // Are negative altitudes even defined?  Yes!  In Mic-E spec to -10,000 meters
            if(!isdigit((int)info[ofs+3]) && info[ofs+3]!='-')  // First char must be digit or '-'
                found=0;
            for(i=4; found && i<9; i++)         // check data format for next 5 chars
                if(!isdigit((int)info[ofs+i]))
                    found=0;
    }
    if (found) {
        ofs--;  // was one too much on exit from for loop
        substr(altitude,info+ofs+3,6);
        for (i=ofs;i<=len-9;i++)        // delete altitude from info field
            info[i] = info[i+9];
    } else
        altitude[0] = '\0';
    return(found);
}





// TODO:
// Comment Field
// Storm Data





/*
 *  Extract powergain from APRS info field          "PHG1234/" "PHG1234"  from APRS data extension
 */
int extract_powergain(char *info, char *phgd) {
    int i,found,len;

    found=0;
    len = (int)strlen(info);
    if (len >= 9 && strncmp(info,"PHG",3)==0 && info[7]=='/' && info[8]!='A'  // trailing '/' not defined in Reference...
                 && isdigit((int)info[3]) && isdigit((int)info[5]) && isdigit((int)info[6])) {
        substr(phgd,info,7);
        for (i=0;i<=len-8;i++)        // delete powergain from data extension field
            info[i] = info[i+8];
        return(1);
    } else {
        if (len >= 7 && strncmp(info,"PHG",3)==0
                 && isdigit((int)info[3]) && isdigit((int)info[5]) && isdigit((int)info[6])) {
            substr(phgd,info,7);
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info[i] = info[i+7];
            return(1);
        } else if (len >= 7 && strncmp(info,"RNG",3)==0
                 && isdigit((int)info[3]) && isdigit((int)info[5]) && isdigit((int)info[6])) {
            substr(phgd,info,7);
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info[i] = info[i+7];
            return(1);
        } else {
            phgd[0] = '\0';
            return(0);
        }
    }
}





//WE7U
/*
 *  Extract omnidf from APRS info field          "DFS1234/"    from APRS data extension
 */
int extract_omnidf(char *info, char *phgd) {
    int i,found,len;

    found=0;
    len = (int)strlen(info);
    if (len >= 8 && strncmp(info,"DFS",3)==0 && info[7]=='/'    // trailing '/' not defined in Reference...
                 && isdigit((int)info[3]) && isdigit((int)info[5]) && isdigit((int)info[6])) {
        substr(phgd,info,7);
        for (i=0;i<=len-8;i++)        // delete omnidf from data extension field
            info[i] = info[i+8];
        return(1);
    } else {
        phgd[0] = '\0';
        return(0);
    }
}





//WE7U
/*
 *  Extract signpost data from APRS info field: "{123}", an APRS data extension
 *  Format can be {1}, {12}, or {123}.
 */
int extract_signpost(char *info, char *signpost) {
    int i,found,len,done;

//0123456
//{1}
//{12}
//{121}

    found=0;
    len = (int)strlen(info);
    if ( (len > 2)
            && (info[0] == '{')
            && ( (info[2] == '}' ) || (info[3] == '}' ) || (info[4] == '}' ) ) ) {

        i = 1;
        done = 0;
        while (!done) {                 // Snag up to three digits
            if (isdigit((int)info[i])) {
                signpost[i-1] = info[i];
            }
            else if (info[i] == '}') {  // We're done
                found = i;              // found = position of '}' character
                done++;
            }
            else {                      // Not a signpost object
                signpost[0] = '\0';
                return(0);
            }
            i++;
            if ( (i > 4) && !done) {    // Something is wrong, we should be done by now
                done++;
                signpost[0] = '\0';
                return(0);
            }
        }
        substr(signpost,info+1,found-1);
        found++;
        for (i=0;i<=len-found;i++) {    // delete omnidf from data extension field
            info[i] = info[i+found];
        }
        return(1);
    } else {
        signpost[0] = '\0';
        return(0);
    }
}





static void clear_area(DataRow *p_station) {
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
}





//WE7U
/*
 *  Extract Area Object
 */
void extract_area(DataRow *p_station, char *data) {
    int i, val, len;
    AreaObject temp_area;

    /* NOTE: If we are here, the symbol was the area symbol.  But if this
       is a slightly corrupted packet, we shouldn't blow away the area info
       for this station, since it could be from a previously received good
       packet.  So we will work on temp_area and only copy to p_station at
       the end, returning on any error as we parse. N7TAP */

    //printf("Area Data: %s\n", data);

    len = (int)strlen(data);
    val = data[0] - '0';
    if (val >= 0 && val <= AREA_MAX) {
        temp_area.type = val;
        val = data[4] - '0';
        if (data[3] == '/') {
            if (val >=0 && val <= 9) {
                temp_area.color = val;
            }
            else {
                if (debug_level & 2)
                    puts("Bad area color (/)");
                return;
            }
        }
        else if (data[3] == '1') {
            if (val >=0 && val <= 5) {
                temp_area.color = 10 + val;
            }
            else {
                if (debug_level & 2)
                    puts("Bad area color (1)");
                return;
            }
        }

        val = 0;
        if (isdigit((int)data[1]) && isdigit((int)data[2])) {
            val = (10 * (data[1] - '0')) + (data[2] - '0');
        }
        else {
            if (debug_level & 2)
                puts("Bad area sqrt_lat_off");
            return;
        }
        temp_area.sqrt_lat_off = val;

        val = 0;
        if (isdigit((int)data[5]) && isdigit((int)data[6])) {
            val = (10 * (data[5] - '0')) + (data[6] - '0');
        }
        else {
            if (debug_level & 2)
                puts("Bad area sqrt_lon_off");
            return;
        }
        temp_area.sqrt_lon_off = val;

        for (i = 0; i <= len-7; i++) // delete area object from data extension field
            data[i] = data[i+7];
        len -= 7;

        if (temp_area.type == AREA_LINE_RIGHT || temp_area.type == AREA_LINE_LEFT) {
            if (data[0] == '{') {
                if (sscanf(data, "{%u}", &val) == 1) {
                    temp_area.corridor_width = val & 0xff;
                    for (i = 0; i <= len; i++)
                        if (data[i] == '}')
                            break;
                    val = i+1;
                    for (i = 0; i <= len-val; i++)
                        data[i] = data[i+val]; // delete corridor width
                }
                else {
                    if (debug_level & 2)
                        puts("Bad corridor width identifier");
                    temp_area.corridor_width = 0;
                    return;
                }
            }
            else {
                if (debug_level & 2)
                    puts("No corridor width specified");
                temp_area.corridor_width = 0;
            }
        }
        else {
            temp_area.corridor_width = 0;
        }
    }
    else {
        if (debug_level & 2)
            printf("Bad area type: %c\n", data[0]);
        return;
    }

    memcpy(&(p_station->aprs_symbol.area_object), &temp_area, sizeof(AreaObject));

    if (debug_level & 2) {
        printf("AreaObject: type=%d color=%d sqrt_lat_off=%d sqrt_lon_off=%d corridor_width=%d\n",
                p_station->aprs_symbol.area_object.type,
                p_station->aprs_symbol.area_object.color,
                p_station->aprs_symbol.area_object.sqrt_lat_off,
                p_station->aprs_symbol.area_object.sqrt_lon_off,
                p_station->aprs_symbol.area_object.corridor_width);
    }
}





/*
 *  Extract Time from begin of line      [APRS Reference, chapter 6]
 */
int extract_time(DataRow *p_station, char *data, int type) {
    int len, i;
    int ok = 0;

    // todo: better check of time data ranges
    len = (int)strlen(data);
    if (type == APRS_WX2) {
        // 8 digit time from stand-alone positionless weather stations...
        if (len > 8) {
            // MMDDHHMM   zulu time
            // MM 01-12         todo: better check of time data ranges
            // DD 01-31
            // HH 01-23
            // MM 01-59
            ok = 1;
            for (i=0;ok && i<8;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
                substr(p_station->station_time,data+2,6);
                p_station->station_time_type = 'z';
                for (i=0;i<=len-8;i++)         // delete time from data
                    data[i] = data[i+8];
            }
        }
    } else {
        if (len > 6) {
            // Status messages only with optional zulu format
            // DK7IN: APRS ref says one of 'z' '/' 'h', but I found 'c' at HB9TJM-8   ???
            if (data[6]=='z' || data[6]=='/' || data[6]=='h' || data[6]=='c')
                ok = 1;
            for (i=0;ok && i<6;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
                substr(p_station->station_time,data,6);
                p_station->station_time_type = data[6];
                for (i=0;i<=len-7;i++)         // delete time from data
                    data[i] = data[i+7];
            }
        }
    }
    return(ok);
}





// APRS Data Extensions               [APRS Reference p.27]
//  .../...  Course & Speed, may be followed by others (see p.27)
//  .../...  Wind Dir and Speed
//  PHG....  Station Power and Effective Antenna Height/Gain
//  RNG....  Pre-Calculated Radio Range
//  DFS....  DF Signal Strength and Effective Antenna Height/Gain
//  T../C..  Area Object Descriptor

/* Extract one of several possible APRS Data Extensions */
void process_data_extension(DataRow *p_station, char *data, /*@unused@*/ int type) {
    char temp1[7+1];
    char temp2[3+1];
    char bearing[3+1];
    char nrq[3+1];

    if (p_station->aprs_symbol.aprs_type == '\\' && p_station->aprs_symbol.aprs_symbol == 'l') {
            /* This check needs to come first because the area object extension can look
               exactly like what extract_speed_course will attempt to decode. */
            extract_area(p_station, data);
    } else {
            clear_area(p_station); // we got a packet with a non area symbol, so clear the data

            if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
            //printf("extracted speed/course\n");
                if (atof(temp2) > 0) {
                //printf("course is non-zero\n");
                        //sprintf(p_station->speed,"%06.2f",atof(temp1));     // in knots
                xastir_snprintf(p_station->speed, sizeof(p_station->speed), "%06.2f",atof(temp1));
                        strcpy(p_station->course, temp2);                    // in degrees
            }

            if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                //printf("extracted bearing and NRQ\n");
                strcpy(p_station->bearing,bearing);
                strcpy(p_station->NRQ,nrq);
                p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                }
            } else {
                if (extract_powergain(data,temp1)) {
                        strcpy(p_station->power_gain,temp1);

                if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                    //printf("extracted bearing and NRQ\n");
                    strcpy(p_station->bearing,bearing);
                    strcpy(p_station->NRQ,nrq);
                    p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                }
                }
            else {
                        if (extract_omnidf(data,temp1)) {
                    strcpy(p_station->signal_gain,temp1);   // Grab the SHGD values
                    p_station->bearing[0] = '\0';   // And blank out the bearing/NRQ values
                    p_station->NRQ[0] = '\0';

                    // The spec shows speed/course before DFS, but example packets that
                    // come with DOSaprs show DFSxxxx/speed/course.  We'll take care of
                    // that possibility by trying to decode speed/course again.
                        if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
                        //printf("extracted speed/course\n");
                            if (atof(temp2) > 0) {
                            //printf("course is non-zero\n");
                                    //sprintf(p_station->speed,"%06.2f",atof(temp1));     // in knots
                            xastir_snprintf(p_station->speed, sizeof(p_station->speed), "%06.2f",atof(temp1));
                                    strcpy(p_station->course,temp2);                    // in degrees
                        }
                    }

                    // The spec shows that omnidf and bearing/NRQ can be in the same
                    // packet, which makes no sense, but we'll try to decode it that
                    // way anyway.
                    if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                        //printf("extracted bearing and NRQ\n");
                        strcpy(p_station->bearing,bearing);
                        strcpy(p_station->NRQ,nrq);
                        //p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                    }
                }
                }
            }
        if (extract_signpost(data, temp2)) {
            //printf("extracted signpost data\n");
            strcpy(p_station->signpost,temp2);
        }
    }
}





/* extract all available information from info field */
void process_info_field(DataRow *p_station, char *info, /*@unused@*/ int type) {
    char temp_data[6+1];
    char time_data[MAX_TIME];

    if (extract_altitude(info,temp_data)) {                         // get altitude
        //sprintf(p_station->altitude,"%.2f",atof(temp_data)*0.3048); // feet to meter
        xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%.2f",atof(temp_data)*0.3048);
        strcpy(p_station->altitude_time,get_time(time_data));
        //printf("%.2f\n",atof(temp_data)*0.3048);
    }
    // do other things...
}





////////////////////////////////////////////////////////////////////////////////////////////////////


// type: 18
// call_sign: VE6GRR-15
// path: GPSLV,TCPIP,VE7DIE*
// data: GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,
// from: T
// port: 0
// origin: 
// third_party: 1





/*
 *  Extract data for $GPRMC, it fails if there is no position!!
 */
int extract_RMC(DataRow *p_station, char *data, char *call_sign, char *path) {
    char *temp_ptr;
    char temp_data[40];         // short term string storage, MAX_CALL, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;

    // should we copy it before processing? it changes data: ',' gets substituted by '\0' !!
    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.

    if ( (data == NULL) || (strlen(data) < 37) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_RMC;
    strcpy(p_station->pos_time,get_time(temp_data));    // get_time saves the time in temp_data
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);
    if (strchr(data,',') != NULL) {                                     // there is a ',' in string
        (void)strtok(data,",");                                         // extract GPRMC
        (void)strtok(NULL,",");                                         // extract time ?
        temp_ptr = strtok(NULL,",");                                    // extract valid char
        if (temp_ptr != NULL) {
            strncpy(temp_data,temp_ptr,2);
            if (temp_data[0]=='A' || temp_data[0]=='V') {
                /* V is a warning but we can get good data still ? */   // DK7IN: got no position with 'V' !
                temp_ptr=strtok(NULL,",");                              // extract latitude
                if (temp_ptr!=NULL && temp_ptr[4]=='.') {
                    strncpy(lat_s,temp_ptr,8);
                    lat_s[8] = '\0';    // Terminate it (just in case)

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

// We really should check for a terminating zero at the end of each substring we collect,
// and not rely on strtok exclusively.  We could have an extra-long field due to packet
// corruption and therefore our substrings wouldn't have terminating zeroes.
// We should also check the length of the string we're collecting from to make sure
// we don't run off the end for a short packet.

                    temp_ptr=strtok(NULL,",");                /* get N-S */
                    if (temp_ptr!=NULL) {
                        strncpy(temp_data,temp_ptr,1);
                        lat_s[8]=toupper((int)temp_data[0]);
                        lat_s[9] = '\0';
                        if (lat_s[8] =='N' || lat_s[8] =='S') {
                            temp_ptr=strtok(NULL,",");            /* get long */
                            if(temp_ptr!=NULL && temp_ptr[5] == '.') {
                                strncpy(long_s,temp_ptr,9);
                                long_s[9] = '\0';   // Terminate it, just in case

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

                                temp_ptr=strtok(NULL,",");            /* get E-W */
                                if (temp_ptr!=NULL) {
                                    strncpy(temp_data,temp_ptr,1);
                                    long_s[9] = toupper((int)temp_data[0]);
                                    long_s[10] = '\0';
                                    if (long_s[9] =='E' || long_s[9] =='W') {
                                        p_station->coord_lat = convert_lat_s2l(lat_s);
                                        p_station->coord_lon = convert_lon_s2l(long_s);
                                        ok = 1; // We have enough for a position now
                                        temp_ptr=strtok(NULL,",");        /* Get speed */
                                        if(temp_ptr!=0) {
                                            substr(p_station->speed,temp_ptr,MAX_SPEED);
                                            p_station->speed[MAX_SPEED - 1] = '\0'; // Terminate just in case
                                            // is it always knots, otherwise we need a conversion!
                                            temp_ptr=strtok(NULL,",");    /* Get course */
                                            if (temp_ptr!=NULL) {
                                                substr(p_station->course,temp_ptr,MAX_COURSE);
                                                p_station->course[MAX_COURSE - 1] = '\0'; // Terminate just in case
                                                (void)strtok(NULL,",");                    /* get date of fix */
                                            } else { // Short packet, no course available
                                                strcpy(p_station->course,"000.0");
                                            }
                                        } else { // Short packet, no speed available
                                            strcpy(p_station->speed,"0");
                                        }
                                    } else { // Not 'E' or 'W' for longitude direction
                                    }
                                } else { // Short packet, no longitude direction found
                                }
                            } else { // Short packet, no longitude found
                            }
                        } else { // Not 'N' or 'S' for latitude direction
                        }
                    } else { // Short packet, no latitude direction found
                    }
                } else { // Short packet, no latitude found
                }
            } else { // Validity character was not 'A' or 'V'
            }
        } else { // Short packet, no validity character
        }
    } else { // Short packet, character ',' not found in string
    }
    return(ok);
}





/*
 *  Extract data for $GPGGA
 *
 * GPGGA,hhmmss,ddmm.mmm[m],{N|S},dddmm.mmm[m],{E|W},{0|1|2},nsat,hdop,mmm.m,M,mm.m,M,,[*CHK]
 *
 * hhmmss = UTC
 * ddmm.mmm[m] = Degrees(dd) Minutes (mm.mmm[m]) (may be 3 or 4 digits of precision) Lattitude (N|S=North/South)
 * dddmm.mmm[m] = Degrees (ddd) Minutes (mm.mmm[m]) (may be 3 or 4 digits of precision) Longitude (E|W=East/West)
 * 0|1|2 == invalid/GPS/DGPS
 * nsat=Number of Sattelites being tracked
 * mmm.m,M=Meters MSL
 * mm.mM = Meters above GPS Ellipsoid
 *
 */
int extract_GGA(DataRow *p_station,char *data,char *call_sign, char *path) {
    char *temp_ptr;
    char temp_data[40];         // short term string storage, MAX_CALL, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;

    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
 
    if ( (data == NULL) || (strlen(data) < 35) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GGA;
    strcpy(p_station->pos_time,get_time(temp_data));        // get_time saves the time in temp_data
    strcpy(p_station->altitude_time,get_time(temp_data));   // get_time saves the time in temp_data
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);
    if (strchr(data,',')!=NULL) {
        if (strtok(data,",")!=NULL) { /* get gpgga and throw it away */
            if (strtok(NULL,",")!=NULL)  { /* get time and throw it away */
                temp_ptr = strtok(NULL,",");                 /* get latitude */
                if (temp_ptr !=NULL) {
                    strncpy(lat_s,temp_ptr,8);
                    lat_s[8] = '\0';    // Terminate it (just in case)

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

// We really should check for a terminating zero at the end of each substring we collect,
// and not rely on strtok exclusively.  We could have an extra-long field due to packet
// corruption and therefore our substrings wouldn't have terminating zeroes.
// We should also check the length of the string we're collecting from to make sure
// we don't run off the end for a short packet.

                    temp_ptr = strtok(NULL,",");                       /* get N-S */
                    if (temp_ptr!=NULL) {
                        strncpy(temp_data,temp_ptr,1);
                        lat_s[8]=toupper((int)temp_data[0]);
                        lat_s[9] = '\0';
                        if (lat_s[8] =='N' || lat_s[8] =='S') { // Check validity
                            temp_ptr=strtok(NULL,",");            /* get long */
                            if(temp_ptr!=NULL /* && temp_ptr[5] == '.' */ ) {  // ??
                                strncpy(long_s,temp_ptr,9);
                                long_s[9] = '\0';   // Terminate it, just in case

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

                                temp_ptr=strtok(NULL,",");            /* get E-W */
                                if (temp_ptr!=NULL) {
                                    strncpy(temp_data,temp_ptr,1);
                                    long_s[9]  = toupper((int)temp_data[0]);
                                    long_s[10] = '\0';
                                    if (long_s[9] =='E' || long_s[9] =='W') {   // Check validity
                                        p_station->coord_lat = convert_lat_s2l(lat_s);
                                        p_station->coord_lon = convert_lon_s2l(long_s);
                                        ok = 1;     // We have enough for a position now
                                        temp_ptr = strtok(NULL,",");                   /* get FIX Quality */
                                        if (temp_ptr!=NULL) {
// What are the possible values here for fix quality?
                                            strncpy(temp_data,temp_ptr,2);
                                            if (temp_data[0]=='1' || temp_data[0] =='2' ) { // Check for valid fix
                                                temp_ptr=strtok(NULL,",");  // Get sats visible
                                                if (temp_ptr!=NULL) {
                                                    strncpy(p_station->sats_visible,temp_ptr,MAX_SAT);
// Need to check for validity of this number.  Should be 0-12?  Perhaps a few more with WAAS, GLONASS, etc?
                                                    if ( (p_station->sats_visible[1] != '\0') && (p_station->sats_visible[2] != '\0') ) {
                                                        if (debug_level & 1)
                                                            printf("extract_GGA: Exiting, number of sats not valid\n");
                                                        strcpy(p_station->sats_visible,""); // Store empty sats visible
                                                        strcpy(p_station->altitude,""); // Store empty altitude
                                                        ok = 1; // Invalid rest of packet.  We got all but altitude.
                                                        return(ok);
                                                    }
                                                    temp_ptr=strtok(NULL,","); // get horizontal dilution of precision
                                                    if (temp_ptr!=NULL) {
// Need to check for valid number for HDOP instead of just throwing it away
                                                        temp_ptr=strtok(NULL,","); /* Get altitude */
                                                        if (temp_ptr!=NULL) {
                                                            if (strlen(temp_ptr) < MAX_ALTITUDE)    // Not too long
                                                                strcpy(p_station->altitude,temp_ptr); /* Get altitude */
                                                            else // Too long, truncate it
                                                                strncpy(p_station->altitude,temp_ptr,MAX_ALTITUDE);
                                                            p_station->altitude[MAX_ALTITUDE] = '\0'; // Terminate it, just in case
// Need to check for valid altitude before conversion
                                                            // unit is in meters, if not adjust value ???
                                                            temp_ptr=strtok(NULL,",");            /* get UNIT */
                                                            if (temp_ptr!=NULL) {
                                                                strncpy(temp_data,temp_ptr,1);    /* get UNIT */
                                                                if (temp_data[0] != 'M') {
                                                                    //printf("ERROR: should adjust altitude for meters\n");
                                                                //} else {  // Altitude units wrong.  Assume altitude bad
                                                                    strcpy(p_station->altitude,"");
                                                                }
                                                            } else {  // Short packet, no altitude units
                                                                strcpy(p_station->altitude,"");
                                                            }
                                                        } else {  // Short packet, no altitude
                                                            strcpy(p_station->altitude,"");
                                                        }
                                                    } else { // Short packet, no HDOP number
                                                        strcpy(p_station->altitude,"");
                                                    }
                                                } else { // Short packet, no sats visible number
                                                    strcpy(p_station->altitude,"");
                                                }
                                            } else {  // Invalid fix
                                                strcpy(p_station->altitude,"");
                                            }
                                        } else { // Short packet, no fix quality
                                            strcpy(p_station->altitude,"");
                                        }
                                    } else { // 'E' or 'W' not found, bad longitude direction
                                    }
                                } else { // Short packet, no 'E' or 'W' found
                                }
                            } else { // Short packet, no longitude found
                            }
                        } else { // 'N' or 'S' not found, bad latitude direction
                        }
                    } else { // Short packet, no 'N' or 'S' found
                    }
                } else { // Short packet, no latitude found
                }
            } else { // Short packet, no timestamp found
            }
        } else { // Short packet, no 'GPGGA' found.  Then how'd we get here???
        }
    } else { // Short packet, empty data field.  We should never have gotten to this routine.
    }
    return(ok);
}





/*
 *  Extract data for $GPGLL
 */
int extract_GLL(DataRow *p_station,char *data,char *call_sign, char *path) {
    char *temp_ptr;
    char temp_data[40];         // short term string storage, MAX_CALL, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;

    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
  
    if ( (data == NULL) || (strlen(data) < 28) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GLL;
    strcpy(p_station->pos_time,get_time(temp_data));    // get_time saves the time in temp_data
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);
    if (strchr(data,',')!=NULL) {
        (void)strtok(data,",");                    /* get gpgll */
        temp_ptr=strtok(NULL,",");                /* get latitude */
        if (temp_ptr!=NULL && temp_ptr[4]=='.') {
            strncpy(lat_s,temp_ptr,8);
            lat_s[8] = '\0';    // Terminate it (just in case)

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

// We really should check for a terminating zero at the end of each substring we collect,
// and not rely on strtok exclusively.  We could have an extra-long field due to packet
// corruption and therefore our substrings wouldn't have terminating zeroes.
// We should also check the length of the string we're collecting from to make sure
// we don't run off the end for a short packet.

           temp_ptr=strtok(NULL,",");                /* get N-S */
            if (temp_ptr!=NULL) {
                strncpy(temp_data,temp_ptr,1);
                lat_s[8]=toupper((int)temp_data[0]);
                lat_s[9] = '\0';
                if (lat_s[8] =='N' || lat_s[8] =='S') {
                    temp_ptr=strtok(NULL,",");            /* get long */
                    if(temp_ptr!=NULL && temp_ptr[5] == '.') {
                        strncpy(long_s,temp_ptr,9);
                        long_s[9] = '\0';   // Terminate it, just in case

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

                       temp_ptr=strtok(NULL,",");            /* get E-W */
                        if (temp_ptr!=NULL) {
                            strncpy(temp_data,temp_ptr,1);
                            long_s[9]  = toupper((int)temp_data[0]);
                            long_s[10] = '\0';
                            if (long_s[9] =='E' || long_s[9] =='W') {
                                p_station->coord_lat = convert_lat_s2l(lat_s);
                                p_station->coord_lon = convert_lon_s2l(long_s);
                                ok = 1; // We have enough for a position now
                                strcpy(p_station->course,"000.0");  // Fill in with dummy values
                                strcpy(p_station->speed,"0");       // Fill in with dummy values
                                (void)strtok(NULL,",");             // get time, throw it away
                                temp_ptr=strtok(NULL,",");          // get data_valid flag
                                if (temp_ptr!=NULL) {
                                    strncpy(temp_data,temp_ptr,2);
                                    if (temp_data[0]=='A') {
                                        /* A is valid, V is a warning but we can get good data still ? */
                                    } else { // Invalid data
                                    }
                                }
                                else { // Short packet, valid data flag missing
                                }
                            } else { // Not 'E' or 'W' for longitude direction
                            }
                        } else { // Short packet, missing longitude direction
                        }
                    } else { // Short packet, missing longitude
                    }
                } else { // Not 'N' or 'W' for latitude direction
                }
            } else { // Short packet, missing latitude direction
            }
        } else { // Short packet, missing latitude
        }
    } else { // Short packet, no ',' characters found
    }
    return(ok);
}





/*
 *  Add data from APRS information field to station database
 *  Returns a 1 if successful
 */
int data_add(int type ,char *call_sign, char *path, char *data, char from, int port, char *origin, int third_party) {
    DataRow *p_station;
    DataRow *p_time;
    char call[MAX_CALLSIGN+1];
    char new_station;
    long last_lat, last_lon;
    char last_alt[MAX_ALTITUDE];
    char last_speed[MAX_SPEED+1];
    char last_course[MAX_COURSE+1];
    time_t last_stn_sec;
    short last_flag;
    char temp_data[40];         // short term string storage, MAX_CALL, ...
    long l_lat, l_lon;
    double distance;
    char station_id[600];
    int found_pos;
    float value;
    WeatherRow *weather;
    int moving;
    int changed_pos;
    int scrupd;
    int ok, store;
    int ok_to_display;
    int compr_pos;

    // call and path had been validated before
    // Check "data" against the max APRS length, and dump the packet if too long.
    if ( (data != NULL) && (strlen(data) > MAX_INFO_FIELD_SIZE) ) {   // Overly long packet.  Throw it away.
        if (debug_level & 1)
            printf("data_add: Overly long packet.  Throwing it away.\n");
        return(0);  // Not an ok packet
    }

    if (debug_level & 1)
        printf("data_add:\n\ttype: %d\n\tcall_sign: %s\n\tpath: %s\n\tdata: %s\n\tfrom: %c\n\tport: %d\n\torigin: %s\n\tthird_party: %d\n",
            type,
            call_sign,
            path,
            data ? data : "NULL",       // This parameter may be NULL, if exp1 then exp2 else exp3 
            from,
            port,
            origin ? origin : "NULL",   // This parameter may be NULL
            third_party);

    weather = NULL; // only to make the compiler happy...
    found_pos = 1;
    strcpy(call,call_sign);
    p_station = NULL;
    new_station = (char)FALSE;                          // to make the compiler happy...
    last_lat = 0L;
    last_lon = 0L;
    last_stn_sec = sec_now();
    last_alt[0]    = '\0';
    last_speed[0]  = '\0';
    last_course[0] = '\0';
    last_flag      = 0;
    ok = 0;
    store = 0;
    p_time = NULL;                                      // add to end of time sorted list (newest)
    compr_pos = 0;

    if (search_station_name(&p_station,call,1)) {       // If we found the station in our list
//WE7U6
        // Check whether it's a locally-owned object/item
        if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
                && ( ((p_station->flag & ST_OBJECT) != 0)       // And it's an object
                  || ((p_station->flag & ST_ITEM) != 0) ) ) {   // or an item
            // Do nothing.  We don't want to re-order it in the time-ordered
            // list so that it'll expire from the queue normally.
        }
        else {
            move_station_time(p_station,p_time);        // update time, change position in time sorted list
            new_station = (char)FALSE;                  // we have seen this one before
        }
    } else {
        p_station = add_new_station(p_station,p_time,call);     // create storage
        new_station = (char)TRUE;                       // for new station
    }

    if (p_station != NULL) {
        last_lat = p_station->coord_lat;                // remember last position
        last_lon = p_station->coord_lon;
        last_stn_sec = p_station->sec_heard;
        strcpy(last_alt,p_station->altitude);
        strcpy(last_speed,p_station->speed);
        strcpy(last_course,p_station->course);
        last_flag = p_station->flag;

        ok = 1;                         // succeed as default
        switch (type) {

            case (APRS_MICE):           // Mic-E format
            case (APRS_FIXED):          // '!'
            case (APRS_MSGCAP):         // '='
                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                }
                if (ok) {
                    strcpy(p_station->pos_time,get_time(temp_data));
                    (void)extract_weather(p_station,data,compr_pos);    // look for weather data first
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude
                    substr(p_station->comments,data,MAX_COMMENTS);
                    p_station->record_type = NORMAL_APRS;
                    if (type == APRS_MSGCAP)
                        p_station->flag |= ST_MSGCAP;           // set "message capable" flag
                    else
                        p_station->flag &= (~ST_MSGCAP);        // clear "message capable" flag
                }
                break;

            case (APRS_DOWN):           // '/'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                    }
                }
                if (ok) {
                    strcpy(p_station->pos_time,get_time(temp_data));
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);
                    substr(p_station->comments,data,MAX_COMMENTS);
                    p_station->record_type = DOWN_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_DF):             // '@'
            case (APRS_MOBILE):         // '@'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                    }
                }
                if (ok) {
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);
                    substr(p_station->comments,data,MAX_COMMENTS);
                    if(type == APRS_MOBILE)
                        p_station->record_type = MOBILE_APRS;
                    else
                        p_station->record_type = DF_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_GRID):
                ok = extract_position(p_station, &data, type);
                if (ok) { 
                    if (debug_level & 1)
                        printf("data_add: Got grid data for %s\n", call);
                    substr(p_station->comments,data,MAX_COMMENTS);
                } else {
                    if (debug_level & 1)
                        printf("data_add: Bad grid data for %s : %s\n", call, data);
                }
                break;

            case (STATION_CALL_DATA):
                p_station->record_type = NORMAL_APRS;
                found_pos = 0;
                break;

            case (APRS_STATUS):         // '>' Status Reports     [APRS Reference, chapter 16]
                (void)extract_time(p_station, data, type);              // we need a time
                // todo: could contain Maidenhead or beam heading+power
                substr(p_station->comments,data,MAX_COMMENTS);          // store status text
                p_station->flag |= (ST_STATUS);                         // set "Status" flag
                p_station->record_type = NORMAL_APRS;                   // ???
                found_pos = 0;
                break;

            case (OTHER_DATA):          // Other Packets          [APRS Reference, chapter 19]
                // non-APRS beacons, treated as status reports until we get a real one
                if ((p_station->flag & (~ST_STATUS)) == 0) {            // only store if no status yet
                    substr(p_station->comments,data,MAX_COMMENTS);
                    p_station->record_type = NORMAL_APRS;               // ???
                }
                found_pos = 0;
                break;

            case (APRS_OBJECT):
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                    }
                }
                p_station->flag |= ST_OBJECT;                           // Set "Object" flag
                if (ok) {
                    strcpy(p_station->pos_time,get_time(temp_data));
                    strcpy(p_station->origin,origin);                   // define it as object
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);
                    substr(p_station->comments,data,MAX_COMMENTS);
                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_ITEM):
                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                }
                p_station->flag |= ST_ITEM;                             // Set "Item" flag
                if (ok) {
                    strcpy(p_station->pos_time,get_time(temp_data));
                    strcpy(p_station->origin,origin);                   // define it as item
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);
                    substr(p_station->comments,data,MAX_COMMENTS);
                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag
                }
                break;

            case (APRS_WX1):    // weather in '@' packet
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                    }
                }
                if (ok) {
                    (void)extract_weather(p_station,data,compr_pos);
                    p_station->record_type = (char)APRS_WX1;
                    substr(p_station->comments,data,MAX_COMMENTS);
                }
                break;

            case (APRS_WX2):            // '_'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    (void)extract_weather(p_station,data,0);            // look for weather data first
                    p_station->record_type = (char)APRS_WX2;
                    found_pos = 0;
                }
                break;

            case (APRS_WX4):            // '#'          Peet Bros U-II (mph)
            case (APRS_WX6):            // '*'          Peet Bros U-II (km/h)
            case (APRS_WX3):            // '!'          Peet Bros Ultimeter 2000 (data logging mode)
            case (APRS_WX5):            // '$ULTW'      Peet Bros Ultimeter 2000 (packet mode)
                if (get_weather_record(p_station)) {    // get existing or create new weather record
                    weather = p_station->weather_data;
                    if (type == APRS_WX3)   // Peet Bros Ultimeter 2000 data logging mode
                        decode_U2000_L(1,(unsigned char *)data,weather);
                    else if (type == APRS_WX5) // Peet Bros Ultimeter 2000 packet mode
                        decode_U2000_P(1,(unsigned char *)data,weather);
                    else    // Peet Bros Ultimeter-II
                        decode_Peet_Bros(1,(unsigned char *)data,weather,type);
                    p_station->record_type = (char)type;
                    strcpy(weather->wx_time, get_time(temp_data));
                    weather->wx_sec_time = sec_now();
                    found_pos = 0;
                }
                break;

            case (GPS_RMC):             // $GPRMC
                ok = extract_RMC(p_station,data,call_sign,path);
                break;

            case (GPS_GGA):             // $GPGGA
                ok = extract_GGA(p_station,data,call_sign,path);
                break;

            case (GPS_GLL):             // $GPGLL
                ok = extract_GLL(p_station,data,call_sign,path);
                break;

            default:
                printf("ERROR: UNKNOWN TYPE in data_add\n");
                ok = 0;
                break;
        }
    }

    if (!ok) {  // non-APRS beacon, treat it as Other Packet   [APRS Reference, chapter 19]
        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE + 1];
            strcpy(filtered_data,data-1);
            makePrintable(filtered_data);
            printf("store non-APRS data as status: %s: |%s|\n",call,filtered_data);
        }

        // GPRMC etc. without a position is here too, but it should not be stored as status!
        
        // store it as status report until we get a real one
        if ((p_station->flag & (~ST_STATUS)) == 0) {         // only store it if no status yet
            substr(p_station->comments,data-1,MAX_COMMENTS);
            p_station->record_type = NORMAL_APRS;               // ???
        }
        ok = 1;            
        found_pos = 0;
    }

    curr_sec = sec_now();
    if (ok) {
        // data packet is valid
        // announce own echo, we soon discard that packet...
        if (!new_station && is_my_call(p_station->call_sign,1)
                && strchr(path,'*') != NULL) {
            upd_echo(path);   // store digi that echoes my signal...
            statusline(langcode("BBARSTA033"),0);   // Echo from digipeater
        }
        // check if data is just a secondary echo from another digi
        if ((last_flag & ST_VIATNC) == 0
                || (curr_sec - last_stn_sec) > 15
                || p_station->coord_lon != last_lon 
                || p_station->coord_lat != last_lat)
            store = 1;                     // don't store secondary echos
    }

    if (!ok && new_station)
        delete_station_memory(p_station);       // remove unused record

    if (store) {           
        // we now have valid data to store into database
        // make time index unique by adding a serial number

//WE7U6 
        // Check whether it's a locally-owned object/item
        if ( (is_my_call(p_station->origin,1))                  // If station is owned by me
                && !new_station                                 // and we've seen this station before
                && ( ((p_station->flag & ST_OBJECT) != 0)       // and it's an object
                  || ((p_station->flag & ST_ITEM) != 0) ) ) {   // or an item
            // Do nothing.  We don't want to update the last-heard time
            // so that it'll expire from the queue normally.
        }
        else {
            p_station->sec_heard = curr_sec;    // Give it a new timestamp
        }

        if (curr_sec != last_sec) {     // todo: if old time calculate time_sn from database
            last_sec = curr_sec;
            next_time_sn = 0;           // restart time serial number
        }
        p_station->time_sn = next_time_sn++;            // todo: warning if serial number too high
        if (from == DATA_VIA_TNC) {                     // heard via TNC
            p_station->flag |= ST_VIATNC;               // set "via TNC" flag
            p_station->heard_via_tnc_last_time = sec_now();
            p_station->last_heard_via_tnc = 0l;
            p_station->heard_via_tnc_port = port;
        } else {                                        // heard other than TNC
            if (new_station) {                          // new station
                p_station->last_heard_via_tnc = 0L;
                p_station->flag &= (~ST_VIATNC);        // clear "via TNC" flag
                p_station->heard_via_tnc_last_time = 0l;
            } else {                                    // old station
                p_station->last_heard_via_tnc++;
                // if the last 20 times this station was heard other than tnc clear the heard tnc data
                if (p_station->last_heard_via_tnc > 20) {
                    p_station->last_heard_via_tnc = 0l;
                    p_station->flag &= (~ST_VIATNC);        // clear "via TNC" flag
                }
            }
        }
        p_station->last_port_heard = port;
        p_station->data_via = from;
        strcpy(p_station->packet_time,get_time(temp_data)); // get_time returns value in temp_data
        substr(p_station->node_path,path,MAX_PATH);
        p_station->flag |= ST_ACTIVE;
        if (third_party)
            p_station->flag |= ST_3RD_PT;               // set "third party" flag
        else
            p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
        if (origin != NULL && strcmp(origin,"INET") == 0)  // special treatment for inet names
            strcpy(p_station->origin,origin);           // to keep them separated from calls
        if (origin != NULL && strcmp(origin,"INET-NWS") == 0)  // special treatment for NWS
            strcpy(p_station->origin,origin);           // to keep them separated from calls
        if (origin == NULL || origin[0] == '\0')        // normal call
            p_station->origin[0] = '\0';                // undefine possible former object with same name
        if (!third_party && ((p_station->flag & ST_VIATNC) != 0)
                && strchr(p_station->node_path,'*') == NULL)
            p_station->flag |= (ST_LOCAL);              // set "local" flag
        else
            p_station->flag &= (~ST_LOCAL);             // clear "local" flag

        p_station->num_packets += 1;
        redo_list = (int)TRUE;          // we may need to update the lists

        if (found_pos) {        // if station has a position with the data
            if (position_on_extd_screen(p_station->coord_lat,p_station->coord_lon))
                p_station->flag |= (ST_INVIEW);   // set   "In View" flag
            else
                p_station->flag &= (~ST_INVIEW);  // clear "In View" flag
        }

        scrupd = 0;
        if (new_station) {
            if (debug_level & 256) {
                printf("New Station %s\n", p_station->call_sign);
            }
            if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0)
                p_station->flag |= (ST_MOVING); // it has a speed, so it's moving
            if (position_on_screen(p_station->coord_lat,p_station->coord_lon)) {

                if (p_station->coord_lat != 0 && p_station->coord_lon != 0) {   // discard undef positions from screen
                    if (!altnet || is_altnet(p_station) ) {
                        display_station(da,p_station,1);
                        scrupd = 1;                         // ???
                    }
                }
            }
        } else {        // we had seen this station before...
            if (debug_level & 256) {
                printf("New Data for %s %ld %ld\n", p_station->call_sign,
                    p_station->coord_lat, p_station->coord_lon);
            }
            if (found_pos && position_defined(p_station->coord_lat,p_station->coord_lon,1)) { // ignore undefined and 0N/0E
                if (debug_level & 256) {
                    printf("  Valid position for %s\n",
                        p_station->call_sign);
                }
                if (p_station->track_data != NULL) {
                    if (debug_level & 256) {
                        printf("Station has a trail: %s\n",
                            p_station->call_sign);
                    }
                    moving = 1;                         // it's moving if it has a trail
                                }
                else {
                    if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0) {
                        if (debug_level & 256) {
                            printf("Speed detected on %s\n",
                                p_station->call_sign);
                        }
                        moving = 1;                     // declare it moving, if it has a speed
                                        }
                    else {
                        if (debug_level & 256) {
                            printf("Position defined: %d, Changed: %s\n",
                                position_defined(last_lat, last_lon, 1),
                                (p_station->coord_lat != last_lat ||
                                p_station->coord_lon != last_lon) ?
                                "Yes" : "No");
                        }
                        if (position_defined(last_lat,last_lon,1)
                                && (p_station->coord_lat != last_lat || p_station->coord_lon != last_lon)) {
                            if (debug_level & 256) {
                                printf("Position Change detected on %s\n",
                                    p_station->call_sign);
                            }
                            moving = 1;                 // it's moving if it has changed the position
                                                }
                        else {
                            if (debug_level & 256) {
                                printf("Station %s still appears stationary.\n",
                                    p_station->call_sign);
                                printf(" %s stationary at %ld %ld (%ld %ld)\n",
                                    p_station->call_sign,
                                    p_station->coord_lat, p_station->coord_lon,
                                    last_lat,             last_lon);
                            }
                            moving = 0;
                                                }
                                        }
                                }
                changed_pos = 0;
                if (moving == 1) {                      
                    p_station->flag |= (ST_MOVING);
                    // we have a moving station, process trails
                    if (atoi(p_station->speed) < TRAIL_MAX_SPEED) {     // reject high speed data (undef gives 0)
                        // we now may already have the 2nd position, so store the old one first
                        if (debug_level & 256) {
                            printf("Station %s valid speed %s\n",
                                p_station->call_sign, p_station->speed);
                        }
                        if (p_station->track_data == NULL) {
                            if (debug_level & 256) {
                                printf("Station %s no trail history.\n",
                                    p_station->call_sign);
                            }
                            if (position_defined(last_lat,last_lon,1)) {  // ignore undefined and 0N/0E
                                if (debug_level & 256) {
                                    printf("Storing old position for %s\n",
                                        p_station->call_sign);
                                }
                                (void)store_trail_point(p_station,last_lon,last_lat,last_stn_sec,last_alt,last_speed,last_course,last_flag);
                                                        }
                                                }
                        //if (   p_station->coord_lon != last_lon
                        //    || p_station->coord_lat != last_lat ) {
                        // we don't store redundant points (may change this
                                                // later ?)
                                                //
                        // there are often echoes delayed 15 minutes or so
                        // it looks ugly on the trail, so I want to discard them
                        // This also discards immediate echoes
                        if (!is_trailpoint_echo(p_station)) {
                            (void)store_trail_point(p_station,p_station->coord_lon,p_station->coord_lat,p_station->sec_heard,p_station->altitude,p_station->speed,p_station->course,p_station->flag);
                            changed_pos = 1;
                        }
                        else if (debug_level & 256) {
                            printf("Trailpoint echo detected for %s\n",
                                p_station->call_sign);
                        }
                    }
                                        else {
                        if (debug_level & 256 || debug_level & 1)
                            printf("Speed over %d mph\n",TRAIL_MAX_SPEED);
                    }
                            
                    if (track_station_on == 1)          // maybe we are tracking a station
                        track_station(da,tracking_station_call,p_station);
                } // moving...

                // now do the drawing to the screen
                ok_to_display = !altnet || is_altnet(p_station); // Optimization step, needed twice below.
                scrupd = 0;
                if (changed_pos == 1 && station_trails && ((p_station->flag & ST_INVIEW) != 0)) {
                    if (ok_to_display) {
                        if (debug_level & 256) {
                            printf("Adding Solid Trail for %s\n",
                            p_station->call_sign);
                                                }
                        draw_trail(da,p_station,1);         // update trail
                        scrupd = 1;
                    }
                    else if (debug_level & 256) {
                        printf("Skipped trail for %s (altnet)\n",
                            p_station->call_sign);
                    }
                }
                if (position_on_screen(p_station->coord_lat,p_station->coord_lon)) {
                    if (changed_pos == 1 || !position_defined(last_lat,last_lon,0)) {
                        if (ok_to_display) {
                            display_station(da,p_station,1);// update symbol
                            scrupd = 1;
                        }
                    }
                }
            } // defined position
        }

        if (scrupd) {
            if (scale_y > 2048 || p_station->data_via == 'F')
                redraw_on_new_data = 1;         // less redraw activity
            else
                redraw_on_new_data = 2;         // better response
        }

        // announce stations in the status line
        if (!is_my_call(p_station->call_sign,1) && !wait_to_redraw) {
            if (new_station) {
                if (p_station->origin[0] == '\0')   // new station
                    //sprintf(station_id,langcode("BBARSTA001"),p_station->call_sign);
                    xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA001"),p_station->call_sign);
                else                                // new object
                    //sprintf(station_id,langcode("BBARSTA000"),p_station->call_sign);
                    xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA000"),p_station->call_sign);
            } else                                  // updated data
                //sprintf(station_id,langcode("BBARSTA002"),p_station->call_sign);
                xastir_snprintf(station_id, sizeof(station_id), langcode("BBARSTA002"),p_station->call_sign);

            statusline(station_id,0);
        }

        // announce new station with sound file or speech synthesis
        if (new_station && !wait_to_redraw) {   // && !is_my_call(p_station->call_sign,1) // ???
            if (sound_play_new_station)
                play_sound(sound_command,sound_new_station);
#ifdef HAVE_FESTIVAL
            if (festival_speak_new_station) {
                //sprintf(station_id,"%s, %s",langcode("WPUPCFA005"),p_station->call_sign);
                xastir_snprintf(station_id, sizeof(station_id), "%s, %s",langcode("WPUPCFA005"),p_station->call_sign);
                SayText(station_id);
            }
#endif
        }

        // check for range and DX
        if (found_pos && !is_my_call(p_station->call_sign,1)) {
            // if station has a position with the data
            /* Check Audio Alarms based on incoming packet */
            /* FG don't care if this is on screen or off get position */
            l_lat = convert_lat_s2l(my_lat);
            l_lon = convert_lon_s2l(my_long);

            // Get distance in nautical miles.
            value = (float)calc_distance_course(l_lat,l_lon,
                    p_station->coord_lat,p_station->coord_lon,temp_data,sizeof(temp_data));
            distance = value * cvt_kn2len;
        
            /* check ranges */
            if ((distance > atof(prox_min)) && (distance < atof(prox_max)) && sound_play_prox_message) {
                //sprintf(station_id,"%s < %.3f %s",p_station->call_sign, distance,
                //        units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                xastir_snprintf(station_id, sizeof(station_id), "%s < %.3f %s",p_station->call_sign, distance,
                        units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                statusline(station_id,0);
                play_sound(sound_command,sound_prox_message);
                /*printf("%s> PROX distance %f\n",p_station->call_sign, distance);*/
            }
#ifdef HAVE_FESTIVAL
            if ((distance > atof(prox_min)) && (distance < atof(prox_max)) && festival_speak_proximity_alert) {
                if (units_english_metric) {
                    if (distance < 1.0)
                        //sprintf(station_id, langcode("SPCHSTR005"), p_station->call_sign,
                //            (int)(distance * 1760), langcode("SPCHSTR004")); // say it in yards
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), p_station->call_sign,
                            (int)(distance * 1760), langcode("SPCHSTR004")); // say it in yards
                    else if ((int)((distance * 10) + 0.5) % 10)
                        //sprintf(station_id, langcode("SPCHSTR006"), p_station->call_sign, distance,
                            //langcode("SPCHSTR003")); // say it in miles with one decimal
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR006"), p_station->call_sign, distance,
                        langcode("SPCHSTR003")); // say it in miles with one decimal
                    else
                        //sprintf(station_id, langcode("SPCHSTR005"), p_station->call_sign, (int)(distance + 0.5),
                            //langcode("SPCHSTR003")); // say it in miles with no decimal
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), p_station->call_sign, (int)(distance + 0.5),
                        langcode("SPCHSTR003")); // say it in miles with no decimal
                } else {
                    if (distance < 1.0)
                //sprintf(station_id, langcode("SPCHSTR005"), p_station->call_sign,
                //  (int)(distance * 1000), langcode("SPCHSTR002")); // say it in meters
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), p_station->call_sign,
                        (int)(distance * 1000), langcode("SPCHSTR002")); // say it in meters
                    else if ((int)((distance * 10) + 0.5) % 10)
                        //sprintf(station_id, langcode("SPCHSTR006"), p_station->call_sign, distance,
                            //langcode("SPCHSTR001")); // say it in kilometers with one decimal
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR006"), p_station->call_sign, distance,
                        langcode("SPCHSTR001")); // say it in kilometers with one decimal
                    else
                //sprintf(station_id, langcode("SPCHSTR005"), p_station->call_sign, (int)(distance + 0.5),
                //  langcode("SPCHSTR001")); // say it in kilometers with no decimal
                xastir_snprintf(station_id, sizeof(station_id), langcode("SPCHSTR005"), p_station->call_sign, (int)(distance + 0.5),
                    langcode("SPCHSTR001")); // say it in kilometers with no decimal
                }
                SayText(station_id);
            }
#endif
            /* FG really should check the path before we do this and add setup for these ranges */
            if ((distance > atof(bando_min)) && (distance < atof(bando_max)) &&
                    sound_play_band_open_message && from == DATA_VIA_TNC) {
                //sprintf(station_id,"%s %s %.1f %s",p_station->call_sign, langcode("UMBNDO0001"),
                //    distance, units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                xastir_snprintf(station_id, sizeof(station_id), "%s %s %.1f %s",p_station->call_sign, langcode("UMBNDO0001"),
                        distance, units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                statusline(station_id,0);
                play_sound(sound_command,sound_band_open_message);
                /*printf("%s> BO distance %f\n",p_station->call_sign, distance);*/
            }
#ifdef HAVE_FESTIVAL
            if ((distance > atof(bando_min)) && (distance < atof(bando_max)) &&
                   festival_speak_band_opening && from == DATA_VIA_TNC) {
                //sprintf(station_id,"Heard, D X, %s, %s %.1f %s",p_station->call_sign, langcode("UMBNDO0001"),
                //    distance, units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                xastir_snprintf(station_id, sizeof(station_id), "Heard, D X, %s, %s %.1f %s",p_station->call_sign, langcode("UMBNDO0001"),
                    distance, units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"));
                SayText(station_id);
            }
#endif
        } // end found_pos

    }   // valid data into database
    return(ok);
}   // End of data_add() function





void my_station_gps_change(char *pos_long, char *pos_lat, char *course, char *speed, /*@unused@*/ char speedu, char *alt, char *sats) {
    long pos_long_temp, pos_lat_temp;
    char temp_data[40];   // short term string storage
    char temp_lat[12];
    char temp_long[12];
    DataRow *p_station;
    DataRow *p_time;

    p_station = NULL;
    if (!search_station_name(&p_station,my_callsign,1)) {  // find my data in the database
        p_time = NULL;          // add to end of time sorted list
        p_station = add_new_station(p_station,p_time,my_callsign);
    }
    p_station->flag |= ST_ACTIVE;
    p_station->data_via = 'L';
    p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
    p_station->record_type = NORMAL_APRS;
    substr(p_station->node_path,"local",MAX_PATH);
    strcpy(p_station->packet_time,get_time(temp_data));
    strcpy(p_station->pos_time,get_time(temp_data));
    p_station->flag |= ST_MSGCAP;               // set "message capable" flag

    /* convert to long and weed out any odd data */
    pos_long_temp = convert_lon_s2l(pos_long);
    pos_lat_temp  = convert_lat_s2l(pos_lat);

    /* convert to back to clean string for config data */
    convert_lon_l2s(pos_long_temp, temp_data, sizeof(temp_data), CONVERT_HP_NORMAL);
    //sprintf(temp_long,"%c%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[2], temp_data[4],temp_data[5],
    //        temp_data[7],temp_data[8], temp_data[9], temp_data[10]);
    xastir_snprintf(temp_long, sizeof(temp_long), "%c%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[2], temp_data[4],temp_data[5],
            temp_data[7],temp_data[8], temp_data[9], temp_data[10]);
    convert_lat_l2s(pos_lat_temp, temp_data, sizeof(temp_data), CONVERT_HP_NORMAL);
    //sprintf(temp_lat,"%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[3],temp_data[4], temp_data[6],
    //        temp_data[7], temp_data[8],temp_data[9]);
    xastir_snprintf(temp_lat, sizeof(temp_lat), "%c%c%c%c.%c%c%c%c",temp_data[0],temp_data[1],temp_data[3],temp_data[4], temp_data[6],
            temp_data[7], temp_data[8],temp_data[9]);

    /* fill the data in */    // ???????????????
    strcpy(my_lat,temp_lat);
    strcpy(my_long,temp_long);
    p_station->coord_lat = convert_lat_s2l(my_lat);
    p_station->coord_lon = convert_lon_s2l(my_long);

    if ((p_station->coord_lon != pos_long_temp) || (p_station->coord_lat != pos_lat_temp)) {
        /* check to see if enough to change pos on screen */
        if ((pos_long_temp>x_long_offset) && (pos_long_temp<(x_long_offset+(long)(screen_width *scale_x)))) {
            if ((pos_lat_temp>y_lat_offset) && (pos_lat_temp<(y_lat_offset+(long)(screen_height*scale_y)))) {
                if((labs((p_station->coord_lon+(scale_x/2))-pos_long_temp)/scale_x)>0
                        || (labs((p_station->coord_lat+(scale_y/2))-pos_lat_temp)/scale_y)>0) {
                    //redraw_on_new_data = 1;   // redraw next chance
                    //redraw_on_new_data = 2;     // better response?
                    if (debug_level & 256) {
                        printf("Redraw on new gps data \n");
                    }
                    statusline(langcode("BBARSTA038"),0);
                }
                else if (debug_level & 256) {
                    printf("New Position same pixel as old.\n");
                }
            }
            else if (debug_level & 256) {
                printf("New Position is off edge of screen.\n");
            }
        }
        else if (debug_level & 256) {
            printf("New position is off side of screen.\n");
        }
    }

    p_station->coord_lat = pos_lat_temp;    // DK7IN: we have it already !??
    p_station->coord_lon = pos_long_temp;

    strcpy(p_station->altitude_time,get_time(temp_data));
    my_last_altitude_time = sec_now();
    strcpy(p_station->speed_time,get_time(temp_data));
    strcpy(p_station->speed,speed);
    // is speed always in knots, otherwise we need a conversion!
    strcpy(p_station->course,course);
    strcpy(p_station->altitude,alt);
    // altu;    unit should always be meters  ????

    if(debug_level & 256)
        printf("GPS MY_LAT <%s> MY_LONG <%s> MY_ALT <%s>\n",
            my_lat, my_long, alt);

    /* get my last altitude meters to feet */
    my_last_altitude=(long)(atof(alt)*3.28084);

    /* get my last course in deg */
    my_last_course=atoi(course);

    /* get my last speed knots to mph */
    my_last_speed=(int)(atof(speed)*1.1508);
    strcpy(p_station->sats_visible,sats);

    //if (   p_station->coord_lon != last_lon
    //    || p_station->coord_lat != last_lat ) {
    // we don't store redundant points (may change this later ?)
    // there are often echoes delayed 15 minutes or so
    // it looks ugly on the trail, so I want to discard them
    // This also discards immediate echoes
    if (!is_trailpoint_echo(p_station)) {
        (void)store_trail_point(p_station,
                                p_station->coord_lon,
                                p_station->coord_lat,
                                p_station->sec_heard,
                                p_station->altitude,
                                p_station->speed,
                                p_station->course,
                                p_station->flag);
    }
    if (debug_level & 256) {
        printf("Adding Solid Trail for %s\n",
        p_station->call_sign);
    }
    draw_trail(da,p_station,1);         // update trail
    display_station(da,p_station,1);    // update symbol

    if (track_station_on == 1)          // maybe we are tracking ourselves?
        track_station(da,tracking_station_call,p_station);

    //redraw_on_new_data = 1;   // redraw next chance
    redraw_on_new_data = 2;     // Immediate update of symbols/tracks
}





void my_station_add(char *my_callsign, char my_group, char my_symbol, char *my_long, char *my_lat, char *my_phg, char *my_comment, char my_amb) {
    DataRow *p_station;
    DataRow *p_time;
    char temp_data[40];   // short term string storage
    char *strp;

    p_station = NULL;
    if (!search_station_name(&p_station,my_callsign,1)) {  // find call 
        p_time = NULL;          // add to end of time sorted list
        p_station = add_new_station(p_station,p_time,my_callsign);
    }
    p_station->flag |= ST_ACTIVE;
    p_station->data_via = 'L';
    p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
    p_station->record_type = NORMAL_APRS;
    substr(p_station->node_path,"local",MAX_PATH);
    strcpy(p_station->packet_time,get_time(temp_data));
    strcpy(p_station->pos_time,get_time(temp_data));
    p_station->flag |= ST_MSGCAP;               // set "message capable" flag

    /* Symbol overlay */
    if(my_group != '/' && my_group != '\\') {   // overlay
        p_station->aprs_symbol.aprs_type = '\\';
        p_station->aprs_symbol.special_overlay = my_group;
    } else {                                    // normal symbol
        p_station->aprs_symbol.aprs_type = my_group;
        p_station->aprs_symbol.special_overlay = ' ';
    }
    p_station->aprs_symbol.aprs_symbol = my_symbol;

    p_station->pos_amb = my_amb;
    strncpy(temp_data, my_lat, 9);
    temp_data[9] = '\0';
    strp = &temp_data[20];
    strncpy(strp, my_long, 10);
    strp[10] = '\0';
    switch (my_amb) {
    case 1: // 1/10th minute
        temp_data[6] = strp[7] = '5';
        break;
    case 2: // 1 minute
        temp_data[5] = strp[6] = '5';
        temp_data[6] = '0';
        strp[7]      = '0';
        break;
    case 3: // 10 minutes
        temp_data[3] = strp[4] = '5';
        temp_data[5] = temp_data[6] = '0';
        strp[6]      = strp[7]      = '0';
        break;
    case 4: // 1 degree
        temp_data[2] = strp[3] = '3';
        temp_data[3] = temp_data[5] = temp_data[6] = '0';
        strp[4]      = strp[6]      = strp[7]      = '0';
        break;
    case 0:
    default:
        break;
    }
    p_station->coord_lat = convert_lat_s2l(temp_data);
    p_station->coord_lon = convert_lon_s2l(strp);

    if (position_on_extd_screen(p_station->coord_lat,p_station->coord_lon)) {
        p_station->flag |= (ST_INVIEW);   // set   "In View" flag
    } else {
        p_station->flag &= (~ST_INVIEW);  // clear "In View" flag
    }

    substr(p_station->power_gain,my_phg,7);
    substr(p_station->comments,my_comment,MAX_COMMENTS);

    my_last_course = 0;         // set my last course in deg to zero
    redo_list = (int)TRUE;      // update active station lists
}





void display_packet_data(void) {
    int i;
    int pos;

    pos=0;
    if((Display_data_dialog != NULL) && (redraw_on_new_packet_data==1)) {
        XtVaSetValues(Display_data_text,XmNcursorPosition,&pos,NULL);

        // Known with some versions of Motif to have a memory leak:
        //XmTextSetString(Display_data_text,"");
        XmTextReplace(Display_data_text, (XmTextPosition) 0,
            XmTextGetLastPosition(Display_data_text), "");


        for (i=0; i<packet_data_display; i++) {
            XmTextInsert(Display_data_text,pos,packet_data[i]);
            pos += strlen(packet_data[i]);
            XtVaSetValues(Display_data_text,XmNcursorPosition,pos,NULL);
        }
        XmTextShowPosition(Display_data_text,0);
    }
    redraw_on_new_packet_data=0;
}





void packet_data_add(char *from, char *line) {
    int i;
    int offset;

    offset=0;
    if (line[0]==(char)3)
        offset=1;

    if ( (Display_packet_data_type==1 && strcmp(from,langcode("WPUPDPD005"))==0) ||
            (Display_packet_data_type==2 && strcmp(from,langcode("WPUPDPD006"))==0) || Display_packet_data_type==0) {

        redraw_on_new_packet_data=1;
        if (packet_data_display<15) {
            if (strlen(line)<256) {
                strcpy(packet_data[packet_data_display],from);
                strcat(packet_data[packet_data_display],"-> ");
                strcat(packet_data[packet_data_display],line+offset);
                strcat(packet_data[packet_data_display++],"\n\0");
            } else {
                strcpy(packet_data[packet_data_display],from);
                strcat(packet_data[packet_data_display],"-> ");
                strncat(packet_data[packet_data_display],line+offset,256);
                strcat(packet_data[packet_data_display++],"\n\0");
            }
        } else {
            packet_data_display=15;
            for (i=0; i<14; i++)
                strcpy(packet_data[i],packet_data[i+1]);

            if (strlen(line)<256) {
                strcpy(packet_data[14],from);
                strcat(packet_data[14],"-> ");
                strcat(packet_data[14],line+offset);
                strcat(packet_data[14],"\n\0");
            } else {
                strcpy(packet_data[14],from);
                strcat(packet_data[14],"-> ");
                strncat(packet_data[14],line+offset,256);
                strcat(packet_data[14],"\n\0");
            }
        }
    }
}





/*
 *  Decode Mic-E encoded data
 */
int decode_Mic_E(char *call_sign,char *path,char *info,char from,int port,int third_party) {
    int  i;
    int  offset;
    unsigned char s_b1;
    unsigned char s_b2;
    unsigned char s_b3;
    unsigned char s_b4;
    unsigned char s_b5;
    unsigned char s_b6;
    unsigned char s_b7;
    int  n,w,l;
    int  d,m,h;
    char temp[MAX_TNC_LINE_SIZE+1];     // Note: Must be big in case we get long concatenated packets
    char new_info[MAX_TNC_LINE_SIZE+1]; // Note: Must be big in case we get long concatenated packets
    int  course;
    int  speed;
    int  msg1,msg2,msg3,msg;
    int  info_size;
    long alt;
    int  msgtyp;
    char rig_type[10];
    int ok;
        
    // MIC-E Data Format   [APRS Reference, chapter 10]

    // todo:  error check
    //        drop wrong positions from receive errors...
    //        drop 0N/0E position (p.25)
    
    /* First 7 bytes of info[] contains the APRS data type ID,    */
    /* longitude, speed, course.                    */
    /* The 6-byte destination field of path[] contains latitude,    */
    /* N/S bit, E/W bit, longitude offset, message code.        */
    /*

    MIC-E Destination Field Format:
    -------------------------------
    Ar1DDDD0 Br1DDDD0 Cr1MMMM0 Nr1MMMM0 Lr1HHHH0 Wr1HHHH0 CrrSSID0
    D = Latitude Degrees.
    M = Latitude Minutes.
    H = Latitude Hundredths of Minutes.
    ABC = Message bits, complemented.
    N = N/S latitude bit (N=1).
    W = E/W longitude bit (W=1).
    L = 100's of longitude degrees (L=1 means add 100 degrees to longitude
    in the Info field).
    C = Command/Response flag (see AX.25 specification).
    r = reserved for future use (currently 0).
    
    */
    /****************************************************************************
    * I still don't handle:                                                     *
    *    Custom message bits                                                    *
    *    SSID special routing                                                   *
    *    Beta versions of the MIC-E (which use a slightly different format).    *
    *                                                                           *
    * DK7IN : lat/long with custom msg works, altitude/course/speed works       *
    *****************************************************************************/

    if (debug_level & 1)
        printf("decode_Mic_E:  FOUND MIC-E\n");

    // Note that the first MIC-E character was not passed to us, so we're
    // starting just past it.
    // Check for valid symbol table character.  Should be '/' or '\' only.
    if (info[7] != '/' && info[7] != '\\') {        // Symbol table char not in correct spot
        if (info[6] == '/' || info[6] == '\\') {    // Found it back one char in string
            // Don't print out the full info string here because it
            // can contain unprintable characters.  In fact, we
            // should check the chars we do print out to make sure
            // they're printable, else print a space char.
            printf("decode_Mic_E: Symbol table (%c), symbol (%c) swapped or corrupted packet?  Call=%s, Path=%s\n",
                ((info[7] > 0x1f) && (info[7] < 0x7f)) ? info[7] : ' ',
                ((info[6] > 0x1f) && (info[6] < 0x7f)) ? info[6] : ' ',
                call_sign,
                path);
        }
        if (debug_level & 1) {
            printf("Returned from data_add, invalid symbol table character: %c\n",info[7]);
        }
        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for valid symbol.  Should be between '!' and '~' only.
    if (info[6] < '!' || info[6] > '~') {
        if (debug_level & 1)
            printf("Returned from data_add, invalid symbol\n");

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for minimum MIC-E size.
    if (strlen(info) < 8) {
        if (debug_level & 1)
            printf("Returned from data_add, packet too short\n");

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    //printf("Path1:%s\n",path);

    msg1 = (int)( ((unsigned char)path[0] & 0x40) >>4 );
    msg2 = (int)( ((unsigned char)path[1] & 0x40) >>5 );
    msg3 = (int)( ((unsigned char)path[2] & 0x40) >>6 );
    msg = msg1 | msg2 | msg3;   // We now have the complemented message number in one variable
    msg = msg ^ 0x07;           // And this is now the normal message number
    msgtyp = 0;                 // DK7IN: Std message, I have to add custom msg decoding

    //printf("Msg: %d\n",msg);

    /* Snag the latitude from the destination field, Assume TAPR-2 */
    /* DK7IN: latitude now works with custom message */
    s_b1 = (unsigned char)( (path[0] & 0x0f) + (char)0x2f );
    //printf("path0:%c\ts_b1:%c\n",path[0],s_b1);
    if (path[0] & 0x10)     // A-J
        s_b1 += (unsigned char)1;

    if (s_b1 > (unsigned char)0x39)        // K,L,Z
        s_b1 = (unsigned char)0x20;
    //printf("s_b1:%c\n",s_b1);
 
    s_b2 = (unsigned char)( (path[1] & 0x0f) + (char)0x2f );
    //printf("path1:%c\ts_b2:%c\n",path[1],s_b2);
    if (path[1] & 0x10)     // A-J
        s_b2 += (unsigned char)1;

    if (s_b2 > (unsigned char)0x39)        // K,L,Z
        s_b2 = (unsigned char)0x20;
    //printf("s_b2:%c\n",s_b2);
 
    s_b3 = (unsigned char)( (path[2] & (char)0x0f) + (char)0x2f );
    //printf("path2:%c\ts_b3:%c\n",path[2],s_b3);
    if (path[2] & 0x10)     // A-J
        s_b3 += (unsigned char)1;

    if (s_b3 > (unsigned char)0x39)        // K,L,Z
        s_b3 = (unsigned char)0x20;
    //printf("s_b3:%c\n",s_b3);
 
    s_b4 = (unsigned char)( (path[3] & 0x0f) + (char)0x30 );
    //printf("path3:%c\ts_b4:%c\n",path[3],s_b4);
    if (s_b4 > (unsigned char)0x39)        // L,Z
        s_b4 = (unsigned char)0x20;
    //printf("s_b4:%c\n",s_b4);
 
    s_b5 = (unsigned char)( (path[4] & 0x0f) + (char)0x30 );
    //printf("path4:%c\ts_b5:%c\n",path[4],s_b5);
    if (s_b5 > (unsigned char)0x39)        // L,Z
        s_b5 = (unsigned char)0x20;
    //printf("s_b5:%c\n",s_b5);
 
    s_b6 = (unsigned char)( (path[5] & 0x0f) + (char)0x30 );
    //printf("path5:%c\ts_b6:%c\n",path[5],s_b6);
    if (s_b6 > (unsigned char)0x39)        // L,Z
        s_b6 = (unsigned char)0x20;
    //printf("s_b6:%c\n",s_b6);
 
    s_b7 =  (unsigned char)path[6];        // SSID, not used here
    //printf("path6:%c\ts_b7:%c\n",path[6],s_b7);
 
    //printf("\n");

    n = (int)((path[3] & 0x40) == (char)0x40);  // N/S Lat Indicator
    l = (int)((path[4] & 0x40) == (char)0x40);  // Longitude Offset
    w = (int)((path[5] & 0x40) == (char)0x40);  // W/E Long Indicator

    //printf("n:%d\tl:%d\tw:%d\n",n,l,w);

    /* Put the latitude string into the temp variable */
    //sprintf(temp,"%c%c%c%c.%c%c%c%c",s_b1,s_b2,s_b3,s_b4,s_b5,s_b6,
    //        (n ? 'N': 'S'), info[7]);   // info[7] = symbol table
    xastir_snprintf(temp, sizeof(temp), "%c%c%c%c.%c%c%c%c",s_b1,s_b2,s_b3,s_b4,s_b5,s_b6,
            (n ? 'N': 'S'), info[7]);   // info[7] = symbol table

    /* Compute degrees longitude */
    strcpy(new_info,temp);
    d = (int) info[0]-28;

    if (l)
        d += 100;

    if ((180<=d)&&(d<=189))  // ??
        d -= 80;

    if ((190<=d)&&(d<=199))  // ??
        d -= 190;

    /* Compute minutes longitude */
    m = (int) info[1]-28;
    if (m>=60)
        m -= 60;

    /* Compute hundredths of minutes longitude */
    h = (int) info[2]-28;
    /* Add the longitude string into the temp variable */
    //sprintf(temp,"%03d%02d.%02d%c%c",d,m,h,(w ? 'W': 'E'), info[6]);
    xastir_snprintf(temp, sizeof(temp), "%03d%02d.%02d%c%c",d,m,h,(w ? 'W': 'E'), info[6]);
    strcat(new_info,temp);

    /* Compute speed in knots */
    speed = (int)( ( info[3] - (char)28 ) * (char)10 );
    speed += ( (int)( (info[4] - (char)28) / (char)10) );
    if (speed >= 800)
        speed -= 800;       // in knots

    /* Compute course */
    course = (int)( ( ( (info[4] - (char)28) % 10) * (char)100) + (info[5] - (char)28) );
    if (course >= 400)
        course -= 400;

    /*  ???
        printf("info[4]-28 mod 10 - 4 = %d\n",( ( (int)info[4]) - 28) % 10 - 4);
        printf("info[5]-28 = %d\n", ( (int)info[5]) - 28 );
    */
    //sprintf(temp,"%03d/%03d",course,speed);
    xastir_snprintf(temp, sizeof(temp), "%03d/%03d",course,speed);
    strcat(new_info,temp);
    offset = 8;   // start of rest of info

    /* search for rig type in Mic-E data */
    rig_type[0] = '\0';
    if (info[offset] != '\0' && (info[offset] == '>' || info[offset] == ']')) {
        /* detected type code:     > TH-D7    ] TM-D700 */
        if (info[offset] == '>')
            strcpy(rig_type," TH-D7");
        else
            strcpy(rig_type," TM-D700");

        offset++;
    }

    info_size = (int)strlen(info);
    /* search for compressed altitude in Mic-E data */  // {
    if (info_size >= offset+4 && info[offset+3] == '}') {  // {
        /* detected altitude  ___} */
        alt = ((((long)info[offset] - (long)33) * (long)91 +(long)info[offset+1] - (long)33) * (long)91
                    + (long)info[offset+2] - (long)33) - 10000;  // altitude in meters
        alt /= 0.3048;                                // altitude in feet, as in normal APRS

        //32808 is -10000 meters, or 10 km (deepest ocean), which is as low as a MIC-E
        //packet may go.  Upper limit is mostly a guess.
        if ( (alt > 500000) || (alt < -32809) ) {  // Altitude is whacko.  Skip it.
            if (debug_level & 1)
                printf("decode_Mic_E:  Altitude is whacko:  %ld feet, skipping altitude...\n", alt);
            offset += 4;
        }
        else {  // Altitude is ok
            //sprintf(temp," /A=%06ld",alt);
            xastir_snprintf(temp, sizeof(temp), " /A=%06ld",alt);
            offset += 4;
            strcat(new_info,temp);
        }
    }

    /* start of comment */
    if (strlen(rig_type) > 0) {
        //sprintf(temp,"%s",rig_type);
        xastir_snprintf(temp, sizeof(temp), "%s",rig_type);
        strcat(new_info,temp);
    }

    strcat(new_info," Mic-E ");
    if (msgtyp == 0) {
        switch (msg) {
            case 1:
                strcat(new_info,"Enroute");
                break;

            case 2:
                strcat(new_info,"In Service");
                break;

            case 3:
                strcat(new_info,"Returning");
                break;

            case 4:
                strcat(new_info,"Committed");
                break;

            case 5:
                strcat(new_info,"Special");
                break;

            case 6:
                strcat(new_info,"Priority");
                break;

            case 7:
                strcat(new_info,"Emergency");
                break;

            default:
                strcat(new_info,"Off Duty");
        }
    } else {
        //sprintf(temp,"Custom%d",msg);
        xastir_snprintf(temp, sizeof(temp), "Custom%d",msg);
        strcat(new_info,temp);
    }

    if (info[offset] != '\0') {
        /* Append the rest of the message to the expanded MIC-E message */
        for (i=offset; i<info_size; i++)
            temp[i-offset] = info[i];

        temp[info_size-offset] = '\0';
        strcat(new_info," ");
        strcat(new_info,temp);
    }

    if (debug_level & 1) {
        printf("decode_Mic_E:  Done decoding MIC-E\n");
        printf("APRS_MICE, %s, %s, %s, %d, %d, NULL, %d\n",call_sign,path,new_info,from,port,third_party);
        // type:        APRS_MICE,
        // callsign:    N0EST-9,
        // path:        TTPQ9P,W0MXW-1,WIDE,N0QK-1*,
        // new_info:    4401.90N/09228.79W>278/007 /A=-05685 TM-D700 Mic-E Off Duty N0EST  ,
        // from:        70,
        // port:        -1,
        //              NULL,
        // third_party: 0
    }

    ok = data_add(APRS_MICE,call_sign,path,new_info,from,port,NULL,third_party);

    if (debug_level & 1)
        printf("Returned from data_add, end of function\n");

    return(ok);
}   // End of decode_Mic_E()





/*
 *  Directed Station Query (query to my station)   [APRS Reference, chapter 15]
 */
int process_directed_query(char *call,char *path,char *message,char from) {
    DataRow *p_station;
    char from_call[MAX_CALLSIGN+1];
    char temp[100];
    int ok;

    if (debug_level & 1)
        printf("process_directed_query: %s\n",message);

    ok = 0;
    if (!ok && strncmp(message,"APRSD",5) == 0 && from != 'F') {  // stations heard direct
        pad_callsign(from_call,call);
        //sprintf(temp,":%s:Directs=",from_call);
        xastir_snprintf(temp, sizeof(temp), ":%s:Directs=",from_call);
        p_station = n_first;
        while (p_station != NULL) {
            if ((p_station->flag & ST_ACTIVE) != 0) {        // ignore deleted objects
                if ((p_station->flag & ST_VIATNC) != 0) {    // test "via TNC" flag
        // DK7IN: I think the implementation is wrong, we deliver stations heard via a digi too...
                    if (strlen(temp)+strlen(p_station->call_sign) < 65) {
                        strcat(temp," ");
                        strcat(temp,p_station->call_sign);
                    } else
                        break;
                }
            }
            p_station = p_station->n_next;
        }
        transmit_message_data(call,temp);
        ok = 1;
    }
    if (!ok && strncmp(message,"APRSH",5)==0) {
        ok = 1;
    }
    if (!ok && strncmp(message,"APRSM",5)==0) {
        ok = 1;
    }
    if (!ok && strncmp(message,"APRSO",5)==0) {
        ok = 1;
    }
    if (!ok && strncmp(message,"APRSP",5) == 0 && from != 'F') {
        transmit_now = 1;       //send position
        ok = 1;
    }
    if (!ok && strncmp(message,"APRSS",5)==0) {
        ok = 1;
    }
    if (!ok && (strncmp(message,"APRST",5)==0
            ||  strncmp(message,"PING?",5)==0) && from != 'F') {
        pad_callsign(from_call,call);
        //sprintf(temp,":%s:PATH= %s>%s",from_call,call,path);    // correct format ?????
        xastir_snprintf(temp, sizeof(temp), ":%s:PATH= %s>%s",from_call,call,path);    // correct format ?????
        transmit_message_data(call,temp);
        ok = 1;
    }
    if (!ok && strncasecmp("VER",message,3) == 0 && from != 'F') { // not in Reference !???
        pad_callsign(from_call,call);
        //sprintf(temp,":%s:%s",from_call,VERSIONLABEL);
        xastir_snprintf(temp, sizeof(temp), ":%s:%s",from_call,VERSIONLABEL);
        transmit_message_data(call,temp);
        if (debug_level & 1)
            printf("Sent to %s:%s\n",call,temp);
    }
    return(ok);
}





/*
 *  Station Capabilities, Queries and Responses      [APRS Reference, chapter 15]
 */
int process_query( /*@unused@*/ char *call_sign, /*@unused@*/ char *path,char *message,char from,int port, /*@unused@*/ int third_party) {
    char temp[100];
    int ok;

    ok = 0;
    if (!ok && strncmp(message,"APRS?",5)==0) {
        ok = 1;
    }
    if (!ok && strncmp(message,"IGATE?",6)==0) {
        if (operate_as_an_igate && from != 'F') {
            //sprintf(temp,"<IGATE,MSG_CNT=%d,LOC_CNT=%d",(int)igate_msgs_tx,stations_types(3));
            xastir_snprintf(temp, sizeof(temp), "<IGATE,MSG_CNT=%d,LOC_CNT=%d",(int)igate_msgs_tx,stations_types(3));
            output_my_data(temp,port,0,0);
        }
        ok = 1;
    }
    if (!ok && strncmp(message,"WX?",3)==0) {
        ok = 1;
    }
    return(ok);
}





/*
 *  Status Reports                              [APRS Reference, chapter 16]
 */
int process_status( /*@unused@*/ char *call_sign, /*@unused@*/ char *path, /*@unused@*/ char *message, /*@unused@*/ char from, /*@unused@*/ int port, /*@unused@*/ int third_party) {

//    popup_message(langcode("POPEM00018"),message);  // What is it ???
    return(1);
}





/*
 *  shorten_path
 *
 * What to do with this one?
 *      APW250,TCPIP*,ZZ2RMV-5*
 * We currently convert it to:
 *      APW250
 * It's a packet that went across the 'net, then to RF, then back to
 * the 'net.  We should probably drop it altogether?
 *
 *  Gets rid of unused digipeater fields (after the asterisk) and the
 *  TCPIP field if it exists.  Used for creating the third-party
 *  headers for igating purposes.  Note that for TRACEn-N and WIDEn-N
 *  digi's, it's impossible to tell via the '*' character whether that
 *  part of the path was used, but we can tell by the difference of
 *  'n' and 'N'.  If they're different, then that part of the path was
 *  used.  If it has counted down to just a TRACE or a WIDE (or TRACE7
 *  or WIDE5), then it should have a '*' after it like normal.
 */
void shorten_path( char *path, char *short_path ) {
    int i,j,found_trace_wide,found_asterisk;
    char *ptr;


    if ( (path != NULL) && (strlen(path) >= 1) ) {

        strcpy(short_path,path);

        // Terminate the path at the end of the last used digipeater
        // This is trickier than it seems due to WIDEn-N and TRACEn-N
        // digipeaters.

        // Take a run through the entire path string looking for unused
        // TRACE/WIDE paths.
        for ( i = (strlen(path)-1); i >= 0; i-- ) { // Count backwards
            // If we find ",WIDE3-3" or ",TRACE7-7" (numbers match),
            // jam '\0' in at the comma.  These are unused digipeaters.
            if (   (strstr(&short_path[i],",WIDE7-7") != NULL)
                || (strstr(&short_path[i],",WIDE6-6") != NULL)
                || (strstr(&short_path[i],",WIDE5-5") != NULL)
                || (strstr(&short_path[i],",WIDE4-4") != NULL)
                || (strstr(&short_path[i],",WIDE3-3") != NULL)
                || (strstr(&short_path[i],",WIDE2-2") != NULL)
                || (strstr(&short_path[i],",WIDE1-1") != NULL)
                || (strstr(&short_path[i],",TRACE7-7") != NULL)
                || (strstr(&short_path[i],",TRACE6-6") != NULL)
                || (strstr(&short_path[i],",TRACE5-5") != NULL)
                || (strstr(&short_path[i],",TRACE4-4") != NULL)
                || (strstr(&short_path[i],",TRACE3-3") != NULL)
                || (strstr(&short_path[i],",TRACE2-2") != NULL)
                || (strstr(&short_path[i],",TRACE1-1") != NULL) ) {
                short_path[i] = '\0';
            }
        }


        // Take another run through short_string looking for used
        // TRACE/WIDE paths.  Also look for '*' characters and flag
        // if we see any.  If no '*' found, but a used TRACE/WIDE
        // path found, chop the path after the used TRACE/WIDE.  This
        // is to modify paths like this:
        //     APRS,PY1AYH-15*,RELAY,WIDE3-2,PY1EU-1
        // to this:
        //     APRS,PY1AYH-15*,RELAY,WIDE3-2
        j = 0;
        found_trace_wide = 0;
        found_asterisk = 0;
        for ( i = (strlen(short_path)-1); i >= 0; i-- ) { // Count backwards

            if (short_path[i] == '*')
                found_asterisk++;

            // Search for TRACEn/WIDEn.  If found (N!=n is guaranteed
            // by the previous loop) set the lower increment for the next
            // loop just past the last TRACEn/WIDEn found.  The used part
            // of the TRACEn/WIDEn will still remain in our shorter path.
            if (   (strstr(&short_path[i],"WIDE7") != NULL)
                || (strstr(&short_path[i],"WIDE6") != NULL)
                || (strstr(&short_path[i],"WIDE5") != NULL)
                || (strstr(&short_path[i],"WIDE4") != NULL)
                || (strstr(&short_path[i],"WIDE3") != NULL)
                || (strstr(&short_path[i],"WIDE2") != NULL)
                || (strstr(&short_path[i],"WIDE1") != NULL)
                || (strstr(&short_path[i],"TRACE7") != NULL)
                || (strstr(&short_path[i],"TRACE6") != NULL)
                || (strstr(&short_path[i],"TRACE5") != NULL)
                || (strstr(&short_path[i],"TRACE4") != NULL)
                || (strstr(&short_path[i],"TRACE3") != NULL)
                || (strstr(&short_path[i],"TRACE2") != NULL)
                || (strstr(&short_path[i],"TRACE1") != NULL) ) {
                j = i;
                found_trace_wide++;
                break;  // We only want to find the right-most one.
                        // We've found a used digipeater!
            }
        }


        // Chop off any unused digi's after a used TRACEn/WIDEn
        if (!found_asterisk && found_trace_wide) {
            for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
                if (short_path[i] == ',') {
                    short_path[i] = '\0';   // Terminate the string
                }
            }
        }


        // At this point, if we found a TRACEn or WIDEn, the "j"
        // variable will be non-zero.  If not then it'll be zero and
        // we'll run completely through the shorter path converting
        // '*' characters to '\0'.
        found_asterisk = 0;
        for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
            if (short_path[i] == '*') {
                short_path[i] = '\0';   // Terminate the string
                found_asterisk++;
            }
        }


        // Check for TCPIP or TCPXX as the last digipeater.  If present,
        // remove them.  TCPXX means that the packet came from an unregistered
        // user, and those packets will be rejected in igate.c before they're
        // sent to RF anyway.  igate.c will check for it's presence in path,
        // not in short_path, so we're ok here to get rid of it in short_path.
        if (strlen(short_path) >= 5) {  // Get rid of "TCPIP" & "TCPXX"

            ptr = &short_path[strlen(short_path) - 5];
            if (   (strcasecmp(ptr,"TCPIP") == 0)
                || (strcasecmp(ptr,"TCPXX") == 0) ) {
                *ptr = '\0';
            }
            if ( (strlen(short_path) >= 1)  // Get rid of possible ending comma
                    && (short_path[strlen(short_path) - 1] == ',') ) {
                short_path[strlen(short_path) - 1] = '\0';
            }
        }


        // We might have a string with zero used digipeaters.  In this case
        // we will have no '*' characters and no WIDEn-N/TRACEn-N digis.
        // Get rid of everything except the destination call.  These packets
        // must have been heard directly by an igate station.
        if (!found_trace_wide && !found_asterisk) {
            for ( i = (strlen(short_path)-1); i >= j; i-- ) { // Count backwards
                if (short_path[i] == ',') {
                    short_path[i] = '\0';   // Terminate the string
                }
            }
        }


        // The final step:  Remove any asterisks in the path.
        // We'll insert our own on the way out to RF again.
        for ( i = 0; i < (int)(strlen(short_path) - 1); i++ ) {
            if (short_path[i] == '*') {
                for (j = i; j <= (int)(strlen(short_path) - 1); j++ ) {
                  short_path[j] = short_path[j+1];  // Shift left by one char
                }
            }
        }
 
    
    }
    else {
        short_path[0] = '\0';   // We were passed an empty string or a NULL.
    }

    if (debug_level & 1) {
        printf("%s\n",path);
        printf("%s\n\n",short_path);
    }
}





/*
 *  Mesages, Bulletins and Announcements         [APRS Reference, chapter 14]
 */
int decode_message(char *call,char *path,char *message,char from,int port,int third_party) {
    char *temp_ptr;
    char ipacket_message[300];
    char from_call[MAX_CALLSIGN+1];
    char ack[20];
    int ok, len;
    char addr[9+1];
    char addr9[9+1];
    char msg_id[5+1];
    char orig_msg_id[5+1];
    char ack_string[6];
    int done;


    // :xxxxxxxxx:____0-67____             message              printable, except '|', '~', '{'
    // :BLNn     :____0-67____             general bulletin     printable, except '|', '~'
    // :BLNnxxxxx:____0-67____           + Group Bulletin
    // :BLNX     :____0-67____             Announcement
    // :NWS-xxxxx:____0-67____             NWS Service Bulletin
    // :xxxxxxxxx:ackn1-5n               + ack
    // :xxxxxxxxx:rejn1-5n               + rej
    // :xxxxxxxxx:____0-67____{n1-5n     + message
    // :NTS....
    //  01234567890123456
    // 01234567890123456    old
    // we get message with already extracted data ID

    if (debug_level & 1)
        printf("decode_message: start\n");

    if (debug_level & 1) {
        if ( (message != NULL) && (strlen(message) > (MAX_MESSAGE_LENGTH + 10) ) ) { // Overly long message.  Throw it away.  We're done.
            printf("decode_message: LONG message.  Dumping it.\n");
            return(0);
        }
    }

    ack_string[0] = '\0';   // Clear out the Reply/Ack result string

    len = (int)strlen(message);
    ok = (int)(len > 9 && message[9] == ':');
    if (ok) {
        substr(addr9,message,9);                // extract addressee
        strcpy(addr,addr9);
        (void)remove_trailing_spaces(addr);
        message = message + 10;                 // pointer to message text
        temp_ptr = strstr(message,"{");         // look for message ID
        msg_id[0] = '\0';
        if (temp_ptr != NULL) {
            substr(msg_id,temp_ptr+1,5);        // extract message ID, could be non-digit
            temp_ptr[0] = '\0';                 // adjust message end (chops off message ID)
        }

        // Save the original msg_id away.
        strncpy(orig_msg_id,msg_id,5+1);

        // Check for Reply/Ack protocol in msg_id, which looks like
        // this:  "{XX}BB", where XX is the sequence number for the
        // message, and BB is the ack for the previous message from
        // my station.  I've also seen this from APRS+: "{XX}B", so
        // perhaps this is also possible "{X}B" or "{X}BB}".  We can
        // also get auto-reply responses from APRS+ that just have
        // "}X" or "}XX" at the end.  We decode those as well.
        //
        temp_ptr = strstr(msg_id,"}"); // look for Reply Ack in msg_id

        if (temp_ptr != NULL) { // Found Reply/Ack protocol!
            int zz = 1;
            int yy = 0;

            if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                printf("Found Reply/Ack:%s\n",message);
                printf("Orig_msg_id:%s\t",msg_id);
            }

// Put this code into the UI message area as well (if applicable).

            // Separate out the extra ack so that we can deal with
            // it properly.
            while (temp_ptr[zz] != '\0') {
                ack_string[yy++] = temp_ptr[zz++];
            }
            ack_string[yy] = '\0';  // Terminate the string

            // Terminate it here so that rest of decode works
            // properly.  We can get duplicate messages
            // otherwise.
            temp_ptr[0] = '\0'; // adjust msg_id end

            if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                printf("New_msg_id:%s\tReply_ack:%s\n\n",msg_id,ack_string);
            }

        }
        else {  // Look for Reply Ack in message without sequence
                // number
            temp_ptr = strstr(message,"}");

            if (temp_ptr != NULL) {
                int zz = 1;
                int yy = 0;

                if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                    printf("Found Reply/Ack:%s\n",message);
                }

// Put this code into the UI message area as well (if applicable).

                while (temp_ptr[zz] != '\0') {
                    ack_string[yy++] = temp_ptr[zz++];
                }
                ack_string[yy] = '\0';  // Terminate the string

                // Terminate it here so that rest of decode works
                // properly.  We can get duplicate messages
                // otherwise.
                temp_ptr[0] = '\0'; // adjust message end

                if ( (debug_level & 1) && (is_my_call(addr,1)) ) {
                    printf("Reply_ack:%s\n\n",ack_string);
                }
            } 
        }
 
        done = 0;
    } else
        done = 1;                               // fall through...
    if (debug_level & 1)
        printf("1\n");
    len = (int)strlen(message);
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"ack",3) == 0) {              // ACK
        substr(msg_id,message+3,5);
        // printf("ACK: %s: |%s| |%s|\n",call,addr,msg_id);
        if (is_my_call(addr,1)) {
            clear_acked_message(call,addr,msg_id);  // got an ACK for me
            msg_record_ack(call,addr,msg_id,0);     // Record the ack for this message
        }
        else {                                          // ACK for other station
            /* Now if I have Igate on and I allow to retransmit station data           */
            /* check if this message is to a person I have heard on my TNC within an X */
            /* time frame. If if is a station I heard and all the conditions are ok    */
            /* spit the ACK out on the TNC -FG                                         */
            if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1)) {
                char short_path[100];

                /*printf("Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,call,addr); { */
                shorten_path(path,short_path);
                //sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
                xastir_snprintf(ipacket_message, sizeof(ipacket_message), "}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
                output_igate_rf(call,addr,path,ipacket_message,port,third_party);
                igate_msgs_tx++;
            }
        }
        done = 1;
    }
    if (debug_level & 1)
        printf("2\n");
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"rej",3) == 0) {              // REJ
        substr(msg_id,message+3,5);
        // printf("REJ: %s: |%s| |%s|\n",call,addr,msg_id);
        // we ignore it
        done = 1;
    }
    if (debug_level & 1)
        printf("3\n");
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"BLN",3) == 0) {                       // Bulletin
        long dummy;

        // printf("found BLN: |%s| |%s|\n",addr,message);
        bulletin_message(from,call,addr,message,sec_now());
        (void)msg_data_add(addr,call,message,"",MESSAGE_BULLETIN,from,&dummy);
        done = 1;
    }
    if (debug_level & 1)
        printf("4\n");

    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0 && is_my_call(addr,1)) { // Message for me with msg_id
                                                             // (sequence number)
        time_t last_ack_sent;
        long record;

// Remember to put this code into the UI message area as well (if
// applicable).

        // Check for Reply/Ack
        if (strlen(ack_string) != 0) {  // Have a free-ride ack to deal with
            clear_acked_message(call,addr,ack_string);  // got an ACK for me
            msg_record_ack(call,addr,ack_string,0);   // Record the ack for this message
        }

        // Save the ack 'cuz we might need it while talking to this
        // station.  We need it to implement Reply/Ack protocol.
        store_most_recent_ack(call,msg_id);
 
        // printf("found Msg w line to me: |%s| |%s|\n",message,msg_id);
        last_ack_sent = msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&record); // id_fixed

        new_message_data += 1;
        (void)check_popup_window(call, 2);  // Calls update_messages()
        //update_messages(1); // Force an update
        if (sound_play_new_message)
            play_sound(sound_command,sound_new_message);
#ifdef HAVE_FESTIVAL
/* I re-use ipacket_message as my string buffer */
        if (festival_speak_new_message_alert) {
            //sprintf(ipacket_message,"You have a new message from %s.",call);
            xastir_snprintf(ipacket_message, sizeof(ipacket_message), "You have a new message from %s.",call);
            SayText(ipacket_message);
        }
        if (festival_speak_new_message_body) {
            //sprintf(ipacket_message," %s",message);
            xastir_snprintf(ipacket_message, sizeof(ipacket_message), " %s",message);
            SayText(ipacket_message);
        }

#endif
        // Only send an ack out once per 30 seconds
        if ( (from != 'F')  // Not from a log file
                && ((last_ack_sent + 30 ) < sec_now()) ) {

            //printf("Sending ack: %ld %ld %ld\n",last_ack_sent,sec_now(),record);

            // Update the last_ack_sent field for the message
            msg_update_ack_stamp(record);

            pad_callsign(from_call,call);         /* ack the message */

            // In this case we want to send orig_msg_id back, not
            // the (possibly) truncated msg_id.  This is per Bob B's
            // Reply/Ack spec, sent to xastir-dev on Nov 14, 2001.
            xastir_snprintf(ack, sizeof(ack), ":%s:ack%s",from_call,orig_msg_id);
            transmit_message_data(call,ack);
            if (auto_reply == 1) {
                if (debug_level & 2)
                    printf("Send autoreply to <%s> from <%s> :%s\n",call,my_callsign,auto_reply_message);
                if (!is_my_call(call,1))
                    output_message(my_callsign,call,auto_reply_message);
            }
        }

else {
//printf("Skipping ack: %ld %ld\n",last_ack_sent,sec_now());
}

        done = 1;
    }
    if (debug_level & 1)
        printf("5\n");
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"NWS-",4) == 0) {             // NWS weather alert
        long dummy;

        //printf("found NWS: |%s| |%s| |%s|\n",addr,message,msg_id);      // could have sort of line number
        (void)msg_data_add(addr,call,message,msg_id,MESSAGE_NWS,from,&dummy);
        (void)alert_message_scan();
        done = 1;
        if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1)) { // { for my editor...
            char short_path[100];

            shorten_path(path,short_path);
            //sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);
            output_nws_igate_rf(call,path,ipacket_message,port,third_party);
        }
    }
    if (debug_level & 1)
        printf("6a\n");
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"SKY",3) == 0) {  // NWS weather alert additional info
        long dummy;

        //printf("found SKY: |%s| |%s| |%s|\n",addr,message,msg_id);      // could have sort of line number
        (void)msg_data_add(addr,call,message,msg_id,MESSAGE_NWS,from,&dummy);
        (void)alert_message_scan();
        done = 1;
        if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1)) { // { for my editor...
            char short_path[100];

            shorten_path(path,short_path);
            //sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
            xastir_snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);
            output_nws_igate_rf(call,path,ipacket_message,port,third_party);
        }
    }
    if (debug_level & 1)
        printf("6b\n");
    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0) {          // other message with linenumber (msg from me?)
        long dummy;

        if (debug_level & 2) printf("found Msg w line: |%s| |%s| |%s|\n",addr,message,msg_id);
        (void)msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&dummy);
        new_message_data += look_for_open_group_data(addr);
        if ((is_my_call(call,1) && check_popup_window(addr, 2) != -1)
                || check_popup_window(call, 0) != -1
                || check_popup_window(addr, 1) != -1) {
            //update_messages(1); // Force an update
        }

        /* Now if I have Igate on and I allow to retransmit station data           */
        /* check if this message is to a person I have heard on my TNC within an X */
        /* time frame. If if is a station I heard and all the conditions are ok    */
        /* spit the message out on the TNC -FG                                     */
        if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1) && !is_my_call(addr,1)) {
            char short_path[100];

            /*printf("Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,
                        call,addr);*/     // {
            shorten_path(path,short_path);
            //sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s{%s",call,short_path,my_callsign,addr9,message,msg_id);
            xastir_snprintf(ipacket_message, sizeof(ipacket_message), "}%s>%s,TCPIP,%s*::%s:%s{%s",call,short_path,my_callsign,addr9,message,msg_id);
            output_igate_rf(call,addr,path,ipacket_message,port,third_party);
            igate_msgs_tx++;
        }
        done = 1;
    }
    if (debug_level & 1)
        printf("7\n");
    //--------------------------------------------------------------------------
    if (!done && len > 2 && message[0] == '?' && is_my_call(addr,1)) { // directed query
        // Smallest query known is "?WX".
        if (debug_level & 1)
            printf("Received a directed query\n");
        done = process_directed_query(call,path,message+1,from);
    }
    if (debug_level & 1)
        printf("8\n");
    //--------------------------------------------------------------------------
    // special treatment required?: Msg w/o line for me ???
    //--------------------------------------------------------------------------
    if (!done) {                                   // message without line number
        long dummy;

        if (debug_level & 4)
            printf("found Msg: |%s| |%s|\n",addr,message);

        (void)msg_data_add(addr,call,message,"",MESSAGE_MESSAGE,from,&dummy);
        new_message_data++;      // ??????
        if (check_popup_window(addr, 1) != -1) {
            //update_messages(1); // Force an update
        }

        // Could be response to a query.  Popup a messsage.
        if ( (message[0] != '?') && is_my_call(addr,1) ) {
            popup_message(langcode("POPEM00018"),message);

            // Check for Reply/Ack.  APRS+ sends an AA: response back
            // for auto-reply, with an embedded free-ride Ack.
            if (strlen(ack_string) != 0) {  // Have an extra ack to deal with
                clear_acked_message(call,addr,ack_string);  // got an ACK for me
                msg_record_ack(call,addr,ack_string,0);   // Record the ack for this message
            }
        }
 
        // done = 1;
    }
    if (debug_level & 1)
        printf("9\n");
    //--------------------------------------------------------------------------
    if (ok)
        (void)data_add(STATION_CALL_DATA,call,path,message,from,port,NULL,third_party);

    if (debug_level & 1)
        printf("decode_message: finish\n");

    return(ok);
}





/*
 *  UI-View format messages, not relevant for APRS, format is not specified in APRS Reference
 */
int decode_UI_message(char *call,char *path,char *message,char from,int port,int third_party) {
    char *temp_ptr;
    char from_call[MAX_CALLSIGN+1];
    char ack[20];
    char addr[9+1];
    int ok, len;
    char msg_id[5+1];
    int done;

    // I'm not sure, but I think they use 2 digit line numbers only
    // extract addr from path
    substr(addr,path,9);
    ok = (int)(strlen(addr) > 0);
    if (ok) {
        temp_ptr = strstr(addr,",");         // look for end of first call
        if (temp_ptr != NULL)
            temp_ptr[0] = '\0';                 // adjust addr end
        ok = (int)(strlen(addr) > 0);
    }
    len = (int)strlen(message);
    ok = (int)(len >= 2);
    if (ok) {
        temp_ptr = strstr(message,"~");         // look for message ID
        msg_id[0] = '\0';
        if (temp_ptr != NULL) {
            substr(msg_id,temp_ptr+1,2);        // extract message ID, could be non-digit
            temp_ptr[0] = '\0';                 // adjust message end
        }
        done = 0;
    } else
        done = 1;                               // fall through...
    len = (int)strlen(message);
    //--------------------------------------------------------------------------
    if (!done && msg_id[0] != '\0' && is_my_call(addr,1)) {      // message for me
        time_t last_ack_sent;
        long record;

        last_ack_sent = msg_data_add(addr,call,message,msg_id,MESSAGE_MESSAGE,from,&record);
        new_message_data += 1;
        (void)check_popup_window(call, 2);
        //update_messages(1); // Force an update
        if (sound_play_new_message)
            play_sound(sound_command,sound_new_message);

        // Only send an ack or autoresponse once per 30 seconds
        if ( (from != 'F')
                && ( (last_ack_sent + 30) < sec_now()) ) {

            //printf("Sending ack: %ld %ld %ld\n",last_ack_sent,sec_now(),record);

            // Record the fact that we're sending an ack now
            msg_update_ack_stamp(record);

            pad_callsign(from_call,call);         /* ack the message */
            //sprintf(ack,":%s:ack%s",from_call,msg_id);
            xastir_snprintf(ack, sizeof(ack), ":%s:ack%s",from_call,msg_id);
            transmit_message_data(call,ack);
            if (auto_reply == 1) {
                if (debug_level & 2)
                    printf("Send autoreply to <%s> from <%s> :%s\n",call,my_callsign,auto_reply_message);

                if (!is_my_call(call,1))
                    output_message(my_callsign,call,auto_reply_message);
            }
        }
        done = 1;
    }
    //--------------------------------------------------------------------------
    if (!done && len == 2 && msg_id[0] == '\0') {                // ACK
        substr(msg_id,message,5);
        if (is_my_call(addr,1)) {
            clear_acked_message(call,addr,msg_id);      // got an ACK for me
            msg_record_ack(call,addr,msg_id,0);      // Record the ack for this message
        }
//        else {                                          // ACK for other station
            /* Now if I have Igate on and I allow to retransmit station data           */
            /* check if this message is to a person I have heard on my TNC within an X */
            /* time frame. If if is a station I heard and all the conditions are ok    */
            /* spit the ACK out on the TNC -FG                                         */
//            if (operate_as_an_igate>1 && from==DATA_VIA_NET && !is_my_call(call,1)) {
//                char short_path[100];
                //printf("Igate check o:%d f:%c myc:%s cf:%s ct:%s\n",operate_as_an_igate,from,my_callsign,call,addr); {

//                shorten_path(path,short_path);
                //sprintf(ipacket_message,"}%s>%s:%s:%s",call,path,addr9,message);
//                sprintf(ipacket_message,"}%s>%s,TCPIP,%s*::%s:%s",call,short_path,my_callsign,addr9,message);
//                output_igate_rf(call,addr,path,ipacket_message,port,third_party);
//                igate_msgs_tx++;
//            }
//        }
        done = 1;
    }
    //--------------------------------------------------------------------------
    if (ok)
        (void)data_add(STATION_CALL_DATA,call,path,message,from,port,NULL,third_party);
    return(ok);
}





/*
 *  Decode APRS Information Field and dispatch it depending on the Data Type ID
 */
void decode_info_field(char *call, char *path, char *message, char *origin, char from, int port, int third_party) {
    char line[MAX_TNC_LINE_SIZE+1];
    char my_data[MAX_TNC_LINE_SIZE+1];
    int  ok_igate_net;
    int  done, ignore;
    char data_id;

    /* remember fixed format starts with ! and can be up to 24 chars in the message */ // ???
    if (debug_level & 1)
        printf("decode_info_field: c:%s p:%s m:%s f:%c\n",call,path,message,from);
    if (debug_level & 1)
        printf("decode_info_field: Past check\n");

    done         = 0;                                   // if 1, packet was decoded
    ignore       = 0;                                   // if 1, don't treat undecoded packets as status text
    ok_igate_net = 0;                                   // if 1, send packet to internet
    my_data[0]   = '\0';                                // not really needed...

    if ( (message != NULL) && (strlen(message) > MAX_TNC_LINE_SIZE) ) { // Overly long message, throw it away.
        if (debug_level & 1)
            printf("decode_info_field: Overly long message.  Throwing it away.\n");
        done = 1;
    }
    else if (message == NULL || strlen(message) == 0) {      // we could have an empty message
        (void)data_add(STATION_CALL_DATA,call,path,NULL,from,port,origin,third_party);
        done = 1;                                       // don't report it to internet
    } else
        strcpy(my_data,message);                        // copy message for later internet transmit

    // special treatment for objects
    if (!done && strlen(origin) > 0 && strncmp(origin,"INET",4)!=0) { // special treatment for object or item
        if (message[0] == '*') {                        // set object
            (void)data_add(APRS_OBJECT,call,path,message+1,from,port,origin,third_party);
            ok_igate_net = 1;                           // report it to internet
        } else if (message[0] == '!') {                 // set item
            (void)data_add(APRS_ITEM,call,path,message+1,from,port,origin,third_party);
            ok_igate_net = 1;                           // report it to internet
        } else
            if (message[0] == '_') {                    // delete object/item
                delete_object(call);                    // ?? does not vanish from map immediately !!???
                ok_igate_net = 1;                       // report it to internet
            }
        done = 1;
    }

    if (!done) {
        data_id = message[0];           // look at the APRS Data Type ID (first char in information field)
        message += 1;                   // extract data ID from information field
        ok_igate_net = 1;               // as default report packet to internet

        if (debug_level & 1) {
            if (ok_igate_net)
                printf("decode_info_field: ok_igate_net can be read\n");
        }

        switch (data_id) {
            case '=':   // Position without timestamp (with APRS messaging)
                if (debug_level & 1)
                    printf("decode_info_field: = (position w/o timestamp)\n");
                done = data_add(APRS_MSGCAP,call,path,message,from,port,origin,third_party);
                break;

            case '!':   // Position without timestamp (no APRS messaging) or Ultimeter 2000 WX
                if (debug_level & 1)
                    printf("decode_info_field: ! (position w/o timestamp or Ultimeter 2000 WX)\n");
                if (message[0] == '!' && is_xnum_or_dash(message+1,40))   // Ultimeter 2000 WX
                    done = data_add(APRS_WX3,call,path,message+1,from,port,origin,third_party);
                else
                    done = data_add(APRS_FIXED,call,path,message,from,port,origin,third_party);
                break;

            case '/':   // Position with timestamp (no APRS messaging)
                if (debug_level & 1)
                    printf("decode_info_field: / (position w/timestamp)\n");
                done = data_add(APRS_DOWN,call,path,message,from,port,origin,third_party);
                break;

            case '@':   // Position with timestamp (with APRS messaging)
                // DK7IN: could we need to test the message length first?
                if ((message[14] == 'N' || message[14] == 'S') &&
                    (message[24] == 'W' || message[24] == 'E')) {       // uncompressed format
                    if (debug_level & 1)
                        printf("decode_info_field: @ (uncompressed position w/timestamp)\n");
                    if (message[29] == '/') {
                        if (message[33] == 'g' && message[37] == 't')
                            done = data_add(APRS_WX1,call,path,message,from,port,origin,third_party);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,from,port,origin,third_party);
                    } else
                        done = data_add(APRS_DF,call,path,message,from,port,origin,third_party);
                } else {                                                // compressed format
                    if (debug_level & 1)
                        printf("decode_info_field: @ (compressed position w/timestamp)\n");
                    if (message[16] >= '!' && message[16] <= 'z') {     // csT is speed/course
                        if (message[20] == 'g' && message[24] == 't')   // Wx data
                            done = data_add(APRS_WX1,call,path,message,from,port,origin,third_party);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,from,port,origin,third_party);
                    } else
                        done = data_add(APRS_DF,call,path,message,from,port,origin,third_party);
                }
                break;

            case '[':   // Maidenhead grid locator beacon (obsolete- but used for meteor scatter)
              done = data_add(APRS_GRID,call,path,message,from,port,origin,third_party);
                break;
            case 0x27:  // Mic-E  Old GPS data (or current GPS data in Kenwood TM-D700)
            case 0x60:  // Mic-E  Current GPS data (but not used in Kennwood TM-D700)
            //case 0x1c:// Mic-E  Current GPS data (Rev. 0 beta units only)
            //case 0x1d:// Mic-E  Old GPS data (Rev. 0 beta units only)
                if (debug_level & 1)
                    printf("decode_info_field: 0x27 or 0x60 (Mic-E)\n");
                done = decode_Mic_E(call,path,message,from,port,third_party);
                break;

            case '_':   // Positionless weather data                [APRS Reference, chapter 12]
                if (debug_level & 1)
                    printf("decode_info_field: _ (positionless wx data)\n");
                done = data_add(APRS_WX2,call,path,message,from,port,origin,third_party);
                break;

            case '#':   // Peet Bros U-II Weather Station (mph)     [APRS Reference, chapter 12]
                if (debug_level & 1)
                    printf("decode_info_field: # (peet bros u-II wx station)\n");
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX4,call,path,message,from,port,origin,third_party);
                break;

            case '*':   // Peet Bros U-II Weather Station (km/h)
                if (debug_level & 1)
                    printf("decode_info_field: * (peet bros u-II wx station)\n");
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX6,call,path,message,from,port,origin,third_party);
                break;

            case '$':   // Raw GPS data or Ultimeter 2000
                if (debug_level & 1)
                    printf("decode_info_field: $ (raw gps or ultimeter 2000)\n");
                if (strncmp("ULTW",message,4) == 0 && is_xnum_or_dash(message+4,44))
                    done = data_add(APRS_WX5,call,path,message+4,from,port,origin,third_party);
                else if (strncmp("GPGGA",message,5) == 0)
                    done = data_add(GPS_GGA,call,path,message,from,port,origin,third_party);
                else if (strncmp("GPRMC",message,5) == 0)
                    done = data_add(GPS_RMC,call,path,message,from,port,origin,third_party);
                else if (strncmp("GPGLL",message,5) == 0)
                    done = data_add(GPS_GLL,call,path,message,from,port,origin,third_party);
                else {
                        // handle VTG and WPT too  (APRS Ref p.25)
                }
                break;

            case ':':   // Message
                if (debug_level & 1)
                    printf("decode_info_field: : (message)\n");
                done = decode_message(call,path,message,from,port,third_party);
                // there could be messages I should not retransmit to internet... ??? Queries to me...
                break;

            case '>':   // Status                                   [APRS Reference, chapter 16]
                if (debug_level & 1)
                    printf("decode_info_field: > (status)\n");
                done = data_add(APRS_STATUS,call,path,message,from,port,origin,third_party);
                break;

            case '?':   // Query
                if (debug_level & 1)
                    printf("decode_info_field: ? (query)\n");
                done = process_query(call,path,message,from,port,third_party);
                ignore = 1;     // don't treat undecoded packets as status text
                break;

            case '~':   // UI-format messages, not relevant for APRS ("Do not use" in Reference)
            case ',':   // Invalid data or test packets             [APRS Reference, chapter 19]
            case '<':   // Station capabilities                     [APRS Reference, chapter 15]
            case '{':   // User-defined APRS packet format     //}
            case '%':   // Agrelo DFJr / MicroFinder
            case '&':   // Reserved -- Map Feature
            case 'T':   // Telemetrie data                          [APRS Reference, chapter 13]
                if (debug_level & 1)
                    printf("decode_info_field: ~,<{%%&T[\n");
                ignore = 1;     // don't treat undecoded packets as status text
                break;
        }

        if (debug_level & 1) {
            if (done)
                printf("decode_info_field: done = 1\n");
            else
                printf("decode_info_field: done = 0\n");
            if (ok_igate_net)
                printf("decode_info_field: ok_igate_net can be read 2\n");
        }

        if (debug_level & 1)
            printf("decode_info_field: done with big switch\n");

        if (!done && !ignore) {         // Other Packets        [APRS Reference, chapter 19]
            done = data_add(OTHER_DATA,call,path,message-1,from,port,origin,third_party);
            ok_igate_net = 0;           // don't put data on internet       ????
            if (debug_level & 1)
                printf("decode_info_field: done with data_add(OTHER_DATA)\n");
        }

        if (!done) {                    // data that we do ignore...
            //printf("decode_info_field: not decoding info: Call:%s ID:%c Msg:|%s|\n",call,data_id,message);
            ok_igate_net = 0;           // don't put data on internet
            if (debug_level & 1)
                printf("decode_info_field: done with ignored data\n");
        }
    }

    if (third_party)
        ok_igate_net = 0;               // don't put third party traffic on internet
    if (is_my_call(call,1))
        ok_igate_net = 0;               // don't put my data on internet     ???

    if (ok_igate_net) {
        if (debug_level & 1)
            printf("decode_info_field: ok_igate_net start\n");

        if (from == DATA_VIA_TNC && strlen(my_data) > 0) {
//WE7U8
            // Here's where we inject our own callsign like this: "WE7U-15,I"
            // in order to provide injection ID for our igate.  Should we also
            // add a '*' character after our callsign?
            //sprintf(line,"%s>%s,%s,I:%s",call,path,my_callsign,my_data);
            xastir_snprintf(line, sizeof(line), "%s>%s,%s,I:%s",call,path,my_callsign,my_data);
            //sprintf(line,"%s>%s:%s",call,path,my_data);

            //printf("decode_info_field: IGATE>NET %s\n",line);
            output_igate_net(line, port,third_party);
        }
    }
    if (debug_level & 1)
        printf("decode_info_field: done\n");
}





/*
 *  Extract object or item data from information field before processing
 */
int extract_object(char *call, char **info, char *origin) {
    int ok, i;

    // Object and Item Reports     [APRS Reference, chapter 11]
    ok = 0;
    // todo: add station originator to database
    if ((*info)[0] == ';') {                    // object
        // fixed 9 character object name with any printable ASCII character
        if (strlen((*info)) > 1+9) {
            substr(call,(*info)+1,9);           // extract object name
            (*info) = (*info) + 10;
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            (void)remove_trailing_spaces(call);
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    } else if ((*info)[0] == ')') {             // item
        // 3 - 9 character item name with any printable ASCII character
        if (strlen((*info)) > 1+3) {
            for (i = 1; i <= 9; i++) {
                if ((*info)[i] == '!' || (*info)[i] == '_') {
                    call[i-1] = '\0';
                    break;
                }
                call[i-1] = (*info)[i];
            }
            call[9] = '\0';  // In case we never saw '!' || '_'
            (*info) = &(*info)[i];
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            //(void)remove_trailing_spaces(call);   // This statement messed up our searching!!! Don't use it!
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    } else {
        printf("Not an object, nor an item!!! call=%s, info=%s, origin=%s.\n",
               call, *info, origin);
    }
    return(ok);
}





/*
 *  Extract third-party traffic from information field before processing
 */
int extract_third_party(char *call, char *path, char **info, char *origin) {
    int ok;
    char *p_call;
    char *p_path;

    p_call = NULL;                              // to make the compiler happy...
    p_path = NULL;                              // to make the compiler happy...
    ok = 0;
    if (!is_my_call(call,1)) {
        // todo: add reporting station call to database ??
        //       but only if not identical to reported call
        (*info) = (*info) +1;                   // strip '}' character
        p_call = strtok((*info),">");           // extract call
        if (p_call != NULL) {
            p_path = strtok(NULL,":");          // extract path
            if (p_path != NULL) {
                (*info) = strtok(NULL,"");      // rest is information field
                if ((*info) != NULL)            // the above looks dangerous, but works on same string
                    if (strlen(p_path) < 100)
                        ok = 1;                 // we have found all three components
            }
        }
    }

    if ((debug_level & 1) && !ok)
        printf("extract_third_party: invalid format from %s\n",call);

    if (ok) {
        strcpy(path,p_path);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers

        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];
            strcpy(filtered_data,path);
            makePrintable(filtered_data);
            printf("extract_third_party: invalid path: %s\n",filtered_data);
        }
    }

    if (ok) {                                         // check callsign
        (void)remove_trailing_asterisk(p_call);       // is an asterisk valid here ???
        if (valid_inet_name(p_call,(*info),origin)) { // accept some of the names used in internet
            strcpy(call,p_call);                      // treat is as object with special origin
        } else if (valid_call(p_call)) {              // accept real AX.25 calls
            strcpy(call,p_call);
        } else {
            ok = 0;
            if (debug_level & 1) {
                char filtered_data[MAX_LINE_SIZE + 1];
                strcpy(filtered_data,p_call);
                makePrintable(filtered_data);
                printf("extract_third_party: invalid call: %s\n",filtered_data);
            }
        }
    }
    return(ok);
}



/*
 *  Extract text inserted by TNC X-1J4 from start of info line
 */
void extract_TNC_text(char *info) {
    int i,j,len;

    if (strncasecmp(info,"thenet ",7) == 0) {   // 1st match
        len = strlen(info)-1;
        for (i=7;i<len;i++) {
            if (info[i] == ')')
                break;
        }
        len++;
        if (i>7 && info[i] == ')' && info[i+1] == ' ') {        // found
            i += 2;
            for (j=0;i<=len;i++,j++) {
                info[j] = info[i];
            }
        }
    }
}



/*
 *  Decode AX.25 line
 *  \r and \n should already be stripped from end of line
 *  line should not be NULL
 *
 * If dbadd is set, add to database.  Otherwise, just return true/false
 * to indicate whether input is valid AX25 line.
 */
int decode_ax25_line(char *line, char from, int port, int dbadd) {
    char *call_sign;
    char *path0;
    char path[100+1];           // new one, we may add an '*'
    char *info;
    char call[MAX_CALL+1];
    char origin[MAX_CALL+1];
    int ok;
    int third_party;
        char backup[MAX_LINE_SIZE+1];

        strcpy(backup, line);

    if (debug_level & 1) {
        char filtered_data[MAX_LINE_SIZE+1];
        strcpy(filtered_data, line);
        makePrintable(filtered_data);
        printf("decode_ax25_line: start parsing %s\n", filtered_data);
    }

    if (line == NULL) {
        printf("decode_ax25_line: line == NULL.\n");
        return(FALSE);
    }
    if ( (line != NULL) && (strlen(line) > MAX_TNC_LINE_SIZE) ) { // Overly long message, throw it away.  We're done.
        if (debug_level & 1)
            printf("decode_ax25_line: LONG packet.  Dumping it.\n");
        return(FALSE);
    }

    if (line[strlen(line)-1] == '\n')           // better: look at other places,
                                                // so that we don't get it here...
        line[strlen(line)-1] = '\0';            // Wipe out '\n', to be sure
    if (line[strlen(line)-1] == '\r')
        line[strlen(line)-1] = '\0';            // Wipe out '\r'

    call_sign   = NULL;
    path0       = NULL;
    info        = NULL;
    origin[0]   = '\0';
    call[0]     = '\0';
    path[0]     = '\0';
    third_party = 0;

    // CALL>PATH:APRS-INFO-FIELD                // split line into components
    //     ^    ^
    ok = 0;
    call_sign = strtok(line,">");               // extract call from AX.25 line
    if (call_sign != NULL) {
        path0 = strtok(NULL,":");               // extract path from AX.25 line
        if (path0 != NULL) {
            info = strtok(NULL,"");             // rest is info_field
            if (info != NULL) {
                if ((info - path0) < 100) {     // check if path could be copied
                    ok = 1;                     // we have found all three components
                }
            }
        }
    }

    if (ok) {
        strcpy(path,path0);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers

        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];
            strcpy(filtered_data,path);
            makePrintable(filtered_data);
            printf("decode_ax25_line: invalid path: %s\n",filtered_data);
        }
    }

    if (ok) {
        extract_TNC_text(info);                 // extract leading text from TNC X-1J4
        if (strlen(info) > 256)                 // first check if information field conforms to APRS specs
            ok = 0;                             // drop packets too long
        if ((debug_level & 1) && !ok) {
            char filtered_data[MAX_LINE_SIZE + 1];
            strcpy(filtered_data,info);
            makePrintable(filtered_data);
            printf("decode_ax25_line: info field too long: %s\n",filtered_data);
        }
    }

    if (ok) {                                                   // check callsign
        (void)remove_trailing_asterisk(call_sign);              // is an asterisk valid here ???
        if (valid_inet_name(call_sign,info,origin)) {           // accept some of the names used in internet
            strcpy(call,call_sign);                             // treat is as object with special origin
        } else if (valid_call(call_sign)) {                     // accept real AX.25 calls
            strcpy(call,call_sign);
        } else {
            ok = 0;
            if (debug_level & 1) {
                char filtered_data[MAX_LINE_SIZE + 1];
                strcpy(filtered_data,call_sign);
                makePrintable(filtered_data);
                printf("decode_ax25_line: invalid call: %s\n",filtered_data);
            }
        }
    }

    if (!dbadd)
    {
        return(ok);
    }

    if (ok && info[0] == '}') {                                 // look for third-party traffic
        ok = extract_third_party(call,path,&info,origin);       // extract third-party data
        third_party = 1;
    }

    if (ok && (info[0] == ';' || info[0] == ')')) {             // look for objects or items
        strcpy(origin,call);
        ok = extract_object(call,&info,origin);                 // extract object data
    }

    if (ok) {
        // decode APRS information field, always called with valid call and path
        // info is a string with 0 - 256 bytes
        // printf("dec: %s (%s) %s\n",call,origin,info);
        if (debug_level & 1) {
            char filtered_data[MAX_LINE_SIZE+80];
            sprintf(filtered_data,
                "Registering data %s %s %s %s %c %d %d\n",
                call, path, info, origin, from, port, third_party);
            makePrintable(filtered_data);
            printf("c/p/i/o fr pt tp: %s", filtered_data);
        }
        decode_info_field(call,path,info,origin,from,port,third_party);
    }

    if (debug_level & 1)
        printf("decode_ax25_line: exiting\n");

    return(ok);
}
    


            

/*
 *  Read a line from file
 */
void  read_file_line(FILE *f) {
    char line[MAX_LINE_SIZE+1];
    char cin;
    int pos;

    pos = 0;
    strcpy(line,"");
    while (!feof(f)) {
        if (fread(&cin,1,1,f) == 1) {
            if (cin != (char)10 && cin != (char)13) {   // normal characters
                if (pos < MAX_LINE_SIZE) {
                    line[pos++] = cin;
                 }
            } else {                                    // CR or LF
                if (cin == (char)10) {                  // Found LF as EOL char
                    line[pos] = '\0';                   // Always add a terminating zero after last char
                    pos = 0;                            // start next line
                    decode_ax25_line(line,'F',-1, 1);   // Decode the packet
                    return;                             // only read line by line
                }
            }
        }
    }
    if (feof(f)) {                                      // Close file if at the end
        (void)fclose(f);
        read_file = 0;
        statusline(langcode("BBARSTA012"),0);           // File done..
        redraw_on_new_data = 2;                         // redraw immediately after finish
    }
}





/*
 *  Center map to new position
 */
void set_map_position(Widget w, long lat, long lon) {
    // see also map_pos() in location.c
    set_last_position();
    mid_y_lat_offset  = lat;
    mid_x_long_offset = lon;
    setup_in_view();  // flag all stations in new screen view
    create_image(w);
    (void)XCopyArea(XtDisplay(w),pixmap_final,XtWindow(w),gc,0,0,screen_width,screen_height,0,0);
}





/*
 *  Search for a station to be located (for Tracking and Find Station)
 */
int locate_station(Widget w, char *call, int follow_case, int get_match, int center_map) {
    DataRow *p_station;
    char call_find[MAX_CALLSIGN+1];
    char call_find1[MAX_CALLSIGN+1];
    int ii;
    int call_len;

    call_len = 0;
    if (!follow_case) {
        for (ii=0; ii<(int)strlen(call); ii++) {
            if (isalpha((int)call[ii]))
                call_find[ii] = toupper((int)call[ii]);         // Problem with lowercase objects/names!!
            else
                call_find[ii] = call[ii];
        }
        call_find[ii] = '\0';
        strcpy(call_find1,call_find);
    } else
        strcpy(call_find1,call);

    if (search_station_name(&p_station,call_find1,get_match)) {
        if (position_defined(p_station->coord_lat,p_station->coord_lon,0)) {
            if (center_map || !position_on_inner_screen(p_station->coord_lat,p_station->coord_lon))
                 // only change map if really neccessary
                set_map_position(w, p_station->coord_lat, p_station->coord_lon);
            return(1);                  // we found it
        }
    }
    return(0);
}





/*
 *  Look for other stations that the tracked one has gotten close to.
 *  and speak a proximity warning.
 *   TODO: 
 *    - sort matches by distance
 *    - set upper bound on number of matches so we don't speak forever
 *    - use different proximity distances for different station types?
 *    - look for proximity to embedded map objects
 */
void search_tracked_station(DataRow **p_tracked) {
    DataRow *t = (*p_tracked);
    DataRow *curr = NULL;


    if (debug_level & 1) {
        char lat[20],lon[20];
    
        convert_lat_l2s(t->coord_lat,
                        lat,
                        sizeof(lat),
                        CONVERT_HP_NORMAL);
        convert_lon_l2s(t->coord_lon,
                        lon,
                        sizeof(lat),
                        CONVERT_HP_NORMAL);

        printf("Searching for stations close to tracked station %s at %s %s ...\n",
                t->call_sign,lat,lon);
    }

    while (next_station_time(&curr)) {
        if (curr != t && curr->flag&ST_ACTIVE) {
            float distance;
            char bearing[10];
            char station_id[600];

            distance =  (float)calc_distance_course(t->coord_lat,
                                                    t->coord_lon, 
                                                                        curr->coord_lat,
                                                    curr->coord_lon,
                                                                        bearing,
                                                    sizeof(bearing)) * cvt_kn2len;
            if (debug_level & 1) 
                printf("Looking at %s: distance %.3f bearing %s (%s)\n",
                        curr->call_sign,distance,bearing,convert_bearing_to_name(bearing,1));

            /* check ranges (copied from earlier prox alert code, above) */
            if ((distance > atof(prox_min)) && (distance < atof(prox_max))) {
                if (debug_level & 1) 
                        printf(" tracked station is near %s!\n",curr->call_sign);

                if (sound_play_prox_message) {
                        sprintf(station_id,"%s < %.3f %s from %s",t->call_sign, distance,
                                units_english_metric?langcode("UNIOP00004"):langcode("UNIOP00005"),
                                curr->call_sign);
                        statusline(station_id,0);
                        play_sound(sound_command,sound_prox_message);
                }
#ifdef HAVE_FESTIVAL
                if (festival_speak_tracked_proximity_alert) {
                        if (units_english_metric) {
                            if (distance < 1.0)
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign,
                                        (int)(distance * 1760), langcode("SPCHSTR004"),
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else if ((int)((distance * 10) + 0.5) % 10)
                                sprintf(station_id, langcode("SPCHSTR008"), t->call_sign, distance,
                                        langcode("SPCHSTR003"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                            else
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign, (int)(distance + 0.5),
                                        langcode("SPCHSTR003"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                        } else {                /* metric */
                            if (distance < 1.0)
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign,
                                        (int)(distance * 1000), langcode("SPCHSTR002"), 
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else if ((int)((distance * 10) + 0.5) % 10)
                                sprintf(station_id, langcode("SPCHSTR008"), t->call_sign, distance,
                                        langcode("SPCHSTR001"), 
                                        convert_bearing_to_name(bearing,1), curr->call_sign);
                            else
                                sprintf(station_id, langcode("SPCHSTR007"), t->call_sign, (int)(distance + 0.5),
                                        langcode("SPCHSTR001"), convert_bearing_to_name(bearing,1),
                                        curr->call_sign);
                        }
                    if (debug_level & 1) 
                        printf(" %s\n",station_id);
                    SayText(station_id);
                }
#endif  /* HAVE_FESTIVAL */
            }
        }
    }   // end of while
}





/*
 *  Change map position if neccessary while tracking a station
 *      we call it with defined station call and position
 */
void track_station(Widget w, char *call_tracked, DataRow *p_station) {
    int found;
    char *call;
    char call_find[MAX_CALLSIGN+1];
    int ii;
    int call_len;
    long x_ofs, y_ofs;
    long new_lat, new_lon;

    call_len = 0;
    found = 0;
    call = p_station->call_sign;
    if (!track_case) {
        for (ii=0; ii<(int)strlen(call_tracked); ii++) {
            if (isalpha((int)call_tracked[ii]))
                call_find[ii] = toupper((int)call_tracked[ii]);
            else
                call_find[ii] = call_tracked[ii];
        }
        call_find[ii] = '\0';
    } else
        strcpy(call_find,call_tracked);

    if (debug_level & 256)
        printf("track_station(): CALL %s %s %s\n",call_tracked, call_find, call);

    if (track_match) {
        if (strcmp(call_find,call) == 0)                // we want an exact match
            found=1;
    } else {
        found=0;
        call_len=(int)(strlen(call)-strlen(call_find));
        if (strlen(call_find)<=strlen(call)) {
            for (ii=0; ii<=call_len; ii++) {
                if (!track_case) {
                    if (strncasecmp(call_find,call+ii,strlen(call_find)) == 0)
                        found=1;
                } else {
                    if (strncmp(call_find,call+ii,strlen(call_find)) == 0)
                        found=1;
                }
            }
        }
    }

    if(found) {                                         // we want to track this station
        new_lat = p_station->coord_lat;                 // center map to station position as default
        new_lon = p_station->coord_lon;
        x_ofs = new_lon - mid_x_long_offset;            // current offset from screen center
        y_ofs = new_lat - mid_y_lat_offset;
        if ((labs(x_ofs) > (screen_width*scale_x/3)) || (labs(y_ofs) > (screen_height*scale_y/3))) {
            // only redraw map if near border (margin 1/6 of screen at each side)
            if (labs(y_ofs) < (screen_height*scale_y/2))
                new_lat  += y_ofs/2;                    // give more space in driving direction
            if (labs(x_ofs) < (screen_width*scale_x/2))
                new_lon += x_ofs/2;
            set_map_position(w, new_lat, new_lon);      // center map to new position
        }
        search_tracked_station(&p_station);
    }
}





//WE7U6
// Make sure to look at the "transmit_compressed_posit" variable
// to decide whether to send a compressed packet.
/*
 *  Create the transmit string for Objects/Items.
 *  Input is a DataRow struct, output is both an integer that says
 *  whether it completed successfully, and a char* that has the
 *  output tx string in it.
 */
int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length) {
    int i, done;
    char lat_str[MAX_LAT];
    char lon_str[MAX_LONG];
    char comment[43+1];                 // max 43 characters of comment
    char time[7+1];
    struct tm *day_time;
    time_t sec;
    char complete_area_color[3];
    int complete_area_type;
    int lat_offset, lon_offset;
    char complete_corridor[6];
    char altitude[10];
    char speed_course[8];
    int temp;
    long temp2;
    char signpost[6];
    int bearing;
    char tempstr[50];
    char object_group;
    char object_symbol;


    (void)remove_trailing_spaces(p_station->call_sign);
    //(void)to_upper(p_station->call_sign);     Not per spec.  Don't use this.

    if ((p_station->flag & ST_OBJECT) != 0) {   // We have an object
        if (!valid_object(p_station->call_sign)) {
            line[0] = '\0';
            return(0);
        }
    }
    else if ((p_station->flag & ST_ITEM) != 0) {    // We have an item
        strcpy(tempstr,p_station->call_sign);
        if (strlen(tempstr) == 1)  // Add two spaces (to make 3 minimum chars)
            //sprintf(p_station->call_sign,"%s  ",tempstr);
            xastir_snprintf(p_station->call_sign, sizeof(p_station->call_sign), "%s  ",tempstr);
        else if (strlen(tempstr) == 2) // Add one space (to make 3 minimum chars)
            //sprintf(p_station->call_sign,"%s ",tempstr);
            xastir_snprintf(p_station->call_sign, sizeof(p_station->call_sign), "%s ",tempstr);

        if (!valid_item(p_station->call_sign)) {
            line[0] = '\0';
            return(0);
        }
    }
    else {  // Not an item or an object, what are we doing here!
        line[0] = '\0';
        return(0);
    }


    // Lat/lon are in Xastir coordinates, so we need to convert
    // them to APRS string format here.
    convert_lat_l2s(p_station->coord_lat, lat_str, sizeof(lat_str), CONVERT_LP_NOSP);
    convert_lon_l2s(p_station->coord_lon, lon_str, sizeof(lon_str), CONVERT_LP_NOSP);


    object_group = p_station->aprs_symbol.aprs_type;
    object_symbol = p_station->aprs_symbol.aprs_symbol;

    strcpy(comment,p_station->comments);
    (void)remove_trailing_spaces(comment);


    // This is for objects only, not items.  Uses current time but
    // should use the transmitted time from the DataRow struct.
    // Which time field in the struct would that be?  Have to find out
    // from the extract_?? code.
    if ((p_station->flag & ST_OBJECT) != 0) {
        sec = sec_now();
        day_time = gmtime(&sec);
        //sprintf(time,"%02d%02d%02dz",day_time->tm_mday,day_time->tm_hour,day_time->tm_min);
        xastir_snprintf(time, sizeof(time), "%02d%02d%02dz",day_time->tm_mday,day_time->tm_hour,day_time->tm_min);
    }


// Handle Generic Options


    // Speed/Course Fields
    //sprintf(speed_course,".../");   // Start with invalid-data string
    xastir_snprintf(speed_course, sizeof(speed_course), ".../");   // Start with invalid-data string
    if (strlen(p_station->course) != 0) {    // Course was entered
        // Need to check for 1 to three digits only, and 001-360 degrees)
        temp = atoi(p_station->course);
        if ( (temp >= 1) && (temp <= 360) ) {
            //sprintf(speed_course,"%03d/",temp);
            xastir_snprintf(speed_course, sizeof(speed_course), "%03d/",temp);
        }
        else if (temp == 0) {   // Spec says 001 to 360 degrees...
            //sprintf(speed_course,"360/");
            xastir_snprintf(speed_course, sizeof(speed_course), "360/");
        }
    }
    if (strlen(p_station->speed) != 0) { // Speed was entered (we only handle knots currently)
        // Need to check for 1 to three digits, no alpha characters
        temp = atoi(p_station->speed);
        if ( (temp >= 0) && (temp <= 999) ) {
            //sprintf(tempstr,"%03d",temp);
            xastir_snprintf(tempstr, sizeof(tempstr), "%03d",temp);
            strcat(speed_course, tempstr);
        }
        else {
            strcat(speed_course, "...");
        }
    }
    else {  // No speed entered, blank it out
        strcat(speed_course, "...");
    }
    if ( (speed_course[0] == '.') && (speed_course[4] == '.') ) {
        speed_course[0] = '\0'; // No speed or course entered, so blank it
    }
    if (p_station->aprs_symbol.area_object.type != AREA_NONE) {  // It's an area object
        speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
    }

    // Altitude Field
    altitude[0] = '\0'; // Start with empty string
    if (strlen(p_station->altitude) != 0) {   // Altitude was entered (we only handle feet currently)
        // Need to check for all digits, and 1 to 6 digits
        if (isdigit((int)p_station->altitude[0])) {
            // Must convert from meters to feet before transmitting
            temp2 = (int)( (atof(p_station->altitude) / 0.3048) + 0.5);
            if ( (temp2 >= 0) && (temp2 <= 999999) ) {
                //sprintf(altitude,"/A=%06ld",temp2);
                xastir_snprintf(altitude, sizeof(altitude), "/A=%06ld",temp2);
            }
        }
    }


// Handle Specific Options


    // Area Objects
    if (p_station->aprs_symbol.area_object.type != AREA_NONE) { // It's an area object

        // Note that transmitted color consists of two characters, from "/0" to "15"
        //sprintf(complete_area_color, "%02d", p_station->aprs_symbol.area_object.color);
        xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02d", p_station->aprs_symbol.area_object.color);
        if (complete_area_color[0] == '0')
            complete_area_color[0] = '/';

        complete_area_type = p_station->aprs_symbol.area_object.type;

        lat_offset = p_station->aprs_symbol.area_object.sqrt_lat_off;
        lon_offset = p_station->aprs_symbol.area_object.sqrt_lon_off;

        // Corridor
        complete_corridor[0] = '\0';
        if ( (complete_area_type == 1) || (complete_area_type == 6) ) {
            if (p_station->aprs_symbol.area_object.corridor_width > 0) {
                //sprintf(complete_corridor, "{%d}",
                //    p_station->aprs_symbol.area_object.corridor_width);
                xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}",
                        p_station->aprs_symbol.area_object.corridor_width);
            }
        }

        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                complete_area_type,
                lat_offset,
                complete_area_color,
                lon_offset,
                speed_course,
                complete_corridor,
                altitude);
        }
        else  {     // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                complete_area_type,
                lat_offset,
                complete_area_color,
                lon_offset,
                speed_course,
                complete_corridor,
                altitude);
        }
    }

    else if ( (p_station->aprs_symbol.aprs_type == '\\') // We have a signpost object
            && (p_station->aprs_symbol.aprs_symbol == 'm' ) ) {
        if (strlen(p_station->signpost) > 0) {
            xastir_snprintf(signpost, sizeof(signpost), "{%s}", p_station->signpost);
        }
        else {  // No signpost data entered, blank it out
            signpost[0] = '\0';
        }
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude,
                signpost);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude,
                signpost);
        }
    }

    else if (p_station->signal_gain[0] != '\0') { // Must be an Omni-DF object/item
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                p_station->signal_gain,
                speed_course,
                altitude);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                p_station->signal_gain,
                speed_course,
                altitude);
        }
    }
    else if (p_station->NRQ[0] != 0) {  // It's a Beam Heading DFS object/item

        if (strlen(speed_course) != 7)
                strcpy(speed_course,"000/000");

        bearing = atoi(p_station->bearing);
        if ( (bearing < 1) || (bearing > 360) )
                bearing = 360;

        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%03i/%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                bearing,
                p_station->NRQ,
                altitude);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%03i/%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                bearing,
                p_station->NRQ,
                altitude);
        }
    }

    else {  // Else it's a normal object/item
        if ((p_station->flag & ST_OBJECT) != 0) {   // It's an object
            xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s",
                p_station->call_sign,
                time,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude);
        }
        else {  // It's an item
            xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s",
                p_station->call_sign,
                lat_str,
                object_group,
                lon_str,
                object_symbol,
                speed_course,
                altitude);
        }
    }

    // If it's a "killed" object, change '*' to an '_'
    if ((p_station->flag & ST_OBJECT) != 0) {               // It's an object
        if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE) {   // It's been killed
            line[10] = '_';
        }
    }
    // If it's a "killed" item, change '!' to an '_'
    else {                                                  // It's an item
        if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE) {   // It's been killed
            done = 0;
            i = 0;
            while ( (!done) && (i < 11) ) {
                if (line[i] == '!') {
                    line[i] = '_';          // mark as deleted object
                    done++;                 // Exit from loop
                }
                i++;
            }
        }
    }
 
    // We need to tack the comment on the end, but need to make
    // sure we don't go over the maximum length for an object/item.
    //printf("Comment: %s\n",comment);
    if (strlen(comment) != 0) {
        temp = 0;
        if ((p_station->flag & ST_OBJECT) != 0) {
            while ( (strlen(line) < 80) && (temp < (int)strlen(comment)) ) {
                //printf("temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
                line[strlen(line) + 1] = '\0';
                line[strlen(line)] = comment[temp++];
            }
        }
        else {  // It's an item
            while ( (strlen(line) < (64 + strlen(p_station->call_sign))) && (temp < (int)strlen(comment)) ) {
                //printf("temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
                line[strlen(line) + 1] = '\0';
                line[strlen(line)] = comment[temp++];
            }
        }
    }

    //printf("line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for objects/items.

    return(1);
}





//WE7U6
// check_and_transmit_objects_items
//
// This function first checks the time.  If it is time to do a retransmit,
// it runs through the received stations list looking for objects/items that
// are locally-owned, any found are retransmitted.
//
// It would be nice to transmit killed objects/items for a short period of
// time and then mark them as finished.  Need to add a counter to the struct
// for keeping track of expiration times.  This would be a good place to
// implement auto-expiration of objects that's been discussed on the mailing
// lists.  Note that object/items will never expire currently, 'cuz they keep
// getting received on at least the loopback interface, updating their imes.
//
// This function depends on the local loopback that is in interface.c.  If we
// don't hear & decode our own packets, we won't have our own objects/items in
// our list.
//
// We need to check DataRow objects for ST_OBJECT or ST_ITEM types that were
// transmitted by our callsign & SSID.  We might also need to modify the
// remove_time() and check_station_remove functions in order not to delete
// our own objects/items too quickly.
//
// insert_time/remove_time/next_station_time/prev_station_time
//
// It would be nice if the create/modify object dialog and this routine went
// through the same functions to create the transmitted packets:
//      main.c:Setup_object_data
//      main.c:Setup_item_data
// Unfortunately those routines snag their data directly from the dialog.
// In order to make them use the same code we'd have to separate out the
// fetch-from-dialog code from the create-transmit-packet code.
//
void check_and_transmit_objects_items(time_t time) {
    DataRow *p_station;     // pointer to station data
    char line[256];


    // Time to re-transmit objects/items?
    if (sec_now() < (last_object_check + POSIT_rate) ) // Check every POSIT_rate seconds
        return;

    if (debug_level & 1)
        printf("Retransmitting objects/items now\n");

    last_object_check = sec_now();

    p_station = n_first;    // Go through received stations alphabetically
    while (p_station != NULL) {

        //printf("%s\t%s\n",p_station->call_sign,p_station->origin);

        if (is_my_call(p_station->origin,1)) {   // If station is owned by me
            if ( ((p_station->flag & ST_OBJECT) != 0)           // And it's an object
                    || ((p_station->flag & ST_ITEM) != 0) ) {   // or an item

                if (debug_level & 1)
                    printf("Found a locally-owned object or item: %s\n",p_station->call_sign);

                // Here we need to re-assemble and re-transmit the object or item
                // Check whether it is a "live" or "killed" object and vary the
                // number of retransmits accordingly.  Actually we should be able
                // to keep retransmitting "killed" objects until they expire out of
                // our station queue with no problems.  If someone wants to ressurect
                // the object we'll get new info into our struct and this function will
                // ignore that object from then on, unless we again snatch control of
                // the object.

                // if signpost, area object, df object, or generic object
                // check p_station->APRS_Symbol->aprs_type:
                //   APRS_OBJECT
                //   APRS_ITEM
                //   APRS_DF (looks like I didn't use this one when I implemented DF objects)

                //   Whether area, df, signpost.
                // Check ->signpost for signpost data.  Check ->df_color also.

                // call_sign, sec_heard, coord_lon, coord_lat, packet_time, origin,
                // aprs_symbol, pos_time, altitude, speed, course, bearing, NRQ,
                // power_gain, signal_gain, signpost, station_time, station_time_type,
                // comments, df_color
                Create_object_item_tx_string(p_station, line, sizeof(line));

                // Attempt to transmit the object/item again
                if (object_tx_disable) {
                    output_my_data(line,-1,0,1);    // Local loopback only
                }
                else {
                    output_my_data(line,-1,0,0);    // Transmit/loopback object data
                }
            }
        }
        p_station = p_station->n_next;  // Get next station
    }
}


