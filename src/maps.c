/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id: maps.c,v 1.311 2003/07/23 21:34:41 we7u Exp $
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
 *
 *
 * Please see separate copyright notice attached to the
 * SHPRingDir_2d() function in this file.
 *
 */

//#define MAP_SCALE_CHECK
//#define FUZZYRASTER


#include "config.h"
#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>

// Needed for Solaris
#include <strings.h>

#ifdef HAVE_IMAGEMAGICK
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME
#undef RETSIGTYPE
/* JMT - stupid ImageMagick */
#define XASTIR_PACKAGE_BUGREPORT PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#define XASTIR_PACKAGE_NAME PACKAGE_NAME
#undef PACKAGE_NAME
#define XASTIR_PACKAGE_STRING PACKAGE_STRING
#undef PACKAGE_STRING
#define XASTIR_PACKAGE_TARNAME PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#define XASTIR_PACKAGE_VERSION PACKAGE_VERSION
#undef PACKAGE_VERSION
#include <magick/api.h>
#undef PACKAGE_BUGREPORT
#define PACKAGE_BUGREPORT XASTIR_PACKAGE_BUGREPORT
#undef XASTIR_PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#define PACKAGE_NAME XASTIR_PACKAGE_NAME
#undef XASTIR_PACKAGE_NAME
#undef PACKAGE_STRING
#define PACKAGE_STRING XASTIR_PACKAGE_STRING
#undef XASTIR_PACKAGE_STRING
#undef PACKAGE_TARNAME
#define PACKAGE_TARNAME XASTIR_PACKAGE_TARNAME
#undef XASTIR_PACKAGE_TARNAME
#undef PACKAGE_VERSION
#define PACKAGE_VERSION XASTIR_PACKAGE_VERSION
#undef XASTIR_PACKAGE_VERSION
#endif // HAVE_IMAGEMAGICK

#include <dirent.h>
#include <netinet/in.h>
#include <Xm/XmAll.h>

#ifdef HAVE_X11_XPM_H
#include <X11/xpm.h>
#ifdef HAVE_LIBXPM // if we have both, prefer the extra library
#undef HAVE_XM_XPMI_H
#endif // HAVE_LIBXPM
#endif // HAVE_X11_XPM_H

#ifdef HAVE_XM_XPMI_H
#include <Xm/XpmI.h>
#endif // HAVE_XM_XPMI_H

#include <X11/Xlib.h>

#include <math.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

struct FtpFile {
  char *filename;
  FILE *stream;
};
#endif

#include "xastir.h"
#include "maps.h"
#include "alert.h"
#include "util.h"
#include "main.h"
#include "datum.h"
#include "draw_symbols.h"
#include "rotated.h"
#include "color.h"
#include "xa_config.h"

#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


// Check for XPM and/or ImageMagick.  We use "NO_GRAPHICS"
// to disable some routines below if the support for them
// is not compiled in.
#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM) || defined(HAVE_IMAGEMAGICK))
  #define NO_GRAPHICS 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM || HAVE_IMAGEMAGICK)

#if !(defined(HAVE_LIBXPM) || defined(HAVE_LIBXPM_IN_XM))
  #define NO_XPM 1
#endif  // !(HAVE_LIBXPM || HAVE_LIBXPM_IN_XM)



// Print options
Widget print_properties_dialog = (Widget)NULL;
static xastir_mutex print_properties_dialog_lock;
Widget rotate_90 = (Widget)NULL;
Widget auto_rotate = (Widget)NULL;
//char  print_paper_size[20] = "Letter";  // Displayed in dialog, but not used yet.
int   print_rotated = 0;
int   print_auto_rotation = 0;
//float print_scale = 1.0;                // Not used yet.
int   print_auto_scale = 0;
//int   print_blank_background_color = 0; // Not used yet.
int   print_in_monochrome = 0;
int   print_resolution = 150;           // 72 dpi is normal for Postscript.
                                        // 100 or 150 dpi work well with HP printer
int   print_invert = 0;                 // Reverses black/white

time_t last_snapshot = 0;               // Used to determine when to take next snapshot
int doing_snapshot = 0;


int mag;
int npoints;    /* number of points in a line */


float geotiff_map_intensity = 0.65;    // Geotiff map color intensity, set from Maps->Geotiff Map Intensity
float imagemagick_gamma_adjust = 0.0;  // Additional imagemagick map gamma correction, set from Maps->Adjust Gamma

// Storage for the index file timestamp
time_t map_index_timestamp;

int grid_size = 0;

void maps_init(void)
{
    init_critical_section( &print_properties_dialog_lock );
}





/*
 *  Calculate NS distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_y(long x, long y) {

    // approximation by looking at +/- 0.5 minutes offset
    (void)(calc_distance(y-3000, x, y+3000, x)/6000.0);
    // but this scale is fixed at 1852/6000
    return((double)(1852.0/6000.0));
}





/*
 *  Calculate EW distance scale at a given location
 *  in meters per Xastir unit
 */
double calc_dscale_x(long x, long y) {

    // approximation by looking at +/- 0.5 minutes offset
    // we should find a better formula...
    return(calc_distance(y, x-3000, y, x+3000)/6000.0);
}


/*
 *  Calculate x map scaling for current location
 *  With that we could have equal distance scaling or a better
 *  view for pixel maps
 */
long get_x_scale(long x, long y, long ysc) {
    long   xsc;
    double sc_x;
    double sc_y;
    
    sc_x = calc_dscale_x(x,y);          // meter per Xastir unit
    sc_y = calc_dscale_y(x,y);
    if (sc_x < 0.01 || ysc > 50000)
        // keep it near the poles (>88 deg) or if big parts of world seen
        xsc = ysc;
    else
        // adjust y scale, so that the distance is identical in both directions:
        xsc = (long)(ysc * sc_y / sc_x +0.4999);
    
    //fprintf(stderr,"Scale: x %5.3fkm/deg, y %5.3fkm/deg, x %ld y %ld\n",sc_x*360,sc_y*360,xsc,ysc);
    return(xsc);
}





/***********************************************************
 * convert_to_xastir_coordinates()
 *
 * Converts from lat/lon to Xastir coordinate system.
 * First two parameters are the output Xastir X/Y values, 
 * 2nd two are the input floating point lat/lon values.
 *
 *              0 (90 deg. or 90N)
 *
 * 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
 *
 *          64,800,000 (-90 deg. or 90S)
 ***********************************************************/
int convert_to_xastir_coordinates ( unsigned long* x,
                                    unsigned long* y,
                                    float f_longitude,
                                    float f_latitude )
{
    int ok = 1;


    if (f_longitude < -180.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Longitude out-of-range (too low):%f\n",f_longitude);
        ok = 0;
    }

    if (f_longitude >  180.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Longitude out-of-range (too high):%f\n",f_longitude);
        ok = 0;
    }

    if (f_latitude <  -90.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Latitude out-of-range (too low):%f\n",f_latitude);
        ok = 0;
    }

    if (f_latitude >   90.0) {
        fprintf(stderr,"convert_to_xastir_coordinates:Latitude out-of-range (too high):%f\n",f_latitude);
        ok = 0;
    }

    if (ok) {
        *y = (unsigned long)(32400000l + (360000.0 * (-f_latitude)));
        *x = (unsigned long)(64800000l + (360000.0 * f_longitude));
    }

    return(ok);
}



/** MAP DRAWING ROUTINES **/


/**********************************************************
 * draw_grid()
 *
 * Draws a lat/lon grid on top of the view.
 **********************************************************/
void draw_grid(Widget w) {
    int coord;
    unsigned char dash[2];

    if (!long_lat_grid)
        return;

    /* Set the line width in the GC */
    (void)XSetForeground (XtDisplay (w), gc, colors[0x08]);
    (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);

    if (0 /*coordinate_system == USE_UTM*/) {
        // Not yet, just teasing... ;-)
    }
    else { // Not UTM coordinate system, draw some lat/long lines
        unsigned int x,x1,x2;
        unsigned int y,y1,y2;
        unsigned int stepsx[3];
        unsigned int stepsy[3];
        int step;
        stepsx[0] = 72000*100;    stepsy[0] = 36000*100;
        stepsx[1] =  7200*100;    stepsy[1] =  3600*100;
        stepsx[2] =   300*100;    stepsy[2] =   150*100;

        //fprintf(stderr,"scale_x: %ld\n",scale_x);
        step = 0;
        if (scale_x <= 6000) step = 1;
        if (scale_x <= 300)  step = 2;

        step += grid_size;
        if (step < 0) {
            grid_size -= step;
            step = 0;
        }
        else if (step > 2) {
            grid_size -= (step - 2);
            step = 2;
        }

        /* draw vertival lines */
        if (y_lat_offset >= 0)
            y1 = 0;
        else
            y1 = -y_lat_offset/scale_y;

        y2 = (180*60*60*100-y_lat_offset)/scale_y;

        if (y2 > (unsigned int)screen_height)
            y2 = screen_height-1;

        coord = x_long_offset+stepsx[step]-(x_long_offset%stepsx[step]);
        if (coord < 0)
            coord = 0;

        for (; coord < x_long_offset+screen_width*scale_x && coord <= 360*60*60*100; coord += stepsx[step]) {

            x = (coord-x_long_offset)/scale_x;

            if ((coord%(648000*100)) == 0) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x, y1, x, y2);
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                continue;
            }
            else if ((coord%(72000*100)) == 0) {
                dash[0] = dash[1] = 8;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            } else if ((coord%(7200*100)) == 0) {
                dash[0] = dash[1] = 4;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            } else if ((coord%(300*100)) == 0) {
                dash[0] = dash[1] = 2;
                (void)XSetDashes (XtDisplay (w), gc, 0, dash, 2);
            }

            (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x, y1, x, y2);
        }

        /* draw horizontal lines */
        if (x_long_offset >= 0)
            x1 = 0;
        else
            x1 = -x_long_offset/scale_x;

        x2 = (360*60*60*100-x_long_offset)/scale_x;
        if (x2 > (unsigned int)screen_width)
            x2 = screen_width-1;

        coord = y_lat_offset+stepsy[step]-(y_lat_offset%stepsy[step]);
        if (coord < 0)
            coord = 0;

        for (; coord < y_lat_offset+screen_height*scale_y && coord <= 180*60*60*100; coord += stepsy[step]) {

            y = (coord-y_lat_offset)/scale_y;

            if ((coord%(324000*100)) == 0) {
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineSolid, CapButt,JoinMiter);
                (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x1, y, x2, y);
                (void)XSetLineAttributes (XtDisplay (w), gc, 1, LineOnOffDash, CapButt,JoinMiter);
                continue;
            } else if ((coord%(36000*100)) == 0) {
                dash[0] = dash[1] = 8;
                (void)XSetDashes (XtDisplay (w), gc, 4, dash, 2);
            } else if ((coord%(3600*100)) == 0) {
                dash[0] = dash[1] = 4;
                (void)XSetDashes (XtDisplay (w), gc, 2, dash, 2);
            } else if ((coord%(150*100)) == 0) {
                dash[0] = dash[1] = 2;
                (void)XSetDashes (XtDisplay (w), gc, 1, dash, 2);
            }

            (void)XDrawLine (XtDisplay (w), pixmap_final, gc, x1, y, x2, y);
        }
    }
}





/**********************************************************
 * get_map_ext()
 *
 * Returns the extension for the filename.  We use this to
 * determine which sort of map file it is.
 **********************************************************/
char *get_map_ext (char *filename) {
    int len;
    int i;
    char *ext;

    ext = NULL;
    len = (int)strlen (filename);
    for (i = len; i >= 0; i--) {
        if (filename[i] == '.') {
            ext = filename + (i + 1);
            break;
        }
    }
    return (ext);
}





/**********************************************************
 * get_map_dir()
 *
 * Used to snag just the pathname from a complete filename.
 * Modifies input parameter "fullpath".
 **********************************************************/
char *get_map_dir (char *fullpath) {
    int len;
    int i;

    len = (int)strlen (fullpath);
    for (i = len; i >= 0; i--) {
        if (fullpath[i] == '/') {
            fullpath[i + 1] = '\0';
            break;
        }
    }
    return (fullpath);
}



/***********************************************************
 * map_visible()
 *
 * Tests whether a particular path/filename is within our
 * current view.  We use this to decide whether to plot or
 * skip a particular image file (major speed-up!).
 * Input coordinates are in the Xastir coordinate system.
 *
 * Had to fix a bug here where the viewport glanced over the
 * edge of the earth, causing strange results like this.
 * Notice the View Edges Top value is out of range:
 *
 *
 *                Bottom         Top          Left       Right
 * View Edges:  31,017,956  4,290,923,492  35,971,339  90,104,075
 *  Map Edges:  12,818,482     12,655,818  64,079,859  64,357,110
 *
 * Left map boundary inside view
 * Right map boundary inside view
 * map_inside_view: 1  view_inside_map: 0  parallel_edges: 0
 * Map not within current view.
 * Skipping map: /usr/local/share/xastir/maps/tif/uk/425_0525_bng.tif
 *
 *
 * I had to check for out-of-bounds numbers for the viewport and
 * set them to min or max values so that this function always
 * works properly.  Here are the bounds of the earth (Xastir
 * Coordinate System):
 *
 *              0 (90 deg. or 90N)
 *
 * 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
 *
 *          64,800,000 (-90 deg. or 90S)
 *
 ***********************************************************/
