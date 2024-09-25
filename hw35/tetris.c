#define _XOPEN_SOURCE 500		/* for random() and srandom() */
#include <stdio.h>
#include <stdlib.h>
#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_color.h>

int block[4][4];		/*current block */
int map[10][20] = {0};		/*map */
int block_x, block_y;		/*location of the block */
int x_size, y_size;		/*size of the block */
int current_block;		/*current block type */
char text[256];			/*some text used for using textout */
bool still_playing = true;      /* false to quit */
long frames;
long score = 0;

void must_init(bool test, const char *description)
{
    if(test) return;

    printf("couldn't initialize %s\n", description);
    exit(1);
}

/* Allegro display funcs */
#define BUFFER_W 320
#define BUFFER_H 240
#define DISP_SCALE 3
#define DISP_W (BUFFER_W * DISP_SCALE)
#define DISP_H (BUFFER_H * DISP_SCALE)

ALLEGRO_DISPLAY* disp;
ALLEGRO_BITMAP* buffer;
ALLEGRO_FONT* font;

void disp_init();
void disp_deinit();
void disp_pre_draw();
void disp_post_draw();

/* Allegro keyboard funcs */
#define KEY_SEEN     1
#define KEY_RELEASED 2
unsigned char key[ALLEGRO_KEY_MAX];

void keyboard_init();
void keyboard_update(ALLEGRO_EVENT* event);

/* Allegro audio funcs */
ALLEGRO_SAMPLE* sample_put;
ALLEGRO_SAMPLE* sample_clear;
ALLEGRO_SAMPLE* sample_gameover;
ALLEGRO_AUDIO_STREAM* music;

void audio_init();
void audio_deinit();

/* start of the box */
#define START_X 10
#define START_Y 5
#define BOX_W 10
#define BOX_H 20
#define PIX 8

/* Sprites funcs */
#define PIXMAP_W 10
#define PIXMAP_H 21

ALLEGRO_BITMAP* sprites[PIXMAP_W][PIXMAP_H];
ALLEGRO_BITMAP* sprites_sheet;

/* loads bitmap from sprite sheet */
ALLEGRO_BITMAP* sprite_grab(int x, int y, int w, int h);
/* load sprites */
void sprites_init();
void sprites_deinit();
/* process keys and update block coordinates */
void sprites_update();
/* Draw box and blocks */
void sprites_draw();

/* game logic funcs */

/* this function checks if the current map fits onto map. Used by rotate */
bool block_fit(void);

/* generates block depending on the kind of block to be generated */
void made_block(int kind);

/* this function generates new block when the current one has been put */
void new_block(void);

/* this function rotates the block. It's called when the rotate button
   is pressed. It uses the made_block function using the same type of
   block but different rotation */
void rotate_block(void);

/* this function goes through map and checks if there are any rows
	that can be deleted (all values in one colum filled in */
void delete_row(void);

/* this function puts current block onto map[][] double array
	using block_x and block_y location. It is called when a block
   cannot move anymore downwards */
void put_block(void);

int main()
{
    must_init(al_init(), "allegro");
    must_init(al_install_keyboard(), "keyboard");

    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    must_init(timer, "timer");

    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    must_init(queue, "queue");

    font = al_create_builtin_font();
    must_init(font, "font");

    disp_init();

    audio_init();

    // initialize random seed
	srandom((int)time(NULL));

    must_init(al_init_primitives_addon(), "primitives");

    must_init(al_init_image_addon(), "image");
    sprites_init();

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    keyboard_init();

    frames = 0;
    score = 0;

    bool done = false;
    bool redraw = true;
    ALLEGRO_EVENT event;

    al_start_timer(timer);

    new_block();

    while(still_playing)
    {
        al_wait_for_event(queue, &event);

        switch(event.type)
        {
            case ALLEGRO_EVENT_TIMER:
                sprites_update();

                if(key[ALLEGRO_KEY_ESCAPE])
                    done = true;

                redraw = true;
                frames++;
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                done = true;
                break;
        }

        if(done)
            break;

        keyboard_update(&event);

        if(redraw && al_is_event_queue_empty(queue))
        {
            disp_pre_draw();
            al_clear_to_color(al_map_rgb(0,0,0));

            sprites_draw();
            disp_post_draw();
            redraw = false;
        }
    }

    disp_deinit();
    sprites_deinit();
    audio_deinit();
    al_destroy_font(font);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);

    return 0;
}

