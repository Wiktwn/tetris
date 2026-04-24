/*--- INCLUDES ---*/

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cyaml/cyaml.h>
#include "term.h"

/*--- PREPROCESSOR MACROS ---*/

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
#define INPUT_BUFFERSIZE 50

// i dont like this very much
#define screen_width  10
#define screen_height 20
#define screen_pixel_width 2
#define screen_real_width 20

#define FLAGS_NOBUCKET 0x1

/*--- STRUCTURES ---*/

struct InputData {
  char keys[INPUT_BUFFERSIZE]; // no way more than 50 characters are entered at once.
  uint64_t length;
};

struct ActionData {
  int16_t rotation_offset; // number of times to rotate
  uint16_t drop_offset;    // number of squares to drop per frame
  int16_t x_offset;
  uint16_t quit;
  uint16_t hard_drop;
};

typedef struct TetraminoData {
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

struct TetraminoShapeGroup {
  uint16_t offset;
  uint32_t nshapes;
};

struct GameSaveData {
  uint32_t highscore;
  char    *highscore_name;
};

/*--- CYAML COFNIG ---*/

static const cyaml_schema_field_t GameSaveData_fields_schema[] = {
  CYAML_FIELD_UINT("highscore", CYAML_FLAG_DEFAULT, struct GameSaveData, highscore),
  CYAML_FIELD_STRING_PTR("highscore_name", CYAML_FLAG_POINTER, struct GameSaveData, highscore_name, 0, CYAML_UNLIMITED),
  CYAML_FIELD_END
};

static const cyaml_schema_value_t GameSaveData_schema = {
  CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, struct GameSaveData, GameSaveData_fields_schema),
};

static const cyaml_config_t cyaml_config = {
  .log_level = CYAML_LOG_WARNING,
  .log_fn = cyaml_log,
  .mem_fn = cyaml_mem,
};

/*--- DATA ---*/

char save_path[128];
char screen[screen_real_width * screen_height];

static const char screen_bar[screen_real_width + 1] = "====================";
static const char px_box[2]    = {'[', ']'};
static const char px_eraser[2] = {' ', ' '};

const int16_t rotation_absolute_max = 1;
const uint16_t drop_max = 1; // no support for multiple drops per frame currently
const int16_t x_absolute_max = 1;

// struct TetraminoData tetramino;
struct TetraminoData tetramino_next = {LINE, DEG_0, 0, 0};
struct ActionData actions;
struct InputData input;

struct timespec timestamp_previous_drop, timestamp_now;
struct timespec force_drop_interval = {1, 0};
uint32_t Game_score = 0;
uint32_t Game_lines_cleared = 0;
uint32_t Game_flags = 0;
uint32_t Game_highscore = 0;
char     Game_highscore_usr[9];

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

struct TetraminoShapeGroup shape_groups[5];
uint32_t shape_groups_length = 0;

/*--- FUNCTIONS ---*/

int timespecGreaterThan(struct timespec t1, struct timespec t2);
struct timespec timespecDifference(const struct timespec *t1, const struct timespec *t0);
void ContextWindow_drawTetramino(Tetramino *tet);
void Tetramino_randomizeShapeBucket(void);
void Game_loadSave(void);

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

  if (!(Game_flags & FLAGS_NOBUCKET)) {
    shape_groups_length = 5;
    Tetramino_randomizeShapeBucket();
  }
}

// static const uint32_t num_rows = screen_height + 2;

void Screen_init() {
  for (uint32_t i = 0; i < screen_real_width * screen_height; i++) {
    screen[i] = ' ';
  }
}

void Screen_print() {
  printf("[>%s<]\n\r", screen_bar);
  
  for (uint32_t i = 0; i < screen_height; i++) {
    printf("[>%.*s<]\n\r", screen_real_width, screen + (screen_real_width * i));
  }

  printf("[>%s<]\n\r", screen_bar);
  fflush(stdout);
}

void Screen_setAt(uint32_t x, uint32_t y, char c) {
  screen[(y * screen_real_width) + x] = c;
}

char Screen_getAt(uint32_t x, uint32_t y) {
  return screen[(y * screen_real_width) + x];
}

uint16_t Tetramino_getShape(Tetramino *tet) {
  return tetramino_shapes[(tet->id * 4) + tet->rotation];
}

