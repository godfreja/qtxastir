/*
 * $Id: x_spider.h,v 1.6 2005/01/08 09:46:48 we7u Exp $
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003-2005  The Xastir Group
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

#ifndef __XASTIR_SERVER_H
#define __XASTIR_SERVER_H

#include <pthread.h>
//#include "xastir.h"


#define NET_CONNECT_TIMEOUT 20
#define SERV_TCP_PORT       2023

char *pname;
extern int pipe_xastir_to_server;
extern int pipe_server_to_xastir;

extern int writen(register int fd, register char *ptr, register int nbytes);
extern int readline(register int fd, register char *ptr, register int maxlen);
extern int Fork_server(int argc, char *argv[], char *envp[]);

#endif /* XASTIR_SERVER_H */

