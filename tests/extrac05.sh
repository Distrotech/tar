#! /bin/sh
# This file is part of GNU tar testsuite.
# Copyright (C) 2004 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
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

# Problem: when extracting selected members from a PAX archive,
# tar 1.14 incorrectly deemed all members to be sparse and
# therefore was not able to properly skip them.
#
# Reported by: Luca Fibbi <fibbi@lamma.rete.toscana.it>
#
# References: <3.0.6.32.20040809113727.00a30e50@localhost>
# http://lists.gnu.org/archive/html/bug-tar/2004-08/msg00008.html

. ./preset
TAR_ARCHIVE_FORMATS="posix"
. $srcdir/before

genfile --length 118 > jeden
genfile --length 223 > dwa
genfile --length 517 > trzy
mksparse sparsefile 512 0 ABCD 1M EFGH 2000K IJKL
genfile --length 110 > cztery

tar cf archive jeden dwa trzy cztery

cat > list <<EOF
jeden
cztery
EOF

mkdir dir
cd dir

tar xvfT ../archive ../list

cd ..

out="\
jeden
cztery
"

. $srcdir/after

