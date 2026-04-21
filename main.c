#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "term.h"

#if defined(__linux__)

#include <unistd.h>

#elif defined(_WIN32) || defined(_WIN64)

#include <io.h>
#define read _read
#define write _write
#define close _close

#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define INRANGE(low, n, high) ((low <= n) && (n < high))
#define NANOSECONDS_PER_SECOND 1000000000
#define INPUT_BUFFERSIZE 50

// uint32_t behavior_flags = 0;
// #define USEBUCKET 1

typedef struct {
  uint16_t id;
  uint16_t rotation;
  int32_t x;
  int32_t y;
} Tetramino;

enum Tetramino_id {
  LINE,
  SQUARE,
  CROSS,
  ELBOW,
  ELBOW_INVERSE,
  ZED,
  ZED_INVERSE
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
  // square
  0b0110011000000000,
  0b0110011000000000,
  0b0110011000000000,
  0b0110011000000000,
  // T
  0b0100111000000000,
  0b0100011001000000,
  0b0000111001000000,
  0b0100110001000000,
  // L
  0b0010111000000000,
  0b0100010001100000,
  0b0000111010000000,
  0b1100010001000000,
  // L - inverse
  0b1000111000000000,
  0b0110010001000000,
  0b0000111000100000,
  0b0100010011000000,
  // Z
  0b1100011000000000,
  0b0010011001000000,
  0b0000110001100000,
  0b0100110010000000,
  // Z - inverse
  0b0110110000000000,
  0b0100011000100000,
  0b0000011011000000,
  0b1000110001000000
};

int16_t tetramino_xconstraints[4 * 7 * 2] = {
  // Line
  0, 4,
  2, 1,
  0, 4,
  1, 1,
  // Square
  1, 2,
  1, 2,
  1, 2,
  1, 2,
  // T
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // L
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // L - Inverse
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // Z
  0, 3,
  1, 2,
  0, 3,
  0, 2,
  // Z - Inverse
  0, 3,
  1, 2,
  0, 3,
  0, 2
};

int16_t tetramino_yconstraints[4 * 7 * 2] = {
  // Line
  1, 1,
  0, 4,
  2, 1,
  0, 4,
  // Square
  0, 2,
  0, 2,
  0, 2,
  0, 2,
  // T
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // L
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // L - Inverse
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // Z
  0, 2,
  0, 3,
  1, 2,
  0, 3,
  // Z - Inverse
  0, 2,
  0, 3,
  1, 2,
  0, 3
};

struct TetraminoShapeGroup {
  uint16_t offset;
  uint32_t nshapes;
};

struct TetraminoShapeGroup shape_groups[5];

void Tetramino_initializeShapeGroups(void) {
  enum GroupIds {
    gLINE,
    gSQUARE,
    gCROSS,
    gELBOW,
    gZED
  };
    
  shape_groups[gLINE]   = (struct TetraminoShapeGroup){0, 1};
  shape_groups[gSQUARE] = (struct TetraminoShapeGroup){1, 1};
  shape_groups[gCROSS]  = (struct TetraminoShapeGroup){2, 1};
  shape_groups[gELBOW]  = (struct TetraminoShapeGroup){3, 2};
  shape_groups[gZED]    = (struct TetraminoShapeGroup){5, 2};
}

char screen[screen_real_width * screen_height];
static const char screen_bar[screen_real_width + 1] = "====================";
// static const uint32_t num_rows = screen_height + 2;

void Screen_Init() {
  for (uint32_t i = 0; i < screen_real_width * screen_height; i++) {
    screen[i] = ' ';
  }
}

void Screen_Print() {
  printf("[>%s<]\n\r", screen_bar);
  
  for (uint32_t i = 0; i < screen_height; i++) {
    printf("[>%.*s<]\n\r", screen_real_width, screen + (screen_real_width * i));
  }

  printf("[>%s<]\n\r", screen_bar);
}

void Screen_SetAt(uint32_t x, uint32_t y, char c) {
  screen[(y * screen_real_width) + x] = c;
}

char Screen_GetAt(uint32_t x, uint32_t y) {
  return screen[(y * screen_real_width) + x];
}

