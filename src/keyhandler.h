#ifndef KEYHANDLER_H_
#define KEYHANDLER_H_

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef void(*KeyhandlerFunc)(void *userData, SDL_Scancode scancode, SDL_Keymod keymod);

typedef bool(*KeyhandlerPredicate)(void *userData);

typedef struct _Keyhandler Keyhandler;

typedef enum {
    KM_NONE,
    KM_SHIFT,
    KM_ALT,
    KM_SHIFT_ALT,
    KM_CTRL
} KeyhandlerModifier;
/**
 * Initialize the keyhandler.
 *
 * Returns Keyhandler which must be closed with keyhandler_close() on exit
 * Returns NULL if not able to initialize
 */
Keyhandler *keyhandler_init();

/**
 * Close the keyhandler
 */
void keyhandler_close(Keyhandler *keyhandler);

/**
 * Register keyhandler function
 *
 * The keyhandlerFunc is called if the scancode and modifierMask matches the
 * keypress, and the predicate function returns true.
 */
void keyhandler_register(
        Keyhandler *keyhandler,
        SDL_Scancode scancode,
        KeyhandlerModifier keyhandlerModifier,
        KeyhandlerPredicate predicateFunc,
        KeyhandlerFunc keyhandlerFunc,
        void *userData
);

/**
 * Handle key press
 */
void keyhandler_handle(
        Keyhandler *keyhandler,
        SDL_Scancode scancode,
        SDL_Keymod keymod
);

#endif /* KEYHANDLER_H_ */
