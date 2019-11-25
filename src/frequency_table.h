#ifndef FREQUENCY_TABLE_H_
#define FREQUENCY_TABLE_H_

#include <SDL2/SDL.h>

typedef struct _FrequencyTable FrequencyTable;

/**
 * Initalize a frequency table
 *
 * notes - The number of notes in the frequency table
 * scaleFactor - The factor with which to scale the frequency to increase precision
 * baseNote - The basenote for index 0 relative to 440 Hz (-45 means C0)
 *
 */
FrequencyTable *frequencyTable_init(Uint8 notes, Uint8 scaleFactor, Sint8 baseNote);

/**
 * Close frequency table instance
 */
void frequencyTable_close(FrequencyTable *frequencyTable);

/**
 * Get scaler for frequency table values - the returned value should be divided
 * with this value to get the actual frequency
 */
Uint8 frequencyTable_getScaleFactor(FrequencyTable *frequencyTable);

/**
 * Return frequency in its upscaled form. Should be transformed before being
 * put to audio use or compared with real frequencies using the getScaleFactor()
 * method or the getRealFrequency() convenience method.
 */
Uint32 frequencyTable_getScaledValue(FrequencyTable *frequencyTable, Uint8 note);

/**
 * Return the highest (upscaled) value in the frequency table
 */
Uint32 frequencyTable_getHighestScaledFrequency(FrequencyTable *frequencyTable);

/**
 * Return the lowest (upscaled) value in the frequency table
 */
Uint32 frequencyTable_getLowestScaledFrequency(FrequencyTable *frequencyTable);


#endif /* FREQUENCY_TABLE_H_ */
