#include "keyhandler.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#define MAX_HANDLERS_PER_SCANCODE 20

typedef struct _KeyhandlerMapping {
    KeyhandlerModifier keyhandlerModifier;
    KeyhandlerFunc keyhandlerFunc;
    KeyhandlerPredicate predicate;
    void *userData;
} KeyhandlerMapping;

typedef struct _Keyhandler {
    KeyhandlerMapping keyHandler[256][MAX_HANDLERS_PER_SCANCODE];
} Keyhandler;


Keyhandler *keyhandler_init() {
    Keyhandler *keyhandler = calloc(1, sizeof(Keyhandler));
    return keyhandler;
}

/**
 * Close the keyhandler
 */
void keyhandler_close(Keyhandler *keyhandler) {
    if (keyhandler != NULL) {
        free(keyhandler);
    }
}

void keyhandler_register(
        Keyhandler *keyhandler,
        SDL_Scancode scancode,
        KeyhandlerModifier keyhandlerModifier,
        KeyhandlerPredicate predicateFunc,
        KeyhandlerFunc keyhandlerFunc,
        void *userData
) {
    for (int i = 0; i < MAX_HANDLERS_PER_SCANCODE; i++) {
        KeyhandlerMapping *km = &keyhandler->keyHandler[scancode][i];

        if (km->keyhandlerFunc == NULL) {
            km->keyhandlerFunc = keyhandlerFunc;
            km->predicate = predicateFunc;
            km->keyhandlerModifier = keyhandlerModifier;
            km->userData = userData;
            break;
        }
    }
}

bool _keyhandler_validateModifier(SDL_Keymod keymod, KeyhandlerModifier modifier) {
    switch (modifier) {
    case KM_NONE:
        return keymod == 0;
    case KM_SHIFT:
        return keymod == KMOD_LSHIFT || keymod == KMOD_RSHIFT;
    case KM_ALT:
        return keymod == KMOD_LALT || keymod == KMOD_RALT;
    case KM_SHIFT_ALT:
        return ((keymod & KMOD_LALT) || (keymod & KMOD_RALT)) && ((keymod & KMOD_LSHIFT) || (keymod & KMOD_RSHIFT));
    case KM_CTRL:
        return keymod == KMOD_LCTRL || keymod == KMOD_RCTRL;
    }
    return false;
}

void keyhandler_handle(
        Keyhandler *keyhandler,
        SDL_Scancode scancode,
        SDL_Keymod keymod
) {
    for (int i = 0; i < MAX_HANDLERS_PER_SCANCODE; i++) {
        KeyhandlerMapping *km = &keyhandler->keyHandler[scancode][i];
//        printf("%d\n", i);
        if (
                (km->keyhandlerFunc != NULL) &&
                _keyhandler_validateModifier(keymod, km->keyhandlerModifier) &&
                (km->predicate == NULL || km->predicate(km->userData))
        ) {
            km->keyhandlerFunc(km->userData, scancode, keymod);
            break;
        }
    }
}
