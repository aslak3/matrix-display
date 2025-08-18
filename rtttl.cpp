#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "matrix_display.h"
#include "rtttl.h"

/* RTTTL format:
 *
 * HauntHouse: d=4,o=5,b=108: 2a4, 2e, 2d#, 2b4, 2a4, 2c, 2d, 2a#4, \
 * 2e., e, 1f4, 1a4, 1d#, 2e., d, 2c., b4, 1a4, 1p, 2a4, 2e, 2d#, 2b4, \
 * 2a4, 2c, 2d, 2a#4, 2e., e, 1f4, 1a4, 1d#, 2e., d, 2c., b4, 1a4
 *
 * (All spaces should be removed prior to parsing)
 *
 * Name: This should be under 16 chars. Or 10?
 *
 * d=default duration of a note, from 1 (full note) to 32 (32th of a note)
 * o=octave - from 4 to 7
 * b=tempo in beats per minute
 *
 * Notes are size of note followed by the note followed by the octave.
 *
 * Size of note may be missing, in which case the default is used (1 to 32)
 *
 * Actual notes are letters a to g, with optional # (sharp). This is the only
 * mandatory part. p is a special note, meaning pause. If Pausing the octave should
 * be missing.
 *
 * Lastly the octave, from 4 to 7. If missing the default is used.
 **/

rtttl::rtttl(void)
{
    memset(&header, 0, sizeof(header));
}

// Copy the tune in and parses the RTTTL header
bool rtttl::parse_header(char *buffer)
{
    int c = 0;

    strncpy(tune, buffer, sizeof(tune));
    p = tune;

    while (*p && *p != ':') {
        header.name[c++] = *p++;
    }
    header.name[c] = '\0';
    p++; // Skip over the :

    while (*p && *p != ':' &&
        (!header.default_duration || !header.default_octave || !header.beats_per_minute)
    ) {
        char key[256];
        char value[256];

        c = 0;
        while (*p && (*p != '=' && *p != ':')) {
            key[c++] = *p++;
        }
        key[c++] = '\0';
        p++; // Skip over the =

        c = 0;
        while (*p && (*p != ',' && *p != ':')) {
            value[c++] = *p++;
        }
        value[c++] = '\0';
        if (*p && *p == ',') {
            p++; // Skip over the ,
        }

        if (strcmp(key, "d") == 0) {
            header.default_duration = atol(value);
        } else if (strcmp(key, "o") == 0) {
            header.default_octave = atol(value);
        } else if (strcmp(key, "b") == 0) {
            header.beats_per_minute = atol(value);
        } else {
            return false;
        }
    }

    p++; // Skip over the :, onto notes!

    // Sanity checks.
    if (header.default_duration < 1 || header.default_duration > 32) {
        return false;
    }
    if (header.default_octave < 4 || header.default_octave > 7) {
        return false;
    }
    if (header.beats_per_minute == 0) {
        return false;
    }

    DEBUG_printf("Name: %s\n", header.name);
    DEBUG_printf("Default duration: %d\n", header.default_duration);
    DEBUG_printf("Default octave: %d\n", header.default_octave);
    DEBUG_printf("Beats per minute: %d\n", header.beats_per_minute);

    return true;
}

bool rtttl::get_next_note(rtttl_note_t *note)
{
    int c;

    char temp[256];
    memset(temp, 0, 256);

    // See if we have reached the end.
    if (!*p) {
        return false;
    }

    c = 0;
    while (*p && isdigit(*p)) {
        temp[c++] = *p++;
    }
    temp[c++] = '\0';

    int duration = atol(temp);
    if (duration == 0) {
        duration = header.default_duration;
    }
    else if (duration < 1 || duration > 32) {
        DEBUG_printf("Bad duration\n");
        return false;
    }

    int key_number = -1;
    bool sharp = false;
    bool pause = false;
    if (*p && (*p >= 'a' && *p <= 'g')) {
        key_number = (*p++ - 'a' + 5) % 7;
        if (*p && *p == '#') {
            sharp = true;
            p++;
        }
    }
    else if (*p && *p == 'p') {
        pause = true;
        p++;
    }
    else {
        DEBUG_printf("Not a note char\n");
        return false;
    }

    c = 0;
    while (*p && isdigit(*p)) {
        temp[c++] = *p++;
    }
    temp[c++] = '\0';

    int octave = atol(temp);
    if (octave == 0) {
        octave = header.default_octave;
    }
    else if (octave < 4 || octave > 7) {
        DEBUG_printf("Bad octave\n");
        return false;
    }

    if (*p && *p == ',') {
        p++; // Skip over the comma, if there is one.
    }

    int ms_per_beat = (60 * 1000) / header.beats_per_minute;
    int duration_ms = ms_per_beat / duration * 4;

    // If we are pausing, the frequency will be 0.
    int frequency_hz = 0;

    if (!pause) {
        if (!sharp) {
            frequency_hz = flat_note_table[key_number];
        }
        else {
            frequency_hz = sharp_note_table[key_number];
        }
        frequency_hz = frequency_hz << (octave - 4);
    }

    note->frequency_hz = frequency_hz;
    note->duration_ms = duration_ms;

    return true;
}
