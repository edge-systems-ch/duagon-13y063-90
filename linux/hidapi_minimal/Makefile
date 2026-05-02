CC = gcc
CFLAGS = -fPIC -Wall -Wextra -g
LDFLAGS = -shared
DEBUGFLAGS = -O0 -DDEBUG # Use this for printing debug messages
TARGET_LIB_STATIC = libhidapi_minimal_13y063-90.a
TARGET_LIB_SHARED = libhidapi_minimal_13y063-90.so
INSTALLDIR = lib
BOARD = F229

ifeq ($(BOARD),F229)
	CFLAGS := $(CFLAGS) -DF229
else ifeq ($(BOARD),ME10)
	CFLAGS := $(CFLAGS) -DME10
else ifeq ($(BOARD),G239)
	CFLAGS := $(CFLAGS) -DG239
endif

# Specify extensions of files to delete when cleaning
CLEANEXTS   = o a so

SRCS = $(shell echo hidapi_minimal_13y063-90.c)
HEADERS = $(shell echo hidapi_minimal_13y063-90.h)
OBJS = $(SRCS:.c=.o)

# Default target
.PHONY: all
all: $(TARGET_LIB_STATIC) $(TARGET_LIB_SHARED)

$(TARGET_LIB_STATIC): $(OBJS)
	ar rcs $(TARGET_LIB_STATIC) $(OBJS)

$(TARGET_LIB_SHARED): $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $(TARGET_LIB_SHARED) $(OBJS)

.PHONY: install
install:
	mkdir -p $(INSTALLDIR)
	cp -p $(TARGET_LIB_STATIC) $(INSTALLDIR)
	cp -p $(TARGET_LIB_SHARED) $(INSTALLDIR)

.PHONY: clean
clean:
	for file in $(CLEANEXTS); do rm -f *.$$file; done

