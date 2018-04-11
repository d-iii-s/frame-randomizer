.PHONY: all clean

# Settings

MAKEFLAGS += --no-builtin-rules --no-builtin-variables

CC = g++
CC_OPTS = -O -g -Wall -Wextra -Werror
LD = g++
LD_OPTS = -O -g

BIN = ../bin

# Files

ALL_EX = $(BIN)/frame-randomizer
ALL_OO = $(addsuffix .o, $(ALL_EX))
ALL_DD = $(addsuffix .dep, $(ALL_EX))

# Rules

all: $(ALL_EX)

clean:
	rm -f *.o
	rm -f *.dep
	rm -f $(ALL_EX)

$(BIN)/frame-randomizer: frame-randomizer.o
	$(LD) $(LD_OPTS) -o $@ $^

%.o: %.c
	$(CC) $(CC_OPTS) -c -o $@ $<

# Deps

-include $(ALL_DD)

%.dep: %.c
	$(CC) -MM -MT "$@ $(basename $@).o" -o $@ $<