int map_visible (unsigned long bottom_map_boundary,
                    unsigned long top_map_boundary,
                    unsigned long left_map_boundary,
                    unsigned long right_map_boundary)
{

    unsigned long view_min_x, view_max_x;
    unsigned long view_min_y, view_max_y;
    int map_inside_view = 0;
    int view_inside_map = 0;
    int parallel_edges = 0;

    view_min_x = (unsigned long)x_long_offset;                         /*   left edge of view */
    if (view_min_x > 129600000ul)
        view_min_x = 0;

    view_max_x = (unsigned long)(x_long_offset + (screen_width * scale_x)); // right edge of view
    if (view_max_x > 129600000ul)
        view_max_x = 129600000ul;

    view_min_y = (unsigned long)y_lat_offset;                          /*    top edge of view */
    if (view_min_y > 64800000ul)
        view_min_y = 0;

    view_max_y = (unsigned long)(y_lat_offset + (screen_height * scale_y)); // bottom edge of view
    if (view_max_y > 64800000ul)
        view_max_y = 64800000ul;

    if (debug_level & 16) {
        fprintf(stderr,"              Bottom     Top       Left     Right\n");

        fprintf(stderr,"View Edges:  %lu  %lu  %lu  %lu\n",
            view_max_y,
            view_min_y,
            view_min_x,
            view_max_x);

        fprintf(stderr," Map Edges:  %lu  %lu  %lu  %lu\n",
            bottom_map_boundary,
            top_map_boundary,
            left_map_boundary,
            right_map_boundary);

        if ((left_map_boundary <= view_max_x) && (left_map_boundary >= view_min_x))
            fprintf(stderr,"Left map boundary inside view\n");

        if ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x))
            fprintf(stderr,"Right map boundary inside view\n");

        if ((top_map_boundary <= view_max_y) && (top_map_boundary >= view_min_y))
            fprintf(stderr,"Top map boundary inside view\n");

        if ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y))
            fprintf(stderr,"Bottom map boundary inside view\n");

        if ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary))
            fprintf(stderr,"Right view boundary inside map\n");

        if ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary))
            fprintf(stderr,"Left view boundary inside map\n");

        if ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary))
            fprintf(stderr,"Bottom view boundary inside map\n");

        if ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary))
            fprintf(stderr,"Top view boundary inside map\n");
    }


    /* In order to determine whether the two rectangles intersect,
    * we need to figure out if any TWO edges of one rectangle are
    * contained inside the edges of the other.
    */

    /* Look for left or right map boundaries inside view */
    if (   (( left_map_boundary <= view_max_x) && ( left_map_boundary >= view_min_x)) ||
            ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x)))
    {
        map_inside_view++;
    }


    /* Look for top or bottom map boundaries inside view */
    if (   ((   top_map_boundary <= view_max_y) && (   top_map_boundary >= view_min_y)) ||
            ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y)))
    {

        map_inside_view++;
    }


    /* Look for right or left view boundaries inside map */
    if (   ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary)) ||
            ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary)))
    {
        view_inside_map++;
    }


    /* Look for top or bottom view boundaries inside map */
    if (   ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary)) ||
        ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary)))
    {
        view_inside_map++;
    }


    /*
    * Look for left/right map boundaries both inside view, but top/bottom
    * of map surround the viewport.  We have a column of the map going
    * through from top to bottom.
    */
    if (   (( left_map_boundary <= view_max_x) && ( left_map_boundary >= view_min_x)) &&
            ((right_map_boundary <= view_max_x) && (right_map_boundary >= view_min_x)) &&
            ((view_max_y <= bottom_map_boundary) && (view_max_y >= top_map_boundary)) &&
            ((view_min_y <= bottom_map_boundary) && (view_min_y >= top_map_boundary)))
    {
        parallel_edges++;
    }


    /*
    * Look for top/bottom map boundaries both inside view, but left/right
    * of map surround the viewport.  We have a row of the map going through
    * from left to right.
    */
    if (   ((   top_map_boundary <= view_max_y) && (   top_map_boundary >= view_min_y)) &&
        ((bottom_map_boundary <= view_max_y) && (bottom_map_boundary >= view_min_y)) &&
        ((view_max_x <= right_map_boundary) && (view_max_x >= left_map_boundary)) &&
        ((view_min_x <= right_map_boundary) && (view_min_x >= left_map_boundary)))
    {
        parallel_edges++;
    }


    if (debug_level & 16)
        fprintf(stderr,"map_inside_view: %d  view_inside_map: %d  parallel_edges: %d\n",
                map_inside_view,
                view_inside_map,
                parallel_edges);

    if ((map_inside_view >= 2) || (view_inside_map >= 2) || (parallel_edges) )
        return (1); /* Draw this pixmap onto the screen */
    else
        return (0); /* Skip this pixmap */
}





/***********************************************************
 * map_visible_lat_lon()
 *
 ***********************************************************/
int map_visible_lat_lon (double f_bottom_map_boundary,
                         double f_top_map_boundary,
                         double f_left_map_boundary,
                         double f_right_map_boundary,
                         char *error_message)
{
    unsigned long bottom_map_boundary,
                  top_map_boundary,
                  left_map_boundary,
                  right_map_boundary;
    int ok, ok2;

    ok = convert_to_xastir_coordinates ( &left_map_boundary,
                                          &top_map_boundary,
                                          (float)f_left_map_boundary,
                                          (float)f_top_map_boundary );

    ok2 = convert_to_xastir_coordinates ( &right_map_boundary,
                                          &bottom_map_boundary,
                                          (float)f_right_map_boundary,
                                          (float)f_bottom_map_boundary );

    if (ok && ok2) {
        return(map_visible( bottom_map_boundary,
                        top_map_boundary,
                        left_map_boundary,
                        right_map_boundary) );
    }
    else {
        fprintf(stderr,"map_visible_lat_lon: problem converting coordinates from lat/lon: %s\n",
            error_message);
        return(0);  // Problem converting, assume that the map is not viewable
    }
}





/**********************************************************
 * draw_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 **********************************************************/
void draw_label_text (Widget w, int x, int y, int label_length, int color, char *label_text) {

    // This draws a gray background rectangle upon which we draw the text.
    // Probably not needed.  It ends up obscuring details underneath.
    //(void)XSetForeground (XtDisplay (w), gc, colors[0x0ff]);
    //(void)XFillRectangle (XtDisplay (w), pixmap, gc, x - 1, (y - 10),(label_length * 6) + 2, 11);

    (void)XSetForeground (XtDisplay (w), gc, color);
    (void)XDrawString (XtDisplay (w), pixmap, gc, x, y, label_text, label_length);
}





// Must make sure that fonts are not loaded again and again, as this
// takes a big chunk of memory each time.  Can you say "memory
// leak"?
//
static XFontStruct *rotated_label_font=NULL;
char *rotated_label_fontname=
    //"-adobe-helvetica-medium-o-normal--24-240-75-75-p-130-iso8859-1";
    //"-adobe-helvetica-medium-o-normal--12-120-75-75-p-67-iso8859-1";
    //"-adobe-helvetica-medium-r-*-*-*-130-*-*-*-*-*-*";
    //"-*-times-bold-r-*-*-13-*-*-*-*-80-*-*";
    //"-*-helvetica-bold-r-*-*-14-*-*-*-*-80-*-*";
    "-*-helvetica-bold-r-*-*-12-*-*-*-*-*-*-*";





/**********************************************************
 * draw_rotated_label_text()
 *
 * Does what it says.  Used to draw strings onto the
 * display.
 *
 * Use "xfontsel" or other tools to figure out what fonts
 * to use here.
 **********************************************************/
void draw_rotated_label_text (Widget w, int rotation, int x, int y, int label_length, int color, char *label_text) {
//    XPoint *corner;
//    int i;
    float my_rotation = (float)((-rotation)-90);


    /* load font */
    if(!rotated_label_font) {
        rotated_label_font=(XFontStruct *)XLoadQueryFont(XtDisplay (w),
                                                rotated_label_fontname);
        if (rotated_label_font == NULL) {    // Couldn't get the font!!!
            fprintf(stderr,"draw_rotated_label_text: Couldn't get font %s\n",
                rotated_label_fontname);
            return;
        }
    }


    // Code to determine the bounding box corner points for the rotated text
//    corner = XRotTextExtents(w,rotated_label_font,my_rotation,x,y,label_text,BLEFT);
//    for (i=0;i<5;i++) {
//        fprintf(stderr,"%d,%d\t",corner[i].x,corner[i].y);
//    }
//    fprintf(stderr,"\n");

    (void)XSetForeground (XtDisplay (w), gc, color);

    //fprintf(stderr,"%0.1f\t%s\n",my_rotation,label_text);

    if (       ( (my_rotation < -90.0) && (my_rotation > -270.0) )
            || ( (my_rotation >  90.0) && (my_rotation <  270.0) ) ) {

        my_rotation = my_rotation + 180.0;
        (void)XRotDrawAlignedString(XtDisplay (w),
            rotated_label_font,
            my_rotation,
            pixmap,
            gc,
            x,
            y,
            label_text,
            BRIGHT);
    }
    else {
        (void)XRotDrawAlignedString(XtDisplay (w),
            rotated_label_font,
            my_rotation,
            pixmap,
            gc,
            x,
            y,
            label_text,
            BLEFT);
    }
}










static void Print_properties_destroy_shell(/*@unused@*/ Widget widget, XtPointer clientData, /*@unused@*/ XtPointer callData) {
    Widget shell = (Widget) clientData;
    XtPopdown(shell);

begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

    XtDestroyWidget(shell);
    print_properties_dialog = (Widget)NULL;

end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties_destroy_shell" );

}





// Print_window:  Prints the drawing area to a Postscript file.
//
static void Print_window( Widget widget, XtPointer clientData, XtPointer callData ) {

#ifdef NO_XPM
    fprintf(stderr,"XPM or ImageMagick support not compiled into Xastir!\n");
#else   // NO_GRAPHICS

#ifndef HAVE_GV
    fprintf(stderr,"GV support not compiled into Xastir!\n");
#else   // HAVE_GV


    char xpm_filename[MAX_FILENAME];
    char ps_filename[MAX_FILENAME];
    char mono[50] = "";
    char invert[50] = "";
    char rotate[50] = "";
    char scale[50] = "";
    char density[50] = "";
    char command[MAX_FILENAME*2];
    char temp[100];
    char format[50] = "-portrait ";


    xastir_snprintf(xpm_filename,
        sizeof(xpm_filename),
        "%s/print.xpm",
        get_user_base_dir("tmp"));

    xastir_snprintf(ps_filename,
        sizeof(ps_filename),
        "%s/print.ps",
        get_user_base_dir("tmp"));

    busy_cursor(appshell);  // Show a busy cursor while we're doing all of this

    // Get rid of Print Properties dialog
    Print_properties_destroy_shell(widget, print_properties_dialog, NULL );


    if ( debug_level & 512 )
        fprintf(stderr,"Creating %s\n", xpm_filename );

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0012") );
    statusline(temp,1);       // Dumping image to file...


    if ( !XpmWriteFileFromPixmap(XtDisplay(appshell),   // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        fprintf(stderr,"ERROR writing %s\n", xpm_filename );
    }
    else {          // We now have the xpm file created on disk

        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, ps_filename );


        // Convert it to a postscript file for printing.  This depends
        // on the ImageMagick command "convert".
        //
        // Other options to try in the future:
        // -label
        //
        if ( print_auto_scale ) {
//            sprintf(scale, "-geometry 612x792 -page 612x792 ");   // "Letter" size at 72 dpi
//            sprintf(scale, "-sample 612x792 -page 612x792 ");     // "Letter" size at 72 dpi
            xastir_snprintf(scale, sizeof(scale), "-page 1275x1650+0+0 "); // "Letter" size at 150 dpi
        }
        else
            scale[0] = '\0';    // Empty string


        if ( print_in_monochrome )
            xastir_snprintf(mono, sizeof(mono), "-monochrome +dither " );    // Monochrome
        else
            xastir_snprintf(mono, sizeof(mono), "+dither ");                // Color


        if ( print_invert )
            xastir_snprintf(invert, sizeof(invert), "-negate " );              // Reverse Colors
        else
            invert[0] = '\0';   // Empty string


        if (debug_level & 512)
            fprintf(stderr,"Width: %ld\tHeight: %ld\n", screen_width, screen_height);


        if ( print_rotated ) {
            xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );
            xastir_snprintf(format, sizeof(format), "-landscape " );
        } else if ( print_auto_rotation ) {
            // Check whether the width or the height of the pixmap is greater.
            // If width is greater than height, rotate the image by 270 degrees.
            if (screen_width > screen_height) {
                xastir_snprintf(rotate, sizeof(rotate), "-rotate -90 " );
                xastir_snprintf(format, sizeof(format), "-landscape " );
                if (debug_level & 512)
                    fprintf(stderr,"Rotating\n");
            } else {
                rotate[0] = '\0';   // Empty string
                if (debug_level & 512)
                    fprintf(stderr,"Not Rotating\n");
            }
        } else {
            rotate[0] = '\0';   // Empty string
            if (debug_level & 512)
                fprintf(stderr,"Not Rotating\n");
        }


        // Higher print densities require more memory and time to process
        xastir_snprintf(density, sizeof(density), "-density %dx%d", print_resolution,
                print_resolution );

        xastir_snprintf(temp, sizeof(temp), langcode("PRINT0013") );
        statusline(temp,1);       // Converting to Postscript...


        // Filters:
        // Point (ok at higher dpi's)
        // Box  (not too bad)
        // Triangle (no)
        // Hermite (no)
        // Hanning (no)
        // Hamming (no)
        // Blackman (better but still not good)
        // Gaussian (no)
        // Quadratic (no)
        // Cubic (no)
        // Catrom (not too bad)
        // Mitchell (no)
        // Lanczos (no)
        // Bessel (no)
        // Sinc (not too bad)

#ifdef HAVE_CONVERT
        xastir_snprintf(command,
            sizeof(command),
            "%s -filter Point %s%s%s%s%s %s %s",
            CONVERT_PATH,
            mono,
            invert,
            rotate,
            scale,
            density,
            xpm_filename,
            ps_filename );
        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command );

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't convert from XPM to PS!\n\n\n");
            return;
        }
#endif  // HAVE_CONVERT

        chmod( ps_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        // Delete temporary xpm file
        if ( !(debug_level & 512) )
            unlink( xpm_filename );

        if ( debug_level & 512 )
            fprintf(stderr,"Printing postscript file %s\n", ps_filename);

// Note: This needs to be changed to "lp" for Solaris.
// Also need to have a field to configure the printer name.  One
// fill-in field could do both.
//
// Since we could be running SUID root, we don't want to be
// calling "system" anyway.  Several problems with it.
/*
        xastir_snprintf(command,
            sizeof(command),
            "%s -Plp %s",
            LPR_PATH,
            ps_filename );
        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command);

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't send to the printer!\n\n\n");
            return;
        }
*/


#ifdef HAVE_GV
        // Bring up the "gv" postscript viewer
        xastir_snprintf(command,
            sizeof(command),
//            "%s %s-scale -2 -media Letter %s &",
            "%s %s-scale -2 %s &",
            GV_PATH,
            format,
            ps_filename );

        if ( debug_level & 512 )
            fprintf(stderr,"%s\n", command);

        if ( system( command ) != 0 ) {
            fprintf(stderr,"\n\nPrint: Couldn't bring up the gv viewer!\n\n\n");
            return;
        }
#endif  // HAVE_GV

/*
        if ( !(debug_level & 512) )
            unlink( ps_filename );
*/

        if ( debug_level & 512 )
            fprintf(stderr,"  Done printing.\n");
    }

    xastir_snprintf(temp, sizeof(temp), langcode("PRINT0014") );
    statusline(temp,1);       // Finished creating print file.

    //popup_message( langcode("PRINT0015"), langcode("PRINT0014") );

#endif  // HAVE_GV
#endif // NO_XPM

}





/*
 *  Auto_rotate
 *
 */
static void  Auto_rotate( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_auto_rotation = atoi(which);
        print_rotated = 0;
        XmToggleButtonSetState(rotate_90, FALSE, FALSE);
    } else {
        print_auto_rotation = 0;
    }
}





