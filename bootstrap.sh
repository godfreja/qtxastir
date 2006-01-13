#!/bin/sh
#
# $Id: bootstrap.sh,v 1.10 2006/01/13 15:34:14 we7u Exp $
#
# Copyright (C) 2000-2005  The Xastir Group
#
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#

echo "5... Removing autom4te.cache directory"
rm -rf autom4te.cache

echo "4... Running aclocal"
aclocal

echo "3... Running autoheader"
autoheader

echo "2... Running autoconf"
autoconf

# Cygwin needs these params to be separate
echo "1... Running automake"
automake -a -c

echo "Bootstrap complete."
