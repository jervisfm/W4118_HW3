CROSS_COMPILE ? =
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
CFLAGS := -Wall -Werror -g -I../tools/gmp/include
LDFLAGS := -L../tools/gmp/lib -lgmp -lm -static


all: trial pollard


trial: trial.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

pollard: pollard.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f pollard.o trial.o trial pollard

.PHONY: clean
