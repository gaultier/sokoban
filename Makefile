
sokoban: main.c $(wildcard *.h)
	$(CC) $(CFLAGS) -Wall -Wextra -Wnull-dereference -Wwrite-strings -std=c99 -g $< -o $@ -lSDL2 $(LDFLAGS)