int16_t *Tetramino_getXBounds(Tetramino *tet) {
  return &tetramino_xconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

int16_t *Tetramino_getYBounds(Tetramino *tet) {
  return &tetramino_yconstraints[(tet->id * 4 * 2) + (tet->rotation * 2)];
}

void Tetramino_draw(Tetramino *tet, const char* px) {
  uint16_t shape = Tetramino_getShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j))) { // check if shape section is opaque
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        
        Screen_setAt(real_x,     tet->y + i, px[0]);
        Screen_setAt(real_x + 1, tet->y + i, px[1]);
      }
    }
  }
}

uint32_t Screen_filledAt(uint32_t x, uint32_t y) {
  return Screen_getAt(x, y) != ' ';
}

uint32_t Tetramino_isColliding(Tetramino *tet) {
  uint16_t shape = Tetramino_getShape(tet);

  for (uint32_t i = 0; i < 4; i++) {
    for (uint32_t j = 0; j < 4; j++) {
      if (!INRANGE(0, (int32_t)(i + tet->y), screen_height) || !INRANGE(0, (int32_t)(j + tet->x), screen_width))
        continue;
      
      if (shape & (0x8000 >> (4 * i + j) )) {
        uint32_t real_x = (tet->x + j) * screen_pixel_width;
        uint32_t real_y =  tet->y + i;
        
        // printf("x=%i\n\r", tet->x);
        // segfault last call        
        if (Screen_filledAt(real_x, real_y)) return 1;
       }
    }
  }

  return 0;
}

void Input_readAllKeys(void) {
  // reset input
  input.length = 0;
  
  size_t nread = Terminal_readAllInputs(input.keys, 50ull);
  input.length = (uint64_t)nread;
}

void Tetramino_constrainToScreen(Tetramino *tet) {
  int16_t *x_bounds = Tetramino_getXBounds(tet);
  int16_t *y_bounds = Tetramino_getYBounds(tet);
  
  tet->x = MAX(-x_bounds[0], MIN(tet->x, screen_width  - x_bounds[1] - x_bounds[0]));
  tet->y =                   MIN(tet->y, screen_height - y_bounds[1] - y_bounds[0]);

  // printf("x=%i\n", tet->x);
  // Screen_getAt(tet->x + x_bounds[0], tet->y);
}

void Input_processEscapeSequnce(uint16_t *i) {
  char c;
  sscanf(input.keys + (*i), "\033[%c", &c);

  switch (c) {
    case 'B': // Down Arrow
      actions.drop_offset++;
      break;
    
    case 'C': // Right Arrow
      actions.x_offset++;
      break;

    case 'D': // Left Arrow
      actions.x_offset--;
      break;
  }
}

