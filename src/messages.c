/*
 * $Id: messages.c,v 1.3 2002/03/05 21:28:24 we7u Exp $
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

#include <Xm/XmAll.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "xastir.h"
#include "main.h"
#include "db.h"
#include "messages.h"
#include "util.h"
#include "interface.h"


char group_data_file[400];
char *group_data_list = NULL;   // Need this NULL for Solaris!
int  group_data_count;
int  group_data_max;

int message_counter;

int auto_reply;
char auto_reply_message[100];

Message_Window mw[MAX_MESSAGE_WINDOWS+1];

Message_transmit message_pool[MAX_OUTGOING_MESSAGES+1];





void clear_message_windows(void) {
    int i;

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

    for (i = 0;  i < MAX_MESSAGE_WINDOWS; i++) {

        if (mw[i].send_message_dialog)
            XtDestroyWidget(mw[i].send_message_dialog);

        mw[i].send_message_dialog = (Widget)NULL;
        strcpy(mw[i].to_call_sign,"");
        mw[i].send_message_call_data = (Widget)NULL;
        mw[i].send_message_message_data = (Widget)NULL;
        mw[i].send_message_text = (Widget)NULL;
    }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_message_windows" );

}





static int group_comp(const void *a, const void *b) {
    if (!*(char *)a)
      return ((int)(*(char *)b != '\0'));
    return strcasecmp(a, b);
}





void group_build_list(char *filename) {
    char *ptr;
    FILE *f;
    struct stat group_stat;

    if (group_data_count == group_data_max) {
        ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);

        if (ptr) {
            group_data_list = ptr;
            group_data_max += 10;
        }
    }
    strcpy(&group_data_list[0], my_callsign);
    strcpy(&group_data_list[10], "XASTIR");
    group_data_count = 2;

    if (! stat(filename, &group_stat) )
        f = fopen(filename, "r");   // File exists
    else
        f = fopen(filename, "w+");  // No file.  Create it and open it.

    if (f == NULL) {
        printf("Couldn't open file for reading -or- appending: %s\n", filename);
        return;
    }

    while (!feof(f)) {
        if (group_data_count == group_data_max) {
            ptr = realloc(group_data_list, (size_t)(group_data_max+10)*10);
            if (ptr) {
                group_data_list = ptr;
                group_data_max += 10;
            }
        }
        if (group_data_count < group_data_max) {
            group_data_list[group_data_count*10] = '\0';
            (void)fgets(&group_data_list[group_data_count*10], 10, f);
            if ((ptr = strchr(&group_data_list[group_data_count*10], '\n')))
                *ptr = '\0';

            if (group_data_list[group_data_count*10])
                group_data_count++;
        }
    }
    (void)fclose(f);
    qsort(group_data_list, (size_t)group_data_count, 10, group_comp);
}





static int group_active(char *from) {
    static struct stat current_group_stat;
    struct stat group_stat;
    (void)remove_trailing_spaces(from);
    if (!stat(group_data_file, &group_stat) && (current_group_stat.st_size != group_stat.st_size ||
    current_group_stat.st_mtime != group_stat.st_mtime || current_group_stat.st_ctime != group_stat.st_ctime)) {
    group_build_list(group_data_file);
    current_group_stat = group_stat;
    }
    if (group_data_list != NULL)        // Causes segfault on Solaris 2.5 without this!
        return (int)(bsearch(from, group_data_list, (size_t)group_data_count, (size_t)10, group_comp) != NULL);
    else
        return(0);
}





int look_for_open_group_data(char *to) {
    int i,found;
    char temp1[MAX_CALLSIGN+1];

begin_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

    found = FALSE;
     for(i = 0; i < MAX_MESSAGE_WINDOWS; i++) {
        /* find station  */
        if(mw[i].send_message_dialog != NULL) {
            strcpy(temp1,XmTextFieldGetString(mw[i].send_message_call_data));
            (void)to_upper(temp1);
            /*printf("Looking at call <%s> for <%s>\n",temp1,to);*/
            if(strcmp(temp1,to)==0) {
                found=(int)TRUE;
                break;
            }
        }
    }

