CC     ?= gcc
CFLAGS ?= -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -g
BUILD  ?= $(abspath ../build)

LIB   := $(BUILD)/libcommon.a
OBJS  := $(BUILD)/common/contact.o $(BUILD)/common/traffic.o \
         $(BUILD)/common/linereader.o $(BUILD)/common/cpu.o \
         $(BUILD)/common/device.o

HDRS  := contact.h traffic.h linereader.h cpu.h device.h

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(BUILD)/common/%.o: %.c $(HDRS) | $(BUILD)/common
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/common:
	mkdir -p $@

clean:
	rm -rf $(BUILD)/common $(LIB)
