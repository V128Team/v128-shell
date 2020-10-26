include ../common.mk

CFLAGS  := -Wall -Werror \
			$(shell pkg-config --cflags swc) \
			$(shell pkg-config --cflags wld) \
			$(shell pkg-config --cflags wayland-client) \
			$(shell pkg-config --cflags wayland-server)

LDFLAGS := -lm -ldl \
			$(shell pkg-config --libs swc) \
			$(shell pkg-config --libs wld) \
			$(shell pkg-config --libs wayland-client) \
			$(shell pkg-config --libs wayland-server)

OBJS := wm.o

wm: $(OBJS)
	$(LD) -o $@ $< $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f wm *.o

install: wm
	$(INSTALL) -m755 wm $(DESTDIR)/bin
