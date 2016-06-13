CC := gcc
CFLAGS := -W -Wall -O2 -DNDEBUG

exactsub: exactsub.c
	$(CC) $(CFLAGS) $^ -o $@
