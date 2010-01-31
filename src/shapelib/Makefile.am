
# $Id: Makefile.am,v 1.10 2010/01/31 02:12:25 we7u Exp $
# Copyright (C) 2004-2010  The Xastir Group

noinst_LIBRARIES=libshape.a
bin_PROGRAMS=shpcreate dbfcreate shpadd dbfadd
libshape_a_SOURCES=shpopen.c \
		shptree.c \
		dbfopen.c \
		shapefil.h

shpcreate_LDADD=libshape.a
dbfcreate_LDADD=libshape.a
shpadd_LDADD=libshape.a
dbfadd_LDADD=libshape.a

EXTRA_DIST=ChangeLog LICENSE.LGPL README README.tree dbf_api.html \
	dbfadd.c dbfcreate.c dbfdump.c libtool makefile.vc makeshape.sh \
        mkinstalldirs shapelib.def shapelib.html shp_api.html \
	shpadd.c shpcreate.c shpdump.c shprewind.c shptest.c shptreedump.c \
	shputils.c stream1.out stream1.sh stream2.out stream2.sh stream3.out
DIST_SUBDIRS=contrib
