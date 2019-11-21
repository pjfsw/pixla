#ifndef SETTINGS_COMPONENT_H_
#define SETTINGS_COMPONENT_H_

#include <stdbool.h>
#include <SDL2/SDL.h>

typedef void (*SettingsAlterationFunc)(void *userData);

typedef char* (*SettingsValueGetterFunc)(void *userData);

typedef bool (*SettingsIsActiveFunc)(void *userData);

typedef struct _SettingsComponent SettingsComponent;

/**
 * Create a new settings component
 */
SettingsComponent *settings_create();

/**
 * Close a settings component
 */
void settings_close(SettingsComponent *settings);

/**
 * Add a setting to the component
 *
 * label The label for this etting
 * decreaseFunc Function to call to decrease value of this setting
 * increaseFunc Function to call to increase value of this setting
 * valueGetter Function to call to get the current value of this setting
 * isActive Function to call to determine if this setting is active/editable
 * userData Any userdata that should be sent to the callbacks
 */
void settings_add(
        SettingsComponent *settings,
        char *label,
        SettingsAlterationFunc decreaseFunc,
        SettingsAlterationFunc increaseFunc,
        SettingsValueGetterFunc valueGetter,
        SettingsIsActiveFunc isActiveFunc,
        void *userData
        );

/**
 * Go to next setting
 */
void settings_next(SettingsComponent *settings);

/**
 * Go to previous setting
 */
void settings_prev(SettingsComponent *settings);

/**
 * Increase current setting
 */
void settings_increase(SettingsComponent *settings);

/**
 * Decrease current setting
 */
void settings_decrease(SettingsComponent *settings);

/**
 * Render the settings component on screen
 */
void settings_render(SettingsComponent *settings, SDL_Renderer *renderer, int x, int y, int maxRows);



#endif /* SETTINGS_COMPONENT_H_ */
