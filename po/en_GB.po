# British English messages for GNU tar
# Copyright (C) 2003 Free Software Foundation, Inc.
# Paul Eggert <eggert@twinsun.com>, 2003.
#
msgid ""
msgstr ""
"Project-Id-Version: GNU tar 1.13.25\n"
"Report-Msgid-Bugs-To: bug-tar@gnu.org\n"
"POT-Creation-Date: 2003-07-04 00:11-0700\n"
"PO-Revision-Date: 2003-07-04 00:11-0700\n"
"Last-Translator: Paul Eggert <eggert@twinsun.com>\n"
"Language-Team: English <en@translate.freefriends.org>\n"
"MIME-Version: 1.0\n"
"Content-Type: text/plain; charset=US-ASCII\n"
"Content-Transfer-Encoding: 8bit\n"

#: lib/getopt.c:838 lib/getopt.c:841
#, c-format
msgid "%s: unrecognized option `--%s'\n"
msgstr "%s: unrecognised option `--%s'\n"

#: lib/getopt.c:849 lib/getopt.c:852
#, c-format
msgid "%s: unrecognized option `%c%s'\n"
msgstr "%s: unrecognised option `%c%s'\n"

# POSIX requires the word "illegal" in the POSIX locale, but the error
# is not really against the law.  This is not the POSIX locale, so fix
# the wording.
#: lib/getopt.c:899 lib/getopt.c:902
#, c-format
msgid "%s: illegal option -- %c\n"
msgstr "%s: unrecognised option `-%c'\n"

# POSIX requires the word "illegal" in the POSIX locale, but the error
# is not really against the law.  This is not the POSIX locale, so fix
# the wording.
#: lib/getopt.c:908 lib/getopt.c:911
#, fuzzy, c-format
msgid "%s: invalid option -- %c\n"
msgstr "%s: unrecognised option `-%c'\n"