/* Allegro display funcs */
void disp_init()
{
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLES, 8, ALLEGRO_SUGGEST);

    disp = al_create_display(DISP_W, DISP_H);
    must_init(disp, "display");

    buffer = al_create_bitmap(BUFFER_W, BUFFER_H);
    must_init(buffer, "bitmap buffer");
}

void disp_deinit()
{
    al_destroy_bitmap(buffer);
    al_destroy_display(disp);
}

void disp_pre_draw()
{
    al_set_target_bitmap(buffer);
}

void disp_post_draw()
{
    al_set_target_backbuffer(disp);
    al_draw_scaled_bitmap(buffer, 0, 0, BUFFER_W, BUFFER_H, 0, 0, DISP_W, DISP_H, 0);

    al_flip_display();
}

/* Allegro audio funcs */
void audio_init()
{
    al_install_audio();
    al_init_acodec_addon();
    al_reserve_samples(128);

    sample_put = al_load_sample("sfx_put.ogg");
    must_init(sample_put, "put sample");
    sample_clear = al_load_sample("sfx_clear.ogg");
    must_init(sample_put, "clear sample");
    sample_gameover = al_load_sample("sfx_gameover.ogg");
    must_init(sample_gameover, "gameover sample");

    music = al_load_audio_stream("music.ogg", 2, 2048);
    must_init(music, "music");
    al_set_audio_stream_playmode(music, ALLEGRO_PLAYMODE_LOOP);
    al_attach_audio_stream_to_mixer(music, al_get_default_mixer());
}

void audio_deinit()
{
    al_destroy_sample(sample_put);
    al_destroy_sample(sample_clear);
    al_destroy_audio_stream(music);
}

/* Allegro keyboard funcs */
void keyboard_init()
{
    memset(key, 0, sizeof(key));
}

void keyboard_update(ALLEGRO_EVENT* event)
{
    switch(event->type)
    {
        case ALLEGRO_EVENT_TIMER:
            for(int i = 0; i < ALLEGRO_KEY_MAX; i++)
                key[i] &= KEY_SEEN;
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            key[event->keyboard.keycode] = KEY_SEEN | KEY_RELEASED;
            break;
        case ALLEGRO_EVENT_KEY_UP:
            key[event->keyboard.keycode] &= KEY_RELEASED;
            break;
    }
}

/* Sprites funcs */

ALLEGRO_BITMAP* sprite_grab(int x, int y, int w, int h)
{
    ALLEGRO_BITMAP* sprite = al_create_sub_bitmap(sprites_sheet, x, y, w, h);
    must_init(sprite, "sprite grab");
    return sprite;
}

/* load sprites */
void sprites_init()
{
    sprites_sheet = al_load_bitmap("sprites.png");
    must_init(sprites_sheet, "spritesheet");

    for (int j = 0; j < PIXMAP_H; j++) {
        for (int i = 0; i < PIXMAP_W; i++) {
            sprites[i][j] = sprite_grab(PIX * i, PIX * j, PIX, PIX);
        }
    }
}

void sprites_deinit()
{
    for (int j = 0; j < PIXMAP_H; j++) {
        for (int i = 0; i < PIXMAP_W; i++) {
            al_destroy_bitmap(sprites[i][j]);
        }
    }
}