/*
 *  Rotate_90
 *
 */
static void  Rotate_90( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_rotated = atoi(which);
        print_auto_rotation = 0;
        XmToggleButtonSetState(auto_rotate, FALSE, FALSE);
    } else {
        print_rotated = 0;
    }
}





/*
 *  Auto_scale
 *
 */
static void  Auto_scale( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_auto_scale = atoi(which);
    } else {
        print_auto_scale = 0;
    }
}





/*
 *  Monochrome
 *
 */
void  Monochrome( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_in_monochrome = atoi(which);
    } else {
        print_in_monochrome = 0;
    }
}





/*
 *  Invert
 *
 */
static void  Invert( /*@unused@*/ Widget widget, XtPointer clientData, XtPointer callData) {
    char *which = (char *)clientData;
    XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

    if(state->set) {
        print_invert = atoi(which);
    } else {
        print_invert = 0;
    }
}





// Print_properties:  Prints the drawing area to a PostScript file
//
// Perhaps later:
// 1) Select an area on the screen to print
// 2) -label
//
void Print_properties( Widget w, XtPointer clientData, XtPointer callData ) {
    static Widget pane, form, button_ok, button_cancel,
            sep, auto_scale,
//            paper_size, paper_size_data, scale, scale_data, blank_background,
//            res_label1, res_label2, res_x, res_y, button_preview,
            monochrome, invert;
    Atom delw;

    if (!print_properties_dialog) {


begin_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties" );


        print_properties_dialog = XtVaCreatePopupShell(langcode("PRINT0001"),xmDialogShellWidgetClass,Global.top,
                                  XmNdeleteResponse,XmDESTROY,
                                  XmNdefaultPosition, FALSE,
                                  NULL);


        pane = XtVaCreateWidget("Print_properties pane",xmPanedWindowWidgetClass, print_properties_dialog,
                          XmNbackground, colors[0xff],
                          NULL);


        form =  XtVaCreateWidget("Print_properties form",xmFormWidgetClass, pane,
                            XmNfractionBase, 2,
                            XmNbackground, colors[0xff],
                            XmNautoUnmanage, FALSE,
                            XmNshadowThickness, 1,
                            NULL);


/*
        paper_size = XtVaCreateManagedWidget(langcode("PRINT0002"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);
XtSetSensitive(paper_size,FALSE);

 
        paper_size_data = XtVaCreateManagedWidget("Print_properties paper_size_data", xmTextFieldWidgetClass, form,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 15,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_FORM,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, paper_size,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(paper_size_data,FALSE);
*/


        auto_rotate  = XtVaCreateManagedWidget(langcode("PRINT0003"),xmToggleButtonWidgetClass,form,
//                                      XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, paper_size_data,
//                                      XmNtopOffset, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(auto_rotate,XmNvalueChangedCallback,Auto_rotate,"1");


        rotate_90  = XtVaCreateManagedWidget(langcode("PRINT0004"),xmToggleButtonWidgetClass,form,
//                                      XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, paper_size_data,
//                                      XmNtopOffset, 5,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, auto_rotate,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(rotate_90,XmNvalueChangedCallback,Rotate_90,"1");


        auto_scale = XtVaCreateManagedWidget(langcode("PRINT0005"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, auto_rotate,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(auto_scale,XmNvalueChangedCallback,Auto_scale,"1");


/*
        scale = XtVaCreateManagedWidget(langcode("PRINT0006"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, auto_rotate,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, auto_scale,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);
XtSetSensitive(scale,FALSE);


        scale_data = XtVaCreateManagedWidget("Print_properties scale_data", xmTextFieldWidgetClass, form,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 15,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, auto_rotate,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, scale,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(scale_data,FALSE);
*/


/*
        blank_background = XtVaCreateManagedWidget(langcode("PRINT0007"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, scale_data,
                                      XmNtopWidget, auto_rotate,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(blank_background,FALSE);
*/


        monochrome = XtVaCreateManagedWidget(langcode("PRINT0008"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
//                                      XmNtopWidget, blank_background,
                                      XmNtopWidget, auto_scale,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(monochrome,XmNvalueChangedCallback,Monochrome,"1");


        invert = XtVaCreateManagedWidget(langcode("PRINT0016"),xmToggleButtonWidgetClass,form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, monochrome,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset ,10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtAddCallback(invert,XmNvalueChangedCallback,Invert,"1");


/*
        res_label1 = XtVaCreateManagedWidget(langcode("PRINT0009"),xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, invert,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);
XtSetSensitive(res_label1,FALSE);


        res_x = XtVaCreateManagedWidget("Print_properties resx_data", xmTextFieldWidgetClass, form,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 15,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, invert,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, res_label1,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_NONE,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(res_x,FALSE);


        res_label2 = XtVaCreateManagedWidget("X",xmLabelWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, invert,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment, XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, res_x,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment, XmATTACH_NONE,
                                      XmNbackground, colors[0xff],
                                      NULL);
XtSetSensitive(res_label2,FALSE);


        res_y = XtVaCreateManagedWidget("Print_properties res_y_data", xmTextFieldWidgetClass, form,
                                      XmNeditable,   TRUE,
                                      XmNcursorPositionVisible, TRUE,
                                      XmNsensitive, TRUE,
                                      XmNshadowThickness,    1,
                                      XmNcolumns, 15,
                                      XmNwidth, ((15*7)+2),
                                      XmNmaxLength, 15,
                                      XmNbackground, colors[0x0f],
                                      XmNtopAttachment,XmATTACH_WIDGET,
                                      XmNtopWidget, invert,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_WIDGET,
                                      XmNleftWidget, res_label2,
                                      XmNleftOffset, 10,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNrightOffset, 10,
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(res_y,FALSE);
*/


        sep = XtVaCreateManagedWidget("Print_properties sep", xmSeparatorGadgetClass,form,
                                      XmNorientation, XmHORIZONTAL,
                                      XmNtopAttachment,XmATTACH_WIDGET,
//                                      XmNtopWidget, res_y,
                                      XmNtopWidget, invert,
                                      XmNtopOffset, 10,
                                      XmNbottomAttachment,XmATTACH_NONE,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment,XmATTACH_FORM,
                                      XmNbackground, colors[0xff],
                                      NULL);


/*
        button_preview = XtVaCreateManagedWidget(langcode("PRINT0010"),xmPushButtonGadgetClass, form,
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
                                      XmNrightOffset, 2,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);
XtSetSensitive(button_preview,FALSE);
*/


//        button_ok = XtVaCreateManagedWidget(langcode("PRINT0011"),xmPushButtonGadgetClass, form,
        button_ok = XtVaCreateManagedWidget(langcode("PRINT0010"),xmPushButtonGadgetClass, form,
                                      XmNtopAttachment, XmATTACH_WIDGET,
                                      XmNtopWidget, sep,
                                      XmNtopOffset, 5,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNbottomOffset, 5,
                                      XmNleftAttachment, XmATTACH_POSITION,
                                      XmNleftPosition, 0,
                                      XmNleftOffset, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 1,
                                      XmNrightOffset, 2,
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
                                      XmNleftOffset, 3,
                                      XmNrightAttachment, XmATTACH_POSITION,
                                      XmNrightPosition, 2,
                                      XmNrightOffset, 5,
                                      XmNbackground, colors[0xff],
                                      XmNnavigationType, XmTAB_GROUP,
                                      XmNtraversalOn, TRUE,
                                      NULL);


//        XtAddCallback(button_preview, XmNactivateCallback, Print_window, "1" );
        XtAddCallback(button_ok, XmNactivateCallback, Print_window, "0" );
        XtAddCallback(button_cancel, XmNactivateCallback, Print_properties_destroy_shell, print_properties_dialog);


        XmToggleButtonSetState(rotate_90,FALSE,FALSE);
        XmToggleButtonSetState(auto_rotate,TRUE,FALSE);


        if (print_auto_rotation)
            XmToggleButtonSetState(auto_rotate, TRUE, TRUE);
        else
            XmToggleButtonSetState(auto_rotate, FALSE, TRUE);


        if (print_rotated)
            XmToggleButtonSetState(rotate_90, TRUE, TRUE);
        else
            XmToggleButtonSetState(rotate_90, FALSE, TRUE);


        if (print_in_monochrome)
            XmToggleButtonSetState(monochrome, TRUE, FALSE);
        else
            XmToggleButtonSetState(monochrome, FALSE, FALSE);


        if (print_invert)
            XmToggleButtonSetState(invert, TRUE, FALSE);
        else
            XmToggleButtonSetState(invert, FALSE, FALSE);


        if (print_auto_scale)
            XmToggleButtonSetState(auto_scale, TRUE, TRUE);
        else
            XmToggleButtonSetState(auto_scale, FALSE, TRUE);
 

//        XmTextFieldSetString(paper_size_data,print_paper_size);


end_critical_section(&print_properties_dialog_lock, "maps.c:Print_properties" );


        pos_dialog(print_properties_dialog);


        delw = XmInternAtom(XtDisplay(print_properties_dialog),"WM_DELETE_WINDOW", FALSE);
        XmAddWMProtocolCallback(print_properties_dialog, delw, Print_properties_destroy_shell, (XtPointer)print_properties_dialog);


        XtManageChild(form);
        XtManageChild(pane);


        XtPopup(print_properties_dialog,XtGrabNone);
        fix_dialog_size(print_properties_dialog);


        // Move focus to the Cancel button.  This appears to highlight the
        // button fine, but we're not able to hit the <Enter> key to
        // have that default function happen.  Note:  We _can_ hit the
        // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(print_properties_dialog);
        XmProcessTraversal(button_cancel, XmTRAVERSE_CURRENT);


    } else {
        (void)XRaiseWindow(XtDisplay(print_properties_dialog), XtWindow(print_properties_dialog));
    }
}





// Create png image (for use in web browsers??).  Requires that "convert"
// from the ImageMagick package be installed on the system.
//
static void* snapshot_thread(void *arg) {

#ifndef NO_XPM
    char xpm_filename[MAX_FILENAME];
    char png_filename[MAX_FILENAME];
#ifdef HAVE_CONVERT
    char command[MAX_FILENAME*2];
#endif  // HAVE_CONVERT


    // The pthread_detach() call means we don't care about the
    // return code and won't use pthread_join() later.  Makes
    // threading more efficient.
    (void)pthread_detach(pthread_self());

    xastir_snprintf(xpm_filename,
        sizeof(xpm_filename),
        "%s/snapshot.xpm",
        get_user_base_dir("tmp"));

    xastir_snprintf(png_filename,
        sizeof(png_filename),
        "%s/snapshot.png",
        get_user_base_dir("tmp"));

    if ( debug_level & 512 )
        fprintf(stderr,"Creating %s\n", xpm_filename );

    if ( !XpmWriteFileFromPixmap( XtDisplay(appshell),    // Display *display
            xpm_filename,                               // char *filename
            pixmap_final,                               // Pixmap pixmap
            (Pixmap)NULL,                               // Pixmap shapemask
            NULL ) == XpmSuccess ) {                    // XpmAttributes *attributes
        fprintf(stderr,"ERROR writing %s\n", xpm_filename );
    }
    else {  // We now have the xpm file created on disk

        chmod( xpm_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

        if ( debug_level & 512 )
            fprintf(stderr,"Convert %s ==> %s\n", xpm_filename, png_filename );

#ifdef HAVE_CONVERT
        // Convert it to a png file.  This depends on having the
        // ImageMagick command "convert" installed.
        xastir_snprintf(command,
            sizeof(command),
            "%s -quality 100 -colors 256 %s %s",
            CONVERT_PATH,
            xpm_filename,
            png_filename );

        if ( system( command ) != 0 ) {
            // We _may_ have had an error.  Check errno to make
            // sure.
            if (errno) {
                fprintf(stderr, "%s\n", strerror(errno));
                fprintf(stderr,
                    "Failed to convert snapshot: %s -> %s\n",
                    xpm_filename,
                    png_filename);
            }
            else {
                fprintf(stderr,
                    "System call return error: convert: %s -> %s\n",
                    xpm_filename,
                    png_filename);
            }
        }
        else {
            chmod( png_filename, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );

            // Delete temporary xpm file
            unlink( xpm_filename );

            if ( debug_level & 512 )
                fprintf(stderr,"  Done creating png.\n");
        }
#endif  // HAVE_CONVERT
    }

#endif // NO_XPM
  
    // Signify that we're all done and that another snapshot can
    // occur.
    doing_snapshot = 0;

    return(NULL);
}





// Starts a separate thread that creates a png image from the
// current displayed image.
//
void Snapshot(void) {
    pthread_t snapshot_thread_id;


    // Check whether we're already doing a snapshot
    if (doing_snapshot)
        return;

    // Time to take another snapshot?
    if (sec_now() < (last_snapshot + 300) ) // New snapshot every five minutes
        return;

    doing_snapshot++;
    last_snapshot = sec_now(); // Set up timer for next time
 
//----- Start New Thread -----

    // Here we start a new thread.  We'll communicate with the main
    // thread via global variables.  Use mutex locks if there might
    // be a conflict as to when/how we're updating those variables.
    //

    if (pthread_create(&snapshot_thread_id, NULL, snapshot_thread, NULL)) {
        fprintf(stderr,"Error creating snapshot thread\n");
    }
    else {
        // We're off and running with the new thread!
    }
}





// Function to remove double-quote characters and spaces that occur
// outside of the double-quote characters.
void clean_string(char *input) {
    char *i;
    char *j;


    //fprintf(stderr,"|%s|\t",input);

    // Remove any double quote characters
    i = index(input,'"');   // Find first quote character, if any

    if (i != NULL) {
        j = index(i+1,'"'); // Find second quote character, if any

        if (j != NULL) {    // Found two quote characters
            j[0] = '\0';    // Terminate the string at the 2nd quote
            strcpy(input,i+1);
        }
        else {  // We only found one quote character.  What to do?
//            fprintf(stderr,"clean_string: Only one quote found!\n");
        }
    }
    //fprintf(stderr,"|%s|\n",input);

    // Remove leading/trailing spaces?
}







// Test map visibility (on screen)
//
// Input parameters are in Xastir coordinate system (fastest for us)
// check_percentage:
//      0 = don't check
//      1 = check map size versus viewport scale.  Return 0 if map
//      is too large/small (percentage-wise) to be displayed.
//
// Returns: MAP_NOT_VIS if map is _not_ visible
//          MAP_IS_VIS if map _is_ visible
//
// Xastir Coordinate System:
//
//              0 (90 deg. or 90N)
//
// 0 (-180 deg. or 180W)      129,600,000 (180 deg. or 180E)
//
//          64,800,000 (-90 deg. or 90S)
//
enum map_onscreen_enum map_onscreen(long left,
                                    long right, 
                                    long top, 
                                    long bottom,
                                    int check_percentage) {
    unsigned long max_x_long_offset;
    unsigned long max_y_lat_offset;
    long map_border_min_x;
    long map_border_max_x;
    long map_border_min_y;
    long map_border_max_y;
    long x_test, y_test;
    enum map_onscreen_enum in_window = MAP_NOT_VIS;

    max_x_long_offset=(unsigned long)(x_long_offset+ (screen_width * scale_x));
    max_y_lat_offset =(unsigned long)(y_lat_offset + (screen_height* scale_y));

    if (debug_level & 16)
      fprintf(stderr,"x_long_offset: %ld, y_lat_offset: %ld, max_x_long_offset: %ld, max_y_lat_offset: %ld\n",
             x_long_offset, y_lat_offset, (long)max_x_long_offset, (long)max_y_lat_offset);

    if (((left <= x_long_offset) && (x_long_offset <= right) &&
         (top <= y_lat_offset) && (y_lat_offset <= bottom)) ||
        ((left <= x_long_offset) && (x_long_offset <= right) &&
         (top <= (long)max_y_lat_offset) && ((long)max_y_lat_offset <= bottom)) ||
        ((left <= (long)max_x_long_offset) && ((long)max_x_long_offset <= right) &&
         (top <= y_lat_offset) && (y_lat_offset <= bottom)) ||
        ((left <= (long)max_x_long_offset) && ((long)max_x_long_offset <= right) &&
         (top <= (long)max_y_lat_offset) && ((long)max_y_lat_offset <= bottom)) ||
        ((x_long_offset <= left) && (left <= (long)max_x_long_offset) &&
         (y_lat_offset <= top) && (top <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= left) && (left <= (long)max_x_long_offset) &&
         (y_lat_offset <= bottom) && (bottom <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= right) && (right <= (long)max_x_long_offset) &&
         (y_lat_offset <= top) && (top <= (long)max_y_lat_offset)) ||
        ((x_long_offset <= right) && (right <= (long)max_x_long_offset) &&
         (y_lat_offset <= bottom) && (bottom <= (long)max_y_lat_offset)))
      in_window = MAP_IS_VIS;
    else {
        // find min and max borders to look at
        //this routine are for those odd sized maps
        if ((long)left > x_long_offset)
          map_border_min_x = (long)left;
        else
          map_border_min_x = x_long_offset;

        if (right < (long)max_x_long_offset)
          map_border_max_x = (long)right;
        else
          map_border_max_x = (long)max_x_long_offset;

        if ((long)top > y_lat_offset)
          map_border_min_y = (long)top;
        else
          map_border_min_y = y_lat_offset;

        if (bottom < (long)max_y_lat_offset)
          map_border_max_y = (long)bottom;
        else
          map_border_max_y = (long)max_y_lat_offset;

        // do difficult check inside map
        for (x_test = map_border_min_x;(x_test <= map_border_max_x && !in_window); x_test += ((scale_x * screen_width) / 10)) {
            for (y_test = map_border_min_y;(y_test <= map_border_max_y && !in_window);y_test += ((scale_y * screen_height) / 10)) {
                if ((x_long_offset <= x_test) && (x_test <= (long)max_x_long_offset) && (y_lat_offset <= y_test) &&
                    (y_test <= (long)max_y_lat_offset))

                  in_window = MAP_IS_VIS;
            }
        }
    }


#ifdef MAP_SCALE_CHECK
    // Check whether map is too large/small for our current scale?
    // Check whether the map extents are < XX% of viewscreen size
    // (both directions), or viewscreen is > XX% of map extents
    // (either direction).  This will knock out maps that are too
    // large/small to be displayed at this zoom level.
//WE7U
    if (in_window && check_percentage) {
        long map_x, map_y, view_x, view_y;
        float percentage = 0.04;


        map_x  = right - left;
        if (map_x < 0)
            map_x = 0;

        map_y  = bottom - top;
        if (map_y < 0)
            map_y = 0;

        view_x = max_x_long_offset - x_long_offset;
        if (view_x < 0)
            view_x = 0;

        view_y = max_y_lat_offset - y_lat_offset;
        if (view_y < 0)
            view_y = 0;

//fprintf(stderr,"\n map_x: %d\n", map_x);
//fprintf(stderr," map_y: %d\n", map_y);
//fprintf(stderr,"view_x: %d\n", view_x);
//fprintf(stderr,"view_y: %d\n", view_y);

        if ((map_x < (view_x * percentage )) && (map_y < (view_x * percentage))) {
            in_window = 0;  // Send back "not-visible" flag
fprintf(stderr,"map too small for view: %d%%\n",(int)(percentage * 100));
        }

//        if ((view_x < (map_x * percentage)) && (view_y < (map_x * percentage))) {
//            in_window = 0;  // Send back "not-visible" flag
//fprintf(stderr,"view too small for map: %d%%\n",(int)(percentage * 100));
//        }

    }
#endif  // MAP_SCALE_CHECK
 

    return (in_window);
}


// Function which checks whether a map is onscreen, but does so by
// finding the map boundaries from the map index.  The only input
// parameter is the complete path/filename.
//
// Returns: MAP_NOT_VIS if map is _not_ visible
//          MAP_IS_VIS if map _is_ visible
//          MAP_NOT_INDEXED if the map is not in the index
//
enum map_onscreen_enum map_onscreen_index(char *filename) {
    unsigned long top, bottom, left, right;
    enum map_onscreen_enum onscreen = MAP_NOT_INDEXED;
    int map_layer, draw_filled, auto_maps;     // Unused in this function


    if (index_retrieve(filename, &bottom, &top, &left, &right,
            &map_layer, &draw_filled, &auto_maps) ) {

        // Map was in the index, check for visibility and scale
        // Check whether the map extents are < XX% of viewscreen
        // size (both directions), or viewscreen is > XX% of map
        // extents (either direction).  This will knock out maps
        // that are too large/small to be displayed at this zoom
        // level.
        if (map_onscreen(left, right, top, bottom, 1)) {
            onscreen = MAP_IS_VIS;
            //fprintf(stderr,"Map found in index and onscreen! %s\n",filename);


// Check whether the map extents are < XX% of viewscreen size (both
// directions), or viewscreen is > XX% of map extents (either
// direction).  This will knock out maps that are too large/small to
// be displayed at this zoom level.


        }
        else {  // Map is not visible
            onscreen = MAP_NOT_VIS;
            //fprintf(stderr,"Map found in index but not onscreen: %s\n",filename);
        }
    }
    else {  // Map is not in the index
        onscreen = MAP_NOT_INDEXED;
        //fprintf(stderr,"Map not found in index: %s\n",filename);
    }
    return(onscreen);
}
 




/**********************************************************
 * draw_map()
 *
 * Function which tries to figure out what type of map or
 * image file we're dealing with, and takes care of getting
 * it onto the screen.  Calls other functions to deal with
 * .geo/.tif/.shp maps.
 *
 * If destination_pixmap == DRAW_NOT, then we'll not draw
 * the map anywhere, but we'll determine the map extents
 * and write them to the map index file.
 **********************************************************/
/* table of map drivers, selected by filename extension */
extern void draw_dos_map(Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry *alert,
                         u_char alert_color,
                         int destination_pixmap,
                         int draw_filled);

extern void draw_palm_image_map(Widget w,
                         char *dir,
                         char *filenm,
                         alert_entry *alert,
                         u_char alert_color,
                         int destination_pixmap,
                         int draw_filled);

#ifdef HAVE_LIBSHP
extern void draw_shapefile_map (Widget w, 
                                char *dir, 
                                char *filenm, 
                                alert_entry *alert,
                                u_char alert_color, 
                                int destination_pixmap,
                                int draw_filled);
#endif /* HAVE_LIBSHP */
#ifdef HAVE_LIBGEOTIFF
extern void draw_geotiff_image_map(Widget w,
                                   char *dir,
                                   char *filenm,
                                   alert_entry *alert,
                                   u_char alert_color,
                                   int destination_pixmap,
                                   int draw_filled);
#endif /* HAVE_LIBGEOTIFF */
extern void draw_geo_image_map(Widget w,
                               char *dir,
                               char *filenm,
                               alert_entry *alert,
                               u_char alert_color,
                               int destination_pixmap,
                               int draw_filled);

extern void draw_gnis_map(Widget w,
                          char *dir,
                          char *filenm,
                          alert_entry *alert,
                          u_char alert_color,
                          int destination_pixmap,
                          int draw_filled);

 struct {
  char *ext;
  enum {none=0,map,pdb,shp,tif,geo,gnis} type;
  void (*func)(Widget w,
               char *dir,
               char *filenm,
               alert_entry *alert,
               u_char alert_color,
               int destination_pixmap,
               int draw_filled);
} map_driver[] = {
  {"map",map,draw_dos_map},
  {"pdb",pdb,draw_palm_image_map},
#ifdef HAVE_LIBSHP
  {"shp",shp,draw_shapefile_map},
#endif /* HAVE_LIBSHP */
#ifdef HAVE_LIBGEOTIFF
  {"tif",tif,draw_geotiff_image_map},
#endif /* HAVE_LIBGEOTIFF */
  {"geo",geo,draw_geo_image_map},
  {"gnis",gnis,draw_gnis_map},
  {NULL,none,NULL},
}, *map_driver_ptr;

void draw_map (Widget w, char *dir, char *filenm, alert_entry *alert,
                u_char alert_color, int destination_pixmap,
                int draw_filled) {
  enum map_onscreen_enum onscreen;
  char *ext;
  char file[MAX_FILENAME];

    if ((ext = get_map_ext(filenm)) == NULL)
      return;
    
    for (map_driver_ptr = map_driver; map_driver_ptr->ext; map_driver_ptr++) {
      if (strcasecmp(ext,map_driver_ptr->ext) == 0) 
	break;			/* found our map_driver */
    }
    if (map_driver_ptr->type == none) {    /* fall thru: unknown map driver */
      // Check whether we're indexing or drawing the map
      if ( (destination_pixmap != INDEX_CHECK_TIMESTAMPS)
	   && (destination_pixmap != INDEX_NO_TIMESTAMPS) ) {
	// We're drawing, not indexing.  Output a warning
	// message.
	fprintf(stderr,"*** draw_map: Unknown map type: %s ***\n", filenm);
      }
      return;
    }

    onscreen = map_onscreen_index(filenm); // Check map index
 
    // Check whether we're indexing or drawing the map
    if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
            || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {

        // We're indexing maps
        if (onscreen != MAP_NOT_INDEXED) // We already have an index entry for this map.
            // This is where we pick up a big speed increase:
            // Refusing to index a map that's already indexed.
            return; // Skip it.
    }
    else {  // We're drawing maps
        // See if map is visible.  If not, skip it.
        if (onscreen == MAP_NOT_VIS) // Map is not visible, skip it.
            return;
    }


    xastir_snprintf(file, sizeof(file), "%s/%s", dir, filenm);

    // Used for debugging.  If we get a segfault on a map, this is
    // often the only way of finding out which map file we can't
    // handle.
    //fprintf(stderr,"draw_map: %s\n",file);

    /* XXX - aren't alerts just shp maps?  Why was there special case code? */
    
    if (map_driver_ptr->func)
      map_driver_ptr->func(w,
                           dir,
                           filenm,
                           alert,
                           alert_color,
                           destination_pixmap,
                           draw_filled);
        
    XmUpdateDisplay (XtParent (da));
}  // End of draw_map()

static void index_update_directory(char *directory);
static void index_update_accessed(char *filename);

/////////////////////////////////////////////////////////////////////
// map_search()
//
// Function which recurses through map directories, finding map
// files.  It's called from load_auto_maps and load_alert_maps.  If
// a map file is found, it is drawn.  We can also call this function
// in indexing mode rather than draw mode, specified by the
// destination_pixmap parameter.
//
// If alert == NULL, we looking for a regular map file to draw.
// If alert != NULL, we have a weather alert to draw.
//
// For alert maps, we need to do things a bit differently, as there
// should be only a few maps that contain all of the alert maps, and we
// can compute which map some of them might be in.  We need to fill in
// the alert structure with the filename that alert is found in.
// For alerts we're not drawing the maps, we're just computing the
// full filename for the alert and filling that struct field in.
//
// The "warn" parameter specifies whether to warn the operator about
// the alert on the console as well.  If it was received locally or
// via local RF, then the answer is yes.  The severe weather may be
// nearby.
//
// We have the timestamp of the map_index.sys file stored away in
// the global:  time_t map_index_timestamp;
// Use that timestamp to compare the map file or GEO file timestamps
// to.  Re-index the map if map_index_timestamp is older.
//
/////////////////////////////////////////////////////////////////////
static void map_search (Widget w, char *dir, alert_entry * alert, int *alert_count,int warn, int destination_pixmap) {
    struct dirent *dl = NULL;
    DIR *dm;
    char fullpath[MAX_FILENAME];
    struct stat nfile;
    const time_t *ftime;
    char this_time[40];
    char *ptr;
    char *map_dir;
    int map_dir_length;

    // We'll use the weather alert directory if it's an alert
    map_dir = alert ? ALERT_MAP_DIR : SELECTED_MAP_DIR;

    map_dir_length = (int)strlen (map_dir);

    if (alert) {    // We're doing weather alerts
        // First check whether the alert->filename variable is filled
        // in.  If so, we've already found the file and can just display
        // that shape in the file
        if (alert->filename[0] == '\0') {   // No filename in struct, so will have
                                            // to search for the shape in the files.
            switch (alert->title[3]) {
                case 'C':   // 'C' in 4th char means county
                    // Use County file c_??????
                    //fprintf(stderr,"%c:County file\n",alert->title[3]);
                    strncpy (alert->filename, "c_", sizeof (alert->filename));
                    break;
                case 'A':   // 'A' in 4th char means county warning area
                    // Use County warning area w_?????
                    //fprintf(stderr,"%c:County warning area file\n",alert->title[3]);
                    strncpy (alert->filename, "w_", sizeof (alert->filename));
                    break;
                case 'Z':
                    // Zone, coastal or offshore marine zone file z_????? or mz?????? or oz??????
                    // oz: ANZ081-086,088,PZZ081-085
                    // mz: AM,AN,GM,LC,LE,LH,LM,LO,LS,PH,PK,PM,PS,PZ,SL
                    // z_: All others
                    if (strncasecmp(alert->title,"AM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"AN",2) == 0) {
                        // Need to check for Z081-Z086, Z088, if so use
                        // oz??????, else use mz??????
                        if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z085",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z086",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z088",4) == 0) ) {
                            //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"GM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LC",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LE",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LH",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LO",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"LS",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PH",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PK",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PM",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PS",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else if (strncasecmp(alert->title,"PZ",2) == 0) {
// Need to check for PZZ081-085, if so use oz??????, else use mz??????
                        if (       (strncasecmp(&alert->title[3],"Z081",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z082",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z083",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z084",4) == 0)
                                || (strncasecmp(&alert->title[3],"Z085",4) == 0) ) {
                            //fprintf(stderr,"%c:Offshore marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "oz", sizeof (alert->filename));
                        }
                        else {
                            //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                            strncpy (alert->filename, "mz", sizeof (alert->filename));
                        }
                    }
                    else if (strncasecmp(alert->title,"SL",2) == 0) {
                        //fprintf(stderr,"%c:Coastal marine zone file\n",alert->title[3]);
                        strncpy (alert->filename, "mz", sizeof (alert->filename));
                    }
                    else {
                        // Must be regular zone file instead of coastal
                        // marine zone or offshore marine zone.
                        //fprintf(stderr,"%c:Zone file\n",alert->title[3]);
                        strncpy (alert->filename, "z_", sizeof (alert->filename));
                    }
                    break;
                default:
                    // Unknown type
//fprintf(stderr,"%c:Can't match weather warning to a Shapefile:%s\n",alert->title[3],alert->title);
                    break;
            }
//            fprintf(stderr,"%s\t%s\t%s\n",alert->activity,alert->alert_status,alert->title);
            //fprintf(stderr,"File: %s\n",alert->filename);
        }

// NOTE:  Need to skip this part if we have a full filename.

        if (alert->filename[0]) {   // We have at least a partial filename
            int done = 0;

            if (strlen(alert->filename) > 3)
                done++; // We already have a filename

            if (!done) {    // We don't have a filename yet

                // Look through the warning directory to find a match for
                // the first few characters that we already figured out.
                // This is designed so that we don't need to know the exact
                // filename, but only the lead three characters in order to
                // figure out which shapefile to use.
                dm = opendir (dir);
                if (!dm) {  // Couldn't open directory
                    xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
                    // If local alert, warn the operator via the
                    // console as well.
                    if (warn)
                        perror (fullpath);
                }
                else {    // We could open the directory just fine
                    while ( (dl = readdir(dm)) && !done ) {
                        int i;

                        // Check the file/directory name for control
                        // characters
                        for (i = 0; i < (int)strlen(dl->d_name); i++) {
                            // Dump out a warning if control
                            // characters other than LF or CR are
                            // found.
                            if ( (dl->d_name[i] != '\n')
                                    && (dl->d_name[i] != '\r')
                                    && (dl->d_name[i] < 0x20) ) {

                                fprintf(stderr,"\nmap_search: Found control char 0x%02x in alert file/alert directory name.  Line was:\n",
                                    dl->d_name[i]);
                                fprintf(stderr,"%s\n",dl->d_name);
                            }
/*
// This part might not work 'cuz we'd be changing a memory area that
// we might have only read access to.  Check this.
                            if (dl->d_name[i] < 0x20) {
                                // Terminate string at any control character
                                dl->d_name[i] = '\0';
                            }
*/
                        }

                        xastir_snprintf(fullpath, sizeof(fullpath), "%s%s", dir, dl->d_name);
                        /*fprintf(stderr,"FULL PATH %s\n",fullpath); */
                        if (stat (fullpath, &nfile) == 0) {
                            ftime = (time_t *)&nfile.st_ctime;
                            switch (nfile.st_mode & S_IFMT) {
                                case (S_IFDIR):     // It's a directory, skip it
                                    break;

                                case (S_IFREG):     // It's a file, check it
                                    /*fprintf(stderr,"FILE %s\n",dl->d_name); */
                                    // Here we look for a match for the
                                    // first 2 characters of the filename.
                                    // 
                                    if (strncasecmp(alert->filename,dl->d_name,2) == 0) {
                                        // We have a match
                                        //fprintf(stderr,"%s\n",fullpath);
                                        // Force last three characters to
                                        // "shp"
                                        dl->d_name[strlen(dl->d_name)-3] = 's';
                                        dl->d_name[strlen(dl->d_name)-2] = 'h';
                                        dl->d_name[strlen(dl->d_name)-1] = 'p';

                                        // Save the filename in the alert
                                        strncpy(alert->filename,dl->d_name,strlen(dl->d_name));
                                        done++;
                                        //fprintf(stderr,"%s\n",dl->d_name);
                                    }
                                    break;

                                default:    // Not dir or file, skip it
                                    break;
                            }
                        }
                    }
                }
                (void)closedir (dm);
            }

            if (done) {    // We found a filename match for the alert
                // Go draw the weather alert (kind'a)
//WE7U
                draw_map (w,
                    dir,                // Alert directory
                    alert->filename,    // Shapefile filename
                    alert,
                    -1,                 // Signifies "DON'T DRAW THE SHAPE"
                    destination_pixmap,
                    1 /* draw_filled */ );
            }
            else {      // No filename found that matches the first two
                        // characters that we already computed.

                //
                // Need code here
                //

            }
        }
        else {  // Still no filename for the weather alert.
                // Output an error message?
            //
            // Need code here
            //

        }
    }


// MAPS, not alerts

    else {  // We're doing regular maps, not weather alerts
        time_t map_timestamp;


        dm = opendir (dir);
        if (!dm) {  // Couldn't open directory
            xastir_snprintf(fullpath, sizeof(fullpath), "aprsmap %s", dir);
            if (warn)
                perror (fullpath);
        }
        else {
            int count = 0;
            while ((dl = readdir (dm))) {
                int i;

                // Check the file/directory name for control
                // characters
                for (i = 0; i < (int)strlen(dl->d_name); i++) {
                    // Dump out a warning if control characters
                    // other than LF or CR are found.
                    if ( (dl->d_name[i] != '\n')
                            && (dl->d_name[i] != '\r')
                            && (dl->d_name[i] < 0x20) ) {

                        fprintf(stderr,"\nmap_search: Found control char 0x%02x in map file/map directory name.  Line was:\n",
                            dl->d_name[i]);
                        fprintf(stderr,"%s\n",dl->d_name);
                    }
/*
// This part might not work 'cuz we'd be changing a memory area that
// we might have only read access to.  Check this.
                    if (dl->d_name[i] < 0x20) {
                        // Terminate string at any control character
                        dl->d_name[i] = '\0';
                    }
*/
                }

                xastir_snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, dl->d_name);
                //fprintf(stderr,"FULL PATH %s\n",fullpath);
                if (stat (fullpath, &nfile) == 0) {
                    ftime = (time_t *)&nfile.st_ctime;
                    switch (nfile.st_mode & S_IFMT) {
                        case (S_IFDIR):     // It's a directory, recurse
                            //fprintf(stderr,"file %c letter %c\n",dl->d_name[0],letter);
                            if ((strcmp (dl->d_name, ".") != 0) && (strcmp (dl->d_name, "..") != 0)) {

                                //fprintf(stderr,"FULL PATH %s\n",fullpath);

                                // If we're indexing, throw the
                                // directory into the map index as
                                // well.
                                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                                        || (destination_pixmap == INDEX_NO_TIMESTAMPS) ) {
                                    char temp_dir[MAX_FILENAME];

                                    // Drop off the base part of the
                                    // path for the indexing,
                                    // usually
                                    // "/usr/local/share/xastir/maps".
                                    // Add a '/' to the end.
                                    xastir_snprintf(temp_dir,
                                        sizeof(temp_dir),
                                        "%s/",
                                        &fullpath[map_dir_length+1]);

                                    // Add the directory to the
                                    // in-memory map index.
                                    index_update_directory(temp_dir);
                                }

                                strcpy (this_time, ctime (ftime));
                                map_search(w, fullpath, alert, alert_count, warn, destination_pixmap);
                            }
                            break;

                        case (S_IFREG):     // It's a file, draw the map
                            /*fprintf(stderr,"FILE %s\n",dl->d_name); */

                            // Get the last-modified timestamp for the map file
                            map_timestamp = (time_t)nfile.st_mtime;

                            // Check whether we're doing indexing or
                            // map drawing.  If indexing, we only
                            // want to index if the map timestamp is
                            // newer than the index timestamp.
                            if (destination_pixmap == INDEX_CHECK_TIMESTAMPS
                                    || destination_pixmap == INDEX_NO_TIMESTAMPS) {
                                // We're doing indexing, not map drawing
                                char temp_dir[MAX_FILENAME];

                                // Drop off the base part of the
                                // path for the indexing, usually
                                // "/usr/local/share/xastir/maps".
                                xastir_snprintf(temp_dir,
                                    sizeof(temp_dir),
                                    "%s",
                                    &fullpath[map_dir_length+1]);

                                // Update the "accessed"
                                // variable in the record
                                index_update_accessed(temp_dir);

// Note:  This is not as efficient as it should be, as we're looking
// through the in-memory map index here just to update the
// "accessed" variable, then in some cases looking through it again
// in the next section for updated maps, or if we're ignoring
// timestamps while indexing.  Looking through a linear linked list
// too many times overall.

                                if ( (destination_pixmap == INDEX_CHECK_TIMESTAMPS)
                                        && (map_timestamp < map_index_timestamp) ) {
                                    // Map is older than index _and_
                                    // we're supposed to check
                                    // timestamps.
                                    count++;
                                    break;  // Skip indexing this file
                                }
                                else {  // Map is newer or we're ignoring timestamps.
                                    // We'll index the map
                                    if (debug_level & 16) {
                                        fprintf(stderr,"Indexing map: %s\n",fullpath);
                                    }
                                }
                            }

                            // Check whether the file is in a subdirectory
                            if (strncmp (fullpath, map_dir, (size_t)map_dir_length) != 0) {
                                draw_map (w,
                                    dir,
                                    dl->d_name,
                                    alert ? &alert[*alert_count] : NULL,
                                    '\0',
                                    destination_pixmap,
                                    1 /* draw_filled */ );
                                if (alert_count && *alert_count)
                                    (*alert_count)--;
                            }
                            else {
                                // File is in the main map directory
                                // Find the '/' character
                                for (ptr = &fullpath[map_dir_length]; *ptr == '/'; ptr++) ;
                                draw_map (w,
                                    map_dir,
                                    ptr,
                                    alert ? &alert[*alert_count] : NULL,
                                    '\0',
                                    destination_pixmap,
                                    1 /* draw_filled */ );
                                if (alert_count && *alert_count)
                                    (*alert_count)--;
                            }
                            count++;
                            break;

                        default:
                            break;
                    }
                }
            }
            if (debug_level & 16)
                fprintf(stderr,"Number of maps queried: %d\n", count);

            (void)closedir (dm);
        }
    }
}





// List pointer for the map index linked list.
map_index_record *map_index_head = NULL;

// Might wish to have another variable in the index which is used to
// record that a file has been indexed recently.  This could be used
// to prune old entries out of the index if a full indexing didn't
// touch a file entry.  Could also delete an entry from the index
// if/when a file can't be opened?





// Function to dissect and free all of the records in a map index
// linked list, leaving it totally empty.
//
static void free_map_index(map_index_record *index_list_head) {
    map_index_record *current;
    map_index_record *temp;


    current = index_list_head;

    while (current != NULL) {
        temp = current;
        current = current->next;
        free(temp);
    }

    index_list_head = NULL;
}





// Function to copy just the properties fields from the backup map
// index to the primary index.  Must match each record before
// copying.  Once it's done, it frees the backup map index.
//
static void map_index_copy_properties(map_index_record *primary_index_head,
        map_index_record *backup_index_head) {
    map_index_record *primary;
    map_index_record *backup;


    backup = backup_index_head;

    // Walk the backup list, comparing the filename field with the
    // primary list.  When a match is found, copy just the
    // Properties fields (map_layer/draw_filled/auto_maps/selected)
    // across to the primary record.
    //
    while (backup != NULL) {
        int done = 0;

        primary = primary_index_head;

        while (!done && primary != NULL) {

            if (strcmp(primary->filename, backup->filename) == 0) { // If match

                if (debug_level & 16) {
                    fprintf(stderr,"Match: %s\t%s\n",
                        primary->filename,
                        backup->filename);
                }

                // Copy the Properties across
                primary->map_layer   = backup->map_layer;
                primary->draw_filled = backup->draw_filled;
                primary->auto_maps   = backup->auto_maps;
                primary->selected    = backup->selected;

                // Done copying this backup record.  Go on to the
                // next.  Skip the rest of the primary list for this
                // iteration.
                done++;
            }
            else {  // No match, walk the primary list looking for one.
                primary = primary->next;
            }
        }

        // Walk the backup list
        backup = backup->next;
    }

    // We're done copying.  Free the backup list.
    free_map_index(backup_index_head);
}





// Function used to add map directories to the in-memory map index.
// Causes an update of the index list in memory.  Input Records are
// inserted in alphanumerical order.  We mark directories in the
// index with a '/' on the end of the name, and zero entries for
// top/bottom/left/right.
// The input directory to this routine MUST have a '/' character on
// the end of it.  This is how we differentiate directories from
// files in the list.
static void index_update_directory(char *directory) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    map_index_record *temp_record = NULL;
    int done = 0;
    int i;


    //fprintf(stderr,"index_update_directory: %s\n", directory );

    // Check for initial bad input
    if ( (directory == NULL)
            || (directory[0] == '\0')
            || (directory[strlen(directory) - 1] != '/')
            || ( (directory[1] == '/') && (strlen(directory) == 1)) ) {
        fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
        return;
    }
    // Make sure there aren't any weird characters in the directory
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(directory); i++ ) {
        // Change any control characters to '\0' chars
        if (directory[i] < 0x20) {

            fprintf(stderr,"\nindex_update_directory: Found control char 0x%02x in map file/map directory name:\n%s\n",
                directory[i],
                directory);

            directory[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if ( (directory[0] == '\0')
            || (directory[strlen(directory) - 1] != '/')
            || ( (directory[1] == '/') && (strlen(directory) == 1))) {
        fprintf(stderr,"index_update_directory: Bad input: %s\n",directory);
        return;
    }

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Search for a matching directory name in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",
        //    current->filename, directory);

        test = strcmp(current->filename, directory);
        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",directory);
            temp_record = current;
            done++; // Exit loop, "current" points to found record
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,directory);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",directory);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {  // Insert between "previous" and "current"
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);
 
            // Fill in some default values for the new record.
            temp_record->selected = 0;
            temp_record->auto_maps = 0;

            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, add a record to
                    // the end of the list.  "previous" points to
                    // the last record in the list or NULL (empty
                    // list).
        //fprintf(stderr,"Not Found: Adding an index record for %s\n",directory);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",directory);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(directory),directory);
 
        // Fill in some default values for the new record.
        temp_record->selected = 0;
        temp_record->auto_maps = 0;
    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",directory);
    //strncpy(temp_record->filename,directory,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",directory);

    temp_record->bottom = 0;
    temp_record->top = 0;
    temp_record->left = 0;
    temp_record->right = 0;
    temp_record->accessed = 1;
    temp_record->map_layer = 0;
    temp_record->draw_filled = 0;
}





// Function called by the various draw_* functions when in indexing
// mode.  Causes an update of the index list in memory.  Input
// parameters are in the Xastir coordinate system due to speed
// considerations.  Records are inserted in alphanumerical order.
void index_update_xastir(char *filename,
                  unsigned long bottom,
                  unsigned long top,
                  unsigned long left,
                  unsigned long right) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    map_index_record *temp_record = NULL;
    int done = 0;
    int i;


    // Check for initial bad input
    if ( (filename == NULL)
            || (filename[0] == '\0')
            || (filename[strlen(filename) - 1] == '/') ) {
        fprintf(stderr,"index_update_xastir: Bad input: %s\n",filename);
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_xastir: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
        fprintf(stderr,"index_update_xastir: Bad input: %s\n",filename);
        return;
    }

    //fprintf(stderr,"index_update_xastir: (%lu,%lu)\t(%lu,%lu)\t%s\n",
    //    bottom, top, left, right, filename );

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Skip dbf and shx map extensions.  Really should make this
    // case-independent...
    if (       strstr(filename,"shx")
            || strstr(filename,"dbf")
            || strstr(filename,"SHX")
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);
        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",filename);
            temp_record = current;
            done++; // Exit the while loop
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

            // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
            temp_record->map_layer = 0;
            temp_record->draw_filled = 0;
            temp_record->selected = 0;
 
            if (       strstr(filename,".geo")
                    || strstr(filename,".GEO")
                    || strstr(filename,".Geo") ) {
                temp_record->auto_maps = 0;
            }
            else {
                temp_record->auto_maps = 1;
            }
         
            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {  // Matching record not found, add a
        // record to the end of the list
        //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

        // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
        temp_record->map_layer = 0;
        temp_record->draw_filled = 0;
        temp_record->selected = 0;

        if (       strstr(filename,".geo")
                || strstr(filename,".GEO")
                || strstr(filename,".Geo") ) {
            temp_record->auto_maps = 0;
        }
        else {
            temp_record->auto_maps = 1;
        }

    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);
    //strncpy(temp_record->filename,filename,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",filename);

    temp_record->bottom = bottom;
    temp_record->top = top;
    temp_record->left = left;
    temp_record->right = right;
    temp_record->accessed = 1;
}





// Function called by the various draw_* functions when in indexing
// mode.  Causes an update of the index list in memory.  Input
// parameters are in lat/long, which are converted to Xastir
// coordinates for storage due to speed considerations.  Records are
// inserted in alphanumerical order.
void index_update_ll(char *filename,
                  double bottom,
                  double top,
                  double left,
                  double right) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    map_index_record *temp_record = NULL;
    int done = 0;
    unsigned long temp_left, temp_right, temp_top, temp_bottom;
    int ok;
    int i;


    // Check for initial bad input
    if ( (filename == NULL)
            || (filename[0] == '\0')
            || (filename[strlen(filename) - 1] == '/') ) {
        fprintf(stderr,"index_update_ll: Bad input: %s\n",filename);
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_ll: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
        fprintf(stderr,"index_update_ll: Bad input: %s\n",filename);
        return;
    }

    //fprintf(stderr,"index_update_ll: (%15.10g,%15.10g)\t(%15.10g,%15.10g)\t%s\n",
    //    bottom, top, left, right, filename );

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Skip dbf and shx map extensions.  Really should make this
    // case-independent...
    if (       strstr(filename,"shx")
            || strstr(filename,"dbf")
            || strstr(filename,"SHX")
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);

        if (test == 0) {
            // Found a match!
            //fprintf(stderr,"Found: Updating entry for %s\n",filename);
            temp_record = current;
            done++; // Exit the while loop
        }

        else if (test > 0) {
            // Found a string past us in the alphabet.  Insert ahead
            // of this last record.

            //fprintf(stderr,"\n%s\n%s\n",current->filename,filename);

            //fprintf(stderr,"Not Found: Inserting an index record for %s\n",filename);
            temp_record = (map_index_record *)malloc(sizeof(map_index_record));

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                temp_record->next = map_index_head;
                map_index_head = temp_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {
                // Insert new record before "current"
                previous->next = temp_record;
                temp_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

            // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
            temp_record->map_layer = 0;
            temp_record->draw_filled = 0;
            temp_record->selected = 0;

            if (       strstr(filename,".geo")
                    || strstr(filename,".GEO")
                    || strstr(filename,".Geo") ) {
                temp_record->auto_maps = 0;
            }
            else {
                temp_record->auto_maps = 1;
            }
 
            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, didn't find alpha
                    // chars after our string either, add record to
                    // the end of the list.

        //fprintf(stderr,"Not Found: Adding an index record for %s\n",filename);
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));
        temp_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = temp_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = temp_record;
            //fprintf(stderr,"Adding to end of list: %s\n",filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);

        // Fill in some default values for the new record
//WE7U
// Here's where we might look at the file extension and assign
// default map_layer/draw_filled fields based on that.
        temp_record->map_layer = 0;
        temp_record->draw_filled = 0;
        temp_record->selected = 0;

        if (       strstr(filename,".geo")
                || strstr(filename,".GEO")
                || strstr(filename,".Geo") ) {
            temp_record->auto_maps = 0;
        }
        else {
            temp_record->auto_maps = 1;
        }

    }

    // Update the values.  By this point we have a struct to fill
    // in, whether it's a new or old struct doesn't matter.  Convert
    // the values from lat/long to Xastir coordinate system.

    // In this case the struct uses MAX_FILENAME for the length of
    // the field, so the below statement is ok.
    xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);

    //strncpy(temp_record->filename,filename,MAX_FILENAME-1);
    //temp_record->filename[MAX_FILENAME-1] = '\0';
//    xastir_snprintf(temp_record->filename,strlen(temp_record->filename),"%s",filename);

    ok = convert_to_xastir_coordinates( &temp_left,
        &temp_top,
        (float)left,
        (float)top);
    if (!ok)
        fprintf(stderr,"%s\n\n",filename);

    ok = convert_to_xastir_coordinates( &temp_right,
        &temp_bottom,
        (float)right,
        (float)bottom);
    if (!ok)
        fprintf(stderr,"%s\n\n",filename);
 
    temp_record->bottom = temp_bottom;
    temp_record->top = temp_top;
    temp_record->left = temp_left;
    temp_record->right = temp_right;
    temp_record->accessed = 1;
}





// Function which will update the "accessed" variable on either a
// directory or a filename in the map index.
static void index_update_accessed(char *filename) {
    map_index_record *current = map_index_head;
    int done = 0;
    int i;


    // Check for initial bad input
    if ( (filename == NULL) || (filename[0] == '\0') ) {
        fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
        return;
    }

    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for control characters
    // and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(filename); i++ ) {
        // Change any control characters to '\0' chars
        if (filename[i] < 0x20) {

            fprintf(stderr,"\nindex_update_accessed: Found control char 0x%02x in map file/map directory name:\n%s\n",
                filename[i],
                filename);

            filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (filename[0] == '\0') {
        fprintf(stderr,"index_update_accessed: Bad input: %s\n",filename);
        return;
    }

    // Skip dbf and shx map extensions.  Really should make this
    // case-independent...
    if (       strstr(filename,"shx")
            || strstr(filename,"dbf")
            || strstr(filename,"SHX")
            || strstr(filename,"DBF") ) {
        return;
    }

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

//fprintf(stderr,"Comparing %s to\n          %s\n",current->filename,filename);

        test = strcmp(current->filename,filename);

        if (test == 0) {
            // Found a match!
//fprintf(stderr,"Found: Updating entry for %s\n\n",filename);
            current->accessed = 1;
            done++; // Exit the while loop
        }

        else {  // Haven't gotten to the correct insertion point yet
            current = current->next;
        }
    }
}





// Function called by map_onscreen_index()
//
// This function returns:
//      0 if the map isn't in the index
//      1 if the map is listed in the index
//      Four parameters listing the extents of the map
//
// The updated parameters are in the Xastir coordinate system for
// speed reasons.
//
// Note that the index retrieval could be made much faster by
// storing the data in a hash instead of a linked list.  This is
// just an initial implementation to see what speedups are possible.
// Hashing might be next.  --we7u
//
// Note that since we've alphanumerically ordered the list, we can
// stop when we hit something after this filename in the alphabet.
// It'll speed things up a bit.  Make this change sometime soon.
//
int index_retrieve(char *filename,
                   unsigned long *bottom,
                   unsigned long *top,
                   unsigned long *left,
                   unsigned long *right,
                   int *map_layer,
                   int *draw_filled,
                   int *auto_maps) {

    map_index_record *current = map_index_head;
    int status = 0;

    if (filename == NULL) {
        return(status);
    }

    // Search for a matching filename in the linked list
    while ( (current != NULL)
            && (strlen(filename) < MAX_FILENAME) ) {

        if (strcmp(current->filename,filename) == 0) {
            // Found a match!
            status = 1;
            *bottom = current->bottom;
            *top = current->top;
            *left = current->left;
            *right = current->right;
            *map_layer = current->map_layer;
            *draw_filled = current->draw_filled;
            *auto_maps = current->auto_maps;
            break;  // Exit the while loop
        }
        else {
            current = current->next;
        }
    }
    return(status);
}





// Saves the linked list pointed to by map_index_head to a file.
// Keeps the same order as the memory linked list.  Delete records
// in the in-memory linked list for which the "accessed" variable is
// 0 or filename is empty.
//
void index_save_to_file() {
    FILE *f;
    map_index_record *current;
    map_index_record *last;
    char out_string[MAX_FILENAME*2];


//fprintf(stderr,"Saving map index to file\n");

    f = fopen(MAP_INDEX_DATA,"w");

    if (f == NULL) {
        fprintf(stderr,"Couldn't create/update map index file: %s\n",
            MAP_INDEX_DATA);
        return;
    }

    current = map_index_head;
    last = current;

    while (current != NULL) {
        int i;

        // Make sure there aren't any weird characters in the
        // filename that might cause problems later.  Look for
        // control characters and convert them to string-end
        // characters.
        for ( i = 0; i < (int)strlen(current->filename); i++ ) {
            // Change any control characters to '\0' chars
            if (current->filename[i] < 0x20) {

                fprintf(stderr,"\nindex_save_to_file: Found control char 0x%02x in map name:\n%s\n",
                    current->filename[i],
                    current->filename);

                current->filename[i] = '\0';    // Terminate it here
            }
        }
 
        // Save to file if filename non-blank and record has the
        // accessed field set.
        if ( (current->filename[0] != '\0')
                && (current->accessed != 0) ) {

            // Write each object out to the file as one
            // comma-delimited line
            xastir_snprintf(out_string,
                sizeof(out_string),
                "%010lu,%010lu,%010lu,%010lu,%05d,%01d,%01d,%s\n",
                current->bottom,
                current->top,
                current->left,
                current->right,
                current->map_layer,
                current->draw_filled,
                current->auto_maps,
                current->filename);

            if (fprintf(f,"%s",out_string) < (int)strlen(out_string)) {
                // Failed to write
                fprintf(stderr,"Couldn't write objects to map index file: %s\n",
                    MAP_INDEX_DATA);
                current = NULL; // All done
            }
            // Set up pointers for next loop iteration
            last = current;
            current = current->next;
        }


        else {
            last = current;
            current = current->next;
        }
/*
//WE7U
        else {  // Delete this record from our list!  It's a record
                // for a map file that doesn't exist in the
                // filesystem anymore.
            if (last == current) {   // We're at the head of the list
                map_index_head = current->next;
                free(current);

                // Set up pointers for next loop iteration
                current = map_index_head;
                last = current;
            }
            else {  // Not the first record in the list
                map_index_record *gone;

                gone = current; // Save ptr to record we wish to delete 
                last->next = current->next; // Unlink from list
                free(gone);

                // Set up pointers for next loop iteration
                // "last" is still ok
                current = last->next;
            }
        }
*/
    }
    (void)fclose(f);
}





//WE7U2
// This function is currently not used.
//
// Function used to add map directories/files to the in-memory map
// index.  Causes an update of the index list in memory.  Input
// records are inserted in alphanumerical order.  This function is
// called from the index_restore_from_file() function below.  When
// this function is called the new record has all of the needed
// information in it.
//
/*
static void index_insert_sorted(map_index_record *new_record) {

    map_index_record *current = map_index_head;
    map_index_record *previous = map_index_head;
    int done = 0;
    int i;


    //fprintf(stderr,"index_insert_sorted: %s\n", new_record->filename );

    // Check for bad input.
    if (new_record == NULL) {
        fprintf(stderr,"index_insert_sorted: Bad input.\n");
        return;
    }
    // Make sure there aren't any weird characters in the filename
    // that might cause problems later.  Look for any control
    // characters and convert them to string-end characters.
    for ( i = 0; i < (int)strlen(new_record->filename); i++ ) {
        if (new_record->filename[i] < 0x20) {

            fprintf(stderr,"\nindex_insert_sorted: Found control char 0x%02x in map name:\n%s\n",
                new_record->filename[i],
                new_record->filename);

            new_record->filename[i] = '\0';    // Terminate it here
        }
    }
    // Check if the string is _now_ bogus
    if (new_record->filename[0] == '\0') {
        fprintf(stderr,"index_insert_sorted: Bad input.\n");
        return;
    }

    //if (map_index_head == NULL)
    //    fprintf(stderr,"Empty list\n");

    // Search for a matching filename in the linked list
    while ((current != NULL) && !done) {
        int test;

        //fprintf(stderr,"Comparing %s to\n          %s\n",
        //    current->filename, new_record->filename);

        test = strcmp(current->filename, new_record->filename);

        if (test == 0) {    // Found a match!
            int selected;

//fprintf(stderr,"Found a match: Updating entry for %s\n",new_record->filename);

            // Save this away temporarily.
            selected = current->selected;

            // Copy the fields across and then free new_record.  We
            // overwrite the contents of the existing record.
            xastir_snprintf(current->filename,
                MAX_FILENAME,
                "%s",
                new_record->filename);
            current->bottom = new_record->bottom;
            current->top = new_record->top;
            current->left = new_record->left;
            current->right = new_record->right;
            current->accessed = 1;
            current->map_layer = new_record->map_layer;
            current->draw_filled = new_record->draw_filled;
            current->selected = selected;   // Restore it
            current->auto_maps = new_record->auto_maps;

            free(new_record);   // Don't need it anymore

            done++; // Exit loop, "current" points to found record
        }
        else if (test > 0) {    // Found a string past us in the
                                // alphabet.  Insert ahead of this
                                // last record.

//fprintf(stderr,"Not Found, inserting: %s\n", new_record->filename);
//fprintf(stderr,"       Before record: %s\n", current->filename);

            if (current == map_index_head) {  // Start of list!
                // Insert new record at head of list
                new_record->next = map_index_head;
                map_index_head = new_record;
                //fprintf(stderr,"Inserting at head of list\n");
            }
            else {  // Insert between "previous" and "current"
                // Insert new record before "current"
                previous->next = new_record;
                new_record->next = current;
                //fprintf(stderr,"Inserting before current\n");
            }

            //fprintf(stderr,"Adding:%d:%s\n",strlen(filename),filename);
 
            // Fill in some default values for the new record that
            // don't exist in the map_index.sys file.
            new_record->selected = 0;

            if (       strstr(new_record->filename,".geo")
                    || strstr(new_record->filename,".GEO")
                    || strstr(new_record->filename,".Geo") ) {
                new_record->auto_maps = 0;
            }
            else {
                new_record->auto_maps = 1;
            }

            //current = current->next;
            done++;
        }
        else {  // Haven't gotten to the correct insertion point yet
            previous = current; // Save ptr to last record
            current = current->next;
        }
    }

    if (!done) {    // Matching record not found, add the record to
        // the end of the list.  "previous" points to the last
        // record in the list or NULL (empty list).

//fprintf(stderr,"Not Found: Adding to end: %s\n",new_record->filename);

        new_record->next = NULL;

        if (previous == NULL) { // Empty list
            map_index_head = new_record;
            //fprintf(stderr,"First record in new list\n");
        }
        else {  // Else at end of list
            previous->next = new_record;
            //fprintf(stderr,"Adding to end of list: %s\n",new_record->filename);
        }

        //fprintf(stderr,"Adding:%d:%s\n",strlen(new_record->filename),new_record->filename);
 
        // Fill in some default values for the new record.
        new_record->selected = 0;

        if (       strstr(new_record->filename,".geo")
                || strstr(new_record->filename,".GEO")
                || strstr(new_record->filename,".Geo") ) {
            new_record->auto_maps = 0;
        }
        else {
            new_record->auto_maps = 1;
        }
    }
}
*/





// sort map index
// simple bubble sort, since we should be sorted already
//
static void index_sort(void) {
    map_index_record *current, *previous, *next;
    int changed = 1;
    int loops = 0; // for debug stats

    previous = map_index_head;
    next = NULL;
    //  fprintf(stderr, "index_sort: start.\n");
    // check if we have any records at all, and at least two
    if ( (previous != NULL) && (previous->next != NULL) ) {
        current = previous->next;
        while ( changed == 1) {
            changed = 0;
            if (current->next != NULL) {next = current->next;}
            if ( strcmp( previous->filename, current->filename) >= 0 ) {
                // out of order - swap them
                current->next = previous;
                previous->next = next;
                map_index_head = current;
                current = previous;
                previous = map_index_head;
                changed = 1;
            }
            
            while ( next != NULL ) {
                if ( strcmp( current->filename, next->filename) >= 0 ) {
                    // out of order - swap them
                    current->next = next->next;
                    previous->next = next;
                    next->next = current;
                    // get ready for the next iteration
                    previous = next;  // current already moved ahead from the swap
                    next = current->next;
                    changed = 1;
                } else {
                    previous = current;
                    current = next;
                    next = current->next;
                }
            }
            previous = map_index_head;
            current = previous->next;
            next = current->next;
            loops++;
        }
    }
    // debug stats
    // fprintf(stderr, "index_sort: ran %d loops.\n", loops);
}





//WE7U2
// Snags the file and creates the linked list pointed to by the
// map_index_head pointer.  The memory linked list keeps the same
// order as the entries in the file.
//
void index_restore_from_file(void) {
    FILE *f;
    map_index_record *temp_record;
    map_index_record *last_record;
    char in_string[MAX_FILENAME*2];

//fprintf(stderr,"\nRestoring map index from file\n");

    if (map_index_head != NULL) {
        fprintf(stderr,"Warning: index_restore_from_file(): map_index_head was non-null!\n");
    }

    map_index_head = NULL;  // Starting with empty list
    last_record = NULL;

    f = fopen(MAP_INDEX_DATA,"r");
    if (f == NULL)  // No map_index file yet
        return;

    while (!feof (f)) { // Loop through entire map_index file

        // Read one line from the file
        if ( get_line (f, in_string, MAX_FILENAME*2) ) {

            if (strlen(in_string) >= 15) {   // We have some data.
                                            // Try to process the
                                            // line.
                char scanf_format[50];
                int processed;
                int i;

//fprintf(stderr,"%s\n",in_string);

                // Tweaked the string below so that it will track
                // along with MAX_FILENAME-1.  We're constructing
                // the string "%lu,%lu,%lu,%lu,%d,%d,%2000c", where
                // the 2000 example number is from MAX_FILENAME.
                xastir_snprintf(scanf_format,
                    sizeof(scanf_format),
                    "%s%d%s",
                    "%lu,%lu,%lu,%lu,%d,%d,%d,%",
                    MAX_FILENAME,
                    "c");
                //fprintf(stderr,"%s\n",scanf_format);

                // Malloc an index record.  We'll add it to the list
                // only if the data looks reasonable.
                temp_record = (map_index_record *)malloc(sizeof(map_index_record));
                memset(temp_record->filename, 0, sizeof(temp_record->filename));
                temp_record->next = NULL;

                processed = sscanf(in_string,
                    scanf_format,
                    &temp_record->bottom,
                    &temp_record->top,
                    &temp_record->left,
                    &temp_record->right,
                    &temp_record->map_layer,
                    &temp_record->draw_filled,
                    &temp_record->auto_maps,
                    temp_record->filename);

                // Do some reasonableness checking on the parameters
                // we just parsed.
//WE7U: Comparison is always false
                if ( (temp_record->bottom < 0l)
                        || (temp_record->bottom > 64800000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: bottom extent incorrect %lu in map name:\n%s\n",
                            temp_record->bottom,
                            temp_record->filename);
                }

 
//WE7U: Comparison is always false
               if ( (temp_record->top < 0l)
                        || (temp_record->top > 64800000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: top extent incorrect %lu in map name:\n%s\n",
                            temp_record->top,
                            temp_record->filename);
                }

//WE7U: Comparison is always false
                if ( (temp_record->left < 0l)
                        || (temp_record->left > 129600000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: left extent incorrect %lu in map name:\n%s\n",
                            temp_record->left,
                            temp_record->filename);
                }

//WE7U: Comparison is always false
                if ( (temp_record->right < 0l)
                        || (temp_record->right > 129600000l) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: right extent incorrect %lu in map name:\n%s\n",
                            temp_record->right,
                            temp_record->filename);
                }

                if ( (temp_record->map_layer < 0)
                        || (temp_record->map_layer > 99999) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: map_layer field incorrect %d in map name:\n%s\n",
                            temp_record->map_layer,
                            temp_record->filename);
                }

                if ( (temp_record->draw_filled < 0)
                        || (temp_record->draw_filled > 1) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: draw_filled field incorrect %d in map name:\n%s\n",
                            temp_record->draw_filled,
                            temp_record->filename);
                }

                if ( (temp_record->auto_maps < 0)
                        || (temp_record->auto_maps > 1) ) {
                    processed = 0;  // Reject this record
                    fprintf(stderr,"\nindex_restore_from_file: auto_maps field incorrect %d in map name:\n%s\n",
                            temp_record->auto_maps,
                            temp_record->filename);
                }

                // Check whether the filename is empty
                if (strlen(temp_record->filename) == 0) {
                    processed = 0;  // Reject this record
                }

                // Check for control characters in the filename.
                // Reject any that have them.
                for (i = 0; i < (int)strlen(temp_record->filename); i++)
                {
                    if (temp_record->filename[i] < 0x20) {

                        processed = 0;  // Reject this record
                        fprintf(stderr,"\nindex_restore_from_file: Found control char 0x%02x in map name:\n%s\n",
                            temp_record->filename[i],
                            temp_record->filename);
                    }
                }


                // Mark the record as accessed at this point.
                // At the stage where we're writing this list off to
                // disk, if the record hasn't been accessed by the
                // re-indexing, it doesn't get written.  This has
                // the effect of flushes deleted files from the
                // index quickly.
                temp_record->accessed = 1;

                // Default is not-selected.  Later we read in the
                // selected_maps.sys file and tweak some of these
                // fields.
                temp_record->selected = 0;

                temp_record->filename[MAX_FILENAME-1] = '\0';

                // If correct number of parameters
                if (processed == 8) {

                    //fprintf(stderr,"Restored: %s\n",temp_record->filename);
 
                    // Insert the new record into the in-memory map
                    // list in sorted order.
                    // --slow for large lists
                    // index_insert_sorted(temp_record);
                    // -- so we just add it to the end of the list
                    // and sort it at the end tp make sure nobody 
                    // messed us up by editting the file by hand
                    if ( last_record == NULL ) { // first record
                        map_index_head = temp_record;
                    } else {
                        last_record->next = temp_record;
                    }
                    last_record = temp_record;


                    // Remember that we may just have attached the
                    // record to our in-memory map list, or we may
                    // have free'ed it in the above function call.
                    // Set the pointer to NULL to make sure we don't
                    // try to do anything else with the memory.
                    temp_record = NULL;
                }
                else {  // sscanf didn't parse the proper number of
                        // items.  Delete the record.
                    free(temp_record);
                }
            }
        }
    }
    (void)fclose(f);
    // now that we have read the whole file, make sure it is sorted
    index_sort();  // probably should check for dup records
}





// map_indexer()
//
// Recurses through the map directories finding map extents
// and recording them in the map index.  Once the indexing is
// complete, write the current index out to a file.
//
// It'd be nice to call index_restore_from_file() from main.c:main()
// so that an earlier copy of the index is restored before the map
// display is created.
//
// If we set the "accessed" variable in the in-memory index to 0 for
// each record and then run the indexer, the save-to-file function
// will delete those with a value of 0 when writing to disk.  Those
// maps no longer exist in the filesystem and should be deleted.  We
// could either wipe them from the in-memory database at that time
// as well, or wipe the whole list and re-read it from disk to get
// the current list.
//
// If parameter is 0, we'll do the smart timestamp-checking
// indexing.
// If 1, we'll erase the in-memory index and do full indexing.
//
void map_indexer(int parameter) {
    struct stat nfile;
    int check_times = 1;
    FILE *f;
    map_index_record *current;
    map_index_record *backup_list_head = NULL;


    if (debug_level & 16)
        fprintf(stderr,"map_indexer() start\n");


    // Find the timestamp on the index file first.  Save it away so
    // that the timestamp for each map file can be compared to it.
    if (stat (MAP_INDEX_DATA, &nfile) != 0) {

        // File doesn't exist yet.  Create it.
        f = fopen(MAP_INDEX_DATA,"w");
        if (f != NULL)
            (void)fclose(f);
        else
            fprintf(stderr,"Couldn't create map index file: %s\n", MAP_INDEX_DATA);
        
        check_times = 0; // Don't check the timestamps.  Do them all. 
    }
    else {  // File exists
        map_index_timestamp = (time_t)nfile.st_mtime;
        check_times = 1;
    }


    if (parameter == 1) {   // Full indexing instead of timestamp-check indexing

        // Move the in-memory index to a backup pointer
        backup_list_head = map_index_head;
        map_index_head = NULL;

//        // Set the timestamp to 0 so that everything gets indexed
//        map_index_timestamp = (time_t)0l;

        check_times = 0;
    }


    // Set the "accessed" field to zero for every record in the
    // index.  Note that the list could be empty at this point.
    current = map_index_head;
    while (current != NULL) {
        current->accessed = 0;
        current = current->next;
    }


    if (check_times)
        map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_CHECK_TIMESTAMPS);
    else
        map_search (NULL, AUTO_MAP_DIR, NULL, NULL, (int)FALSE, INDEX_NO_TIMESTAMPS);

    if (debug_level & 16)
        fprintf(stderr,"map_indexer() middle\n");


    if (parameter == 1) {   // Full indexing instead of timestamp-check indexing
        // Copy the Properties from the backup list to the new list,
        // then free the backup list.
        map_index_copy_properties(map_index_head, backup_list_head);
    }

 
    // Save the updated index to the file
    index_save_to_file();

    if (debug_level & 16)
        fprintf(stderr,"map_indexer() end\n");
}





