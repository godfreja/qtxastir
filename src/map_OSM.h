/*
 * $Id: map_OSM.h,v 1.2 2010/06/15 14:28:40 we7u Exp $
 *
 * Copyright (C) 2010 The Xastir Group
 *
 * This file was contributed by Jerry Dunmire, KA6HLD.
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
 */
#define MAX_OSMSTYLE 1000  // max characters in the a OSM style argument
#define MAX_OSM_URL  1000  // max characters for a OSM URL

void adj_to_OSM_level(
        long *new_scale_x,
        long *new_scale_y);

void draw_OSM_map (Widget w,
        char *filenm,
        int destination_pixmap,
        char *url,
        char *style,
        int nocache);


