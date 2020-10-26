CC := gcc
LD := gcc

DESTDIR ?= /usr

define build_module # dirname
$(eval module := $(1))

all::
	make -C $(module)

clean::
	make -C $(module) clean

install::
	make -C $(module) install
endef
