#!/usr/bin/perl -n

# $Id: langPigLatin.pl,v 1.2 2008/08/30 05:19:14 we7u Exp $

# Copyright (C) 2007-2008  The Xastir Group
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# Look at the README for more information on the program.


# Run it like this:
#
#   cd xastir/config
#   ../scripts/langPigLatin.pl <language-English.sys >language-PigLatin.sys
#
# If you would like, replace the language-English.sys file in the
# destination directory (usually "/usr/local/share/xastir/config")
# with this new file (renaming it to "language-English.sys" of
# course), then restart Xastir and you'll have it displaying in Pig
# Latin!


# Regex strings derived from:
# http://www.perlmonks.org/?node_id=3586


# Change the "$Id: langPigLatin.pl,v 1.2 2008/08/30 05:19:14 we7u Exp $" RCS tag to show that we translated the file.
if (m/^#.*\$Id:/) {
    print "# language-PigLatin.sys, translated from language-English.sys\n";
    print "# Please do not edit this derived file.\n";
    next;
}
# Skip other comment lines
if (m/^#/) { next; }

# Split each line by the '|' character
@pieces = split /\|/;

# Modify the second portion of each line only
$pieces[1] =~ s/\b(qu|y(?=[^t])|[^\W\daeiouy]*)([a-z']+)/$2.($1||w).ay/eg;

# Combine the line again for output to STDOUT
print join '|', @pieces;


