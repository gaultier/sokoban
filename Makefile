
sokoban: main.c
	$(CC) $(CFLAGS) -g $^ -o $@ -lSDL2 -lSDL2_image $(LDFLAGS)