void Input_processAllKeys(void) {
  actions.rotation_offset = 0;
  actions.drop_offset = 0;
  actions.x_offset = 0;
  // actions.id_offset = 0;
  actions.hard_drop = 0;

  for (uint16_t i = 0; i < input.length; i++) {
    switch (input.keys[i]) {
      case 'n':
      case ',': // '<'
        actions.rotation_offset += -1;
        break;

      case 'm':
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

      case '\033':
        Input_processEscapeSequnce(&i);
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

uint32_t Tetramino_hasLanded(Tetramino *tet) {
  int16_t *y_bounds = Tetramino_getYBounds(tet);
  return (tet->y + y_bounds[0] + y_bounds[1]) > screen_height;
}

void Tetramino_attemptLateralMove(Tetramino *tet) {
  if (actions.x_offset == 0) return; // no movement laterally

  // apply hypothetical move
  int16_t px = tet->x;

  // constrain x
  int16_t *x_bounds = Tetramino_getXBounds(tet);
  tet->x = MAX(-x_bounds[0], MIN(tet->x + actions.x_offset, screen_width - x_bounds[1] - x_bounds[0]));

  // test for collision after lateral movement
  if (Tetramino_isColliding(tet)) {
    
    // revert changes
    tet->x = px;
  }
}

void Tetramino_respawn(Tetramino *tet);

void Tetramino_applyActions(Tetramino *tet) {
  // TODO: these rotations are not safe, make them safe retart!
  if (actions.rotation_offset >= 0) {
    tet->rotation += (uint16_t)actions.rotation_offset;
  } else {
    tet->rotation += abs(actions.rotation_offset) * 3;
  }

  // TODO: move into Tetramino_applyActions()
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

  // tet->id = (tet->id + actions.id_offset) % 7;

  // save the last valid state
  Tetramino previous_state = *tet;

  tet->rotation = tet->rotation % 4; // clamp between 0 and 3
  // tet->y += actions.drop_offset;

  if (actions.hard_drop) {

    // drop as far as we can
    while (1) {
      tet->y++;

      if (Tetramino_hasLanded(tet) || Tetramino_isColliding(tet)) {
        tet->y--;
        break;
      }
    }

    // leave a locked version of tet
    Tetramino_draw(tet, px_box);

    // randomize and reset
    Tetramino_respawn(tet);

    // update last dropped timestamp
    clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);
    return;
  }

  tet->y += actions.drop_offset;
  
  Tetramino_attemptLateralMove(tet);
  
  if (Tetramino_hasLanded(tet) || Tetramino_isColliding(tet)) {
    // tetramino has landed or is colliding vertically
    // -> tetramino needs to be locked and a new tetramino dropped
    
    Tetramino_draw(&previous_state, px_box);
    
    // reset
    Tetramino_respawn(tet);
  }

  Tetramino_constrainToScreen(tet);
}

void Tetramino_gotoSpawn(Tetramino *tet) {
  int16_t *x_bounds = Tetramino_getXBounds(tet);
  int16_t *y_bounds = Tetramino_getYBounds(tet);

  int16_t height = y_bounds[1];
  int16_t top_y = y_bounds[0];

  int16_t bottom_y = (top_y + height);
  if (bottom_y > 4) {
    fprintf(stderr, "how did we get here? %d\n", bottom_y);
  }
  
  tet->y = -bottom_y;
  tet->x = (screen_width / 2) - x_bounds[0] - (x_bounds[1] / 2);
}

void Tetramino_randomizeShapeBucket() {
  // shape_groups_length = 5;
  
  for (size_t i = 4; i > 0; i--) {
    size_t j = (size_t)((double)rand() / ((double)RAND_MAX + 1) * (i + 1));

    struct TetraminoShapeGroup temp = shape_groups[j];
    shape_groups[j] = shape_groups[i];
    shape_groups[i] = temp;
  }
}

void Tetramino_randomize(Tetramino *tet) {
  struct TetraminoShapeGroup group;
  
  if (!(Game_flags & FLAGS_NOBUCKET)) {
    group = shape_groups[shape_groups_length - 1];
    shape_groups_length--;
    
    if (shape_groups_length == 0) {
      shape_groups_length = 5;
      Tetramino_randomizeShapeBucket();
    }
  } else {
    group = shape_groups[rand() % 5];
  }
  
  uint32_t shape = group.offset;
  uint32_t varient = rand() % group.nshapes;
  // uint32_t rotation = rand() % 4;
  uint32_t rotation = DEG_0;
  
  tet->id = shape + varient;
  tet->rotation = rotation;
}

void Tetramino_respawn(Tetramino *tet) {
  tet->id = tetramino_next.id;
  tet->rotation = tetramino_next.rotation;
  Tetramino_gotoSpawn(tet);

  Tetramino_randomize(&tetramino_next);
  ContextWindow_drawTetramino(&tetramino_next);
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

uint32_t Screen_shouldClearLine(uint32_t y) {
  // must be run before the falling tetramino is drawn to avoid incorrect line clears
  
  uint32_t j = 0;
  for (j = 0; j < screen_width; j++) {
    if (!Screen_filledAt(j * screen_pixel_width, y)) break;
  }
  
  return j >= screen_width;
}

uint32_t Screen_hasFilledLine(void) {
  // loop backwards cuz its more likely for lines to be filled at the bottom of the screen
  for (int32_t i = 0; i < screen_height; i++) {
    if (Screen_shouldClearLine((uint32_t)i)) return 1;
  }

  return 0;
}

void Screen_dropSection(uint32_t start_y, uint32_t section_height, uint32_t distance) {
  for (uint32_t i = start_y; i < start_y + section_height; i++) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = Screen_getAt(j, i);
      
      Screen_setAt(j, i + distance, c); // set new
      Screen_setAt(j, i, ' '); // clear old
    }
  }
}

void Screen_dropAbove(uint32_t start_y) {
  if (start_y == screen_height - 1)
    return;
  
  for (int32_t i = start_y; i >= 0; i--) {
    for (uint32_t j = 0; j < screen_real_width; j++) {
      char c = Screen_getAt(j, i);
      
      Screen_setAt(j, i + 1, c); // set new
      Screen_setAt(j, i, ' '); // clear old
    }
  }
}

void Screen_clearLine(uint32_t y) {
  for (uint32_t i = 0; i < screen_real_width; i++) {
    Screen_setAt(i, y, ' '); // clear cell
  }
}


void Game_updateScore(uint32_t nlines) {
  Game_lines_cleared += nlines;

  uint32_t scores[4] = {40, 100, 300, 1200};
  if (nlines == 0)
    return;
  Game_score += scores[MIN(nlines - 1, 3)];
}


void Screen_clearDropLines(void) {
  // loop from the bottom of the screen upwards
  for (int32_t i = screen_height - 1; i >= 0; i--) {

    // needs to be a while loop because consecutive line clears will be put into the current row
    uint32_t nlines = 0;
    while (Screen_shouldClearLine(i)) {
      
      Screen_clearLine(i);
      if (i == 0)
        continue;
      
      Screen_dropAbove(i - 1);
      nlines++;
    }

    
    Game_updateScore(nlines);    
  }
}

void fatalError(const char *str) {
  perror(str);
  exit(errno);
}

void Tetris_initialize() {
  snprintf(save_path, sizeof(save_path), "%s/.tetris/save.yaml", getenv("HOME"));
  
  Terminal_saveState();
  Terminal_setRaw();
  Terminal_getCursorPosition();
  Tetramino_initializeShapeGroups();

  Game_loadSave();
  
  // swap to alternate buffer
  write(STDOUT_FILENO, "\033[?1049h", 8);
  // hide cursor
  write(STDOUT_FILENO, "\033[?25l", 6);
  // set cursor to start of terminal
  write(STDOUT_FILENO, "\033[;H", 4);
}

/*--- CONTEXT WINDOW ---*/

struct ContextWindowElements {
  const size_t width;
  const char *bar;
  const char *blank;
  const char *divider;

  const size_t nl_size;
  const char *ansi_nl;
  
  const uint32_t prev_col;
  const uint32_t prev_row;
};

const struct ContextWindowElements CW_elems = {
  .width      =  12,
  .bar        = "[>========<]",
  .blank      = "[>        <]",
  .divider    = "[>++++++++<]",

  .nl_size = 8,
  .ansi_nl = "\033[12D\033[B",
  .prev_col = 27,
  .prev_row = 10,
};

void ContextWindow_drawRow(const void *ptr, size_t nmemb) {
  fwrite(ptr, 1, nmemb, stdout);
  fwrite(CW_elems.ansi_nl, 1, CW_elems.nl_size, stdout);
}

void ContextWindow_drawTetramino(Tetramino *tet) {
  uint16_t shape = Tetramino_getShape(tet);

  /*
  int16_t *x_bounds = Tetramino_getXBounds(tet);
  int16_t *y_bounds = Tetramino_getYBounds(tet);
  int16_t start_x = x_bounds[0];
  int16_t start_y = y_bounds[0];
  int16_t width_x = x_bounds[1];
  int16_t width_y = y_bounds[1];

  int16_t center_x = (width_x / 2) + start_x;
  int16_t center_y = (width_y / 2) + start_y;
  */
  
  printf("\033[%u;%uH", CW_elems.prev_row, CW_elems.prev_col);
  
  for (int32_t i = 0; i < 4; i++) {
    for (int32_t j = 0; j < 4; j++) {
      if (shape & (0x8000 >> (4 * i + j))) { // check if shape section is opaque
        // draw box
        fwrite(px_box, 1, 2, stdout);
      } else {
        // draw nothing
        fwrite("  ", 1, 2, stdout);
      }
    }
    
    // goto next line
    printf("\033[8D\033[B");
  }
  
  fflush(stdout);
}

void ContextWindow_update(void) {
  printf("\033[1;%uH", screen_real_width + 4 + 1);
  ContextWindow_drawRow(CW_elems.bar, CW_elems.width);

  char row_buff[13];
  ContextWindow_drawRow(CW_elems.blank, CW_elems.width);

  // draw score
  ContextWindow_drawRow("[> POINTS <]", CW_elems.width);
  snprintf(row_buff, 13, "[>-%06d-<]", Game_score);
  ContextWindow_drawRow(row_buff, CW_elems.width);

  // draw lines cleared
  ContextWindow_drawRow("[> CLEARS <]", CW_elems.width);
  snprintf(row_buff, 13, "[>-%06d-<]", Game_lines_cleared);
  ContextWindow_drawRow(row_buff, CW_elems.width);
  ContextWindow_drawRow(CW_elems.blank, CW_elems.width);
  
  // draw next piece preview
  ContextWindow_drawRow("[>  NEXT  <]", CW_elems.width);
  ContextWindow_drawRow(CW_elems.divider, CW_elems.width);
  
  // skip drawing over the tetramino area (25, 9)
  for (uint32_t i = 0; i < 4; i++) {
    ContextWindow_drawRow("[>\033[8C<]", 8);
  }
  
  ContextWindow_drawRow(CW_elems.divider, CW_elems.width);

  // draw highscore info
  ContextWindow_drawRow(CW_elems.blank, CW_elems.width);
  ContextWindow_drawRow("[>TOPSCORE<]", CW_elems.width);
  snprintf(row_buff, 13, "[>%8.8s<]", Game_highscore_usr);
  ContextWindow_drawRow(row_buff, CW_elems.width);
  snprintf(row_buff, 13, "[>-%06d-<]", Game_highscore);
  ContextWindow_drawRow(row_buff, CW_elems.width);
  
  // finish off the "window"
  for (uint32_t i = 0; i < 3; i++) ContextWindow_drawRow(CW_elems.blank, CW_elems.width);
  ContextWindow_drawRow(CW_elems.bar, CW_elems.width);

  fflush(stdout);
}

void Game_setFlags(int argc, char *argv[]) {
  char *valid_args[] = {
    "--nobucket"
  };

  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];
    
    for (uint32_t j = 0; j < sizeof(valid_args) / sizeof(char *); j++) {
      if (!strcmp(valid_args[j], arg))
        continue;

      Game_flags |= FLAGS_NOBUCKET;
    }
  }
}

