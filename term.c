#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "term.h"

// linux specific headers
#if defined(__linux__)
#include <unistd.h>
#include <termios.h>
#endif

// Windows specific headers & definitions
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <conio.h>
#include <windows.h>

#define read _read
#define write _write
#define close _close
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


#if defined(__linux__)
#define TerminalIOState struct termios
#elif defined(_WIN32) || defined(_WIN64)
#define TerminalIOState DWORD
#endif

TerminalIOState saved_state;

uint32_t restore_row = 1;
uint32_t restore_col = 1;

void Terminal_saveState() {
#if defined(__linux__)

  tcgetattr(STDIN_FILENO, &saved_state);
  
#elif defined(_WIN32) || defined(_WIN64)
  
  HANDLE stdin_h = GetStdHandle(STD_INPUT_HANDLE);
  GetConsoleMode(stdin_h, &saved_state);

#endif
}

void Terminal_setRaw() {
#if defined(__linux__)

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

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
      perror("Terminal raw");
      exit(errno);
    }

#elif defined(_WIN32) || defined(_WIN64)

  HANDLE stdin_h = GetStdHandle(STD_INPUT_HANDLE);
  DWORD in_mode;
  GetConsoleMode(stdin_h, &in_mode);

  in_mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

  SetConsoleMode(stdin_h, in_mode);

#endif
}

void Terminal_restore() {
#if defined(__linux__)

    // reset terminal to default config
    // swap to original buffer
    write(STDOUT_FILENO, "\033[?1049l", 8);
    // show cursor
    write(STDOUT_FILENO, "\033[?25h", 6);
    // resposition cursor
    char pos_str[32];
    int len = snprintf(pos_str, 32, "\033[%d;%dH", restore_row, restore_col);
    write(STDOUT_FILENO, pos_str, len);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_state)) {
      perror("Terminal restore");
      exit(errno);
    }

#elif defined(_WIN32) || defined(_WIN64)

  HANDLE stdin_h = GetStdHandle(STD_INPUT_HANDLE);
  SetConsoleMode(stdin_h, saved_state);

#endif
}

void Terminal_getCursorPosition() {
  write(STDOUT_FILENO, "\033[6n", 4);
  
  char buff[32];
  int nchars = read(STDIN_FILENO, buff, 32);
  
  buff[MIN(nchars, 31)] = '\0';
  sscanf(buff, "%*c%*c%d;%dR", &restore_row, &restore_col);
}

size_t Terminal_readAllInputs(char *buffer, size_t buffer_size) {
#if defined(__linux__)
  buffer_size++;
  size_t nread;
  char c;
  size_t index = 0;
  
  while ((nread = read(STDIN_FILENO, &c, 1)) == 1 && index < buffer_size) {
    buffer[index] = c;
    index++;
  }

  return index;

#elif defined(_WIN32) || defined(_WIN64)

  char c;
  size_t index = 0;
  while (_kbhit() && index < buffer_size) {
    _read(STDIN_FILENO, &c, 1);
    buffer[index] = c;
    index++;
  }

  return index;

#endif
}
