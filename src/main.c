/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

For a C++ project simply rename the file to .cpp and re-run the build script 

-- Copyright (c) 2020-2024 Jeffery Myers
--
--This software is provided "as-is", without any express or implied warranty. In no event 
--will the authors be held liable for any damages arising from the use of this software.

--Permission is granted to anyone to use this software for any purpose, including commercial 
--applications, and to alter it and redistribute it freely, subject to the following restrictions:

--  1. The origin of this software must not be misrepresented; you must not claim that you 
--  wrote the original software. If you use this software in a product, an acknowledgment 
--  in the product documentation would be appreciated but is not required.
--
--  2. Altered source versions must be plainly marked as such, and must not be misrepresented
--  as being the original software.
--
--  3. This notice may not be removed or altered from any source distribution.

*/

#include "raylib.h"
#include "chip8.h"

#define WINDOW_SCALE 10
#define CYCLES_PER_FRAME 9

int keymap[16] = {
	KEY_KP_1, KEY_KP_2, KEY_KP_3,
	KEY_Q, KEY_W, KEY_E,
	KEY_A, KEY_S, KEY_D,
	KEY_X, KEY_Z, KEY_C,
	KEY_KP_4, KEY_R, KEY_F, KEY_V
};

void display_to_pixels(uint8_t *display, uint8_t *pixels) {
	for (int j = 0; j < DISPLAY_ARRAY_SIZE; j++) {
		uint8_t d_byte = display[j];
		for (int bit = 0; bit < 8; bit++) {
			pixels[8*j+bit] = d_byte & (1<<bit) ? 0xFF : 0x00;
		}
	}
}

int main(int argc, char **argv)
{
	Chip8 chip;
	init_chip(&chip);
	load_rom_from_file(&chip, "/home/mos/REPOS/MISC/chip8-test-suite/bin/1-chip8-logo.ch8");
	uint8_t pixels[PIXEL_COUNT];
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
		.data = pixels,
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
		.mipmaps = 1
	};

	Texture2D screen_texture = LoadTextureFromImage(screen_image);
	
	// game loop
	while (!WindowShouldClose())		// run the loop untill the user presses ESCAPE or presses the Close button on the window
	{
		// deal with input
		int key_pressed = -1;
		for (int index = 0; index < 16; index++) {
			prev_keys[index] = chip.keys[index];
			int keycode = keymap[index];
			chip.keys[index] = IsKeyDown(keycode);

			if (key_pressed == -1 && chip.keys[index] && !prev_keys[index]) {
				key_pressed = index;
			}
		}

		// simulate a few cycles
		for (int cycle = 0; cycle < CYCLES_PER_FRAME; cycle++) {
			uint16_t instr = fetch(&chip);
			int result = execute(&chip, instr, key_pressed);
			printf("exec once, PC = %#04x, instr = %#04x, with return code %d\n", chip.pc, instr, result);
			key_pressed = -1;
		}

		// draw to pixels array and send to texture
		display_to_pixels(chip.display, pixels);
		UpdateTexture(screen_texture, pixels);

		// drawing
		BeginDrawing();

		// Setup the backbuffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		// draw pixels
		Rectangle source = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
		Rectangle dest = {0, 0, DISPLAY_WIDTH * WINDOW_SCALE, DISPLAY_HEIGHT * WINDOW_SCALE};
		Vector2 origin = {0.0f, 0.0f};
		DrawTexturePro(screen_texture, source, dest, origin, 0.0f, RAYWHITE);
		
		// end the frame and get ready for the next one  (display frame, poll input, etc...)
		EndDrawing();
	}

	// destory the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}