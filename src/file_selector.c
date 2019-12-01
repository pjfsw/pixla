#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>

#include "file_selector.h"
#include "screen.h"
#include "strutils.h"
#include "songsuffix.h"

#define MAX_DIR_ENTRIES 500
#define MAX_FILENAME 500
#define MAX_TITLE 20

typedef struct {
    char name[MAX_FILENAME];
} DirEntry;

typedef struct _FileSelector {
    char title[MAX_TITLE];
    DirEntry files[MAX_DIR_ENTRIES];
    int currentFile;
    int fileCount;
    int rowOffset;

    SDL_Color color;
} FileSelector;

FileSelector *fileSelector_init() {
    FileSelector *fileSelector = calloc(1, sizeof(FileSelector));
    fileSelector->color.r = 255;
    fileSelector->color.g = 255;
    fileSelector->color.b = 255;
    fileSelector->color.a = 255;

    return fileSelector;
}

bool _fileSelector_isValidFilename(struct dirent *de) {
    if (de->d_type != DT_REG) {
        return false;
    }

    return strendswith(de->d_name, SONG_SUFFIX);
}

void fileSelector_loadDir(FileSelector *fileSelector, char *title, char *dirname) {
    memset(fileSelector->files, 0, sizeof(DirEntry) * MAX_DIR_ENTRIES);
    strncpy(fileSelector->title, title, MAX_TITLE);
    fileSelector->fileCount = 0;
    fileSelector->rowOffset = 0;
    fileSelector->currentFile = 0;

    DIR *dir = opendir(dirname);
    if (dir != NULL) {
        struct dirent *de;
        while (fileSelector->fileCount < MAX_DIR_ENTRIES && NULL != (de = readdir(dir))) {
            if (!_fileSelector_isValidFilename(de)) {
                continue;
            }
            strncpy(fileSelector->files[fileSelector->fileCount].name, de->d_name, MAX_FILENAME);
            fileSelector->fileCount++;
        }

        closedir(dir);
    }
}

void fileSelector_close(FileSelector *fileSelector) {
    if (fileSelector != NULL) {
        free(fileSelector);
    }
}

void fileSelector_prev(FileSelector *fileSelector) {
    if (fileSelector->currentFile == 0) {
        fileSelector->currentFile = fileSelector->fileCount-1;
    } else {
        fileSelector->currentFile--;
    }
}

void fileSelector_next(FileSelector *fileSelector) {
    if (fileSelector->currentFile == fileSelector->fileCount-1) {
        fileSelector->currentFile = 0;
    } else {
        fileSelector->currentFile++;
    }
}

char *fileSelector_getName(FileSelector *fileSelector) {
    if (fileSelector->fileCount > 0) {
        return fileSelector->files[fileSelector->currentFile].name;
    } else {
        return NULL;
    }
}

void fileSelector_render(FileSelector *fileSelector, SDL_Renderer *renderer, int x, int y, int maxRows) {
    int fileRows = maxRows-1;

    if (fileSelector->currentFile < fileSelector->rowOffset) {
        fileSelector->rowOffset = fileSelector->currentFile;
    }
    if (fileSelector->currentFile > fileSelector->rowOffset+fileRows-1) {
        fileSelector->rowOffset = fileSelector->currentFile-fileRows+1;

    }
    char buf[100];

    screen_print(x,y, fileSelector->title, &fileSelector->color);
    for (int i = 0; i < fileRows; i++) {
        int file = fileSelector->rowOffset + i;
        if (file >= fileSelector->fileCount) {
            break;
        }
        char cursor = file == fileSelector->currentFile ? '>' : ' ';

        snprintf(buf,100,  "%c%s", cursor, fileSelector->files[file].name);
        screen_print(x, y + (1+i) * 10, buf, &fileSelector->color);
    }
}