/* process keys and update block coordinates */
void sprites_update()
{
    if (!(frames % 6)) {
        if (key[ALLEGRO_KEY_LEFT]) {
            block_x--;
            if (!block_fit()) {
                block_x++;
            }
        }
        if (key[ALLEGRO_KEY_RIGHT]) {
            block_x++;
            if (!block_fit()) {
                block_x--;
            }
        }
        if (key[ALLEGRO_KEY_UP]) {
            rotate_block();
        }
        if (key[ALLEGRO_KEY_SPACE]) {
            while (block_fit()) {
                block_y++;
            }
            block_y--;
        }
        if (key[ALLEGRO_KEY_DOWN]) {
            if (block_fit()) {
                block_y++;
            }
            if (!block_fit())
                block_y--;
        }
    }
    if (!(frames % 20)) {
        block_y++;
        if (!block_fit()) {
            block_y--;
            put_block();
            new_block();
        }
    }
    /* if can't put block generate new one */
    if (!block_fit()) {
        put_block();
        new_block();
    }
}
 

/* Draw box and blocks */
void sprites_draw()
{
    /* map border */
    al_draw_rectangle(START_X * PIX, START_Y * PIX, ((START_X + BOX_W) * PIX) + 1, (START_Y + BOX_H) * PIX, al_color_name("blue"), 1);
    /* already placed blocks */
	for (int i = 0; i < BOX_W; i++) {
		for (int j = 0; j < BOX_H; j++) {
			if (map[i][j] != 0) {
                al_draw_tinted_bitmap(sprites[(map[i][j]-1) % PIXMAP_W][(map[i][j]-1) / PIXMAP_W], al_map_rgb_f(1.0, 0.5, 1.0), (START_X + i) * PIX, (START_Y + j) * PIX, 0);
			}
		}
	}
	/* this function draws the current box onto map_buffer */
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (block[i][j] != 0) {
                al_draw_bitmap(sprites[(block[i][j]-1) % PIXMAP_W][(block[i][j]-1) / PIXMAP_W], (START_X + i + block_x) * PIX, (START_Y + j + block_y) * PIX, 0);
			}
		}
	}
 	sprintf(text, "Score: %4ld", score);
    al_draw_text(font, al_map_rgb_f(1,1,1), 10, 10, 0, text);
}

/* game logic funcs */

/* this function checks if the current map fits onto map. Used by rotate */
bool block_fit(void)
{
	/* go through all parts of the block and check if they are in the box range */
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (block[i][j] != 0) {
				if ((block_x + i) > (BOX_W - 1)) {
					return false;
                }
				if ((block_x + i) < 0) {
					return false;
                }
				if ((block_y + j) > (BOX_H - 1)) {
                    return false;
                }
				if (map[block_x + i][block_y + j] != 0) {
				 	return false;
                }
			}
		}
	}
	/* block is in the range of box */
	return true;
}

/* this function generates new block when the current one has been put */
void new_block(void)
{
	/* now generate the current block using the value used by the next_block last time */
	made_block((random() % 7) + 1);
	/* set current locations of the block */
	block_x = 3;
	block_y = 0;
	/* if can't put block the game is over */
	if (!block_fit()) {
        al_play_sample(sample_gameover, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, NULL);
        al_draw_text(font, al_map_rgb(255, 255, 255), BUFFER_W / 2, BUFFER_H / 2, 0, "Game over!");
        printf("Game Over!\n");
        still_playing = false;
	}
}

/* this function rotates the block. It's called when the rotate button
   is pressed. It uses the made_block function using the same type of
   block but different rotation */
void rotate_block(void)
{
	/* save block so that when can't rotate block it can be restored */
	int old_block = current_block;
	
    /* set next rotation type */
	current_block += 10;
	
    /* there can be only four rotations so set block to first one if greater than 4th one */
	if (current_block > 40) {
		current_block -= 40;
    }
	
    /* generate new rotated block */
	made_block(current_block);

	/* if can't put block (could be out of map) restore old one */
	if (!block_fit()) {
		current_block = old_block;
		made_block(current_block);
	}
}

/* this function goes through map and checks if there are any rows
	that can be deleted (all values in one colum filled in */
