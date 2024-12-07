/**
 * Musical notes and their related frequencies
 */

#pragma once

#define N_C0  16.35f
#define N_CS0 17.32f
#define N_D0  18.35f
#define N_DS0 19.45f
#define N_E0  20.60f
#define N_F0  21.83f
#define N_FS0 23.12f
#define N_G0  24.50f
#define N_GS0 25.96f
#define N_A0  27.50f
#define N_AS0 29.14f
#define N_B0  30.87f
#define N_C1  32.70f
#define N_CS1 34.65f
#define N_D1  36.71f
#define N_DS1 38.89f
#define N_E1  41.20f
#define N_F1  43.65f
#define N_FS1 46.25f
#define N_G1  49.00f
#define N_GS1 51.91f
#define N_A1  55.00f
#define N_AS1 58.27f
#define N_B1  61.74f
#define N_C2  65.41f
#define N_CS2 69.30f
#define N_D2  73.42f
#define N_DS2 77.78f
#define N_E2  82.41f
#define N_F2  87.31f
#define N_FS2 92.50f
#define N_G2  98.00f
#define N_GS2 103.83f
#define N_A2  110.00f
#define N_AS2 116.54f
#define N_B2  123.47f
#define N_C3  130.81f
#define N_CS3 138.59f
#define N_D3  146.83f
#define N_DS3 155.56f
#define N_E3  164.81f
#define N_F3  174.61f
#define N_FS3 185.00f
#define N_G3  196.00f
#define N_GS3 207.65f
#define N_A3  220.00f
#define N_AS3 233.08f
#define N_B3  246.94f
#define N_C4  261.63f
#define N_CS4 277.18f
#define N_D4  293.66f
#define N_DS4 311.13f
#define N_E4  329.63f
#define N_F4  349.23f
#define N_FS4 369.99f
#define N_G4  392.00f
#define N_GS4 415.30f
#define N_A4  440.00f
#define N_AS4 466.16f
#define N_B4  493.88f
#define N_C5  523.25f
#define N_CS5 554.37f
#define N_D5  587.33f
#define N_DS5 622.25f
#define N_E5  659.25f
#define N_F5  698.46f
#define N_FS5 739.99f
#define N_G5  783.99f
#define N_GS5 830.61f
#define N_A5  880.00f
#define N_AS5 932.33f
#define N_B5  987.77f
#define N_C6  1046.5f
#define N_CS6 1108.73f
#define N_D6  1174.66f
#define N_DS6 1244.51f
#define N_E6  1318.51f
#define N_F6  1396.91f
#define N_FS6 1479.89f
#define N_G6  1567.89f
#define N_GS6 1661.22f
#define N_A6  1760.00f
#define N_AS6 1864.66f
#define N_B6  1975.53f
#define N_C7  2093.00f
#define N_CS7 2217.46f
#define N_D7  2349.32f
#define N_DS7 2489.02f
#define N_E7  2637.02f
#define N_F7  2793.83f
#define N_FS7 2959.96f
#define N_G7  3135.96f
#define N_GS7 3322.44f
#define N_A7  3520.00f
#define N_AS7 3729.31f
#define N_B7  3951.07f
#define N_C8  4186.01f
#define N_CS8 4434.92f
#define N_D8  4698.63f
#define N_DS8 4978.03f
#define N_E8  5274.04f
#define N_F8  5587.65f
#define N_FS8 5919.91f
#define N_G8  6271.93f
#define N_GS8 6644.88f
#define N_A8  7040.00f
#define N_AS8 7458.62f
#define N_B8  7902.13f

namespace audio_tools {

/**
 * @brief Determination of the frequency of a music note
 * @ingroup tools
 * @author Phil Schatzmann
 * @copyright GPLv3
 * 
 */
class MusicalNotes {
public:
    /// @brief Notes @ingroup tools
    enum MusicalNotesEnum {C, CS, D, DS, E, F, FS, G, GS, A, AS, B};

    /// Determines the frequency of the indicate note and octave (0-8)
    float frequency(MusicalNotesEnum note, uint8_t octave) const {
        if (note>11) return 0;
        if (octave>8) return 0;
        return notes[octave][note];
    }

    /// Determines the frequency of the indicate note index from 0 to 107
    float frequency(uint16_t idx) const {
        MusicalNotesEnum mainNote = (MusicalNotesEnum) (idx % 12);
        uint8_t octave = idx / 12;
        return frequency(mainNote, octave);
    }

    int frequencyCount() const {
        return 108;
    }
    
