#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "inputfield.h"
#include "screen.h"

#define INPUT_MAX_VALUE 100

typedef struct _Inputfield {
    char value[INPUT_MAX_VALUE+1];
} Inputfield;

Inputfield *inputfield_init() {
    Inputfield *input = calloc(1, sizeof(Inputfield));
    return input;
}

void inputfield_setValue(Inputfield *inputfield, char *value) {
    strncpy(inputfield->value, value, INPUT_MAX_VALUE);
}

char *inputfield_getValue(Inputfield *inputfield) {
    return inputfield->value;
}

void inputfield_input(Inputfield *inputfield, char input) {
    int len = strlen(inputfield->value);
    if (len < INPUT_MAX_VALUE) {
        inputfield->value[len] = input;
        inputfield->value[len+1] = 0;
    }
}

void inputfield_delete(Inputfield *inputfield) {
    int len = strlen(inputfield->value);
    if (len > 0) {
        inputfield->value[len-1] = 0;
    }
}

void inputfield_close(Inputfield *inputfield) {
    if (inputfield != NULL) {
        free(inputfield);
    }
}

void inputfield_render(Inputfield *inputfield, SDL_Renderer *renderer, int x, int y, int width) {
    char buf[INPUT_MAX_VALUE];
    strncpy(buf, inputfield->value, width);
    screen_print(x, y, buf, screen_getDefaultColor());
}