uint16_t Tetramino_GetShape(Tetramino *tet) {
  return tetramino_shapes[(tet->id * 4) + tet->rotation];
}

int16_t *Tetramino_GetXBounds(Tetramino *tet) {
  return &tetramino_xconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

int16_t *Tetramino_GetYBounds(Tetramino *tet) {
  return &tetramino_yconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

void Tetramino_Draw(Tetramino *tet, const char* px) {
  uint16_t shape = Tetramino_GetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j))) { // check if shape section is opaque
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        
        Screen_SetAt(real_x,     tet->y + i, px[0]);
        Screen_SetAt(real_x + 1, tet->y + i, px[1]);
      }
    }
  }
}

const char px_box[2]    = {'[', ']'};
const char px_eraser[2] = {' ', ' '};

uint32_t Screen_FilledAt(uint32_t x, uint32_t y) {
  return Screen_GetAt(x, y) != ' ';
}

uint32_t Tetramino_IsColliding(Tetramino *tet) {
  uint16_t shape = Tetramino_GetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j) )) {
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        uint32_t real_y =  tet->y + i;
        
        // printf("x=%i\n\r", tet->x);
        // segfault last call        
        if (Screen_FilledAt(real_x, real_y)) return 1;
       }
    }
  }

  return 0;
}

struct {
  char keys[INPUT_BUFFERSIZE]; // no way more than 50 characters are entered at once.
  uint64_t length;
} input;

struct {
  int16_t rotation_offset; // number of times to rotate
  uint16_t drop_offset;    // number of squares to drop per frame
  int16_t x_offset;
  uint16_t quit;
  uint16_t hard_drop;
} actions;

const int16_t rotation_absolute_max = 1;
const uint16_t drop_max = 1; // no support for multiple drops per frame currently
const int16_t x_absolute_max = 1;

void inputReadAllKeys(void) {
  // reset input
  input.length = 0;
  
  size_t nread = Terminal_readAllInputs(input.keys, 50ull);
  input.length = (uint64_t)nread;
  

  /*
  char c;
  size_t nread;
  // grab characters until no more remain
  while ((nread = read(STDIN_FILENO, &c, 1)) == 1) {
    input.keys[input.length] = c;
    // printf("%c\n", c);
    input.length++;
  }
  */
}

void Tetramino_ConstrainToScreen(Tetramino *tet) {
  int16_t *x_bounds = Tetramino_GetXBounds(tet);
  int16_t *y_bounds = Tetramino_GetYBounds(tet);
  
  tet->x = MAX(-x_bounds[0], MIN(tet->x, screen_width  - x_bounds[1] - x_bounds[0]));
  tet->y =                   MIN(tet->y, screen_height - y_bounds[1] - y_bounds[0]);

  // printf("x=%i\n", tet->x);
  // Screen_GetAt(tet->x + x_bounds[0], tet->y);
}