end_critical_section(&send_message_dialog_lock, "messages.c:look_for_open_group_data" );

    return(found);
}





int check_popup_window(char *from_call_sign, int group) {
    int i,found,j,ret;
    char temp1[MAX_CALLSIGN+1];


    ret =- 1;
    found =- 1;

begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

    for (i = 0; i < MAX_MESSAGE_WINDOWS; i++) {
        /* find station  */
        if (mw[i].send_message_dialog != NULL) {
            strcpy(temp1,XmTextFieldGetString(mw[i].send_message_call_data));
            /*printf("Looking at call <%s> for <%s>\n",temp1,from_call_sign);*/
            if (strcasecmp(temp1, from_call_sign) == 0) {
                found = i;
                break;
            }
        }
    }

end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window" );

    if (found == -1 && (group == 2 || group_active(from_call_sign))) {
        /* no window found Open one! */

begin_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

       i=-1;
        for (j=0; j<MAX_MESSAGE_WINDOWS; j++) {
            if (!mw[j].send_message_dialog) {
                i=j;
                break;
            }
        }

end_critical_section(&send_message_dialog_lock, "messages.c:check_popup_window2" );

        if (i!=-1) {
        if (group == 1) {
        temp1[0] = '*';
        temp1[1] = '\0';
        } else
            temp1[0] = '\0';

        strcat(temp1, from_call_sign);
            Send_message(appshell, temp1, NULL);
            ret=i;
        } else
            printf("No open windows!\n");
    } else
        /* window open! */
        ret=found;

    return(ret);
}





void clear_outgoing_message(int i) {
    message_pool[i].active=MESSAGE_CLEAR;
    strcpy(message_pool[i].to_call_sign,"");
    strcpy(message_pool[i].from_call_sign,"");
    strcpy(message_pool[i].message_line,"");
    strcpy(message_pool[i].seq,"");
    message_pool[i].active_time=0;;
    message_pool[i].next_time=0l;
    message_pool[i].tries=0;
}





void reset_outgoing_messages(void) {
    int i;

    for(i=0;i<MAX_OUTGOING_MESSAGES;i++)
        clear_outgoing_message(i);
}





void clear_outgoing_messages(void) {
    int i;

    for (i=0;i<MAX_OUTGOING_MESSAGES;i++)
        clear_outgoing_message(i);

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

    /* clear message send buttons */
    for (i=0;i<MAX_MESSAGE_WINDOWS;i++) {
        /* find station  */
        if (mw[i].send_message_dialog!=NULL) /* clear submit */
            XtSetSensitive(mw[i].button_ok,TRUE);

    }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_outgoing_messages" );

}





