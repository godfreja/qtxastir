/*
 * $Id: geocoder_gui.c,v 1.4 2004/03/30 20:44:06 we7u Exp $
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

#include "config.h"
#include "snprintf.h"

#include <stdio.h>

#include <Xm/XmAll.h>
#ifdef HAVE_XBAE_MATRIX_H
#include <Xbae/Matrix.h>
#endif  // HAVE_XBAE_MATRIX_H

#include "main.h"
#include "xastir.h"
#include "lang.h"
#include "maps.h"
#include "io.h"
#include "geo.h"

Widget geocoder_place_dialog = (Widget)NULL;
Widget geocoder_zip_data = (Widget)NULL;
Widget geocoder_state_data = (Widget)NULL;
Widget geocoder_locality_data = (Widget)NULL;
Widget geocoder_address_data = (Widget)NULL;
Widget geocoder_map_file_data = (Widget)NULL;
char geocoder_zip_name[50];
char geocoder_state_name[50];
char geocoder_locality_name[50];
char geocoder_address_name[200];
char geocoder_map_filename[200];
static xastir_mutex geocoder_place_dialog_lock;

long destination_coord_lat = 0;
long destination_coord_lon = 0;
int mark_destination = 0;
int show_destination_mark = 0;





void geocoder_gui_init(void)
{
    init_critical_section( &geocoder_place_dialog_lock );
    strcpy(geocoder_zip_name,"");
    strcpy(geocoder_state_name,"");
    strcpy(geocoder_locality_name,"");
    strcpy(geocoder_address_name,"");
}





/**** GEOCODER FIND PLACE ******/

void Geocoder_place_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&geocoder_place_dialog_lock, "geocoder_gui.c:Geocoder_place_destroy_shell" );

    XtDestroyWidget(shell);
    geocoder_place_dialog = (Widget)NULL;

end_critical_section(&geocoder_place_dialog_lock, "geocoder_gui.c:Geocoder_place_destroy_shell" );

}





/*
 *  Geocoder a place by centering the map at its position
 */
void Geocoder_place_now(Widget w, XtPointer clientData, XtPointer callData) {
    struct io_file *index;
    struct geo_location loc;
    char input[256];

    /* find place and go there */
    strcpy(geocoder_zip_name, XmTextFieldGetString(geocoder_zip_data));
    strcpy(geocoder_state_name, XmTextFieldGetString(geocoder_state_data));
    strcpy(geocoder_locality_name, XmTextFieldGetString(geocoder_locality_data));
    strcpy(geocoder_address_name, XmTextFieldGetString(geocoder_address_data));
    strcpy(geocoder_map_filename, XmTextFieldGetString(geocoder_map_file_data));

    (void)remove_trailing_spaces(geocoder_zip_name);
    (void)remove_trailing_spaces(geocoder_state_name);
    (void)remove_trailing_spaces(geocoder_locality_name);
    (void)remove_trailing_spaces(geocoder_address_name);

    index = io_open(geocoder_map_filename);
    xastir_snprintf(input, 255, "%s %s%s%s %s", geocoder_address_name, geocoder_locality_name, (strlen(geocoder_state_name) != 0)?",":"", geocoder_state_name, geocoder_zip_name);
    if (geo_find(index,input,strlen(input),&loc)) {
        long coord_lon, coord_lat;
        char lat_str[20];
        char long_str[20];
        int lon, lat, lons, lats, tmp1;
        char lond = 'E';
        char latd = 'N';
        double res, tmp;

        if (loc.at.longitude < 0) {
            loc.at.longitude = -loc.at.longitude;
            lond = 'W';
        }
        if (loc.at.latitude < 0) {
            loc.at.latitude = -loc.at.latitude;
            latd = 'S';
        }

        lon = loc.at.longitude;
        lat = loc.at.latitude;

        res = loc.at.longitude - lon;
        tmp = (res * 100); 
        tmp = tmp * 60 / 100;
        tmp1 = tmp;
        lon = (lon * 100) + tmp1;
        lons = (tmp - tmp1) * 100;

        res = loc.at.latitude - lat;
        tmp = (res * 100); 
        tmp = tmp * 60 / 100;
        tmp1 = tmp;
        lat = (lat * 100) + tmp1;
        lats = (tmp - tmp1) * 100;

        xastir_snprintf(lat_str, sizeof(lat_str), "%d.%02d%c", lat, lats, latd);
        coord_lat = convert_lat_s2l(lat_str);
        xastir_snprintf(long_str, sizeof(long_str), "%s%d.%02d%c", (lon < 10000)?"0":"", lon, lons, lond);
        coord_lon = convert_lon_s2l(long_str);

        destination_coord_lat = coord_lat;
        destination_coord_lon = coord_lon;
        mark_destination = 1;

        popup_message( langcode("POPEM00029"), geocoder_address_name );
        set_map_position(w, coord_lat, coord_lon);
    }
    else {
        popup_message(langcode("POPEM00025"),geocoder_address_name);
    }
    Geocoder_place_destroy_shell(w, clientData, callData);
}





