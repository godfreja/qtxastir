/*
 * $Id: fcc_data.c,v 1.4 2003/02/04 04:08:37 jtwilley Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include <Xm/XmAll.h>

#include "fcc_data.h"
#include "xa_config.h"
#include "main.h"
#include "xastir.h"





char *call_only(char *callsign) {
    int i;

    for (i=0; i<(int)strlen(callsign); i++) {
         if (!isalnum((int)callsign[i])) {
            callsign[i]='\0';
            i=strlen(callsign)+1;
        }
    }
    return(callsign);
}





/* ====================================================================    */
/*    build a new (or newer if I check the file date) index file    */
/*    check for current ic index file                    */
/*    FG: added a date check in case the FCC file has been updated.    */
/*      appl.dat must have a time stamp newer than the index file time  */
/*      stamp. Use the touch command on the appl.dat file to make the   */
/*      time current if necessary.                                      */
//    How this works:  The index file contains a few callsigns and their
//    offsets into the large database file.  The code uses these as
//    jump-off points to look for a particular call, to speed things up.
/* ******************************************************************** */
int build_fcc_index(int type){
    FILE *fdb;
    FILE *fndx;
    unsigned long call_offset = 0;
    unsigned long x = 0;
    char fccdata[FCC_DATA_LEN+8];
    char database_name[100];
    int found,i,num;

    if (type==1)
        strcpy(database_name,"fcc/appl.dat");
    else
        strcpy(database_name,"fcc/EN.dat");

    /* ====================================================================    */
    /*    If the index file is there, exit                */
    /*                                    */
    if (filethere(get_user_base_dir("data/appl.ndx"))) {
        /* if file is there make sure the index date is newer */
        if (file_time(get_data_base_dir(database_name))<=file_time(get_user_base_dir("data/appl.ndx")))
            return(1);
        else {
            statusline("FCC index old, rebuilding",1);
//            XmTextFieldSetString(text,"FCC index old, rebuilding");
//            XtManageChild(text);
//            XmUpdateDisplay(XtParent(text));     // DK7IN: do we need this ???
        }
    }

    /* ====================================================================    */
    /*    Open the database and index file                */
    /*                                    */
    fdb=fopen(get_data_base_dir(database_name),"rb");
    if (fdb==NULL){
        fprintf(stderr,"Build:Could not open FCC data base: %s\n", get_data_base_dir(database_name) );
        return(0);
    }

    fndx=fopen(get_user_base_dir("data/appl.ndx"),"w");
    if (fndx==NULL){
        fprintf(stderr,"Build:Could not open/create FCC data base index: %s\n", get_user_base_dir("data/appl.ndx") );
        (void)fclose(fdb);
        return(0);
    }

    /* ====================================================================    */
    /*    write out the current callsign and RBA of the db file         */
    /*    skip (index_skip) records and do it again until no more        */
    /*                                    */
    strncpy (fccdata," ",sizeof(fccdata));
    while(!feof(fdb)) {
        call_offset = (unsigned long)ftell(fdb);
        (void)fgets(fccdata, (int)sizeof(fccdata), fdb);
        found=0;
        num=0;
        if (type==2) {
            for(i=0;i<14 && !found;i++) {
                if(fccdata[i]=='|') {
                    num++;
                    if(num==4)
                        found=i+1;
                }
            }
        }
        (void)call_only(fccdata+found);
        fprintf(fndx,"%-6.6s%li\n",fccdata+found,call_offset+found);
        for (x=0;x<=500 && !feof(fdb);x++) {
            if (fgets(fccdata, (int)sizeof(fccdata), fdb)==NULL)
                break;
        }
    }
    (void)fclose(fdb);
    (void)fclose(fndx);

//    XmTextFieldSetString(text,"");
//    XtManageChild(text);
    return(1);
}





