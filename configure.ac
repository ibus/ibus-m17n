# vim:set et ts=4:
#
# ibus-m17n - The m17n engine for IBus
#
# Copyright (c) 2007-2008 Huang Peng <shawn.p.huang@gmail.com>
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
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# if not 1, append datestamp to the version number.
m4_define([ibus_released], [1])
m4_define([ibus_major_version], [1])
m4_define([ibus_minor_version], [3])
m4_define([ibus_micro_version], [3])
m4_define(ibus_maybe_datestamp,
    m4_esyscmd([if test x]ibus_released[ != x1; then date +.%Y%m%d | tr -d '\n\r'; fi]))

m4_define([ibus_version],
    ibus_major_version.ibus_minor_version.ibus_micro_version[]ibus_maybe_datestamp)

AC_INIT([ibus-m17n], [ibus_version], [http://code.google.com/p/ibus/issues/entry],[ibus-m17n])
AM_INIT_AUTOMAKE([1.10])
AC_GNU_SOURCE

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# define PACKAGE_VERSION_* variables
AS_VERSION
AS_NANO
AM_SANITY_CHECK
AM_MAINTAINER_MODE
AM_DISABLE_STATIC
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_LIBTOOL

# check ibus
PKG_CHECK_MODULES(IBUS, [
    ibus-1.0 >= 1.3
])

# check m17n
PKG_CHECK_MODULES(M17N, [
    m17n-shell
])

# check gtk for setup
AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=2.0|3.0|no],[which gtk+ version to compile against (default: 2.0)])],
  [case "$with_gtk" in
     2.0|3.0|no) ;;
     *) AC_MSG_ERROR([invalid gtk version specified]) ;;
   esac],
  [with_gtk=2.0])
AC_MSG_RESULT([$with_gtk])

case "$with_gtk" in
  2.0) GTK_API_VERSION=2.0
       GTK_REQUIRED=2.12.12
       ;;
  3.0) GTK_API_VERSION=3.0
       GTK_REQUIRED=2.90.5
       ;;
esac

if test x$with_gtk != xno; then
  PKG_CHECK_MODULES([GTK], [gtk+-$GTK_API_VERSION >= $GTK_REQUIRED], ,
    with_gtk=no)
  AC_DEFINE([HAVE_SETUP], [1], [Define if setup dialog is installed])
fi

AM_CONDITIONAL([HAVE_GTK],[test x$with_gtk != xno])

# check if minput_list, which is available in m17n-lib 1.6.2+ (CVS)
save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
CFLAGS="$CFLAGS $M17N_CFLAGS"
LIBS="$LIBS $M17N_LIBS"
AC_REPLACE_FUNCS([minput_list])
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

# check if ibus_engine_get_surrounding_text, which is available in ibus-1.3.99+ (git master)
save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
CFLAGS="$CFLAGS $IBUS_CFLAGS"
LIBS="$LIBS $IBUS_LIBS"
AC_CHECK_FUNCS([ibus_engine_get_surrounding_text])
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

# define GETTEXT_* variables
GETTEXT_PACKAGE=ibus-m17n
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Define to the read-only architecture-independent data directory.])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.16.1)


# OUTPUT files
AC_CONFIG_FILES([ po/Makefile.in
Makefile
ibus-m17n.spec
src/Makefile
src/m17n.xml.in
src/default.xml.in
icons/Makefile
m4/Makefile
])

AC_OUTPUT
