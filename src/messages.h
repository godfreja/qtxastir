/*
 * $Id: messages.h,v 1.1 2002/02/02 03:19:15 kg4ijb Exp $
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

#ifndef XASTIR_MESSAGES_H
#define XASTIR_MESSAGES_H

/*
 * Message structures
 *
 */

/* define MESSAGE STATUS */
#define MESSAGE_ACTIVE  'A'
#define MESSAGE_CLEAR 'C'

#define MAX_OUTGOING_MESSAGES 100
#define MAX_MESSAGE_OUTPUT_LENGTH 64
#define MAX_MESSAGE_ORDER 10
#define MAX_TRIES 10
#define MAX_CALLSIGN 20

typedef struct {
    char active;
    char to_call_sign[MAX_CALLSIGN+1];
    char from_call_sign[MAX_CALLSIGN+1];
    char message_line[MAX_MESSAGE_OUTPUT_LENGTH+1];
    char seq[MAX_MESSAGE_ORDER+1];
    time_t active_time;
    time_t next_time;
    int tries;
    int wait_on_first_ack;
} Message_transmit;

#define MAX_MESSAGE_WINDOWS 10

typedef struct {
    char win[10];
    char to_call_sign[MAX_CALLSIGN+1];
    int message_group;
    Widget send_message_dialog;
    Widget send_message_call_data;
    Widget send_message_message_data;
    Widget send_message_text;
    Widget pane, form, button_ok, button_cancel, button_clear_msg, button_submit_call , call, message ;
} Message_Window;


extern Widget auto_msg_on, auto_msg_off;

extern int auto_reply;
extern char auto_reply_message[100];
extern char group_data_file[];

extern void clear_acked_message(char *from, char *to, char *seq);
extern void transmit_message_data(char *to, char *message);
extern void output_message(char *from, char *to, char *message);
extern int check_popup_window(char *from_call_sign, int group);
extern int look_for_open_group_data(char *to);
extern void send_queued(char *to);


/* from messages_gui.c */
extern xastir_mutex send_message_dialog_lock;
extern void messages_gui_init(void);
extern void Send_message(Widget w, XtPointer clientData, XtPointer callData);
extern void Clear_messages(Widget w, XtPointer clientData, XtPointer callData);
extern void Send_message_call(Widget w, XtPointer clientData, XtPointer callData);

// view_message_gui.c
extern int vm_range;
extern int view_message_limit;

#endif  /*  XASTIR_MESSAGES_H */

