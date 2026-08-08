src = $(wildcard *.c)
obj = $(src:.c=.o)
TESTS = ../tests/test_config_simple.c

NAME    := roccat-vulcan
BINDIR  := /usr/bin
UDEVDIR := /etc/udev/rules.d
PKG_CONFIG ?= pkg-config
CFLAGS   += $(shell $(PKG_CONFIG) --cflags libevdev libhidapi-libusb libudev 2>/dev/null)
LDFLAGS  += $(shell $(PKG_CONFIG) --libs libevdev libhidapi-libusb libudev 2>/dev/null)

ifeq ($(strip $(CFLAGS)),)
CFLAGS += -I/usr/include/libevdev-1.0
endif

ifeq ($(strip $(LDFLAGS)),)
LDFLAGS += -levdev -lhidapi-libusb -ludev
endif

.PHONY: all
all: $(NAME)
$(NAME): $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-config: $(TESTS) config.c
	mkdir -p ../tests
	$(CC) $(CFLAGS) -o ../tests/test_config $(TESTS) config.c $(LDFLAGS)

install:
	mkdir -p ${DESTDIR}${BINDIR}
	cp $(NAME) ${DESTDIR}${BINDIR}/
	mkdir -p ${DESTDIR}${UDEVDIR}
	cp *.rules ${DESTDIR}${UDEVDIR}/

uninstall:
	rm -f ${DESTDIR}${BINDIR}/$(NAME)
	rm -f ${DESTDIR}${UDEVDIR}/99-roccat-vulcan.rules

.PHONY: clean
clean:
	rm -f $(obj) $(NAME) tests/test_config