void inputProcessAllKeys(void) {
  actions.rotation_offset = 0;
  actions.drop_offset = 0;
  actions.x_offset = 0;
  // actions.id_offset = 0;
  actions.hard_drop = 0;

  for (uint16_t i = 0; i < input.length; i++) {
    switch (input.keys[i]) {
      case ',': // '<'
        actions.rotation_offset += -1;
        break;
        
      case '.': // '>'
        actions.rotation_offset +=  1;
        break;
        
      case 's':
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

      case ' ': // hard drop
        actions.hard_drop = 1;
        break;
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

uint32_t Tetramino_HasLanded(Tetramino *tet) {
  int16_t *y_bounds = Tetramino_GetYBounds(tet);
  return (tet->y + y_bounds[0] + y_bounds[1]) > screen_height;
}

void Tetramino_AttemptLateralMove(Tetramino *tet) {
  if (actions.x_offset == 0) return; // no movement laterally

  // apply hypothetical move
  int16_t px = tet->x;

  // constrain x
  int16_t *x_bounds = Tetramino_GetXBounds(tet);
  tet->x = MAX(-x_bounds[0], MIN(tet->x + actions.x_offset, screen_width - x_bounds[1] - x_bounds[0]));

  // test for collision after lateral movement
  if (Tetramino_IsColliding(tet)) {
    
    // revert changes
    tet->x = px;
  }
}

void Tetramino_Randomize(Tetramino *tet);
void Tetramino_GotoSpawn(Tetramino *tet);

void Tetramino_ApplyActions(Tetramino *tet) {
  if (actions.rotation_offset >= 0) {
    tet->rotation += (uint16_t)actions.rotation_offset;
  } else {
    tet->rotation += abs(actions.rotation_offset) * 3;
  }

  // tet->id = (tet->id + actions.id_offset) % 7;

  // save the last valid state
  Tetramino previous_state = *tet;

  tet->rotation = tet->rotation % 4; // clamp between 0 and 3
  // tet->y += actions.drop_offset;

  if (actions.hard_drop) {

    // drop as far as we can
    while (1) {
      tet->y++;

      if (Tetramino_HasLanded(tet) || Tetramino_IsColliding(tet)) {
        tet->y--;
        break;
      }
    }

    // leave a locked version of tet
    Tetramino_Draw(tet, px_box);

    // randomize and reset
    Tetramino_Randomize(tet);
    Tetramino_GotoSpawn(tet);
    return;
  }

  tet->y += actions.drop_offset;
  
  Tetramino_AttemptLateralMove(tet);
  
  if (Tetramino_HasLanded(tet) || Tetramino_IsColliding(tet)) {
    // tetramino has landed or is colliding vertically
    // -> tetramino needs to be locked and a new tetramino dropped
    
    Tetramino_Draw(&previous_state, px_box);
    
    // reset
    Tetramino_Randomize(tet);
    Tetramino_GotoSpawn(tet);
  }

  Tetramino_ConstrainToScreen(tet);
}

void Tetramino_GotoSpawn(Tetramino *tet) {
  int16_t *x_bounds = Tetramino_GetXBounds(tet);

  tet->y = -4;
  tet->x = (screen_width / 2) - x_bounds[0] - (x_bounds[1] / 2);
}

void Tetramino_Randomize(Tetramino *tet) {
  struct TetraminoShapeGroup group = shape_groups[rand() % 5];
  
  uint32_t shape = group.offset;
  uint32_t varient = rand() % group.nshapes;
  uint32_t rotation = rand() % 4;
  
  tet->id = shape + varient;
  tet->rotation = rotation;
}

void Tetramino_respawn(Tetramino *tet) {
  Tetramino_Randomize(tet);
  Tetramino_GotoSpawn(tet);
}

struct timespec timespecDifference(const struct timespec *t1, const struct timespec *t0) {
  struct timespec tmp;
  
  // Check if nanoseconds of end is less than start
  if ((t1->tv_nsec - t0->tv_nsec) < 0) {
    tmp.tv_sec = t1->tv_sec - t0->tv_sec - 1; // Borrow 1 second
    tmp.tv_nsec = t1->tv_nsec - t0->tv_nsec + 1000000000L;
  } else {
    tmp.tv_sec = t1->tv_sec - t0->tv_sec;
    tmp.tv_nsec = t1->tv_nsec - t0->tv_nsec;
  }

  return tmp;
}


int timespecGreaterThan(struct timespec t1, struct timespec t2) {
    if (t1.tv_sec > t2.tv_sec) {
        return 1;
    }
    if (t1.tv_sec == t2.tv_sec && t1.tv_nsec > t2.tv_nsec) {
        return 1;
    }
    
    return 0;
}

// functions for handling line clear, tetris, and possibly animations

uint32_t Screen_ShouldClearLine(uint32_t y) {
  // must be run before the falling tetramino is drawn to avoid incorrect line clears
  
  uint32_t j = 0;
  for (j = 0; j < screen_width; j++) {
    if (!Screen_FilledAt(j * screen_pixel_width, y)) break;
  }
  
  return j >= screen_width;
}

uint32_t Screen_HasFilledLine(void) {
  // loop backwards cuz its more likely for lines to be filled at the bottom of the screen
  for (int32_t i = 0; i < screen_height; i++) {
    if (Screen_ShouldClearLine((uint32_t)i)) return 1;
  }

  return 0;
}

void Screen_DropSection(uint32_t start_y, uint32_t section_height, uint32_t distance) {
  for (uint32_t i = start_y; i < start_y + section_height; i++) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = Screen_GetAt(j, i);
      
      Screen_SetAt(j, i + distance, c); // set new
      Screen_SetAt(j, i, ' '); // clear old
    }
  }
}

void Screen_DropAbove(uint32_t start_y) {
  if (start_y == screen_height - 1)
    return;
  
  for (int32_t i = start_y; i >= 0; i--) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = Screen_GetAt(j, i);
      
      Screen_SetAt(j, i + 1, c); // set new
      Screen_SetAt(j, i, ' '); // clear old
    }
  }
}