/* moved these here and made them static so it will function on FREEBSD */
#define MAX_ALERT 7000
// If we comment this out, we link, but get a segfault at runtime.
// Take out the "static" and we get a segfault when we zoom out too
// far with the lakes or counties shapefile loaded.  No idea why
// yet.  --we7u
//static alert_entry alert[MAX_ALERT];
static int alert_count;





/*******************************************************************
 * fill_in_new_alert_entries()
 *
 * Fills in the index and filename portions of any alert entries
 * that are missing them.  This function should be called at the
 * point where we've just received a new weather alert.
 *

//WE7U
// Later we should change this so that it doesn't scan the entire
// message list, but is passed the important info directly from the
// decode routines in db.c, and the message should NOT be added to
// the message list.
//WE7U

 *
 * This function is designed to use ESRI Shapefile map files.  The
 * base directory where the Shapefiles are located is passed to us
 * in the "dir" variable.
 *
 * map_search() fills in the filename field of the alert struct.
 * draw_shapefile_map() fills in the index field.
 *******************************************************************/
void fill_in_new_alert_entries(Widget w, char *dir) {
    int ii;
    char alert_scan[MAX_FILENAME], *dir_ptr;


    alert_count = MAX_ALERT - 1;

    // Set up our path to the wx alert maps
    memset (alert_scan, 0, sizeof (alert_scan));    // Zero our alert_scan string
    strncpy (alert_scan, dir, MAX_FILENAME-10); // Fetch the base directory
    strcat (alert_scan, "/");   // Complete alert directory is now set up in the string
    dir_ptr = &alert_scan[strlen (alert_scan)]; // Point to end of path

    // Iterate through the weather alerts we currently have on
    // our list.  It looks like we wish to just fill in the
    // alert struct and to determine whether the alert is within
    // our viewport here.  We don't really wish to draw the
    // alerts at this stage, that comes just a bit later in this
    // routine.
    for (ii = 0; ii < alert_max_count; ii++) {

        // Check whether alert slot is empty/filled
        if (alert_list[ii].title[0] == '\0') {  // It's empty,
            // skip this iteration of the loop and advance to
            // the next slot.
            continue;
        }
        else if (!alert_list[ii].filename[0]) { // Filename is
            // empty, we need to fill it in.

            //fprintf(stderr,"load_alert_maps() Title: %s\n",alert_list[ii].title);

            // The last parameter denotes loading into
            // pixmap_alerts instead of pixmap or pixmap_final.
            // Note that just calling map_search does not get
            // the alert areas drawn on the screen.  The
            // draw_map() function called by map_search just
            // fills in the filename field in the struct and
            // exits.
            //
            // The "warn" parameter (next to last) specifies whether
            // to dump warnings out to the console as well.  If the
            // warning was received on local RF or locally, warn the
            // operator (the weather must be near).
            map_search (w,
                alert_scan,
                &alert_list[ii],
                &alert_count,
                (int)alert_list[ii].flags[1],
                DRAW_TO_PIXMAP_ALERTS);

            //fprintf(stderr,"Title1:%s\n",alert_list[ii].title);
        }
    }
}





