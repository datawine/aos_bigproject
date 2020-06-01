CC = gcc
CFLAGS = -std=c99 -Wall -Wno-unused-result -Werror -O2

all: sandbox

sandbox: sandbox.c
	$(CC) $(CFLAGS) sandbox.c -o $@

.PHONY: clean
clean:
	rm -rf sandbox