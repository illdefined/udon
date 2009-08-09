CC = clang
LIBS = -lX11

udon: udon.c
	$(CC) -std=c99 -Wall $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f udon
