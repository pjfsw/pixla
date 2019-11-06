#ifndef SCREEN_H_
#define SCREEN_H_

#include <stdbool.h>

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

#endif /* SCREEN_H_ */