/* ====================================================================    */
/*    Check for ic data base file                    */
/*    Check/build the index                        */
/*                                    */
/* ******************************************************************** */
int check_fcc_data(void) {
    int fcc_data_available = 0;
    if (filethere(get_data_base_dir("fcc/EN.dat")) && filethere(get_data_base_dir("fcc/appl.dat"))) {
        if(file_time(get_data_base_dir("fcc/appl.dat"))<=file_time(get_data_base_dir("fcc/EN.dat"))) {
            /*fprintf(stderr,"NEW FORMAT FCC DATA FILE is NEWER THAN OLD FCC FORMAT\n");*/
            if (build_fcc_index(2))
                fcc_data_available=2;
            else {
                fprintf(stderr,"Check:Could not build fcc data base index\n");
                fcc_data_available=0;
            }
        } else {
            /*fprintf(stderr,"OLD FORMAT FCC DATA FILE is NEWER THAN NEW FCC FORMAT\n");*/
            if (build_fcc_index(1))
                fcc_data_available=1;
            else {
                fprintf(stderr,"Check:Could not build fcc data base index\n");
                fcc_data_available=0;
            }
        }
    } else {
        if (filethere(get_data_base_dir("fcc/EN.dat"))) {
            /*fprintf(stderr,"NO OLD FCC, BUT NEW FORMAT FCC DATA AVAILABLE\n");*/
            if (build_fcc_index(2))
                fcc_data_available=2;
            else {
                fprintf(stderr,"Check:Could not build fcc data base index\n");
                fcc_data_available=0;
            }
        } else {
            if (filethere(get_data_base_dir("fcc/appl.dat"))) {
                /*fprintf(stderr,"NO NEW FCC, BUT OLD FORMAT FCC DATA AVAILABLE\n");*/
                if (build_fcc_index(1))
                    fcc_data_available=1;
                else {
                    fprintf(stderr,"Check:Could not build fcc data base index\n");
                    fcc_data_available=0;
                }
            }
        }
    }
    return(fcc_data_available);
}