void delete_row(void)
{
	int sum, i, j = 0;
	
    /* loop through all 20 rows */
	for (i = 19; i >= 0; i--) {
		sum = 0;	/* sum holds number of non-empty cells */
		/* loop through all columns of the row */
		for (j = 0; j < 10; j++) {
			if (map[j][i] != 0)
				sum++;	/* if cell not empty increment sum */
		}
		/* if sum == 10 it means that the row is full so move all rows above it down */
		if (sum == 10) {
			for (j = i; j >= 1; j--)
				for (int k = 0; k < 10; k++)
					map[k][j] = map[k][j - 1];
			i += 2;
            al_play_sample(sample_clear, 1, 0, 0.75, ALLEGRO_PLAYMODE_ONCE, NULL);
            score = score + 100;
		}
	}
}

/* this function puts current block onto map[][] double array
	using block_x and block_y location. It is called when a block
   cannot move anymore downwards */
void put_block(void)
{
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (block[i][j] != 0)
				map[block_x + i][block_y + j] = block[i][j];
		}
	}
    al_play_sample(sample_put, 1, 0, 1, ALLEGRO_PLAYMODE_ONCE, NULL);
	/* now check if block deletes any rows */
	delete_row();
}


/* generates block depending on the kind of block to be generated */
/* Blocks positions follows original Tengen rotation system */
/* https://harddrop.com/wiki/Tetris_(NES,_Tengen) */
void made_block(int kind)
{
    /* clear block array */
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			block[i][j] = 0;
	
    switch (kind) {
	case 1:
		block[1][0] = 14;   /* .##. */
        block[2][0] = 15;   /* ##.. */
        block[0][1] = 23;   /* .... */
        block[1][1] = 24;   /* .... */
		break;
	case 11:
		block[0][0] = 1;    /* #... */
        block[0][1] = 11;   /* ##.. */
        block[1][1] = 12;   /* .#.. */
        block[1][2] = 22;   /* .... */
		break;
	case 21:
		block[1][0] = 14;   /* .##. */
        block[2][0] = 15;   /* ##.. */
        block[0][1] = 23;   /* .... */
        block[1][1] = 24;   /* .... */
		break;
	case 31:
		block[0][0] = 1;    /* #... */
        block[0][1] = 11;   /* ##.. */
        block[1][1] = 12;   /* .#.. */
        block[1][2] = 22;   /* .... */
		break;
	case 2:
		block[0][0] = 31;   /* ##.. */
        block[1][0] = 32;   /* ##.. */
        block[0][1] = 41;   /* .... */
        block[1][1] = 42;   /* .... */
		break;
	case 12:
		block[0][0] = 31;   /* ##.. */
        block[1][0] = 32;   /* ##.. */
        block[0][1] = 41;   /* .... */
        block[1][1] = 42;   /* .... */
		break;
	case 22:
		block[0][0] = 31;   /* ##.. */
        block[1][0] = 32;   /* ##.. */
        block[0][1] = 41;   /* .... */
        block[1][1] = 42;   /* .... */
		break;
	case 32:
		block[0][0] = 31;   /* ##.. */
        block[1][0] = 32;   /* ##.. */
        block[0][1] = 41;   /* .... */
        block[1][1] = 42;   /* .... */
		break;
	case 3:
		block[0][0] = 68;   /* ###. */
        block[1][0] = 69;   /* .#.. */
        block[2][0] = 70;   /* .... */
        block[1][1] = 79;   /* .... */
		break;
	case 13:
		block[1][0] = 52;   /* .#.. */
        block[0][1] = 61;   /* ##.. */
        block[1][1] = 62;   /* .#.. */
        block[1][2] = 72;   /* .... */
		break;
	case 23:
		block[1][0] = 64;   /* .#.. */
        block[0][1] = 73;   /* ###. */
        block[1][1] = 74;   /* .... */
        block[2][1] = 75;   /* .... */
		break;
	case 33:
		block[0][0] = 56;   /* #... */
        block[0][1] = 66;   /* ##.. */
        block[1][1] = 67;   /* #... */
        block[0][2] = 76;   /* .... */
		break;
	case 4:
        block[0][0] = 93;   /* ##.. */
        block[1][0] = 94;   /* .##. */
		block[1][1] = 104;  /* .... */
        block[2][1] = 105;  /* .... */
		break;
	case 14:
		block[1][0] = 82;   /* .#.. */
        block[0][1] = 91;   /* ##.. */
        block[1][1] = 92;   /* #... */
        block[0][2] = 101;  /* .... */
		break;
	case 24:
        block[0][0] = 93;   /* ##.. */
        block[1][0] = 94;   /* .##. */
		block[1][1] = 104;  /* .... */
        block[2][1] = 105;  /* .... */
		break;
	case 34:
		block[1][0] = 82;   /* .#.. */
        block[0][1] = 91;   /* ##.. */
        block[1][1] = 92;   /* #... */
        block[0][2] = 101;  /* .... */
		break;
	case 5:
		block[0][0] = 128;  /* ###. */
        block[1][0] = 129;  /* #... */
        block[2][0] = 130;  /* .... */
        block[0][1] = 138;  /* .... */
		break;
	case 15:
		block[0][0] = 111;  /* ##.. */
        block[1][0] = 112;  /* .#.. */
        block[1][1] = 122;  /* .#.. */
        block[1][2] = 132;  /* .... */
		break;
	case 25:
		block[2][0] = 125;  /* ..#. */
        block[0][1] = 133;  /* ###. */
        block[1][1] = 134;  /* .... */
        block[2][1] = 135;  /* .... */
		break;
	case 35:
		block[0][0] = 116;  /* #... */
        block[0][1] = 126;  /* #... */
        block[0][2] = 136;  /* ##.. */
        block[1][2] = 137;  /* .... */
		break;
	case 6:
		block[0][0] = 153;  /* ###. */
        block[1][0] = 154;  /* ..#. */
        block[2][0] = 155;  /* .... */
        block[2][1] = 165;  /* .... */
		break;
	case 16:
		block[1][0] = 147;  /* .#.. */
        block[1][1] = 157;  /* .#.. */
        block[0][2] = 166;  /* ##.. */
        block[1][2] = 167;  /* .... */
        break;
	case 26:
		block[0][0] = 158;  /* #... */
        block[0][1] = 168;  /* ###. */
        block[1][1] = 169;  /* .... */
        block[2][1] = 170;  /* .... */
		break;
	case 36:
		block[0][0] = 141;  /* ##.. */
        block[1][0] = 142;  /* #... */
        block[0][1] = 151;  /* #... */
        block[0][2] = 161;  /* .... */
		break;
	case 7:
		block[0][0] = 202;  /* #### */
        block[1][0] = 203;  /* .... */
        block[2][0] = 204;  /* .... */
        block[3][0] = 205;  /* .... */
		break;
	case 17:
		block[1][0] = 171;  /* .#.. */
        block[1][1] = 181;  /* .#.. */
        block[1][2] = 191;  /* .#.. */
        block[1][3] = 201;  /* .#.. */
		break;
	case 27:
		block[0][0] = 202;  /* #### */
        block[1][0] = 203;  /* .... */
        block[2][0] = 204;  /* .... */
        block[3][0] = 205;  /* .... */
		break;
	case 37:
		block[1][0] = 171;  /* .#.. */
        block[1][1] = 181;  /* .#.. */
        block[1][2] = 191;  /* .#.. */
        block[1][3] = 201;  /* .#.. */
		break;
	default:
		//Error("Unknown element to be generated.");
		break;
	}

	if ((kind == 7) || (kind == 27)) {
		x_size = 4;
		y_size = 1;
	} else if ((kind == 17) || (kind == 37)) {
		x_size = 1;
		y_size = 4;
	} else if (kind % 10 == 2) {
		x_size = 2;
		y_size = 2;
	} else if ((kind / 10 == 0) || (kind / 10 == 2)) {
		x_size = 3;
		y_size = 2;
	} else if ((kind / 10 == 1) || (kind / 10 == 3)) {
		x_size = 2;
		y_size = 3;
	}
	current_block = kind;
}