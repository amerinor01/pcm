PWD := $(shell pwd)
CC := clang
CFLAGS := -I$(PWD)/include

SRC := \
    src/dummy_datapath_impl.c \
    src/pcc_sw_ctx.c \
    test/main.c \
    test/reno.c

TARGET := reno_test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f $(TARGET)