#ifndef STRUTILS_H_
#define STRUTILS_H_

#include <stdbool.h>

/**
 * Returns true if the string str ends with suffix
 */
bool strendswith(char *str, char *suffix);

/**
 * Copy string without specified suffix
 * str and target  can be the same array
 *
 * If maxlen is less than the length of str, the suffix will not be removed
 */
void strnosuffix(char *target, char *str, char *suffix, int maxlen);

#endif /* STRUTILS_H_ */