int search_fcc_data_appl(char *callsign, FccAppl *data) {
    FILE *f;
    char line[200];
    int line_pos;
    char data_in[16385];
    int found, xx, bytes_read;
    char temp[15];
    int len;
    int which;
    int i,ii;
    int pos_it;
    int llen;
    char calltemp[8];
    int pos,ix,num;
    FILE *fndx;
    long call_offset = 0;
    char char_offset[16];
    char index[32];

    strcpy(data->id_file_num,"");
    strcpy(data->type_purpose,"");
    data->type_applicant=' ';
    strcpy(data->name_licensee,"");
    strcpy(data->text_street,"");
    strcpy(data->text_pobox,"");
    strcpy(data->city,"");
    strcpy(data->state,"");
    strcpy(data->zipcode,"");
    strcpy(data->date_issue,"");
    strcpy(data->date_expire,"");
    strcpy(data->date_last_change,"");
    strcpy(data->id_examiner,"");
    data->renewal_notice=' ';
    strcpy(temp,callsign);
    (void)call_only(temp);

    xastir_snprintf(calltemp, sizeof(calltemp), "%-6.6s", temp);
// calltemp doesn't appear to get used anywhere...

    /* add end of field data */
    strcat(temp,"|");
    len=(int)strlen(temp);
    found=0;
    line_pos=0;
    /* check the database again */
    which = check_fcc_data();

    // Check for first letter of a U.S. callsign
    if (! (callsign[0] == 'A' || callsign[0] == 'K' || callsign[0] == 'N' || callsign[0] == 'W') )
        return(0);  // Not found

    // ====================================================================
    // Search thru the index, get the RBA 
    // 
    // This gives us a jumping-off point to start looking in the right
    // neighborhood for the callsign of interest.
    //
    fndx=fopen(get_user_base_dir("data/appl.ndx"),"r");
    if (fndx!=NULL){
        (void)fgets(index,(int)sizeof(index),fndx);
        strncpy(char_offset,&index[6],16);

        // Search through the indexes looking for a callsign which is
        // close to the callsign of interest.  If callsign is later in
        // the alphabet than the current index, snag the next index.
        while (!feof(fndx) && strncmp(callsign,index,6) > 0) {
            strncpy(char_offset,&index[6],16);
            (void)fgets(index,(int)sizeof(index),fndx);
        }
    } else {
        fprintf(stderr,"Search:Could not open FCC data base index: %s\n", get_user_base_dir("data/appl.ndx") );
        return (0);
    }
    call_offset = atol(char_offset);

    (void)fclose(fndx);

    /* ====================================================================    */
    /*    Continue with the original search                */
    /*                                                                */

    f=NULL;
    switch (which) {
        case(1):
            f=fopen(get_data_base_dir("fcc/appl.dat"),"r");
            break;

        case(2):
            f=fopen(get_data_base_dir("fcc/EN.dat"),"r");
            break;

        default:
            break;
    }
    if (f!=NULL) {
        (void)fseek(f, call_offset,SEEK_SET);
        while (!feof(f) && !found) {
            bytes_read=(int)fread(data_in,1,16384,f);
            if (bytes_read>0) {
                for (xx=0;(xx<bytes_read) && !found;xx++) {
                    if(data_in[xx]!='\n' && data_in[xx]!='\r') {
                        if (line_pos<200) {
                            line[line_pos++]=data_in[xx];
                            line[line_pos]='\0';
                        }
                    } else {
                        line_pos=0;
                        /*fprintf(stderr,"line:%s\n",line);*/
                        pos=0;
                        num=0;
                        if (which==2) {
                            for (ix=0;ix<14 && !pos;ix++) {
                                if (line[ix]=='|') {
                                    num++;
                                    if (num==4)
                                        pos=ix+1;
                                }
                            }
                        }
                        if (strncmp(line+pos,temp,(size_t)len)==0) {
                            found=1;
                            /*fprintf(stderr,"line:%s\n",line);*/
                            llen=(int)strlen(line);
                            /* replace "|" with 0 */
                            for (ii=pos;ii<llen;ii++) {
                                if (line[ii]=='|')
                                    line[ii]='\0';
                            }
                            pos_it=pos;
                            for (i=0; i<15; i++) {
                                for (ii=pos_it;ii<llen;ii++) {
                                    if (line[ii]=='\0') {
                                        pos_it=ii;
                                        ii=llen+1;
                                    }
                                }
                                pos_it++;
                                if (line[pos_it]!='\0') {
                                    /*fprintf(stderr,"DATA %d %d:%s\n",i,pos_it,line+pos_it);*/
                                    switch (which) {
                                        case(1):
                                            switch(i) {
                                                case(0):
                                                    strcpy(data->id_file_num,line+pos_it);
                                                    break;

                                                case(1):
                                                    strcpy(data->type_purpose,line+pos_it);
                                                    break;

                                                case(2):
                                                    data->type_applicant=line[pos_it];
                                                    break;

                                                case(3):
                                                    strcpy(data->name_licensee,line+pos_it);
                                                    break;

                                                case(4):
                                                    strcpy(data->text_street,line+pos_it);
                                                    break;

                                                case(5):
                                                    strcpy(data->text_pobox,line+pos_it);
                                                    break;

                                                case(6):
                                                    strcpy(data->city,line+pos_it);
                                                    break;

                                                case(7):
                                                    strcpy(data->state,line+pos_it);
                                                    break;

                                                case(8):
                                                    strcpy(data->zipcode,line+pos_it);
                                                    break;

                                                case(9):
                                                    strcpy(data->date_issue,line+pos_it);
                                                    break;

                                                case(11):
                                                    strcpy(data->date_expire,line+pos_it);
                                                    break;

                                                case(12):
                                                    strcpy(data->date_last_change,line+pos_it);
                                                    break;

                                                case(13):
                                                    strcpy(data->id_examiner,line+pos_it);
                                                    break;

                                                case(14):
                                                    data->renewal_notice=line[pos_it];
                                                    break;

                                                default:
                                                    break;
                                            }
                                            break;

                                        case(2):
                                            switch (i) {
                                                case(0):
                                                    strcpy(data->id_file_num,line+pos_it);
                                                    break;

                                                case(2):
                                                    strcpy(data->name_licensee,line+pos_it);
                                                    break;

                                                case(10):
                                                    strcpy(data->text_street,line+pos_it);
                                                    break;

                                                case(11):
                                                    strcpy(data->city,line+pos_it);
                                                    break;

                                                case(12):
                                                    strcpy(data->state,line+pos_it);
                                                    break;

                                                case(13):
                                                    strcpy(data->zipcode,line+pos_it);
                                                    break;

                                                default:
                                                    break;
                                            }
                                            break;

                                        default:
                                            break;
                                    }
                                }
                            }
                        }
                        else {
                            // Check whether we passed the alphabetic
                            // location for the callsign.  Return if so.
                            if ( (temp[0] < line[pos]) ||
                                    ( (temp[0] == line[pos]) && (temp[1] < line[pos+1]) ) ) {
                                popup_message("Callsign Search", "Callsign Not Found!");
                                //fprintf(stderr,"%c%c\t%c%c\n",temp[0],temp[1],line[pos],line[pos+1]);
                                (void)fclose(f);
                                return(0);
                            }
                        }
                    }
                }
            }
        }
        (void)fclose(f);
    } else {
        fprintf(stderr,"Could not open FCC appl data base at: %s\n", get_data_base_dir("fcc/") );
    }
    return(found);
}


