#include <ncurses.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
  uint16_t id;
  uint16_t rotation;
  int32_t x;
  int32_t y;
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

uint16_t tetramino_xconstraints[4 * 7 * 2] = {
  // Line
  0, 4,
  2, 1,
  0, 4,
  1, 1,
  // L - Inverse
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // L
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // Square
  1, 2,
  1, 2,
  1, 2,
  1, 2,
  // Z - Inverse
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // T
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // Z
  0, 3,
  1, 2,
  0, 3,
  0, 2
};

uint16_t tetramino_yconstraints[4 * 7 * 2] = {
  // Line
  1, 1,
  0, 4,
  2, 1,
  0, 4,
  // L - Inverse
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // L
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // Square
  0, 2,
  0, 2,
  0, 2,
  0, 2,
  // Z - Inverse
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // T
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // Z
  0, 2,
  0, 3,
  1, 2,
  0, 3
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
  printf("[>%s<]\n\r", screen_bar);
  
  for (uint32_t i = 0; i < screen_height; i++) {
    printf("[>%.*s<]\n\r", screen_real_width, screen + (screen_real_width * i));
  }

  printf("[>%s<]\n\r", screen_bar);
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

uint16_t* tetraminoGetXBounds(Tetramino *tet) {
  return &tetramino_xconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

uint16_t* tetraminoGetYBounds(Tetramino *tet) {
  return &tetramino_yconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
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

struct {
  char keys[50]; // no way more than 50 characters are entered at once.
  uint16_t length;
} input;

struct {
  int16_t rotation_offset; // number of times to rotate
  uint16_t drop_offset;    // number of squares to drop per frame
  int16_t x_offset;
  uint16_t quit;
} actions;

const int16_t rotation_absolute_max = 1;
const uint16_t drop_max = 1; // no support for multiple drops per frame currently
const int16_t x_absolute_max = 1;

void inputReadAllKeys(void) {
  // reset input
  input.length = 0;

  int c;
  // grab characters until no more remain
  while ((c = getch()) != ERR) {
    input.keys[input.length] = (char)c;
    input.length++;
  }
}

void tetraminoConstrainToScreen(Tetramino *tet) {
  uint16_t *x_bounds = tetraminoGetXBounds(tet);
  uint16_t *y_bounds = tetraminoGetYBounds(tet);
  
  tet->x = MAX(x_bounds[0], MIN(tet->x, screen_width  - x_bounds[1] - x_bounds[0]));
  tet->y = MAX(y_bounds[0], MIN(tet->y, screen_height - y_bounds[1] - y_bounds[0]));
}

void inputProcessAllKeys(void) {
  actions.rotation_offset = 0;
  actions.drop_offset = 0;
  actions.x_offset = 0;

  for (uint16_t i = 0; i < input.length; i++) {
    switch (input.keys[i]) {
      case ',': // '<'
        actions.rotation_offset += -1;
        break;
        
      case '.': // '>'
        actions.rotation_offset +=  1;
        break;
        
      case 's':
      case ' ':
        actions.drop_offset++;
        break;

      case 'a':
        actions.x_offset += -1;
        break;
      case 'd':
        actions.x_offset +=  1;
        break;

      case 'q':
        actions.quit = 1;
        return;
    }
  }

  if (abs(actions.rotation_offset) > rotation_absolute_max) {
    int16_t sign = (actions.rotation_offset >= 0) ? 1 : -1;
    actions.rotation_offset = rotation_absolute_max * sign;
  }

  if (actions.drop_offset > drop_max) {
    actions.drop_offset = drop_max;
  }

  if (abs(actions.x_offset) > x_absolute_max) {
    int16_t sign = (actions.x_offset >= 0) ? 1 : -1;
    actions.x_offset = x_absolute_max * sign;
  }
}

void tetraminoApplyActions(Tetramino *tet) {
  if (actions.rotation_offset >= 0) {
    tet->rotation += (uint16_t)actions.rotation_offset;
  } else {
    tet->rotation += abs(actions.rotation_offset) * 3;
  }

  tet->rotation = tet->rotation % 4;
  tet->y += actions.drop_offset;
  tet->x = MAX(0, (int16_t)tet->x + actions.x_offset);

  tetraminoConstrainToScreen(tet);
}

int main(void) {
  Tetramino tet = (Tetramino){ZED_INVERSE, DEG_180, 0, 0};
  // initialize ncurses
  initscr();

  cbreak(); // disable newline buffering
  noecho(); // disables echoing of stdin
  nodelay(stdscr, TRUE); // makes checking input nonblocking, gets ERR if no avaliable input
  curs_set(0); // hide cursor
  
  // initialize all characters to whitespace
  screenInit();

  // main game loop
  while (1) {
    clock_t time_start, time_end;
    time_start = clock();

    inputReadAllKeys();
    inputProcessAllKeys();

    if (actions.quit) break;
    
    // erase previous tetramino from screen
    tetraminoDraw(&tet, px_eraser);

    tetraminoApplyActions(&tet);

    // update background data
    // tet.y++;
    
    // if (tetraminoIsColliding(&tet)) {
    //   tet.y--;
    //   tetraminoDraw(&tet, px_box);
      
    //   tet.x = 0;
    //   tet.y = 0;
    // }

    tetraminoDraw(&tet, px_box);
    
    // display new screen
    screenPrint();

    // reset cursor
    printf("\r\033[%uA", num_rows + 1);

    time_end = clock();

    double time_elapsed = (double)(time_end - time_start);
    time_elapsed /= CLOCKS_PER_SEC;

    // sleep for ~66 ms (15 fps)
    usleep((66.6f - time_elapsed) * 1000);
  }

  // clean up ncurses and reset terminal
  curs_set(1);
  endwin();
    
  return 0;
}
