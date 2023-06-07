
sokoban: main.c
	$(CC) $(CFLAGS) -g $^ -o $@ -lSDL2 $(LDFLAGS)
