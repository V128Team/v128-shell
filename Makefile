DESTDIR ?= /usr

WAYLAND_PROTOCOLS := $(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER   := $(shell pkg-config --variable=wayland_scanner wayland-scanner)

CFLAGS := \
	 $(shell pkg-config --cflags wlroots) \
	 $(shell pkg-config --cflags wayland-server) \
	 $(shell pkg-config --cflags xkbcommon) \
     -fno-omit-frame-pointer

LIBS := \
	 $(shell pkg-config --libs wlroots) \
	 $(shell pkg-config --libs wayland-server) \
	 $(shell pkg-config --libs xkbcommon) \
	 -lpthread -ldl

SRCS := \
	xdg-shell-protocol.c \
	v128-shell.c \
	log.c \
	subprogram.c \
	v128-logo.c \
	background.c \
	tracy/TracyClient.cpp

OBJS := $(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(SRCS)))

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c: xdg-shell-protocol.h
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

%.o: %.cpp
	$(CXX) $(CFLAGS) -g -DWLR_USE_UNSTABLE -Itracy -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -g -Werror -Wall -DWLR_USE_UNSTABLE -I. -c -o $@ $<

v128-shell: $(OBJS)
	$(CXX) $(CFLAGS) -rdynamic -g -Werror -I. -DWLR_USE_UNSTABLE -o $@ $(OBJS) $(LIBS)

clean:
	rm -f v128-shell xdg-shell-protocol.h xdg-shell-protocol.c $(OBJS)

install:
	install -m755 -d $(DESTDIR)/usr/bin
	install -m755 v128-shell $(DESTDIR)/usr/bin
	install -m755 -o1000 -g1000 -d $(DESTDIR)/var/log/v128

.DEFAULT_GOAL=v128-shell
.PHONY: clean