void Game_exit(void) {
  Terminal_restore();
  
  struct GameSaveData save = {
    .highscore      = MAX(Game_score, Game_highscore),
    .highscore_name = "Will",
  };
   
  cyaml_err_t err = cyaml_save_file(save_path, &cyaml_config, &GameSaveData_schema, &save, 0);

  if (err != CYAML_OK) {
    fprintf(stderr, "Error writing save data: %s\n", cyaml_strerror(err));
    exit(1);
  }
}

void Game_loadSave(void) {
  struct GameSaveData *save;

  cyaml_err_t err = cyaml_load_file(save_path, &cyaml_config, &GameSaveData_schema, (cyaml_data_t **)(&save), NULL);

  if (err != CYAML_OK) {
    fprintf(stderr, "Error reading save data: %s\n", cyaml_strerror(err));
    *save = (struct GameSaveData){0, "ERR"};
    exit(1);
  }

  Game_highscore = save->highscore;
  strncpy(Game_highscore_usr, save->highscore_name, 8);
  Game_highscore_usr[sizeof(Game_highscore_usr) - 1] = '\0'; // ensure null terminator

  err = cyaml_free(&cyaml_config, &GameSaveData_schema, save, 0);
  if (err != CYAML_OK) {
    fprintf(stderr, "Error freeing save data: %s\n", cyaml_strerror(err));
  }
}

