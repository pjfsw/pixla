#include "settings_component.h"
#include "screen.h"

#define MAX_SETTINGS 100

typedef struct {
    SettingsAlterationFunc increaseFunc;
    SettingsAlterationFunc decreaseFunc;
    SettingsValueGetterFunc valueGetter;
    SettingsIsActiveFunc isActiveFunc;
    char *label;
    void *userData;
} SettingsItem;

typedef struct _SettingsComponent {
    SettingsItem *settings[MAX_SETTINGS];
    int currentSetting;
    int settingCount;
    int rowOffset;
} SettingsComponent;

/**
 * Create a new settings component
 */
SettingsComponent *settings_create() {
    SettingsComponent *settings = calloc(1, sizeof(SettingsComponent));
    return settings;
}

/**
 * Close a settings component
 */
void settings_close(SettingsComponent *settings) {
    if (settings != NULL) {
        for (int i = 0; i < MAX_SETTINGS; i++) {
            if (settings->settings[i] != NULL) {
                free(settings->settings[i]);
                settings->settings[i] = NULL;
            }
        }

        free(settings);
        settings = NULL;
    }
}

void settings_add(
        SettingsComponent *settings,
        char *label,
        SettingsAlterationFunc decreaseFunc,
        SettingsAlterationFunc increaseFunc,
        SettingsValueGetterFunc valueGetter,
        SettingsIsActiveFunc isActiveFunc,
        void *userData) {
    for (int i = 0; i < MAX_SETTINGS; i++) {
        if (settings->settings[i] == NULL) {
            SettingsItem *settingsItem = calloc(1, sizeof(SettingsItem));
            settingsItem->label = label;
            settingsItem->decreaseFunc = decreaseFunc;
            settingsItem->increaseFunc = increaseFunc;
            settingsItem->isActiveFunc = isActiveFunc;
            settingsItem->valueGetter = valueGetter;
            settingsItem->userData = userData;
            settings->settings[i] = settingsItem;
            settings->settingCount++;
            return;
        }
    }

}

void settings_next(SettingsComponent *settings) {
    if (settings->currentSetting == settings->settingCount -1) {
        settings->currentSetting = 0;
    } else {
        settings->currentSetting++;
    }
}

void settings_prev(SettingsComponent *settings) {
    if (settings->currentSetting == 0) {
        settings->currentSetting = settings->settingCount - 1;
    } else {
        settings->currentSetting--;
    }
}

void settings_increase(SettingsComponent *settings) {
    SettingsItem *setting = settings->settings[settings->currentSetting];
    setting->increaseFunc(setting->userData);

}

void settings_decrease(SettingsComponent *settings) {
    SettingsItem *setting = settings->settings[settings->currentSetting];
    setting->decreaseFunc(setting->userData);
}

void settings_render(SettingsComponent *settings, SDL_Renderer *renderer, int x, int y, int maxRows) {
    SDL_Color color = {.r = 255, .g=255, .b=255, .a=255};

    if (settings->currentSetting < settings->rowOffset) {
        settings->rowOffset = settings->currentSetting;
    }
    if (settings->currentSetting >= settings->rowOffset + maxRows) {
        settings->rowOffset = settings->currentSetting - maxRows + 1;
    }

    int rowsToRender = settings->settingCount-settings->rowOffset;
    if (rowsToRender > maxRows) {
        rowsToRender = maxRows;
    }
    char buf[100];

    for (int row = 0; row < rowsToRender; row++) {
        SettingsItem *item = settings->settings[settings->rowOffset+row];
        char *label = item->label;
        char *value = item->valueGetter(item->userData);
        char cursor = settings->rowOffset+row == settings->currentSetting ? '>' : ' ';
        sprintf(buf, "%c%s: %s",cursor, label, value);
        screen_print(x,y + row * 10, buf, &color);
            //Font .. SDL_Copy to render herperp
     //   _settings_print(renderer, buf, x, y);
    }

}

