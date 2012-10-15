CROSS_COMPILE = arm-none-linux-gnueabi-gcc
CC := $(CROSS_COMPILE)
LD := $(CROSS_COMPILE)
CFLAGS :=  -Wall -Werror -g -I../tools/gmp/include
LDFLAGS := -L../tools/gmp/lib -lgmp -lm -static
ORIENT_OBJ = orient_lock.o

all: trial pollard selector


trial: trial.c orient_lock
	$(CC) -o $@ $< $(ORIENT_OBJ) $(CFLAGS) $(LDFLAGS)

pollard: pollard.c orient_lock.o 
	$(CC) -o $@ $(ORIENT_OBJ) $< $(CFLAGS) $(LDFLAGS)

selector: selector.c orient_lock.o
	$(CC) -o $@ $(ORIENT_OBJ) $< $(CFLAGS) $(LDFLAGS)

orient_lock: orient_lock.c
	$(CC) -c $@.c $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f pollard.o trial.o selector.o orient_lock.o trial pollard selector

.PHONY: clean
