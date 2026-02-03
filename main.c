#include <ncurses.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

typedef struct {
  uint16_t id;
  uint16_t rotation;
  uint32_t x;
  uint32_t y;
} Tetramino;

enum Tetramino_id {
  LINE,
  ELBOW_INVERSE,
  ELBOW,
  SQUARE,
  ZED_INVERSE,
  CROSS,
  ZED
};

enum Tetramino_rotation {
  DEG_0,
  DEG_90,
  DEG_180,
  DEG_270
};

// cursed af
#define screen_width  10
#define screen_height 20
#define screen_pixel_width 2
#define screen_real_width 20

uint16_t tetramino_shapes[4 * 7] = {
  // line
  0b0000111100000000,
  0b0010001000100010,
  0b0000000011110000,
  0b0100010001000100,
  // L - inverse
  0b1000111000000000,
  0b0110010001000000,
  0b0000111000100000,
  0b0100010011000000,
  // L
  0b0010111000000000,
  0b0100010001100000,
  0b0000111010000000,
  0b1100010001000000,
  // square
  0b0110011000000000,
  0b0110011000000000,
  0b0110011000000000,
  0b0110011000000000,
  // Z - inverse
  0b0110110000000000,
  0b0100011000100000,
  0b0000011011000000,
  0b1000110001000000,
  // T
  0b0100111000000000,
  0b0100011001000000,
  0b0000111001000000,
  0b0100110001000000,
  // Z
  0b1100011000000000,
  0b0010011001000000,
  0b0000110001100000,
  0b0100110010000000
};

char screen[screen_real_width * screen_height];
static const char screen_bar[screen_real_width + 1] = "====================";
static const uint32_t num_rows = screen_height + 2;

void screenInit() {
  for (uint32_t i = 0; i < screen_real_width * screen_height; i++) {
    screen[i] = ' ';
  }
}

void screenPrint() {
  printf("[>%s<]\n", screen_bar);
  
  for (uint32_t i = 0; i < screen_height; i++) {
    printf("[>%.*s<]\n", screen_real_width, screen + (screen_real_width * i));
  }

  printf("[>%s<]\n", screen_bar);
}

void screenSetAt(uint32_t x, uint32_t y, char c) {
  screen[(y * screen_real_width) + x] = c;
}

char screenGetAt(uint32_t x, uint32_t y) {
  return screen[(y * screen_real_width) + x];
}

uint16_t tetraminoGetShape(Tetramino *tet) {
  return tetramino_shapes[(tet->id * 4) + tet->rotation];
}

void tetraminoDraw(Tetramino *tet, const char* px) {
  uint16_t shape = tetraminoGetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (shape & (0x8000 >> (4 * i + j) )) {
        uint32_t real_x =  (tet->x + j) * screen_pixel_width;
        
        screenSetAt(real_x,     tet->y + i, px[0]);
        screenSetAt(real_x + 1, tet->y + i, px[1]);
      }
    }
  }
}

const char px_box[2]    = {'[', ']'};
const char px_eraser[2] = {' ', ' '};

void tetraminoUpdatePosition(Tetramino *tet) {
  // update position
  tet->y++;
}

uint32_t screenFilledAt(uint32_t x, uint32_t y) {
  return (screenGetAt(x, y) != ' ') ? 1 : 0;
}

uint32_t tetraminoIsColliding(Tetramino *tet) {
  uint16_t shape = tetraminoGetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (shape & (0x8000 >> (4 * i + j) )) {
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        uint32_t real_y =  tet->y + i;
        
        if (screenFilledAt(real_x, real_y)) return 1;
        if (real_y > screen_height) return 1;
      }
    }
  }

  return 0;
}

int main(void) {
  Tetramino tet = (Tetramino){ZED_INVERSE, DEG_180, 0, 0};

  // initialize ncurses
  initscr();

  // disable buffering and echoing of user input. Does not effect control characters
  cbreak();
  noecho();
  
  // initialize all characters to whitespace
  screenInit();

  // main game loop
  while (1) {
    clock_t time_start, time_end;
    time_start = clock();
    
    // erase previous tetramino from screen
    tetraminoDraw(&tet, px_eraser
              );

    // update background data
    tet.y++;
    
    if (tetraminoIsColliding(&tet)) {
      tet.y--;
      tetraminoDraw(&tet, px_box
                );
      
      tet.x = 0;
      tet.y = 0;
    }

    tetraminoDraw(&tet, px_box
              );
    
    // display new screen
    screenPrint();

    // reset cursor
    printf("\r\033[%uA", num_rows);

    time_end = clock();

    double time_elapsed = (double)(time_end - time_start);
    time_elapsed /= CLOCKS_PER_SEC;

    // sleep for ~66 ms (15 fps)
    usleep((250.0f - time_elapsed) * 1000);
  }

  // clean up ncurses
  endwin();
    
  return 0;
}