/*******************************************************************
 * load_alert_maps()
 *
 * Used to load weather alert maps, based on NWS weather alerts that
 * are received.  Called from create_image() and refresh_image().
 * This function is designed to use ESRI Shapefile map files.  The
 * base directory where the Shapefiles are located is passed to us
 * in the "dir" variable.
 *
 * map_search() fills in the filename field of the alert struct.
 * draw_shapefile_map() fills in the index field.
 *******************************************************************/
void load_alert_maps (Widget w, char *dir) {
    int ii, level;
    unsigned char fill_color[] = {  (unsigned char)0x69,    // gray86
                                    (unsigned char)0x4a,    // red2
                                    (unsigned char)0x63,    // yellow2
                                    (unsigned char)0x66,    // cyan2
                                    (unsigned char)0x61,    // RoyalBlue
                                    (unsigned char)0x64,    // ForestGreen
                                    (unsigned char)0x62 };  // orange3


// TODO:
// Figure out how to pass a quantity of zones off to the map drawing
// routines, then we can draw them all with one pass through each
// map file.  Alphanumerically sort the zones to make it easier for
// the map drawing functions?  Note that the indexing routines fill
// in both the filename and the shapefile index for each record.
//
// Alternative:  Call map_draw for each filename listed and have the
// draw_shapefile function iterate through the array looking for all
// filename matches, pulling non-negative indexes out of each index
// field for matches and drawing them.  That should be fast and
// require no sorting of the array.  Downside:  The alerts won't be
// layered based on alert level unless we modify the above:  Drawing
// each file once for each alert-level in the proper layering order.
// Perhaps we could keep a list of which filenames have been called,
// and only call each one once per load_alert_maps() call.


// Just for a test
//draw_shapefile_map (w, dir, filenm, alert, alert_color, destination_pixmap);
//draw_shapefile_map (w, dir, "c_16my01.shp", NULL, '\0', DRAW_TO_PIXMAP_ALERTS);


// Are we drawing them in reverse order so that the important 
// alerts end up drawn on top of the less important alerts?
// Actually, since the alert_list isn't currently ordered at all,
// perhaps we need to order it by priority, then by map file, so
// that we can draw the shapes from each map file in the correct
// order.  This might cause each map file to be drawn up to three
// times (once for each priority level), but that's better than
// calling each map for each zone as is done now.


    for (ii = alert_max_count - 1; ii >= 0; ii--) {

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now)
            return;

        if (disable_all_maps)
            return;

        //  Check whether the alert slot is filled/empty
        if (alert_list[ii].title[0] == '\0') // Empty slot
            continue;

//        if (alert_list[ii].flags[0] == 'Y' && (level = alert_active (&alert_list[ii], ALERT_ALL))) {
        if ( (level = alert_active(&alert_list[ii], ALERT_ALL) ) ) {
            if (level >= (int)sizeof (fill_color))
                level = 0;

            // The last parameter denotes drawing into pixmap_alert
            // instead of pixmap or pixmap_final.

            //fprintf(stderr,"Drawing %s\n",alert_list[ii].filename);
            //fprintf(stderr,"Title4:%s\n",alert_list[ii].title);

            // Attempt to draw alert
            if ( (alert_list[ii].alert_level != 'C')             // Alert not cancelled
                    && (alert_list[ii].index != -1) ) {          // Shape found in shapefile

                if (map_visible_lat_lon(alert_list[ii].bottom_boundary, // Shape visible
                        alert_list[ii].top_boundary,
                        alert_list[ii].left_boundary,
                        alert_list[ii].right_boundary,
                        alert_list[ii].title) ) {    // Error text if failure

                    draw_map (w,
                        dir,
                        alert_list[ii].filename,
                        &alert_list[ii],
                        fill_color[level],
                        DRAW_TO_PIXMAP_ALERTS,
                        1 /* draw_filled */ );
                }
                else {
                    //fprintf(stderr,"Alert not visible\n");
                }
            }
            else {
                // Cancelled alert, can't find the shapefile, or not
                // in our viewport, don't draw it!
                //fprintf(stderr,"Alert cancelled or shape not found\n");
            }
        }
    }

    //fprintf(stderr,"Done drawing all active alerts\n");

    if (alert_display_request()) {
        alert_redraw_on_update = redraw_on_new_data = 2;
    }
}





