PREFIX ?= /usr/local
OPTIMIZATIONS ?= -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -O3 -fno-finite-math-only -pipe
ENABLE_CONVOLUTION ?= no

INCLUDEPATHS = -I../b_overdrive/ -I../b_whirl  -I../b_reverb -I../b_conv
bindir = $(PREFIX)/bin
sharedir = $(PREFIX)/share/setBfree
CFLAGS = $(OPTIMIZATIONS) -Wall -fPIC
CFLAGS+= $(INCLUDEPATHS)
CFLAGS+= -DSHAREDIR="\"$(sharedir)\""
CFLAGS+= -DVERSION="\"$(VERSION)\""
LOADLIBES = -lm -lpthread
BXCC=$(CC)

OBJS= \
	midi.o \
	vibrato.o \
	tonegen.o \
	program.o \
	pgmParser.o \
	cfgParser.o \
	main.o \
	../b_reverb/reverb.o \
	../b_whirl/eqcomp.o \
	../b_whirl/whirl.o \
	../b_overdrive/overdrive.o

HEADERS = \
  cfgParser.h \
	main.h \
	midi.h \
	pgmParser.h \
	program.h \
	vibrato.h \
	tonegen.h \
	defaultpgm.h \
	../b_reverb/reverb.h \
	../b_whirl/eqcomp.h \
	../b_whirl/whirl.h \
	../b_overdrive/overdrive.h


ifeq ($(shell pkg-config --exists jack || echo no), no)
  $(error "JACK is required - install libjack-dev or libjack-jackd2-dev")
endif

ifeq ($(shell pkg-config --exists alsa && echo yes), yes)
  CFLAGS+=-DHAVE_ASEQ `pkg-config --cflags alsa`
  LOADLIBES+=`pkg-config --libs alsa`
else
  $(info "compliling w/o ALSA Sequencer - libasound is missing - JACK-midi is alwasy on")
endif


CFLAGS+=`pkg-config --cflags jack`
LOADLIBES+=`pkg-config --libs jack`

ifeq ($(shell pkg-config --exists sndfile \
	&& test -f /usr/include/zita-convolver.h -o -f /usr/local/include/zita-convolver.h \
	&& echo yes), $(ENABLE_CONVOLUTION))
  CFLAGS+= -DHAVE_ZITACONVOLVE `pkg-config --cflags sndfile`
  LOADLIBES+=-lzita-convolver `pkg-config --libs sndfile`
  OBJS+=../b_conv/convolution.o
  HEADERS+=../b_conv/convolution.h
  BXCC=$(CXX)
else
  $(info "compliling w/o ZITA Convolution Reverb -- needs libzita-convolver3 and libsndfile")
endif

all: setBfree

setBfree: $(OBJS) $(HEADERS)
	$(BXCC) -o $(@) $(OBJS) $(LOADLIBES) $(CFLAGS)

programd: program.c pgmParser.c program.h pgmParser.h main.h
	$(CC) -Wall -o $(@) -DPRG_MAIN program.c pgmParser.c $(INCLUDEPATHS)

configd: cfgParser.c main.h
	$(CC) -Wall -o $(@) -DCFG_MAIN cfgParser.c

defaultpgm.h: programd ../pgm/default.pgm
	$(info re-generating default program include-file)
	./programd ../pgm/default.pgm > defaultpgm.h

# override .o in parent dirs -- needed for parallel builds

../b_reverb/reverb.o: ../b_reverb/reverb.[ch]
	+$(MAKE) -C ../b_reverb reverb.o

../b_whirl/eqcomp.o: ../b_whirl/eqcomp.[ch]
	+$(MAKE) -C ../b_whirl eqcomp.o

../b_whirl/whirl.o: ../b_whirl/whirl.[ch]
	+$(MAKE) -C ../b_whirl whirl.o

../b_overdrive/overdrive.o: \
  ../b_overdrive/filterTools.[ch] \
  ../b_overdrive/overdrive.h \
  ../b_overdrive/overmaker.[ch] \
  ../b_overdrive/overmakerdefs.h \
  ../b_overdrive/ovt_biased.[ch]
	+$(MAKE) -C ../b_overdrive overdrive.o

../b_conv/convolution.o: ../b_conv/convolution.cc ../b_conv/convolution.h
	+$(MAKE) -C ../b_conv convolution.o

clean:
	rm -f *.o core *.log *~ setBfree configd programd

install: setBfree
	install -d $(DESTDIR)$(bindir)
	install -m755 setBfree $(DESTDIR)$(bindir)
	install -m755 setBfree-start.sh $(DESTDIR)$(bindir)/setBfree-start
	install -d $(DESTDIR)$(sharedir)/cfg
	install -d $(DESTDIR)$(sharedir)/pgm
	install -m644 ../cfg/*.cfg $(DESTDIR)$(sharedir)/cfg
	install -m644 ../pgm/*.pgm $(DESTDIR)$(sharedir)/pgm

uninstall:
	rm -f $(DESTDIR)$(bindir)/setBfree
	rm -f $(DESTDIR)$(sharedir)/cfg/*
	rm -f $(DESTDIR)$(sharedir)/pgm/*
	-rmdir $(DESTDIR)$(bindir)
	-rmdir $(DESTDIR)$(sharedir)/cfg
	-rmdir $(DESTDIR)$(sharedir)/pgm
	-rmdir $(DESTDIR)$(sharedir)

.PHONY: clean all install uninstall