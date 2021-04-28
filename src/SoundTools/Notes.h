/**
 * Musical notes and their related frequencies
 */

#pragma once

#define C0  16
#define CS0 17
#define D0  18
#define DS0 19
#define E0  21
#define F0  22
#define FS0 23
#define G0  25
#define GS0 26
#define A0  28
#define AS0 29
#define B0  31
#define C1  33
#define CS1 35
#define D1  37
#define DS1 39
#define E1  41
#define F1  44
#define FS1 46
#define G1  49
#define GS1 52
#define A1  55
#define AS1 58
#define B1  62
#define C2  65
#define CS2 69
#define D2  73
#define DS2 78
#define E2  82
#define F2  87
#define FS2 93
#define G2  98
#define GS2 104
#define A2  110
#define AS2 117
#define B2  123
#define C3  131
#define CS3 139
#define D3  147
#define DS3 156
#define E3  165
#define F3  175
#define FS3 185
#define G3  196
#define GS3 208
#define A3  220
#define AS3 233
#define B3  247
#define C4  262
#define CS4 277
#define D4  294
#define DS4 311
#define E4  330
#define F4  349
#define FS4 370
#define G4  392
#define GS4 415
#define A4  440
#define AS4 466
#define B4  494
#define C5  523
#define CS5 554
#define D5  587
#define DS5 622
#define E5  659
#define F5  698
#define FS5 740
#define G5  784
#define GS5 831
#define A5  880
#define AS5 932
#define B5  988
#define C6  1047
#define CS6 1109
#define D6  1175
#define DS6 1245
#define E6  1319
#define F6  1397
#define FS6 1480
#define G6  1568
#define GS6 1661
#define A6  1760
#define AS6 1865
#define B6  1976
#define C7  2093
#define CS7 2217
#define D7  2349
#define DS7 2489
#define E7  2637
#define F7  2794
#define FS7 2960
#define G7  3136
#define GS7 3322
#define A7  3520
#define AS7 3729
#define B7  3951
#define C8  4186
#define CS8 4435
#define D8  4699
#define DS8 4978
#define E8  5274
#define F8  5588
#define FS8 5920
#define G8  6272
#define GS8 6645
#define A8  7040
#define AS8 7459
#define B8  7902

namespace sound_tools {

/**
 * @brief Determination of the frequency of a music note
 * 
 */
class Notes {
public:
    enum NotesEnum {C, CS, D, DS, E, F, FS, G, GS, A, AS, B};

    /// Determines the frequency of the indicate note and octave (0-8)
    int frequency(NotesEnum note, uint8_t octave){
        if (note>11) return 0;
        if (octave>7) return 0;
        return notes[octave][note];
    }

    /// Determines the frequency of the indicate note index from 0 to 107
    int frequency(uint16_t idx){
        NotesEnum mainNote = (NotesEnum) (idx % 12);
        uint8_t level = idx / 12;
        return frequency(mainNote, level);
    }
    
    /// Determines the frequency of the indicate main note index (0-6)  and octave (0-8)
    int mainFrequency(uint8_t mainNoteIdx, uint8_t octave){
        if (mainNoteIdx>6) return 0;
        static uint16_t mainNotes[] = {0,2,4,5,7,9,11};
        NotesEnum note = (NotesEnum) mainNotes[mainNoteIdx];
        return frequency(note, octave);
    }
    
    /// Determines the frequency of the indicate main note index from 0 to 62
    int mainFrequency(uint64_t idx){
        uint8_t mainNote = idx % 7;
        uint8_t level = idx /7;
        return mainFrequency(mainNote, level);
    }

protected:

    uint16_t notes[9][12] = {
        {C0, CS0, D0, DS0, E0, F0, FS0, G0, GS0, A0, AS0, B0},
        {C1, CS1, D1, DS1, E1, F1, FS1, G1, GS1, A1, AS1, B1},
        {C2, CS2, D2, DS2, E2, F2, FS2, G2, GS2, A2, AS2, B2},
        {C3, CS3, D3, DS3, E3, F3, FS3, G3, GS3, A3, AS3, B3},
        {C4, CS4, D4, DS4, E4, F4, FS4, G4, GS4, A4, AS4, B4},
        {C5, CS5, D5, DS5, E5, F5, FS5, G5, GS5, A5, AS5, B5},
        {C6, CS6, D6, DS6, E6, F6, FS6, G6, GS6, A6, AS6, B6},
        {C7, CS7, D7, DS7, E7, F7, FS7, G7, GS7, A7, AS7, B7},
        {C8, CS8, D8, DS8, E8, F8, FS8, G8, GS8, A8, AS8, B8}
    };


};

}