// Here's the head of our sorted-by-layer maps list
static map_index_record *map_sorted_list_head = NULL;


static void empty_map_sorted_list(void) {
    map_index_record *current = map_sorted_list_head;

    while (map_sorted_list_head != NULL) {
        current = map_sorted_list_head;
        map_sorted_list_head = current->next;
        free(current);
    }
}





// Insert a map into the list at the end of the maps with the same
// layer number.  We'll need to look up the parameters for it from
// the master map_index list and then attach a new record to our new
// sorted list in the proper place.
//
// This function should be called when we're first starting up
// Xastir and anytime that selected_maps.sys is changed.
//
static void insert_map_sorted(char *filename){
    map_index_record *current;
    map_index_record *last;
    map_index_record *temp_record;
    unsigned long bottom;
    unsigned long top;
    unsigned long left;
    unsigned long right;
    int map_layer;
    int draw_filled;
    int auto_maps;
    int done;


    if (index_retrieve(filename,
            &bottom,
            &top,
            &left,
            &right,
            &map_layer,
            &draw_filled,
            &auto_maps)) {    // Found a match

        // Allocate a new record
        temp_record = (map_index_record *)malloc(sizeof(map_index_record));

        // Fill in the values
        xastir_snprintf(temp_record->filename,MAX_FILENAME,"%s",filename);
        temp_record->bottom = bottom;
        temp_record->top = top;
        temp_record->left = left;
        temp_record->right = right;
        temp_record->map_layer = map_layer;
        temp_record->draw_filled = draw_filled;
        temp_record->auto_maps = auto_maps;
        temp_record->selected = 1;  // Always, we already know this!
        temp_record->accessed = 0;
        temp_record->next = NULL;

        // Now find the proper place for it and insert it in
        // layer-order into the list.
        current = map_sorted_list_head;
        last = map_sorted_list_head;
        done = 0;

        // Possible cases:
        // Empty list
        // insert at beginning of list
        // insert at end of list
        // insert between other entries

        if (map_sorted_list_head == NULL) {
            // Empty list.  Insert record.
            map_sorted_list_head = temp_record;
            done++;
        }
        else if (map_layer < current->map_layer) {
            // Insert at beginning of list
            temp_record->next = current;
            map_sorted_list_head = temp_record;
            done++;
        }
        else {  // Need to insert between records or at end of list
            while (!done && (current != NULL) ) {
                if (map_layer >= current->map_layer) {  // Not to our layer yet
                    last = current;
                    current = current->next;    // May point to NULL now
                }
                else if (map_layer < current->map_layer) {
                    temp_record->next = current;
                    last->next = temp_record;
                    done++;
                }
            }
        }
        // Handle running off the end of the list
        if (!done && (current == NULL) ) {
            last->next = temp_record;
        }
    }
    else {
        // We failed to find it in the map index
    }
}





