# Makefile.am for seq42

SUBDIRS = pixmaps

AM_CXXFLAGS = $(GTKMM_CFLAGS) $(JACK_CFLAGS) $(LASH_CFLAGS) -Wall
seq42_LDADD = $(GTKMM_LIBS) $(ALSA_LIBS) $(JACK_LIBS) $(LASH_LIBS)

bin_PROGRAMS = seq42

seq42_SOURCES = \
	configfile.cpp configfile.h \
	controllers.h \
	event.cpp event.h \
	font.cpp font.h \
	globals.h \
	keybindentry.cpp keybindentry.h \
	lash.cpp lash.h \
	lfownd.cpp lfownd.h \
	maintime.cpp maintime.h \
	mainwnd.cpp mainwnd.h \
	midibus.cpp midibus.h \
	midibus_portmidi.cpp midibus_portmidi.h \
	midifile.cpp midifile.h \
	mutex.cpp mutex.h \
	options.cpp options.h \
	optionsfile.cpp optionsfile.h \
	perfnames.cpp perfnames.h \
	perform.cpp perform.h \
	perfroll.cpp perfroll.h \
	perfroll_input.cpp perfroll_input.h \
	perftime.cpp perftime.h \
	seq42.cpp \
	seqdata.cpp seqdata.h \
	seqedit.cpp seqedit.h \
	seqevent.cpp seqevent.h \
	seqkeys.cpp seqkeys.h \
	seqlist.cpp seqlist.h \
	seqroll.cpp seqroll.h \
	seqtime.cpp seqtime.h \
	sequence.cpp sequence.h \
	tempo.cpp tempo.h \
	tempopopup.cpp tempopopup.h \
	track.cpp track.h \
	trackedit.cpp trackedit.h \
	trackmenu.cpp trackmenu.h \
	userfile.cpp userfile.h

EXTRA_DIST = configwin32.h

MOSTLYCLEANFILES = *~