int main(int argc, char *argv[]) {
  Game_setFlags(argc, argv);
  
  // i dont wanna go through replacing every call to printf with fwrite or a custom function
  setvbuf(stdout, NULL, _IOFBF, BUFSIZ);
    
  Tetramino tet = (Tetramino){LINE, DEG_0, 0, 0};
    
  // initialize ncurses
  atexit(Game_exit);
  Tetris_initialize();

  clock_gettime(CLOCK_MONOTONIC, &timestamp_previous_drop);

  // ensure new rng seed
  srand(time(0));  
  // initialize all characters to whitespace
  Screen_init();

  // reset and randomize tetramino
  Tetramino_randomize(&tetramino_next);
  Tetramino_respawn(&tet);
    
  // main game loop
  while (1) {
    struct timespec frame_start, frame_end;
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    // read and process inputs into actions
    Input_readAllKeys();
    Input_processAllKeys();

    if (actions.quit) break;
    
    // erase previous tetramino from screen
    Tetramino_draw(&tet, px_eraser);

    // apply user inputs
    Tetramino_applyActions(&tet);

    if (Screen_hasFilledLine()) {
      Screen_clearDropLines();
    }

    // draw new tetramino with updated position
    Tetramino_draw(&tet, px_box);
    
    // display new screen
    // reset cursor
    write(STDOUT_FILENO, "\033[;H", 4);
    Screen_print();
    ContextWindow_update();

    clock_gettime(CLOCK_MONOTONIC, &frame_end);
    struct timespec frame_time = timespecDifference(&frame_end, &frame_start);

    // sleep for ~66 ms (15 fps)
    struct timespec frame_total_duration = {0, 66666667};
    struct timespec frame_remaining = timespecDifference(&frame_total_duration, &frame_time);
    nanosleep(&frame_remaining, NULL);
  }
  
  exit(0);
}
