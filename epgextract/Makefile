# tvhepg-extract — standalone EPG-from-TS extractor, reusing tvheadend's real
# EIT parser (eit.c/epg.c with this fork's ARIB patches).
#
# Links a curated subset of tvheadend's already-compiled objects (from
# ../src/build.linux) + this tool's sources + a stub layer for leaf subsystems
# we don't need (dvr/htsp/notify/imagecache/subscriptions/...). Run inside the
# builder container AFTER `make build` produced the tvheadend objects:
#     make -C epgextract        # -> epgextract/tvhepg-extract

TVH ?= ../src/src
BLD ?= ../src/build.linux/src

CC     ?= gcc
CFLAGS += -fms-extensions -O2 -I$(TVH) -I../src/build.linux -I../src
# Minimal runtime libs actually used by the curated object subset:
#   aribb24  - ARIB-STD-B24 text decode (dvb_charset)   [essential]
#   pcre2-8  - idnode/prop regex   crypto - uuid/idnode hashing
#   z        - filebundle gzip     m/pthread/dl/rt - libc
# NOT linked: aribb25/dvbcsa/pcsclite/avahi/dbus/uriparser/x264 (their tvheadend
# objects aren't in the subset), so the .deb needs none of them.
LDLIBS := -laribb24 -lpcre2-8 -lcrypto -lz -lm -lpthread -ldl -lrt

# Curated subset of prebuilt tvheadend objects (the real EIT/EPG/idnode/charset
# code). Anything eit.c/epg.c reference that is NOT here is satisfied by
# tvhepg_stubs.c (leaf no-ops) — see that file's ledger.
TVHOBJS := \
  $(BLD)/epggrab/module/eit.o $(BLD)/epggrab/module/eitpatternlist.o \
  $(BLD)/epggrab/support/freesat_huffman.o \
  $(BLD)/epg.o $(BLD)/epgdb.o $(BLD)/channels.o \
  $(BLD)/idnode.o $(BLD)/prop.o \
  $(BLD)/lang_str.o $(BLD)/lang_codes.o $(BLD)/string_list.o \
  $(BLD)/input/mpegts/dvb_charset.o $(BLD)/input/mpegts/dvb_support.o \
  $(BLD)/input/mpegts/dvb_psi_lib.o \
  $(BLD)/htsmsg.o $(BLD)/htsmsg_json.o $(BLD)/htsmsg_binary.o $(BLD)/htsbuf.o \
  $(BLD)/misc/json.o $(BLD)/filebundle.o $(BLD)/settings.o \
  $(BLD)/ratinglabels.o $(BLD)/utils.o $(BLD)/uuid.o $(BLD)/intlconv.o \
  $(BLD)/memoryinfo.o $(BLD)/tvh_thread.o $(BLD)/wrappers.o

TOOLOBJS := libtvhepg.o tvhepg_stubs.o main.o

all: tvhepg-extract

%.o: %.c libtvhepg.h
	$(CC) $(CFLAGS) -c $< -o $@

tvhepg-extract: $(TOOLOBJS)
	$(CC) -o $@ $(TOOLOBJS) $(TVHOBJS) $(LDLIBS)

clean:
	rm -f $(TOOLOBJS) tvhepg-extract

.PHONY: all clean
