GCC_DBG_OPTS= -g
CC=gcc

all:
	$(CC) -o hopper hopper.c
debug:
	$(CC) $(GCC_DBG_OPTS) -o hopper hopper.c
clean:
	rm hopper
