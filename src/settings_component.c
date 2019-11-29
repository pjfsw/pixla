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
    int userIndex;
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
                free(settings->settings[i]->label);
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
        void *userData,
        int userIndex) {
    for (int i = 0; i < MAX_SETTINGS; i++) {
        if (settings->settings[i] == NULL) {
            SettingsItem *settingsItem = calloc(1, sizeof(SettingsItem));
            settingsItem->label = calloc(strlen(label)+1, 1);
            strcpy(settingsItem->label, label);
            settingsItem->decreaseFunc = decreaseFunc;
            settingsItem->increaseFunc = increaseFunc;
            settingsItem->isActiveFunc = isActiveFunc;
            settingsItem->valueGetter = valueGetter;
            settingsItem->userData = userData;
            settingsItem->userIndex = userIndex;
            settings->settings[i] = settingsItem;
            settings->settingCount++;
            return;
        }
    }

}

bool _settings_isActive(SettingsItem *setting) {
    return setting->isActiveFunc == NULL || setting->isActiveFunc(setting->userData, setting->userIndex);
}

void _settings_next(SettingsComponent *settings) {
    if (settings->currentSetting == settings->settingCount -1) {
        settings->currentSetting = 0;
    } else {
        settings->currentSetting++;
    }
}

void settings_next(SettingsComponent *settings) {
    do {
        _settings_next(settings);
    } while (!_settings_isActive(settings->settings[settings->currentSetting]));
}

void _settings_prev(SettingsComponent *settings) {
    if (settings->currentSetting == 0) {
        settings->currentSetting = settings->settingCount - 1;
    } else {
        settings->currentSetting--;
    }
}

void settings_prev(SettingsComponent *settings) {
    do {
        _settings_prev(settings);
    } while (!_settings_isActive(settings->settings[settings->currentSetting]));
}

void settings_increase(SettingsComponent *settings) {
    SettingsItem *setting = settings->settings[settings->currentSetting];
    if (_settings_isActive(setting)) {
        setting->increaseFunc(setting->userData, setting->userIndex);
    }

}

void settings_decrease(SettingsComponent *settings) {
    SettingsItem *setting = settings->settings[settings->currentSetting];
    if (_settings_isActive(setting)) {
        setting->decreaseFunc(setting->userData, setting->userIndex);
    }
}

void settings_render(SettingsComponent *settings, SDL_Renderer *renderer, int x, int y, int maxRows) {
    SDL_Color color = {.r = 255, .g=255, .b=255, .a=255};

    while (!_settings_isActive(settings->settings[settings->currentSetting])) {
        _settings_prev(settings);
    }

    if (settings->currentSetting < settings->rowOffset) {
        settings->rowOffset = settings->currentSetting;
    }


    int lastValidPos = settings->currentSetting;
    for (int rows = 0, settingPos = lastValidPos; rows < maxRows && settingPos >= 0; settingPos--) {
        SettingsItem *item = settings->settings[settingPos];
        if (item->isActiveFunc != NULL && !item->isActiveFunc(item->userData, item->userIndex)) {
            continue;
        }
        lastValidPos = settingPos;
        rows++;
    }
    if (lastValidPos > settings->rowOffset) {
        settings->rowOffset = lastValidPos;
    }
    char buf[100];

    int settingToRender = settings->rowOffset;

    for (int row = 0; row < maxRows && settingToRender < settings->settingCount; settingToRender++) {
        SettingsItem *item = settings->settings[settingToRender];
        if (item->isActiveFunc != NULL && !item->isActiveFunc(item->userData, item->userIndex)) {
            continue;
        }
        char *label = item->label;
        char *value = item->valueGetter(item->userData, item->userIndex);
        char cursor = settingToRender == settings->currentSetting ? '>' : ' ';
        sprintf(buf, "%c%s: %s",cursor, label, value);
        screen_print(x,y + row * 10, buf, &color);
        row++;
            //Font .. SDL_Copy to render herperp
     //   _settings_print(renderer, buf, x, y);
    }

}

