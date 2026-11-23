CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O1

arinm2c: src/arinm2c.c
	$(CC) $(CFLAGS) -o $@ src/arinm2c.c

check: arinm2c
	sh tests/run.sh

clean:
	rm -f arinm2c
