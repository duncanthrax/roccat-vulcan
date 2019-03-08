src = $(wildcard *.c)
obj = $(src:.c=.o)

CFLAGS  = -I/usr/include/libevdev-1.0
LDFLAGS = -lhidapi-libusb -levdev

roccat-vulcan: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) roccat-vulcan
