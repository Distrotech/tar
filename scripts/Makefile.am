# Copyright (C) 2004 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

libexec_SCRIPTS=backup.sh dump-remind
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT=backup.sh dump-remind
sbin_SCRIPTS=backup
EXTRA_DIST=$(libexec_SCRIPTS) backup.in dump-remind.in backup-specs
CLEANFILES=backup dump-remind

backup: $(srcdir)/backup.in
	sed "s,\@libexecdir\@,$(libexecdir),;\
             s,\@sysconfdir\@,$(sysconfdir),;\
             s,\@PACKAGE\@,$(PACKAGE),;\
             s,\@VERSION\@,$(VERSION),;\
             s,\@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),;\
             s,\@DATE_FORMAT_OK\@,$(DATE_FORMAT_OK),;@BACKUP_SED_COND@" $? > $@

dump-remind: $(srcdir)/dump-remind.in
	sed "s,\@libexecdir\@,$(libexecdir),;\
             s,\@sysconfdir\@,$(sysconfdir),;\
             s,\@PACKAGE\@,$(PACKAGE),;\
             s,\@VERSION\@,$(VERSION),;\
             s,\@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),;\
             s,\@DATE_FORMAT_OK\@,$(DATE_FORMAT_OK),;@BACKUP_SED_COND@" $? > $@