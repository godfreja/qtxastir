#!/bin/sh
#
# $Id: bootstrap.sh,v 1.1 2006/01/25 19:36:27 n0vh Exp $
#
# Copyright (C) 2000-2005  The Xastir Group
#
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#

echo "Removing autom4te.cache directory"
rm -rf autom4te.cache

echo "Running aclocal"
aclocal

echo "Running autoheader"
autoheader

echo "Running autoconf"
autoconf

# Cygwin needs these params to be separate
echo "Running automake"
automake -a -c

