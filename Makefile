# configuration
CC      = gcc
LD      = gcc
CFLAGS  = -g -O2 -Wall
LDFLAGS = -lm

# targets and sources
BIN = joinr
SRC = src/main.c

# built from sources
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

# all
all: $(BIN)

$(BIN): $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

# clean everything up
clean:
	rm -f $(OBJ) $(DEP)

# template rules
%.o: %.c Makefile
	$(CC) -c $< -o $@ $(CFLAGS) -MD -MP -I.

# dependencies
-include $(DEP)

# phony rules
.PHONY: all clean

# user commands
-include user.mk