    /// Determines the frequency of the indicate main note index (0-6)  and octave (0-8)
    float mainFrequency(uint8_t mainNoteIdx, uint8_t octave) const {
        if (mainNoteIdx>6) return 0;
        static int mainNotes[] = {0,2,4,5,7,9,11};
        MusicalNotesEnum note = (MusicalNotesEnum) mainNotes[mainNoteIdx];
        return frequency(note, octave);
    }
    
    /// Determines the frequency of the indicate main note index from 0 to 62
    float mainFrequency(uint64_t idx) const {
        uint8_t mainNote = idx % 7;
        uint8_t level = idx /7;
        return mainFrequency(mainNote, level);
    }

    /// Returns true if the frequency is audible (in the range of 20 Hz to 20 kHz)
    bool isAudible(float frequency) const {
        return frequency >= 20 && frequency<=20000;
    }

    /// Determines the closes note for a frequency. We also return the frequency difference
    const char* note(float frequency, float &diff) const {
        float* all_notes = (float*) notes;
        const int note_count = 12*9;

        // find closest note
        float min_diff = frequency;
        int min_pos = 0;
        for (int j=0; j<note_count; j++){
            int tmp = abs(frequency - all_notes[j]);
            if (tmp<min_diff){
                min_diff = tmp;
                min_pos = j;
            }
        }

        float noteFrequency = all_notes[min_pos];
        diff = frequency - noteFrequency;
        return notes_str[min_pos];
    }

    /// Determines the closes note for a frequency
    const char* note(float frequency) const {
        float diff;
        return note(frequency, diff);
    }

    /// Provides the note name for an index position
    const char* noteAt(int idx) {
        return notes_str[idx];
    }

    /// Determine frequency of MIDI note
    float  midiNoteToFrequency(int x) const {
        float a = 440.0f; //frequency of A (coomon value is 440Hz)
        return (a / 32.0f) * powf(2.0f, ((x - 9) / 12.0f));
    }

    /// Provide MIDI note for frequency
    int frequencyToMidiNote(float freq) const {
        return logf(freq/440.0f)/logf(2) * 12.0f + 69.0f;
    }

    float stkNoteToFrequency(int noteNumber) const {
        return  220.0f * powf( 2.0f, (noteNumber - 57.0f) / 12.0f );
    }

protected:

    float notes[9][12] = {
        {N_C0, N_CS0, N_D0, N_DS0, N_E0, N_F0, N_FS0, N_G0, N_GS0, N_A0, N_AS0, N_B0},
        {N_C1, N_CS1, N_D1, N_DS1, N_E1, N_F1, N_FS1, N_G1, N_GS1, N_A1, N_AS1, N_B1},
        {N_C2, N_CS2, N_D2, N_DS2, N_E2, N_F2, N_FS2, N_G2, N_GS2, N_A2, N_AS2, N_B2},
        {N_C3, N_CS3, N_D3, N_DS3, N_E3, N_F3, N_FS3, N_G3, N_GS3, N_A3, N_AS3, N_B3},
        {N_C4, N_CS4, N_D4, N_DS4, N_E4, N_F4, N_FS4, N_G4, N_GS4, N_A4, N_AS4, N_B4},
        {N_C5, N_CS5, N_D5, N_DS5, N_E5, N_F5, N_FS5, N_G5, N_GS5, N_A5, N_AS5, N_B5},
        {N_C6, N_CS6, N_D6, N_DS6, N_E6, N_F6, N_FS6, N_G6, N_GS6, N_A6, N_AS6, N_B6},
        {N_C7, N_CS7, N_D7, N_DS7, N_E7, N_F7, N_FS7, N_G7, N_GS7, N_A7, N_AS7, N_B7},
        {N_C8, N_CS8, N_D8, N_DS8, N_E8, N_F8, N_FS8, N_G8, N_GS8, N_A8, N_AS8, N_B8}
    };

    const char *notes_str[9*12]= {
         "C0","CS0","D0","DS0","E0","F0","FS0","G0","GS0","A0","AS0","B0",
         "C1","CS1","D1","DS1","E1","F1","FS1","G1","GS1","A1","AS1","B1",
         "C2","CS2","D2","DS2","E2","F2","FS2","G2","GS2","A2","AS2","B2",
         "C3","CS3","D3","DS3","E3","F3","FS3","G3","GS3","A3","AS3","B3",
         "C4","CS4","D4","DS4","E4","F4","FS4","G4","GS4","A4","AS4","B4",
         "C5","CS5","D5","DS5","E5","F5","FS5","G5","GS5","A5","AS5","B5",
         "C6","CS6","D6","DS6","E6","F6","FS6","G6","GS6","A6","AS6","B6",
         "C7","CS7","D7","DS7","E7","F7","FS7","G7","GS7","A7","AS7","B7",
         "C8","CS8","D8","DS8","E8","F8","FS8","G8","GS8","A8","AS8","B8"
    };


};

}

