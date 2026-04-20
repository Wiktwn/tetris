#ifndef TERM_H
#define TERM_H

#include <stddef.h>

void Terminal_saveState(void);
void Terminal_setRaw(void);
void Terminal_restore(void);
void Terminal_getCursorPosition(void);
size_t Terminal_readAllInputs(char *buffer, size_t buffer_size);

#endif
