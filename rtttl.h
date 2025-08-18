typedef struct {
    char name[16];
    int default_duration;
    int default_octave;
    int beats_per_minute;
} rtttl_header_t;

typedef struct {
    int frequency_hz;
    int duration_ms;
} rtttl_note_t;

class rtttl {
    public:
        rtttl(void);
        bool parse_header(char *buffer);
        bool get_next_note(rtttl_note_t *note);

    private:
        char tune[1024];
        char *p;
        rtttl_header_t header;

        const int flat_note_table[7] = {
            262,                // C
            293,                // D
            329,                // E
            349,                // F
            391,                // G
            440,                // A
            493,                // B
        };
        const int sharp_note_table[7] = {
            277,                // C#
            311,                // D#
            0,                  // (E#)
            369,                // F#
            415,                // G#
            466,                // A#
            0,                  // (B#)
        };
};
