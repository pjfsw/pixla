#ifndef FILE_SELECTOR_H_
#define FILE_SELECTOR_H_

#include <SDL2/SDL.h>

typedef struct _FileSelector FileSelector;

FileSelector *fileSelector_init();

void fileSelector_loadDir(FileSelector *fileSelector, char *title, char *dir);

void fileSelector_close(FileSelector *fileSelector);

void fileSelector_prev(FileSelector *fileSelector);

void fileSelector_next(FileSelector *fileSelector);

char *fileSelector_getName(FileSelector *fileSelector);

void fileSelector_render(FileSelector *fileSelector, SDL_Renderer *renderer, int x, int y, int maxRows);


#endif /* FILE_SELECTOR_H_ */
