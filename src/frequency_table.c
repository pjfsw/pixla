#include "frequency_table.h"
#include <stdio.h>

//#define _FREQUENCY_TABLE_SIZE 96
//#define _FREQUENCY_TABLE_SCALER 10

typedef struct _FrequencyTable {
    Uint32 *frequencies;
    Uint8 notes;
    Uint8 scaleFactor;
    Uint8 baseNote;
} FrequencyTable;

FrequencyTable *frequencyTable_init(Uint8 notes, Uint8 scaleFactor, Sint8 baseNote) {
    FrequencyTable *frequencyTable = calloc(1, sizeof(FrequencyTable));
    frequencyTable->frequencies = calloc(notes, sizeof(Uint32));
    frequencyTable->notes = notes;
    frequencyTable->scaleFactor = scaleFactor;
    frequencyTable->baseNote = baseNote;

    for (int note = 0; note < notes; note++) {
        int noteOffset = note+baseNote;
        frequencyTable->frequencies[note] = 440.0 * pow(2, (double)noteOffset/(double)12) * (double)scaleFactor;
        printf("freq[%d]=%d\n", note, frequencyTable->frequencies[note]);
    }
    return frequencyTable;
}

void frequencyTable_close(FrequencyTable *frequencyTable) {
    if (frequencyTable != NULL) {
        free(frequencyTable->frequencies);
        free(frequencyTable);
    }
}

Uint8 frequencyTable_getScaleFactor(FrequencyTable *frequencyTable) {
    return frequencyTable->scaleFactor;
}

Uint32 frequencyTable_getScaledValue(FrequencyTable *frequencyTable, Uint8 note) {
    return frequencyTable->frequencies[note];
}

Uint8 frequencyTable_size(FrequencyTable *frequencyTable) {
    return frequencyTable->notes;
}

Uint32 frequencyTable_getHighestScaledFrequency(FrequencyTable *frequencyTable) {
    return frequencyTable->frequencies[frequencyTable->notes-1];
}

Uint32 frequencyTable_getLowestScaledFrequency(FrequencyTable *frequencyTable) {
    return frequencyTable->frequencies[0];
}

