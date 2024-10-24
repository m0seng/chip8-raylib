#include "chip8.h"

// convenient bit operations
#define MASK(m,n) (((1<<((n)-(m)))-1)<<m)
#define SLICE(x,m,n) (((x)>>(m)) & ((1<<((n)-(m)))-1))

// operations to use a uint8_t array as a bit array
void set_bit(uint8_t A[], int k) {
  A[k/8] |= 1 << (k%8);
}
void clear_bit(uint8_t A[], int k) {
  A[k/8] &= ~(1 << (k%8));
}
int test_bit(uint8_t A[], int k) {
  return (A[k/8] & (1 << (k%8))) != 0;
}

// 8 bit uint to 12 bit BCD representation
uint16_t bcd(uint8_t x) {
  uint16_t result = 0;

  while (x > 0) {
    result = (result << 4) | (x % 10);
    x /= 10;
  }

  return result;
}

void clear_display(Chip8 *chip) {
  for (int y = 0; y < DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
      chip->display[y][x] = 0;
    }
  }
}

void init_chip(Chip8 *chip) {
  for (int j = 0; j < RAM_SIZE; j++) {
    chip->ram[j] = 0;
  }

  uint8_t font[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80, // F
  };

  for (int j = 0; j < 80; j++) {
    chip->ram[j + FONT_OFFSET] = font[j];
  }

  for (int j = 0; j < STACK_SIZE; j++) {
    chip->stack[j] = 0;
  }

  for (int j = 0; j < 16; j++) {
    chip->v[j] = 0;
    chip->keys[j] = false;
  }
  
  chip->pc = 0x200;
  chip->i = 0;
  chip->sp = 0;
  chip->delay = 0;
  chip->sound = 0;

  clear_display(chip);
}

void load_rom_from_file(Chip8 *chip, char *filename) {
  FILE* fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "failed to open file...");
    return;
  }

  uint8_t buffer[BUFFER_SIZE];
  size_t readsize;
  uint16_t write_index = 0x200;
  do {
    readsize = fread(buffer, 1, BUFFER_SIZE, fp);
    for (int j = 0; j < readsize; j++) {
      chip->ram[write_index + j] = buffer[j];
    }
    write_index += readsize;
  } while (readsize == BUFFER_SIZE);
  fclose(fp);
}

int arithmetic(Chip8 *chip, uint16_t instr) {
  uint8_t x = SLICE(instr, 8, 12);
  uint8_t y = SLICE(instr, 4, 8);
  uint8_t subop = SLICE(instr, 0, 4);
  uint8_t *v = chip->v;

  switch (subop) {
    case 0x0:
      v[x] = v[y];
      break;
    case 0x1:
      v[x] |= v[y];
      #if QUIRK_LOGIC_VF
      v[0xF] = 0;
      #endif
      break;
    case 0x2:
      v[x] &= v[y];
      #if QUIRK_LOGIC_VF
      v[0xF] = 0;
      #endif
      break;
    case 0x3:
      v[x] ^= v[y];
      #if QUIRK_LOGIC_VF
      v[0xF] = 0;
      #endif
      break;

    case 0x4: {
      // Vx + Vy with carry flag
      uint8_t result = v[x] + v[y];
      uint8_t carry = (result < v[x]);
      v[x] = result;
      v[0xF] = carry;
    } break;

    case 0x5: {
      // Vx - Vy with borrow flag
      uint8_t borrow = (v[x] >= v[y]);
      v[x] -= v[y];
      v[0xF] = borrow;
    } break;

    case 0x6: {
      // shift right 1 bit
      #if SCHIP
        // SCHIP shifts VX
        uint8_t lsb = v[x] & 1;
        v[x] = v[x] >> 1;
        v[0xF] = lsb;
      #else
        // CHIP8 shifts VY
        uint8_t lsb = v[y] & 1;
        v[x] = v[y] >> 1;
        v[0xF] = lsb;
      #endif
    } break;

    case 0x7: {
      // Vy - Vx with borrow flag
      uint8_t borrow = (v[y] >= v[x]);
      v[x] = v[y] - v[x];
      v[0xF] = borrow;
    } break;

    case 0xE: {
      // shift left 1 bit
      #if SCHIP
        // SCHIP shifts VX
        uint8_t msb = v[x] >> 7;
        v[x] = v[x] << 1;
        v[0xF] = msb;
      #else
        // CHIP8 shifts VY
        uint8_t msb = v[y] >> 7;
        v[x] = v[y] << 1;
        v[0xF] = msb;
      #endif
    } break;

    default:
      return C8_INVALID_INSTRUCTION;
      break;
  }

  return C8_SUCCESS;
}

