server_dot_in = Accessibility_Registry.server.in

$(server_dot_in): $(server_dot_in).in
	sed -e "s|\@REGISTRYD_PATH\@|$(libexecdir)|" $< > $@ 

libexec_PROGRAMS = at-spi-registryd

INCLUDES = -I$(top_srcdir)          \
           -I$(top_builddir)        \
           -I$(top_builddir)/libspi \
           -I$(top_srcdir)/libspi   \
           $(WARN_CFLAGS)	\
           $(REGISTRYD_CFLAGS) \
	   -DGTK_LIBDIR=\"$(libdir)\"

at_spi_registryd_SOURCES =		\
	desktop.c		\
	desktop.h		\
	deviceeventcontroller.c	\
	deviceeventcontroller.h	\
	registry-main.c		\
	registry.c		\
	registry.h		\
	ucs2keysym.c

LDADD = $(top_builddir)/libspi/libspi.la $(X_LIBS) $(SM_LIBS) $(XTST_LIBS) $(XEVIE_LIBS) $(REGISTRYD_LIBS) 

serverinfodir = $(libdir)/bonobo/servers
serverinfo_DATA = Accessibility_Registry.server

default_sessiondir = $(sysconfdir)/xdg/autostart
default_session_in_in_files = 				\
	at-spi-registryd.desktop.in.in
default_session_in_files = $(default_session_in_in_files:.desktop.in.in=.desktop.in)
default_session_DATA = $(default_session_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

%.desktop.in: %.desktop.in.in Makefile.am
	sed -e "s,\@registryddir\@,$(libexecdir)," \
		< $< | sed -e "s,\@gconfkey\@,accessibility," \
		> $@

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = Accessibility_Registry.server.in.in
CLEANFILES = 			\
	$(serverinfo_DATA)	\
	$(server_dot_in)	\
	$(default_session_DATA)	\
	$(default_session_in_files)

DISTCLEANFILES =					\
	$(default_session_in_in_files)

