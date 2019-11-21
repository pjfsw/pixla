#ifndef FONT_H_
#define FONT_H_

#include <SDL2/SDL.h>

typedef struct _Font Font;

typedef enum {
    FONT_COLOR1,
    FONT_COLOR2
} FontColor;

Font *font_init(SDL_Color color1, SDL_Color color2);

void font_close(Font *font);

SDL_Texture *font_char(char c, FontColor color);

#endif /* FONT_H_ */
