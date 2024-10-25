#include "raylib.h"
#include "chip8.h"
#include <time.h>

#define WINDOW_SCALE 10
#define CYCLES_PER_FRAME 20

int keymap[16] = {
	KEY_X,
	KEY_ONE, KEY_TWO, KEY_THREE,
	KEY_Q, KEY_W, KEY_E,
	KEY_A, KEY_S, KEY_D,
	KEY_Z, KEY_C,
	KEY_FOUR, KEY_R, KEY_F, KEY_V
};

int main(int argc, char **argv)
{
	Chip8 chip;
	init_chip(&chip);
	load_rom_from_file(
		&chip,
		"/home/mos/REPOS/MISC/chip8-test-suite/bin/6-keypad.ch8"
		// "/home/mos/ROMs/CHIP-8/danm8ku.ch8"
	);
	bool prev_keys[16];

	// Tell the window to use vysnc and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	SetTargetFPS(60);

	// Create the window and OpenGL context
	InitWindow(
		DISPLAY_WIDTH * WINDOW_SCALE,
		DISPLAY_HEIGHT * WINDOW_SCALE,
		"mos chip8"
	);

	Image screen_image = {
		.data = chip.display,
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
		.mipmaps = 1
	};

	Texture2D screen_texture = LoadTextureFromImage(screen_image);

	// seed random
	srand(time(NULL));
	
	// game loop
	while (!WindowShouldClose())
	{
		// decrement clocks if > 0!
		if (chip.delay > 0) {
			chip.delay--;
		}
		if (chip.sound > 0) {
			chip.sound--;
		}
		
		// deal with input
		int key_pressed = -1;
		for (int index = 0; index < 16; index++) {
			prev_keys[index] = chip.keys[index];
			int keycode = keymap[index];
			chip.keys[index] = IsKeyDown(keycode);

			// adjust waiting for key press or key release depending on quirk
			if (
				key_pressed == -1 && (
					(!QUIRK_KEY_RELEASE && chip.keys[index] && !prev_keys[index])
					|| (QUIRK_KEY_RELEASE && !chip.keys[index] && prev_keys[index])
				)
			) {
				key_pressed = index;
			}
		}

		bool display_waited = true;
		// simulate a few cycles
		for (int cycle = 0; cycle < CYCLES_PER_FRAME; cycle++) {
			// printf("%d\n", chip.pc);
			uint16_t instr = fetch(&chip);
			int result = execute(&chip, instr, key_pressed, display_waited);
			if (
				((result == C8_AWAIT_REFRESH) && QUIRK_DISPLAY_WAIT)
				|| result == C8_AWAIT_KEYPRESS
			) {
				break; // just skip ahead to next frame
			}
			key_pressed = -1;
			display_waited = false;
		}

		// send display array straight to texture
		UpdateTexture(screen_texture, chip.display);

		// drawing
		BeginDrawing();

		// Setup the backbuffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		// draw pixels
		Rectangle source = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
		Rectangle dest = {0, 0, DISPLAY_WIDTH * WINDOW_SCALE, DISPLAY_HEIGHT * WINDOW_SCALE};
		Vector2 origin = {0.0f, 0.0f};
		DrawTexturePro(screen_texture, source, dest, origin, 0.0f, RAYWHITE);

		DrawFPS(0, 0);
		
		// end the frame and get ready for the next one  (display frame, poll input, etc...)
		EndDrawing();
	}

	UnloadTexture(screen_texture);

	// destory the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}