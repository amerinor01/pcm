# Makefile for cc_box project

CC      := gcc
CFLAGS  := -std=c2x -O2 -Wall -Wextra -I.
LDFLAGS :=

SRCS    := cc_box.c reno.c dcqcn.c main.c
OBJS    := $(SRCS:.c=.o)
TARGET  := cc_test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c cc_box_api.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