void output_message(char *from, char *to, char *message) {
    int ok,i,j;
    char message_out[MAX_MESSAGE_OUTPUT_LENGTH+1];
    int last_space, message_ptr, space_loc;
    int wait_on_first_ack;
    int error;

    message_ptr=0;
    last_space=0;
    ok=0;
    error=0;
    if (debug_level & 2)
        printf("Output Message from <%s>  to <%s>\n",from,to);

    while (!error && (message_ptr < (int)strlen(message))) {
        ok=0;
        space_loc=0;
        for (j=0;j<MAX_MESSAGE_OUTPUT_LENGTH;j++) {
            if(message[j+message_ptr] != '\0') {
                if(message[j+message_ptr]==' ') {
                    last_space=j+message_ptr+1;
                    space_loc=j;
                }
                if ((j+1)!=MAX_MESSAGE_OUTPUT_LENGTH) {
                    message_out[j]=message[j+message_ptr];
                    message_out[j+1] = '\0';
                } else {
                    if(space_loc!=0)
                        message_out[space_loc] = '\0';
                    else
                        last_space=j+message_ptr;
                }
            } else {
                j=MAX_MESSAGE_OUTPUT_LENGTH+1;
                last_space=strlen(message)+1;
            }
        }
        if (debug_level & 2)
            printf("MESSAGE <%s> %d %d\n",message_out,message_ptr,last_space);

        message_ptr=last_space;
        /* check for others in the queue */
        wait_on_first_ack=0;
        for (i=0; i<MAX_OUTGOING_MESSAGES; i++) {
            if (message_pool[i].active == MESSAGE_ACTIVE &&
        strcmp(to, message_pool[i].to_call_sign) == 0 && strcmp(from, "***") != 0) {
                    wait_on_first_ack=1;
                    i=MAX_OUTGOING_MESSAGES+1;
        }
        }
        for (i=0; i<MAX_OUTGOING_MESSAGES && !ok ;i++) {
            /* Check for clear position*/
            if (message_pool[i].active==MESSAGE_CLEAR) {
                /* found a spot */
                ok=1;
                message_counter++;
                if (message_counter > 99999)
                    message_counter=0;

                message_pool[i].active = MESSAGE_ACTIVE;
                message_pool[i].wait_on_first_ack = wait_on_first_ack;
                strcpy(message_pool[i].to_call_sign,to);
                strcpy(message_pool[i].from_call_sign,from);
                strcpy(message_pool[i].message_line,message_out);
                xastir_snprintf(message_pool[i].seq, sizeof(message_pool[i].seq),
                        "%d", message_counter);
                message_pool[i].active_time=0;
                message_pool[i].next_time= (time_t)30l; /* was 120 -FG */
                if (strcmp(from,"***")!= 0)
                    message_pool[i].tries = 0;
                else
                    message_pool[i].tries = MAX_TRIES-1;
            }
        }
        if(!ok) {
            printf("Output message queue is full!\n");
            error=1;
        }
    }
}





void transmit_message_data(char *to, char *message) {
    DataRow *p_station;

    if (debug_level & 2)
        printf("Transmitting data to %s : %s\n",to,message);

    p_station = NULL;
    if (search_station_name(&p_station,to,1)) {
        if (debug_level & 2)
            printf("found station %s\n",p_station->call_sign);

        if (strcmp(to,my_callsign)!=0) {
            /* check to see if it was heard via a TNC otherwise send it to the last port heard */
            if ((p_station->flag & ST_VIATNC) != 0) {        // heard via TNC ?
                output_my_data(message,p_station->heard_via_tnc_port,0,0);
                /* station heard via tnc but in the past hour? */
                /* if not heard with in the hour try via net */
                if (!heard_via_tnc_in_past_hour(to)) {
                    if (p_station->data_via==DATA_VIA_NET) {
                        /* try last port herd */
                        output_my_data(message,p_station->last_port_heard,0,0);
                    }
                }
            } else {
                /* if not a TNC then a NET port? */
                if (p_station->data_via==DATA_VIA_NET) {
                    /* try last port herd */
                    output_my_data(message,p_station->last_port_heard,0,0);
                } else {
                    /* Not a TNC or a NET try all possible */
                    if (debug_level & 2)
                        printf("VIA any way\n");

                    output_my_data(message,-1,0,0);
                }
            }
        } else {
            /* my station message */
            /* try all possible */
            if (debug_level & 2)
                printf("My call VIA any way\n");

            output_my_data(message,-1,0,0);
        }
    } else {
        /* no data found try every way*/
        /* try all possible */
        if (debug_level & 2)
            printf("VIA any way\n");

        output_my_data(message,-1,0,0);
    }
}





