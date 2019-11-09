#ifndef SCREEN_H_
#define SCREEN_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

/*
 * Initialize screen
 *
 * After screen_init() has been called, screen_close() must be called before
 * termination
 */
bool screen_init();

/*
 * Close screen
 *
 */
void screen_close();

/*
 * Set pointers to column data to render
 *
 * column the column number, 0 to maximum number of columns-1
 * rows the number of rows to expect in the data
 * mainColumn a pointer to an array of char* containing main column data (3 chars)
 */
void screen_setColumn(Uint8 column, Uint8 rows, Sint8 *mainColumn);

void screen_setStepping(int stepping);

void screen_setRowOffset(Sint8 rowOffset);

/*
 * Update screen
 */
void screen_update();

#endif /* SCREEN_H_ */
