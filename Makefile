
sokoban: main.c $(wildcard *.h)
	$(CC) $(CFLAGS) -Wall -Wextra -Wnull-dereference -Wwrite-strings -std=c99 -O2 -g $< -o $@ $(LDFLAGS) -march=native -Wl,--gc-sections $(shell sdl2-config --cflags --libs)