void check_and_transmit_messages(time_t time) {
    int i;
    char temp[200];
    char to_call[40];

    for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
        if (message_pool[i].active==MESSAGE_ACTIVE) {
            if (message_pool[i].wait_on_first_ack!=1) {
                if (message_pool[i].active_time<time) {
                    /* sending message let the tnc and net transmits check to see if we should */
                    if (debug_level & 2)
                        printf("Time %ld Active time %ld next time %ld\n",(long)time,(long)message_pool[i].active_time,(long)message_pool[i].next_time);

                    if (debug_level & 2)
                        printf("Send message#%d to <%s> from <%s>:%s-%s\n",
                            message_pool[i].tries,message_pool[i].to_call_sign,message_pool[i].from_call_sign,message_pool[i].message_line,message_pool[i].seq);

                    pad_callsign(to_call,message_pool[i].to_call_sign);
                    /* Add Leading ":" as per APRS Spec */
                    xastir_snprintf(temp, sizeof(temp), ":%s:%s{%s",
                            to_call, message_pool[i].message_line,message_pool[i].seq);

                    if (debug_level & 2)
                        printf("MESSAGE OUT>%s<\n",temp);

                    transmit_message_data(message_pool[i].to_call_sign,temp);
                    message_pool[i].active_time=time+message_pool[i].next_time;
                    message_pool[i].next_time+=30; /*was (message_pool[i].next_time*2) -FG */
                    if (message_pool[i].next_time > (time_t)600l)
                        message_pool[i].next_time = (time_t)600l;

                    message_pool[i].tries++;
                    if (message_pool[i].tries > MAX_TRIES)
                        clear_outgoing_message(i);
                }
            } else {
                if (debug_level & 2)
                    printf("Message #%s is waiting to have a previous one cleared\n",message_pool[i].seq);
            }
        }
    }
}





void clear_acked_message(char *from, char *to, char *seq) {
    int i,ii;
    int found;
    int lowest;
    char temp1[MAX_CALLSIGN+1];


    (void)remove_trailing_spaces(seq);  // This is IMPORTANT here!!!

    lowest=100000;
    found=-1;
    for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
        if (message_pool[i].active==MESSAGE_ACTIVE) {

            if (debug_level & 1)
                printf("TO <%s> <%s> from <%s> <%s> seq <%s> <%s>\n",to,message_pool[i].to_call_sign,from,message_pool[i].from_call_sign,seq,message_pool[i].seq);

            if (strcmp(message_pool[i].to_call_sign,from)==0) {
                if (debug_level & 1)
                    printf("Matched message to_call_sign\n");
                if (strcmp(message_pool[i].from_call_sign,to)==0) {
                    if (debug_level & 1)
                        printf("Matched message from_call_sign\n");
                    if (strcmp(message_pool[i].seq,seq)==0) {
                        if (debug_level & 2)
                            printf("Found and cleared\n");

                        clear_outgoing_message(i);
                        /* now find and release next message, look for the lowest sequence? */
                        for (i=0; i<MAX_OUTGOING_MESSAGES;i++) {
                            if (message_pool[i].active==MESSAGE_ACTIVE) {
                                if (strcmp(message_pool[i].to_call_sign,from)==0) {
                                    if (atoi(message_pool[i].seq)<lowest) {
                                        lowest=atoi(message_pool[i].seq);
                                        found=i;
                                    }
                                }
                            }
                        }
                        /* now release that message */
                        if (found!=-1)
                            message_pool[found].wait_on_first_ack=0;
                        else {
                            /* if no more clear the send button */

begin_critical_section(&send_message_dialog_lock, "messages.c:clear_acked_message" );

                            for (ii=0;ii<MAX_MESSAGE_WINDOWS;ii++) {
                                /* find station  */
                                if (mw[ii].send_message_dialog!=NULL) {
                                    strcpy(temp1,XmTextFieldGetString(mw[ii].send_message_call_data));
                                    (void)to_upper(temp1);
                                    if (strcmp(temp1,from)==0) {
                                        /*clear submit*/
                                        XtSetSensitive(mw[ii].button_ok,TRUE);
                                    }
                                }
                            }

end_critical_section(&send_message_dialog_lock, "messages.c:clear_acked_message" );

                        }
                    }
                    else {
                        if (debug_level & 1)
                            printf("Sequences didn't match: %s %s\n",message_pool[i].seq,seq);
                    }
                }
            }
        }
    }
}





void send_queued(char *to) {
    int i;

    for (i=0; i<MAX_OUTGOING_MESSAGES ;i++) {
        /* Check for messages to call */
        if (message_pool[i].active==MESSAGE_ACTIVE)
            if (strcmp(to,message_pool[i].to_call_sign)==0)
                message_pool[i].active_time=0;

    }
}


