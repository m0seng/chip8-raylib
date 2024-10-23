#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define RAM_SIZE 4096
#define STACK_SIZE 12
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define PIXEL_COUNT DISPLAY_WIDTH*DISPLAY_HEIGHT
#define DISPLAY_ARRAY_SIZE PIXEL_COUNT/8
#define FONT_HEIGHT 5

#define SCHIP false

enum C8Error {
  C8_SUCCESS,
  C8_INVALID_INSTRUCTION,
  C8_STACK_OVERFLOW,
  C8_STACK_UNDERFLOW,
  C8_AWAIT_KEYPRESS,
};

typedef struct Chip8 {
  uint8_t ram[RAM_SIZE];

  uint8_t v[16];
  uint16_t pc;
  uint16_t i;

  uint16_t stack[STACK_SIZE];
  uint8_t sp;

  uint8_t delay;
  uint8_t sound;

  bool keys[16];

  uint8_t display[DISPLAY_ARRAY_SIZE];
} Chip8;

void init_chip(Chip8 *chip);
void load_rom_from_file(Chip8 *chip, char *filename);
uint16_t fetch(Chip8 *chip);
int execute(Chip8 *chip, uint16_t instr, int key_pressed);