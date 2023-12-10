MAKEFLAGS = -rR
CC = gcc
CFLAGS += -O -Wall -Wextra

prog = sledit

.PHONY: all
all: $(prog)

.PHONY: run
run:
	@stty -echo raw
	@-./$(prog)
	@stty sane

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
