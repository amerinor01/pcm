CC       := clang
CFLAGS   := -fPIC -Iinclude -Wall -Wextra -O3
LDFLAGS  := -shared

SRCDIR   := src
INCDIR   := include
LIBDIR   := lib

SRC      := \
    $(SRCDIR)/driver.c \
    $(SRCDIR)/pcmc_impl.c \
    $(SRCDIR)/pcmh_impl.c
OBJ      := $(SRC:.c=.o)
TARGET   := $(LIBDIR)/libpcm.so

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p $(LIBDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBDIR):
	mkdir -p $(LIBDIR)

clean:
	rm -f $(OBJ) $(TARGET)

distclean: clean
	rm -rf $(LIBDIR)

.PHONY: all clean distclean