/**********************************************************
 * load_auto_maps()
 *
 * NEW:  Uses the in-memory map_index to scan through the
 * maps.
 *
 * OLD: Recurses through the map directories looking for
 * maps to load.
 **********************************************************/
void load_auto_maps (Widget w, char *dir) {
    map_index_record *current = map_index_head;


    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        return;
    }

    // Skip the sorting of the maps if we don't need to do it
    if (re_sort_maps) {

        //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

        // Empty the sorted list first.  We'll create a new one.
        empty_map_sorted_list();

        // Run through the entire map_index linked list
        while (current != NULL) {
            if (auto_maps_skip_raster
                    && (   strstr(current->filename,"geo")
                        || strstr(current->filename,"GEO")
                        || strstr(current->filename,"tif")
                        || strstr(current->filename,"TIF"))) {
                // Skip this map
            }
            else {  // Draw this map

                //fprintf(stderr,"Loading: %s/%s\n",SELECTED_MAP_DIR,current->filename);

                //WE7U
                insert_map_sorted(current->filename);
 
/*
                draw_map (w,
                    SELECTED_MAP_DIR,
                    current->filename,
                    NULL,
                    '\0',
                    DRAW_TO_PIXMAP);
*/
            }
            current = current->next;
        }

        // All done sorting until something is changed in the Map
        // Chooser.
        re_sort_maps = 0;

        //fprintf(stderr,"*** DONE sorting the selected maps.\n");
    }

    // We have the maps in sorted order.  Run through the list and
    // draw them.  Only include those that have the auto_maps field
    // set to 1.
    current = map_sorted_list_head;
    while  (current != NULL) {

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        // Update to screen
        (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
        return;
    }

    if (disable_all_maps) {
        // Update to screen
        (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
        return;
    }

        // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

        // Draw the maps in sorted-by-layer order
        if (current->auto_maps) {
            draw_map (w,
                SELECTED_MAP_DIR,
                current->filename,
                NULL,
                '\0',
                DRAW_TO_PIXMAP,
                current->draw_filled);
        }

        current = current->next;
    }
}





