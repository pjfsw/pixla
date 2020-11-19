#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "strutils.h"

bool strendswith(char *str, char *suffix) {
    int len = strlen(str);
    int slen = strlen(suffix);
    if (len >= slen && !strncasecmp(&str[len-slen], suffix, slen)) {
        return true;
    }

    return false;
}

void strnosuffix(char *target, char *str, char *suffix, int maxlen) {
    char *tmp = malloc(maxlen+1);
    strncpy(tmp, str, maxlen);

    if (strendswith(tmp, suffix)) {
        memset(target, 0, maxlen);
        strncpy(target, tmp, strlen(tmp)-strlen(suffix));
        free(tmp);
    } else {
        strncpy(target, tmp, maxlen);
        free(tmp);
    }
}

