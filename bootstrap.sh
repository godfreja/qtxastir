#!/bin/sh
#
# $Id: bootstrap.sh,v 1.8 2004/09/27 00:29:04 tvrusso Exp $
#
# Copyright (C) 2000-2004  The Xastir Group
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

echo "Bootstrap complete."