void Screen_ClearLine(uint32_t y) {
  for (uint32_t i = 0; i < screen_real_width; i++) {
    Screen_SetAt(i, y, ' '); // clear cell
  }
}

void Screen_ClearDropLines(void) {
  // loop from the bottom of the screen upwards
  for (int32_t i = screen_height - 1; i >= 0; i--) {
    if (Screen_ShouldClearLine(i)) {

      Screen_ClearLine(i);
      if (i == 0)
        continue;
      
      Screen_DropAbove(i - 1);
    }
  }
}

void fatalError(const char *str) {
  perror(str);
  exit(errno);
}

void Tetris_initialize() {
  Terminal_saveState();
  Terminal_setRaw();
  Terminal_getCursorPosition();
  
  // swap to alternate buffer
  write(STDOUT_FILENO, "\033[?1049h", 8);
  // hide cursor
  write(STDOUT_FILENO, "\033[?25l", 6);
  // set cursor to start of terminal
  write(STDOUT_FILENO, "\033[;H", 4);
}



int main(void) {
  Tetramino_initializeShapeGroups();
  
  Tetramino tet = (Tetramino){LINE, DEG_0, 0, 0};
  struct timespec force_drop_interval = {1, 0};
  struct timespec timestamp_previous_drop, timestamp_now;
    
  // initialize ncurses
  atexit(Terminal_restore);
  Tetris_initialize();

  clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);

  // ensure new rng seed
  srand(time(0));  
  // initialize all characters to whitespace
  Screen_Init();

  // reset and randomize tetramino
  Tetramino_respawn(&tet);
  
  // main game loop
  while (1) {
    struct timespec frame_start, frame_end;
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    // read and process inputs into actions
    inputReadAllKeys();
    inputProcessAllKeys();

    if (actions.quit) break;

    // TODO: move into Tetramino_ApplyActions()
    if (actions.drop_offset != 0 && !actions.hard_drop) {
      // user dropped manually
      clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);
    } else {
      //check if we should force a drop
      clock_gettime(CLOCK_MONOTONIC, &timestamp_now);
      struct timespec time_passed = timespecDifference(&timestamp_now, &timestamp_previous_drop);
            
      if (timespecGreaterThan(time_passed, force_drop_interval)) {
        actions.drop_offset = 1;
        clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);
      }
    }
    
    // erase previous tetramino from screen
    Tetramino_Draw(&tet, px_eraser);

    // apply user inputs
    Tetramino_ApplyActions(&tet);

    if (Screen_HasFilledLine()) {
      Screen_ClearDropLines();
    }

    // draw new tetramino with updated position
    Tetramino_Draw(&tet, px_box);
    
    // display new screen
    Screen_Print();

    // reset cursor
    // printf("\r\033[%uA", num_rows);
    write(STDOUT_FILENO, "\033[;H", 4);

    clock_gettime(CLOCK_MONOTONIC, &frame_end);
    struct timespec frame_time = timespecDifference(&frame_end, &frame_start);

    // sleep for ~66 ms (15 fps)
    struct timespec frame_total_duration = {0, 66666667};
    struct timespec frame_remaining = timespecDifference(&frame_total_duration, &frame_time);
    nanosleep(&frame_remaining, NULL);
  }
  
  exit(0);
}
