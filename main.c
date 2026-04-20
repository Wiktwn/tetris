#include <termios.h>
#include <errno.h>
// #include <ncurses.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "term.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define INRANGE(low, n, high) ((low <= n) && (n < high))
#define NANOSECONDS_PER_SECOND 1000000000

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

int16_t tetramino_xconstraints[4 * 7 * 2] = {
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

int16_t tetramino_yconstraints[4 * 7 * 2] = {
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
// static const uint32_t num_rows = screen_height + 2;

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

int16_t *tetraminoGetXBounds(Tetramino *tet) {
  return &tetramino_xconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

int16_t *tetraminoGetYBounds(Tetramino *tet) {
  return &tetramino_yconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

void tetraminoDraw(Tetramino *tet, const char* px) {
  uint16_t shape = tetraminoGetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j))) { // check if shape section is opaque
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        
        screenSetAt(real_x,     tet->y + i, px[0]);
        screenSetAt(real_x + 1, tet->y + i, px[1]);
      }
    }
  }
}

const char px_box[2]    = {'[', ']'};
const char px_eraser[2] = {' ', ' '};

uint32_t screenFilledAt(uint32_t x, uint32_t y) {
  return screenGetAt(x, y) != ' ';
}

uint32_t tetraminoIsColliding(Tetramino *tet) {
  uint16_t shape = tetraminoGetShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j) )) {
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        uint32_t real_y =  tet->y + i;
        
        // printf("x=%i\n\r", tet->x);
        // segfault last call        
        if (screenFilledAt(real_x, real_y)) return 1;
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
  uint16_t hard_drop;
} actions;

const int16_t rotation_absolute_max = 1;
const uint16_t drop_max = 1; // no support for multiple drops per frame currently
const int16_t x_absolute_max = 1;

void inputReadAllKeys(void) {
  // reset input
  input.length = 0;

  char c;
  size_t nread;
  
  // grab characters until no more remain
  while ((nread = read(STDIN_FILENO, &c, 1)) == 1) {
    input.keys[input.length] = (char)c;
    // printf("%c\n", c);
    input.length++;
  }
}

void tetraminoConstrainToScreen(Tetramino *tet) {
  int16_t *x_bounds = tetraminoGetXBounds(tet);
  int16_t *y_bounds = tetraminoGetYBounds(tet);
  
  tet->x = MAX(-x_bounds[0], MIN(tet->x, screen_width  - x_bounds[1] - x_bounds[0]));
  tet->y =                   MIN(tet->y, screen_height - y_bounds[1] - y_bounds[0]);

  // printf("x=%i\n", tet->x);
  // screenGetAt(tet->x + x_bounds[0], tet->y);
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

uint32_t tetraminoHasLanded(Tetramino *tet) {
  int16_t *y_bounds = tetraminoGetYBounds(tet);
  return (tet->y + y_bounds[0] + y_bounds[1]) > screen_height;
}

void tetraminoAttemptLateralMove(Tetramino *tet) {
  if (actions.x_offset == 0) return; // no movement laterally

  // apply hypothetical move
  int16_t px = tet->x;

  // constrain x
  int16_t *x_bounds = tetraminoGetXBounds(tet);
  tet->x = MAX(-x_bounds[0], MIN(tet->x + actions.x_offset, screen_width - x_bounds[1] - x_bounds[0]));

  // test for collision after lateral movement
  if (tetraminoIsColliding(tet)) {
    
    // revert changes
    tet->x = px;
  }
}

void tetraminoRandomize(Tetramino *tet);
void tetraminoGotoSpawn(Tetramino *tet);

void tetraminoApplyActions(Tetramino *tet) {
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

      if (tetraminoHasLanded(tet) || tetraminoIsColliding(tet)) {
        tet->y--;
        break;
      }
    }

    // leave a locked version of tet
    tetraminoDraw(tet, px_box);

    // randomize and reset
    tetraminoRandomize(tet);
    tetraminoGotoSpawn(tet);
    return;
  }

  tet->y += actions.drop_offset;
  
  tetraminoAttemptLateralMove(tet);
  
  if (tetraminoHasLanded(tet) || tetraminoIsColliding(tet)) {
    // tetramino has landed or is colliding vertically
    // -> tetramino needs to be locked and a new tetramino dropped
    
    tetraminoDraw(&previous_state, px_box);
    
    // reset
    tetraminoRandomize(tet);
    tetraminoGotoSpawn(tet);
  }

  tetraminoConstrainToScreen(tet);
}