/*******************************************************************
 * load_maps()
 *
 * Loads maps, draws grid, updates the display.
 *
 * We now create a linked list of maps in layer-order and use this
 * list to draw the maps.  This preserves the correct ordering in
 * all cases.  The layer to draw each map is specified in the
 * map_index.sys file (fifth parameter).  Eventually code will be
 * added to the Map Chooser in order to change the layer each map is
 * drawn at.
 *******************************************************************/
void load_maps (Widget w) {
    FILE *f;
    char mapname[MAX_FILENAME];
    int i;
    char selected_dir[MAX_FILENAME];
    map_index_record *current;


    if (debug_level & 16)
        fprintf(stderr,"Load maps start\n");

    HandlePendingEvents(app_context);
    if (interrupt_drawing_now) {
        // Update to screen
        (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
        return;
    }

    // Skip the sorting of the maps if we don't need to do it
    if (re_sort_maps) {

        //fprintf(stderr,"*** Sorting the selected maps by layer...\n");

        // Empty the sorted list first.  We'll create a new one.
        empty_map_sorted_list();
 
        // Make sure the string is empty before we start
        selected_dir[0] = '\0';

        // Create empty file if it doesn't exist
        (void)filecreate(SELECTED_MAP_DATA);

        f = fopen (SELECTED_MAP_DATA, "r");
        if (f != NULL) {
            if (debug_level & 16)
                fprintf(stderr,"Load maps Open map file\n");

            while (!feof (f)) {

                // Grab one line from the file
                if ( fgets( mapname, MAX_FILENAME-1, f ) != NULL ) {

                    // Forced termination (just in case)
                    mapname[MAX_FILENAME-1] = '\0';

                    // Get rid of the newline at the end
                    for (i = strlen(mapname); i > 0; i--) {
                        if (mapname[i] == '\n')
                            mapname[i] = '\0'; 
                    }

                    if (debug_level & 16)
                        fprintf(stderr,"Found mapname: %s\n", mapname);

                    // Test for comment
                    if (mapname[0] != '#') {


                        // Check whether it's a directory that was
                        // selected.  If so, save it in a special
                        // variable and use that to match all the files
                        // inside the directory.  Note that with the way
                        // we have things ordered in the list, the
                        // directories appear before their member files.
                        if (mapname[strlen(mapname)-1] == '/') {
                            // Found a directory.  Save the name.
                            xastir_snprintf(selected_dir,
                                sizeof(selected_dir),
                                "%s",
                                mapname);

//fprintf(stderr,"Selected %s directory\n",selected_dir);

                            // Here we need to run through the map_index
                            // list to find all maps that match the
                            // currently selected directory.  Attempt to
                            // load all of those maps as well.

//fprintf(stderr,"Load all maps under this directory: %s\n",selected_dir);

                            // Point to the start of the map_index list
                            current = map_index_head;

                            while (current != NULL) {

                               if (strstr(current->filename,selected_dir)) {

                                    if (current->filename[strlen(current->filename)-1] != '/') {

//fprintf(stderr,"Loading: %s\n",current->filename);

                                        //WE7U
                                        insert_map_sorted(current->filename);
 
/*
                                        draw_map (w,
                                            SELECTED_MAP_DIR,
                                            current->filename,
                                            NULL,
                                            '\0',
                                            DRAW_TO_PIXMAP);
*/

                                    }
                                }
                                current = current->next;
                            }
                        }
                        // Else must be a regular map file
                        else { 
//fprintf(stderr,"%s\n",mapname);
//start_timer();

                            //WE7U
                            insert_map_sorted(mapname);

/*
                            draw_map (w,
                                SELECTED_MAP_DIR,
                                mapname,
                                NULL,
                                '\0',
                                DRAW_TO_PIXMAP);
*/

//stop_timer();
//print_timer_results();

                            if (debug_level & 16)
                                fprintf(stderr,"Load maps -%s\n", mapname);

                            XmUpdateDisplay (da);
                        }
                    }
                }
                else {  // We've hit EOF
                    break;
                }

            }
            (void)fclose (f);
            statusline(" ",1);      // delete status line
        }
        else
            fprintf(stderr,"Couldn't open file: %s\n", SELECTED_MAP_DATA);

        // All done sorting until something is changed in the Map
        // Chooser.
        re_sort_maps = 0;

        //fprintf(stderr,"*** DONE sorting the selected maps.\n");
    }

    // We have the maps in sorted order.  Run through the list and
    // draw them.
    current = map_sorted_list_head;
    while  (current != NULL) {

        HandlePendingEvents(app_context);
        if (interrupt_drawing_now) {
            statusline(" ",1);      // delete status line
            // Update to screen
            (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
            return;
        }

        if (disable_all_maps) {
            // Update to screen
            (void)XCopyArea(XtDisplay(da),pixmap,XtWindow(da),gc,0,0,screen_width,screen_height,0,0);
            return;
        }
 
        // Debug
//        fprintf(stderr,"Drawing level:%05d, file:%s\n",
//            current->map_layer,
//            current->filename);

        // Draw the maps in sorted-by-layer order
        draw_map (w,
            SELECTED_MAP_DIR,
            current->filename,
            NULL,
            '\0',
            DRAW_TO_PIXMAP,
            current->draw_filled);
 

        current = current->next;
    }

    if (debug_level & 16)
        fprintf(stderr,"Load maps stop\n");
}


