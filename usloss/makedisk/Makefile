COBJS = makedisk.o
CFLAGS = -I../src

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

makedisk: $(COBJS)
	$(CC) -o makedisk $(COBJS)

clean:
	rm -f $(COBJS) makedisk
