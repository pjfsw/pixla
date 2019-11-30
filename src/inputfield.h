#ifndef INPUTFIELD_H_
#define INPUTFIELD_H_

#include <SDL2/SDL.h>

typedef struct _Inputfield Inputfield;

Inputfield *inputfield_init();

void inputfield_setValue(Inputfield *inputfield, char *value);

char *inputfield_getValue(Inputfield *inputfield);

void inputfield_input(Inputfield *inputfield, char input);

void inputfield_delete(Inputfield *inputfield);

void inputfield_close(Inputfield *inputfield);

void inputfield_render(Inputfield *inputfield, SDL_Renderer *renderer, int x, int y, int width);

#endif /* INPUTFIELD_H_ */
