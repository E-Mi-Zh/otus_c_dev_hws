all: tetris

tetris: tetris.c
	$(CC) -o tetris tetris.c `pkg-config allegro-5 allegro_font-5 allegro_primitives-5 allegro_audio-5 allegro_acodec-5 allegro_image-5 allegro_color-5 --libs --cflags` -lm  -Wall -Wextra -Wpedantic -std=c11

clean:
	$(RM) *.o tetris core

.PHONY: all clean