void  Show_dest_toggle( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        show_destination_mark = atoi(which);
    }
    else {
        show_destination_mark = 0;
    }
}





void Geocoder_place(/*@unused@*/ Widget w, /*@unused@*/ XtPointer clientData, /*@unused@*/ XtPointer callData) {
    static Widget pane, form, button_ok, button_cancel, sep,
        zip, state, locality, address, map_file, show_dest_toggle;
    Atom delw;

    if (!geocoder_place_dialog) {

begin_critical_section(&geocoder_place_dialog_lock, "geocoder_gui.c:Geocoder_place" );

        geocoder_place_dialog = XtVaCreatePopupShell(langcode("PULDNMP014"),
                xmDialogShellWidgetClass,Global.top,
                XmNdeleteResponse,XmDESTROY,
                XmNdefaultPosition, FALSE,
                NULL);

        pane = XtVaCreateWidget("Geocoder_place pane",xmPanedWindowWidgetClass, geocoder_place_dialog,
                XmNbackground, colors[0xff],
                NULL);

        form =  XtVaCreateWidget("Geocoder_place form",xmFormWidgetClass, pane,
                XmNfractionBase, 2,
                XmNbackground, colors[0xff],
                XmNautoUnmanage, FALSE,
                XmNshadowThickness, 1,
                NULL);

//        address = XtVaCreateManagedWidget(langcode("FEATURE004"),xmLabelWidgetClass, form,
        address = XtVaCreateManagedWidget("Address:",xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        geocoder_address_data = XtVaCreateManagedWidget("Geocoder_address_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_FORM,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, address,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

//        locality = XtVaCreateManagedWidget(langcode("FEATURE003"),xmLabelWidgetClass, form,
        locality = XtVaCreateManagedWidget("City:",xmLabelWidgetClass, form,
 
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, address,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        geocoder_locality_data = XtVaCreateManagedWidget("Geocoder_address_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 32,
                XmNwidth, ((32*7)+2),
                XmNmaxLength, 30,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, address,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, locality,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        state = XtVaCreateManagedWidget(langcode("FEATURE002"),xmLabelWidgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locality,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        geocoder_state_data = XtVaCreateManagedWidget("Geocoder_state_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 4,
                XmNwidth, ((4*7)+2),
                XmNmaxLength, 2,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, locality,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, state,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

//        show_dest_toggle = XtVaCreateManagedWidget(langcode("FEATURE002"),xmToggleButtonGadgetClass, form,
        show_dest_toggle = XtVaCreateManagedWidget("Mark Destination",xmToggleButtonGadgetClass, form,
                XmNvisibleWhenOff, TRUE,
                XmNindicatorSize, 12,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, locality,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, geocoder_state_data,
                XmNleftOffset, 20,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                MY_FOREGROUND_COLOR,
                MY_BACKGROUND_COLOR,
                NULL);

        XtAddCallback(show_dest_toggle,XmNvalueChangedCallback,Show_dest_toggle,"1");
        if (show_destination_mark)
            XmToggleButtonSetState(show_dest_toggle,TRUE,FALSE);


//        zip = XtVaCreateManagedWidget(langcode("FEATURE001"),xmLabelWidgetClass, form,
        zip = XtVaCreateManagedWidget("Zip Code:",xmLabelWidgetClass, form,
 
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, state,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        geocoder_zip_data = XtVaCreateManagedWidget("Geocoder_zip_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 12,
                XmNwidth, ((12*7)+2),
                XmNmaxLength, 10,
                XmNbackground, colors[0x0f],
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, state,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, zip,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_NONE,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

//        map_file = XtVaCreateManagedWidget(langcode("FEATURE006"),xmLabelWidgetClass, form,
        map_file = XtVaCreateManagedWidget("Geocoding File",xmLabelWidgetClass, form,
 
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, zip,
                XmNtopOffset, 10,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNleftOffset, 10,
                XmNrightAttachment, XmATTACH_NONE,
                XmNbackground, colors[0xff],
                NULL);

        geocoder_map_file_data = XtVaCreateManagedWidget("geocoder_map_file_data", xmTextFieldWidgetClass, form,
                XmNeditable,   TRUE,
                XmNcursorPositionVisible, TRUE,
                XmNsensitive, TRUE,
                XmNshadowThickness,    1,
                XmNcolumns, 40,
                XmNwidth, ((40*7)+2),
                XmNmaxLength, 199,
                XmNbackground, colors[0x0f],
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget, zip,
                XmNtopOffset, 5,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNleftWidget, map_file,
                XmNleftOffset, 10,
                XmNrightAttachment,XmATTACH_FORM,
                XmNrightOffset, 10,
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);


        sep = XtVaCreateManagedWidget("Geocoder_place sep", xmSeparatorGadgetClass,form,
                XmNorientation, XmHORIZONTAL,
                XmNtopAttachment,XmATTACH_WIDGET,
                XmNtopWidget,geocoder_map_file_data,
                XmNtopOffset, 10,
                XmNbottomAttachment,XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment,XmATTACH_FORM,
                XmNbackground, colors[0xff],
                NULL);

        button_ok = XtVaCreateManagedWidget(langcode("WPUPLSP005"),xmPushButtonGadgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 0,
                XmNleftOffset, 5,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 1,
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, form,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, sep,
                XmNtopOffset, 5,
                XmNbottomAttachment, XmATTACH_FORM,
                XmNbottomOffset, 5,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 2,
                XmNrightOffset, 5,
                XmNbackground, colors[0xff],
                XmNnavigationType, XmTAB_GROUP,
                XmNtraversalOn, TRUE,
                NULL);

        XtAddCallback(button_ok, XmNactivateCallback, Geocoder_place_now, geocoder_place_dialog);
        XtAddCallback(button_cancel, XmNactivateCallback, Geocoder_place_destroy_shell, geocoder_place_dialog);

        XmTextFieldSetString(geocoder_zip_data,geocoder_zip_name);
        XmTextFieldSetString(geocoder_state_data,geocoder_state_name);
        XmTextFieldSetString(geocoder_locality_data,geocoder_locality_name);
        XmTextFieldSetString(geocoder_address_data,geocoder_address_name);
        XmTextFieldSetString(geocoder_map_file_data,geocoder_map_filename);

        pos_dialog(geocoder_place_dialog);

        delw = XmInternAtom(XtDisplay(geocoder_place_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(geocoder_place_dialog, delw, Geocoder_place_destroy_shell, (XtPointer)geocoder_place_dialog);

        XtManageChild(form);
        XtManageChild(pane);

end_critical_section(&geocoder_place_dialog_lock, "geocoder_gui.c:Geocoder_place" );

        XtPopup(geocoder_place_dialog,XtGrabNone);
        fix_dialog_size(geocoder_place_dialog);

        XmProcessTraversal(button_ok, XmTRAVERSE_CURRENT);

    } else {
        (void)XRaiseWindow(XtDisplay(geocoder_place_dialog), XtWindow(geocoder_place_dialog));
    }
}