// returns fetched instruction and increments PC
uint16_t fetch(Chip8 *chip) {
  uint16_t instr = (chip->ram[chip->pc] << 8) | chip->ram[chip->pc + 1];
  chip->pc += 2;
  return instr;
}

int execute(Chip8 *chip, uint16_t instr, int key_pressed, bool display_waited) {
  // assume PC increment already happened before this

  uint8_t op = SLICE(instr, 12, 16);
  uint8_t x = SLICE(instr, 8, 12);
  uint8_t y = SLICE(instr, 4, 8);
  uint16_t nnn = SLICE(instr, 0, 12);
  uint8_t nn = SLICE(instr, 0, 8);
  uint8_t n = SLICE(instr, 0, 4);

  switch (op) {
    case 0x0:
      switch (instr) {
        case 0x00E0:
          // 00E0: clear screen
          clear_display(chip);
          break;
        case 0x00EE:
          // 00EE: return from subroutine
          if (chip->sp == 0) {
            // stack underflow!
            return C8_STACK_UNDERFLOW;
          } else {
            (chip->sp)--;
            chip->pc = chip->stack[chip->sp]; // pop
          }
          break;
        default:
          return C8_INVALID_INSTRUCTION;
          break;
      }
      break;

    case 0x1:
      // 1NNN: jump to address NNN
      chip->pc = nnn;
      break;

    case 0x2:
      // 2NNN: call subroutine at NNN
      if (chip->sp >= STACK_SIZE) {
        // stack overflow!
        return C8_STACK_OVERFLOW;
      } else {
        chip->stack[chip->sp] = chip->pc; // push
        (chip->sp)++;
        chip->pc = nnn;
      }
      break;

    case 0x3:
      // 3XNN: skip next instr. if Vx == NN
      if (chip->v[x] == nn) {
        (chip->pc) += 2;
      }
      break;

    case 0x4:
      // 4XNN: skip next instr. if Vx != NN
      if (chip->v[x] != nn) {
        (chip->pc) += 2;
      }
      break;

    case 0x5:
      // 5XY0: skip next instr. if Vx == Vy
      if (n == 0) {
        if (chip->v[x] == chip->v[y]) {
          (chip->pc) += 2;
        }
      } else {
        return C8_INVALID_INSTRUCTION;
      }
      break;

    case 0x6:
      // 6XNN: set Vx to NN
      chip->v[x] = nn;
      break;

    case 0x7:
      // 7XNN: add NN to Vx (no carry)
      chip->v[x] += nn;
      break;

    case 0x8: {
      // 8XYN: arithmetic operations
      int return_value = arithmetic(chip, instr);
      return return_value;
    } break;

    case 0x9:
      // 9XY0: skip next instr. if Vx != Vy
      if (n == 0) {
        if (chip->v[x] != chip->v[y]) {
          (chip->pc) += 2;
        }
      } else {
        return C8_INVALID_INSTRUCTION;
      }
      break;
    
    case 0xA:
      // ANNN: set I to NNN
      chip->i = nnn;
      break;

    case 0xB:
      #if SCHIP
        // BXNN: jump to NNN + Vx
        chip->pc = nnn + chip->v[x];
      #else
        // BNNN: jump to NNN + V0
        chip->pc = nnn + chip->v[0];
      #endif
      break;
    
    case 0xC:
      // CXNN: Vx = rand() & NN
      chip->v[x] = rand() & nn;
      break;

    case 0xD: {
      // DXYN: draw N-tall sprite at (Vx, Vy) from *I
      // VF = 1 iff any pixels 1 -> 0

      // first of all, display wait quirk
      if (QUIRK_DISPLAY_WAIT && !display_waited) {
        chip->pc -= 2; // come back to this instruction
        return C8_AWAIT_REFRESH;
      }

      bool erased = false;

      // modulo'd coordinates
      int vx = chip->v[x] % DISPLAY_WIDTH;
      int vy = chip->v[y] % DISPLAY_HEIGHT;

      for (int sy = 0; sy < n; sy++) {
        for (int sx = 0; sx < 8; sx++) {
          #if QUIRK_CLIPPING
          if (vy + sy >= DISPLAY_HEIGHT || vx + sx >= DISPLAY_WIDTH) {
            continue;
          }
          #endif

          // shift by (7 - sx) instead of sx is to deal with endianness
          uint8_t source = (
            chip->ram[(chip->i + sy) & MASK(0, 12)]
            & (1 << (7 - sx))
          ) ? 0xFF : 0x00;

          uint8_t *target = &(
            chip->display[(vy+sy) % DISPLAY_HEIGHT][(vx+sx) % DISPLAY_WIDTH]
          );
          *target ^= source;
          if (source & ~(*target)) {
            erased = true;
          }
        }
      }

      chip->v[0xF] = erased ? 1 : 0;
    } break;

    // E instructions: register as long as key held
    // F instructions: register only on key down?
    
    case 0xE:
      switch (nn) {
        case 0x9E:
          // EX9E: skip next instr. if key in Vx pressed
          if (chip->keys[chip->v[x]]) {
            chip->pc += 2;
          }
          break;
        case 0xA1:
          // EXA1: skip next instr. if key in Vx not pressed
          if (! (chip->keys[chip->v[x]])) {
            chip->pc += 2;
          }
          break;
        default:
          return C8_INVALID_INSTRUCTION;
          break;
      }
      break;

    case 0xF:
      switch (nn) {
        case 0x07:
          // FX07: Vx = delay
          chip->v[x] = chip->delay;
          break;
        case 0x0A:
          // FX0A: key press awaited, then stored in Vx
          if (key_pressed >= 0) {
            chip->v[x] = key_pressed;
          } else {
            chip->pc -= 2; // make sure to return to this instruction
            return C8_AWAIT_KEYPRESS;
          }
          break;
        case 0x15:
          // FX15: set delay timer to Vx
          chip->delay = chip->v[x];
          break;
        case 0x18:
          // FX18: set sound timer to Vx
          chip->sound = chip->v[x];
          break;
        case 0x1E:
          // FX1E: add Vx to I (no carry)
          chip->i += chip->v[x];
          break;
        case 0x29: {
          // FX29: I = location of sprite for character Vx
          // assume font is stored from 0x000 with height FONT_HEIGHT
          uint8_t char_index = chip->v[x] & MASK(0, 4);
          chip->i = FONT_OFFSET + (char_index * FONT_HEIGHT);
        } break;
        case 0x33: {
          // FX33: store BCD of Vx at *I
          int vx = chip->v[x];
          uint16_t i = chip->i & MASK(0, 12);
          chip->ram[i] = (vx / 100) % 10;
          chip->ram[i + 1] = (vx / 10) % 10;
          chip->ram[i + 2] = vx % 10;
        } break;
        case 0x55: {
          // FX55: store V0 to Vx inclusive at *I
          // mask I to 12 bits first
          uint16_t i = chip->i & MASK(0, 12);
          for (int j = 0; j <= x; j++) {
            chip->ram[i + j] = chip->v[j];
          }
          #if !(SCHIP)
            // CHIP8 sets I past range of store
            chip->i += x + 1;
          #endif
        } break;
        case 0x65: {
          // FX65: load V0 to Vx inclusive from *I
          // mask I to 12 bits first
          uint16_t i = chip->i & MASK(0, 12);
          for (int j = 0; j <= x; j++) {
            chip->v[j] = chip->ram[i + j];
          }
          #if !(SCHIP)
            // CHIP8 sets I past range of load
            chip->i += x + 1;
          #endif
        } break;
        default:
          return C8_INVALID_INSTRUCTION;
          break;
      }
      break;

    default:
      return C8_INVALID_INSTRUCTION;
      break;
  }

  return C8_SUCCESS; // no errors
}