void tetraminoGotoSpawn(Tetramino *tet) {
  int16_t *x_bounds = tetraminoGetXBounds(tet);
  // int16_t *y_bounds = tetraminoGetYBounds(tet);

  tet->y = -4;
  tet->x = (screen_width / 2) - x_bounds[0] - (x_bounds[1] / 2);
}

void tetraminoRandomize(Tetramino *tet) {
  tet->id = rand() % 7;
  tet->rotation = rand() % 4;
}

double timespecDifference(const struct timespec *t1, const struct timespec *t0) {
  time_t difference_seconds   = t1->tv_sec  - t0->tv_sec;
  long difference_nanoseconds = t1->tv_nsec - t0->tv_nsec;

  if (difference_nanoseconds < 0) {
    // normalize values
    difference_seconds--;
    difference_nanoseconds += NANOSECONDS_PER_SECOND;
  }

  return (double)difference_seconds + ((double)difference_nanoseconds / NANOSECONDS_PER_SECOND);
}

// functions for handling line clear, tetris, and possibly animations

uint32_t screenShouldClearLine(uint32_t y) {
  // must be run before the falling tetramino is drawn to avoid incorrect line clears
  
  uint32_t j = 0;
  for (j = 0; j < screen_width; j++) {
    if (!screenFilledAt(j * screen_pixel_width, y)) break;
  }
  
  return j >= screen_width;
}

uint32_t screenHasFilledLine(void) {
  // loop backwards cuz its more likely for lines to be filled at the bottom of the screen
  for (int32_t i = 0; i < screen_height; i++) {
    if (screenShouldClearLine((uint32_t)i)) return 1;
  }

  return 0;
}

void screenDropSection(uint32_t start_y, uint32_t section_height, uint32_t distance) {
  for (uint32_t i = start_y; i < start_y + section_height; i++) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = screenGetAt(j, i);
      
      screenSetAt(j, i + distance, c); // set new
      screenSetAt(j, i, ' '); // clear old
    }
  }
}

void screenDropAbove(uint32_t start_y) {
  if (start_y == screen_height - 1)
    return;
  
  for (int32_t i = start_y; i >= 0; i--) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = screenGetAt(j, i);
      
      screenSetAt(j, i + 1, c); // set new
      screenSetAt(j, i, ' '); // clear old
    }
  }
}

void screenClearLine(uint32_t y) {
  for (uint32_t i = 0; i < screen_real_width; i++) {
    screenSetAt(i, y, ' '); // clear cell
  }
}

void screenClearDropLines(void) {
  // loop from the bottom of the screen upwards
  for (int32_t i = screen_height - 1; i >= 0; i--) {
    if (screenShouldClearLine(i)) {

      screenClearLine(i);
      if (i == 0)
        continue;
      
      screenDropAbove(i - 1);
    }
  }
}

void fatalError(const char *str) {
  perror(str);
  exit(errno);
}

struct termios original_terminal;

void Terminal_setRaw() {
#ifdef __linux__
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);

    // raw.c_iflag &= ~(IXON);   // disable XON / XOFF control flow
    raw.c_iflag &= ~(ICRNL);  // disable automatic input translation
    // raw.c_iflag &= ~(BRKINT); // disable break kill signal
    raw.c_iflag &= ~(INPCK);  // disable parity checking
    raw.c_iflag &= ~(ISTRIP); // disable stripping of 8th bit

    // raw.c_oflag &= ~(OPOST);  // disable output pre-processing

    raw.c_lflag &= ~(ECHO);   // disable character echoing
    raw.c_lflag &= ~(ICANON); // disable cannonical mode
    // raw.c_lflag &= ~(ISIG);   // disable interupt and kill signals
    raw.c_lflag |= CS8;       // set the character size to 8

    raw.c_cc[VMIN] = 0;       // set min chars read to 0; allows reading nothing
    raw.c_cc[VTIME] = 1;      // set max wait time for read() to 0.1s

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw))
        fatalError("terminal raw");
