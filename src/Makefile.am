# vim:set noet ts=4:
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

AM_CFLAGS = \
	$(IBUS_CFLAGS) \
	$(M17N_CFLAGS) \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	$(NULL)
AM_LDADD = \
	$(IBUS_LIBS) \
	$(M17N_LIBS) \
	$(NULL)

check_PROGRAMS = \
	test-m17n \
	$(NULL)

TESTS = \
	$(check_PROGRAMS) \
	$(NULL)

TESTS_ENVIRONMENT = IBUS_M17N_PKGDATADIR=$(builddir)

test_m17n_SOURCES = \
	test.c \
	$(NULL)
test_m17n_CFLAGS = \
	$(AM_CFLAGS) \
	$(NULL)
test_m17n_LDADD = \
	libm17ncommon.la \
	$(AM_LDADD) \
	$(NULL)

test: ibus-engine-m17n
	$(builddir)/ibus-engine-m17n

libexec_PROGRAMS = ibus-engine-m17n

noinst_LTLIBRARIES = libm17ncommon.la

libm17ncommon_la_SOURCES = \
	m17nutil.c \
	m17nutil.h \
	$(NULL)
libm17ncommon_la_LIBADD = $(LTLIBOBJS)

ibus_engine_m17n_SOURCES = \
	main.c \
	engine.c \
	engine.h \
	$(NULL)
ibus_engine_m17n_LDADD = \
	libm17ncommon.la \
	$(IBUS_LIBS) \
	$(M17N_LIBS) \
	$(NULL)

if HAVE_GTK
libexec_PROGRAMS += ibus-setup-m17n

ibus_setup_m17n_SOURCES = \
	setup.c \
	$(NULL)
ibus_setup_m17n_CFLAGS = \
	$(GTK_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)
ibus_setup_m17n_LDADD = \
	libm17ncommon.la \
	$(IBUS_LIBS) \
	$(M17N_LIBS) \
	$(GTK_LIBS) \
	$(NULL)

setupdir = $(pkgdatadir)/setup
dist_setup_DATA = \
	ibus-m17n-preferences.ui \
	$(NULL)
endif

pkgdata_DATA = \
	default.xml \
	$(NULL)

component_DATA = \
	m17n.xml \
	$(NULL)
componentdir = $(datadir)/ibus/component

EXTRA_DIST = \
	m17n.xml.in \
	default.xml \
	$(desktop_in_in_files) \
	$(schemas_DATA) \
	$(NULL)

DISTCLEANFILES = \
	m17n.xml.in \
	$(NULL)

CLEANFILES = \
	m17n.xml \
	$(desktop_DATA)	\
	$(desktop_in_files) \
	$(NULL)

edit = sed \
	-e 's|@VERSION[@]|$(VERSION)|g' \
	-e 's|@libexecdir[@]|$(libexecdir)|g' \
	-e 's|@pkgdatadir[@]|$(pkgdatadir)|g'

m17n.xml: m17n.xml.in
	$(AM_V_GEN) $(edit) $< > $@.tmp && mv $@.tmp $@

desktop_in_in_files = ibus-setup-m17n.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.in.in=.in)
desktop_files = $(desktop_in_files:.desktop.in=.desktop)

$(desktop_in_files):
	$(AM_V_GEN) $(MSGFMT) --desktop --template $@.in -o $@-t \
	  -d $(top_srcdir)/po && mv $@-t $@

$(desktop_files):
	@rm -f $@
	$(AM_V_GEN) srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@-t && mv $@-t $@

ibus-setup-m17n.desktop.in: ibus-setup-m17n.desktop.in.in
ibus-setup-m17n.desktop: ibus-setup-m17n.desktop.in

desktopdir=$(datadir)/applications
desktop_DATA = $(desktop_files)

schemas_DATA = \
	org.freedesktop.ibus.engine.m17n.gschema.xml \
	$(NULL)

schemasdir = $(datadir)/glib-2.0/schemas/

install-data-hook:
	if test -z "$(DESTDIR)"; then \
	    glib-compile-schemas $(schemasdir); \
	fi

uninstall-hook:
	SCHEMAS_FILES=`ls $(schemasdir)/*.gschema.xml` || true;         \
	if test -z "$$SCHEMAS_FILES" &&                                 \
	    test -f $(schemasdir)/gschemas.compiled; then               \
	    rm $(schemasdir)/gschemas.compiled;                         \
	fi
