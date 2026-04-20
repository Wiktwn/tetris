#ifndef TERM_H
#define TERM_H

void Terminal_saveState(void);
void Terminal_setRaw(void);
void Terminal_restore(void);
void Terminal_getCursorPosition(void);

#endif