#endif
#if defined(_WIN32) || defined(_WIN64)
  
#endif

}

uint32_t restore_row = 1;
uint32_t restore_col = 1;

void Terminal_restore() {
#ifdef __linux__
    // reset terminal to default config
    
    // swap to original buffer
    write(STDOUT_FILENO, "\033[?1049l", 8);
    // show cursor
    write(STDOUT_FILENO, "\033[?25h", 6);
    // resposition cursor
    char pos_str[32];
    int len = snprintf(pos_str, 32, "\033[%d;%dH", restore_row, restore_col);
    write(STDOUT_FILENO, pos_str, len);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal))
        fatalError("terminal restore");

#endif
#if defined(_WIN32) || defined(_WIN64)
  
#endif

}

void terminal_getCursorPosition() {
  // fflush(stdin);
  write(STDOUT_FILENO, "\033[6n", 4);
  char buff[32];
  int nchars = read(STDIN_FILENO, buff, 32);
  buff[MIN(nchars, 31)] = '\0';

  /*
  for (int i = 0; i < 32; i++) {
    char c = buff[i];
    if (c == '\033') {
      printf("ESC\n");
    } else {
      printf("%c\n", c);
    }
  }
  */
  
  sscanf(buff, "%*c%*c%d;%dR", &restore_row, &restore_col);
}

void Tetris_initialize() {
#ifdef __linux__
  tcgetattr(STDIN_FILENO, &original_terminal);
  
  Terminal_setRaw();
  terminal_getCursorPosition();
  // swap to alternate buffer
  write(STDOUT_FILENO, "\033[?1049h", 8);
  // hide cursor
  write(STDOUT_FILENO, "\033[?25l", 6);
  write(STDOUT_FILENO, "\033[;H", 4); // set cursor to start of terminal
#endif
#if defined(_WIN32) || defined(_WIN64)
  
#endif
}

int main(void) { 
  Tetramino tet = (Tetramino){LINE, DEG_0, 0, 0};
  double force_drop_interval = 1.0f; // seconds
  struct timespec timestamp_previous_drop, timestamp_now;

    
  // initialize ncurses
  atexit(Terminal_restore);
  Tetris_initialize();

  clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);

  // ensure new rng seed
  srand(time(0));  
  // initialize all characters to whitespace
  screenInit();

  // reset and randomize tetramino
  tetraminoRandomize(&tet);
  tetraminoGotoSpawn(&tet);
  
  // main game loop
  while (1) {
    clock_t time_start, time_end;
    time_start = clock();

    // read and process inputs into actions
    inputReadAllKeys();
    inputProcessAllKeys();

    if (actions.quit) break;

    // TODO: move into tetraminoApplyActions()
    if (actions.drop_offset != 0 && !actions.hard_drop) {
      // user dropped manually
      clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);
    } else {
      //check if we should force a drop
      clock_gettime(CLOCK_MONOTONIC, &timestamp_now);
      double time_passed = timespecDifference(&timestamp_now, &timestamp_previous_drop);
            
      if (time_passed > force_drop_interval) {
        actions.drop_offset = 1;
        clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);
      }
    }
    
    // erase previous tetramino from screen
    tetraminoDraw(&tet, px_eraser);

    // apply user inputs
    tetraminoApplyActions(&tet);

    if (screenHasFilledLine()) {
      screenClearDropLines();
    }

    // draw new tetramino with updated position
    tetraminoDraw(&tet, px_box);
    
    // display new screen
    screenPrint();

    // reset cursor
    // printf("\r\033[%uA", num_rows);
    write(STDOUT_FILENO, "\033[;H", 4);

    time_end = clock();

    double time_elapsed = (double)(time_end - time_start);
    time_elapsed /= CLOCKS_PER_SEC;

    // sleep for ~66 ms (15 fps)
    usleep((66.7f - time_elapsed) * 1000);
  }
  
  exit(0);
}
