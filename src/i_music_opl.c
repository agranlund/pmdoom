// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	System interface for sound.
//
//-----------------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#ifdef ENABLE_SDLMIXER
#include <SDL_mixer.h>
#endif

#include "doomdef.h"
#include "doomstat.h"
#include "z_zone.h"

#include "m_misc.h"
#include "i_system.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_music_opl.h"
#include "i_music_midi.h"
#include "sounds.h"
#include "i_swap.h"
#include "w_wad.h"
#include "z_zone.h"
#include "md_midi.h"
#include "mus2mid.h"
#include "memio.h"
#include "opl.h"

#include <mint/osbind.h>

#define MAXMIDLENGTH (96 * 1024)

#define MIDI_CHANNELS_PER_TRACK 16

#define MIDI_RPN_MSB                 0x00
#define MIDI_RPN_PITCH_BEND_SENS_LSB 0x00
#define MIDI_RPN_FINE_TUNING_LSB     0x01
#define MIDI_RPN_COARSE_TUNING_LSB   0x02
#define MIDI_RPN_NULL                0x7F


typedef enum
{
    MIDI_EVENT_NOTE_OFF        = 0x80,
    MIDI_EVENT_NOTE_ON         = 0x90,
    MIDI_EVENT_AFTERTOUCH      = 0xA0,
    MIDI_EVENT_CONTROLLER      = 0xB0,
    MIDI_EVENT_PROGRAM_CHANGE  = 0xC0,
    MIDI_EVENT_CHAN_AFTERTOUCH = 0xD0,
    MIDI_EVENT_PITCH_BEND      = 0xE0,

    MIDI_EVENT_SYSEX           = 0xF0,
    MIDI_EVENT_SYSEX_SPLIT     = 0xF7,
    MIDI_EVENT_META            = 0xFF,
} midi_event_type_t;


typedef enum
{
    MIDI_CONTROLLER_BANK_SELECT_MSB = 0x00,
    MIDI_CONTROLLER_MODULATION      = 0x01,
    MIDI_CONTROLLER_BREATH_CONTROL  = 0x02,
    MIDI_CONTROLLER_FOOT_CONTROL    = 0x04,
    MIDI_CONTROLLER_PORTAMENTO      = 0x05,
    MIDI_CONTROLLER_DATA_ENTRY_MSB  = 0x06,
    MIDI_CONTROLLER_VOLUME_MSB      = 0x07,
    MIDI_CONTROLLER_PAN             = 0x0A,
    MIDI_CONTROLLER_EXPRESSION      = 0x0B,

    MIDI_CONTROLLER_BANK_SELECT_LSB = 0x20,
    MIDI_CONTROLLER_DATA_ENTRY_LSB  = 0x26,
    MIDI_CONTROLLER_VOLUME_LSB      = 0X27,

    MIDI_CONTROLLER_HOLD1_PEDAL     = 0x40,
    MIDI_CONTROLLER_SOFT_PEDAL      = 0x43,

    MIDI_CONTROLLER_REVERB          = 0x5B,
    MIDI_CONTROLLER_CHORUS          = 0x5D,

    MIDI_CONTROLLER_NRPN_LSB        = 0x62,
    MIDI_CONTROLLER_NRPN_MSB        = 0x63,
    MIDI_CONTROLLER_RPN_LSB         = 0x64,
    MIDI_CONTROLLER_RPN_MSB         = 0x65,

    MIDI_CONTROLLER_ALL_SOUND_OFF   = 0x78,
    MIDI_CONTROLLER_RESET_ALL_CTRLS = 0x79,
    MIDI_CONTROLLER_ALL_NOTES_OFF   = 0x7B,

    MIDI_CONTROLLER_POLY_MODE_OFF   = 0x7E,
    MIDI_CONTROLLER_POLY_MODE_ON    = 0x7F,
} midi_controller_t;

typedef enum
{
    MIDI_META_SEQUENCE_NUMBER       = 0x00,

    MIDI_META_TEXT                  = 0x01,
    MIDI_META_COPYRIGHT             = 0x02,
    MIDI_META_TRACK_NAME            = 0x03,
    MIDI_META_INSTR_NAME            = 0x04,
    MIDI_META_LYRICS                = 0x05,
    MIDI_META_MARKER                = 0x06,
    MIDI_META_CUE_POINT             = 0x07,

    MIDI_META_CHANNEL_PREFIX        = 0x20,
    MIDI_META_END_OF_TRACK          = 0x2F,

    MIDI_META_SET_TEMPO             = 0x51,
    MIDI_META_SMPTE_OFFSET          = 0x54,
    MIDI_META_TIME_SIGNATURE        = 0x58,
    MIDI_META_KEY_SIGNATURE         = 0x59,
    MIDI_META_SEQUENCER_SPECIFIC    = 0x7F,
} midi_meta_event_type_t;


typedef enum
{
    EMIDI_CONTROLLER_TRACK_DESIGNATION = 0x6E,
    EMIDI_CONTROLLER_TRACK_EXCLUSION   = 0x6F,
    EMIDI_CONTROLLER_PROGRAM_CHANGE    = 0x70,
    EMIDI_CONTROLLER_VOLUME            = 0x71,
    EMIDI_CONTROLLER_LOOP_BEGIN        = 0x74,
    EMIDI_CONTROLLER_LOOP_END          = 0x75,
    EMIDI_CONTROLLER_GLOBAL_LOOP_BEGIN = 0x76,
    EMIDI_CONTROLLER_GLOBAL_LOOP_END   = 0x77,
} emidi_controller_t;

#define GENMIDI_NUM_INSTRS      128
#define GENMIDI_NUM_PERCUSSION  47
#define GENMIDI_HEADER          "#OPL_II#"
#define GENMIDI_FLAG_FIXED      0x0001         /* fixed pitch */
#define GENMIDI_FLAG_2VOICE     0x0004         /* double voice (OPL3) */
#define PERCUSSION_LOG_LEN      16

typedef struct __attribute__((packed))
{
    byte tremolo;
    byte attack;
    byte sustain;
    byte waveform;
    byte scale;
    byte level;
} genmidi_op_t;

typedef struct __attribute__((packed))
{
    genmidi_op_t modulator;
    byte feedback;
    genmidi_op_t carrier;
    byte unused;
    short base_note_offset;
} genmidi_voice_t;

typedef struct __attribute__((packed))
{
    unsigned short flags;
    byte fine_tuning;
    byte fixed_note;
    genmidi_voice_t voices[2];
} genmidi_instr_t;

typedef struct
{
    genmidi_instr_t *instrument;
    int volume;
    int volume_base;
    int pan;
    int bend;
} opl_channel_data_t;

typedef struct opl_voice_s opl_voice_t;

struct opl_voice_s
{
    int index;
    int op1, op2;
    int array;
    genmidi_instr_t *current_instr;
    unsigned int current_instr_voice;
    opl_channel_data_t *channel;
    unsigned int key;
    unsigned int note;
    unsigned int freq;
    unsigned int note_volume;
    unsigned int car_volume;
    unsigned int mod_volume;
    unsigned int reg_pan;
    unsigned int priority;
};

// Operators used by the different voices.
static const int voice_operators[2][OPL_NUM_VOICES] = {
    { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 },
    { 0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d, 0x13, 0x14, 0x15 }
};

// Frequency values to use for each note.
static const unsigned short frequency_curve[] = {

    0x133, 0x133, 0x134, 0x134, 0x135, 0x136, 0x136, 0x137,   // -1
    0x137, 0x138, 0x138, 0x139, 0x139, 0x13a, 0x13b, 0x13b,
    0x13c, 0x13c, 0x13d, 0x13d, 0x13e, 0x13f, 0x13f, 0x140,
    0x140, 0x141, 0x142, 0x142, 0x143, 0x143, 0x144, 0x144,
    0x145, 0x146, 0x146, 0x147, 0x147, 0x148, 0x149, 0x149,   // -2
    0x14a, 0x14a, 0x14b, 0x14c, 0x14c, 0x14d, 0x14d, 0x14e,
    0x14f, 0x14f, 0x150, 0x150, 0x151, 0x152, 0x152, 0x153,
    0x153, 0x154, 0x155, 0x155, 0x156, 0x157, 0x157, 0x158,
    // These are used for the first seven MIDI note values:
    0x158, 0x159, 0x15a, 0x15a, 0x15b, 0x15b, 0x15c, 0x15d,   // 0
    0x15d, 0x15e, 0x15f, 0x15f, 0x160, 0x161, 0x161, 0x162,
    0x162, 0x163, 0x164, 0x164, 0x165, 0x166, 0x166, 0x167,
    0x168, 0x168, 0x169, 0x16a, 0x16a, 0x16b, 0x16c, 0x16c,
    0x16d, 0x16e, 0x16e, 0x16f, 0x170, 0x170, 0x171, 0x172,   // 1
    0x172, 0x173, 0x174, 0x174, 0x175, 0x176, 0x176, 0x177,
    0x178, 0x178, 0x179, 0x17a, 0x17a, 0x17b, 0x17c, 0x17c,
    0x17d, 0x17e, 0x17e, 0x17f, 0x180, 0x181, 0x181, 0x182,
    0x183, 0x183, 0x184, 0x185, 0x185, 0x186, 0x187, 0x188,   // 2
    0x188, 0x189, 0x18a, 0x18a, 0x18b, 0x18c, 0x18d, 0x18d,
    0x18e, 0x18f, 0x18f, 0x190, 0x191, 0x192, 0x192, 0x193,
    0x194, 0x194, 0x195, 0x196, 0x197, 0x197, 0x198, 0x199,
    0x19a, 0x19a, 0x19b, 0x19c, 0x19d, 0x19d, 0x19e, 0x19f,   // 3
    0x1a0, 0x1a0, 0x1a1, 0x1a2, 0x1a3, 0x1a3, 0x1a4, 0x1a5,
    0x1a6, 0x1a6, 0x1a7, 0x1a8, 0x1a9, 0x1a9, 0x1aa, 0x1ab,
    0x1ac, 0x1ad, 0x1ad, 0x1ae, 0x1af, 0x1b0, 0x1b0, 0x1b1,
    0x1b2, 0x1b3, 0x1b4, 0x1b4, 0x1b5, 0x1b6, 0x1b7, 0x1b8,   // 4
    0x1b8, 0x1b9, 0x1ba, 0x1bb, 0x1bc, 0x1bc, 0x1bd, 0x1be,
    0x1bf, 0x1c0, 0x1c0, 0x1c1, 0x1c2, 0x1c3, 0x1c4, 0x1c4,
    0x1c5, 0x1c6, 0x1c7, 0x1c8, 0x1c9, 0x1c9, 0x1ca, 0x1cb,
    0x1cc, 0x1cd, 0x1ce, 0x1ce, 0x1cf, 0x1d0, 0x1d1, 0x1d2,   // 5
    0x1d3, 0x1d3, 0x1d4, 0x1d5, 0x1d6, 0x1d7, 0x1d8, 0x1d8,
    0x1d9, 0x1da, 0x1db, 0x1dc, 0x1dd, 0x1de, 0x1de, 0x1df,
    0x1e0, 0x1e1, 0x1e2, 0x1e3, 0x1e4, 0x1e5, 0x1e5, 0x1e6,
    0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed, 0x1ed,   // 6
    0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5,
    0x1f6, 0x1f6, 0x1f7, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc,
    0x1fd, 0x1fe, 0x1ff, 0x200, 0x201, 0x201, 0x202, 0x203,
    // First note of looped range used for all octaves:
    0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x20a, 0x20b,   // 7
    0x20c, 0x20d, 0x20e, 0x20f, 0x210, 0x210, 0x211, 0x212,
    0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21a,
    0x21b, 0x21c, 0x21d, 0x21e, 0x21f, 0x220, 0x221, 0x222,
    0x223, 0x224, 0x225, 0x226, 0x227, 0x228, 0x229, 0x22a,   // 8
    0x22b, 0x22c, 0x22d, 0x22e, 0x22f, 0x230, 0x231, 0x232,
    0x233, 0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23a,
    0x23b, 0x23c, 0x23d, 0x23e, 0x23f, 0x240, 0x241, 0x242,
    0x244, 0x245, 0x246, 0x247, 0x248, 0x249, 0x24a, 0x24b,   // 9
    0x24c, 0x24d, 0x24e, 0x24f, 0x250, 0x251, 0x252, 0x253,
    0x254, 0x256, 0x257, 0x258, 0x259, 0x25a, 0x25b, 0x25c,
    0x25d, 0x25e, 0x25f, 0x260, 0x262, 0x263, 0x264, 0x265,
    0x266, 0x267, 0x268, 0x269, 0x26a, 0x26c, 0x26d, 0x26e,   // 10
    0x26f, 0x270, 0x271, 0x272, 0x273, 0x275, 0x276, 0x277,
    0x278, 0x279, 0x27a, 0x27b, 0x27d, 0x27e, 0x27f, 0x280,
    0x281, 0x282, 0x284, 0x285, 0x286, 0x287, 0x288, 0x289,
    0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290, 0x292, 0x293,   // 11
    0x294, 0x295, 0x296, 0x298, 0x299, 0x29a, 0x29b, 0x29c,
    0x29e, 0x29f, 0x2a0, 0x2a1, 0x2a2, 0x2a4, 0x2a5, 0x2a6,
    0x2a7, 0x2a9, 0x2aa, 0x2ab, 0x2ac, 0x2ae, 0x2af, 0x2b0,
    0x2b1, 0x2b2, 0x2b4, 0x2b5, 0x2b6, 0x2b7, 0x2b9, 0x2ba,   // 12
    0x2bb, 0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c2, 0x2c3, 0x2c4,
    0x2c5, 0x2c7, 0x2c8, 0x2c9, 0x2cb, 0x2cc, 0x2cd, 0x2ce,
    0x2d0, 0x2d1, 0x2d2, 0x2d4, 0x2d5, 0x2d6, 0x2d8, 0x2d9,
    0x2da, 0x2dc, 0x2dd, 0x2de, 0x2e0, 0x2e1, 0x2e2, 0x2e4,   // 13
    0x2e5, 0x2e6, 0x2e8, 0x2e9, 0x2ea, 0x2ec, 0x2ed, 0x2ee,
    0x2f0, 0x2f1, 0x2f2, 0x2f4, 0x2f5, 0x2f6, 0x2f8, 0x2f9,
    0x2fb, 0x2fc, 0x2fd, 0x2ff, 0x300, 0x302, 0x303, 0x304,
    0x306, 0x307, 0x309, 0x30a, 0x30b, 0x30d, 0x30e, 0x310,   // 14
    0x311, 0x312, 0x314, 0x315, 0x317, 0x318, 0x31a, 0x31b,
    0x31c, 0x31e, 0x31f, 0x321, 0x322, 0x324, 0x325, 0x327,
    0x328, 0x329, 0x32b, 0x32c, 0x32e, 0x32f, 0x331, 0x332,
    0x334, 0x335, 0x337, 0x338, 0x33a, 0x33b, 0x33d, 0x33e,   // 15
    0x340, 0x341, 0x343, 0x344, 0x346, 0x347, 0x349, 0x34a,
    0x34c, 0x34d, 0x34f, 0x350, 0x352, 0x353, 0x355, 0x357,
    0x358, 0x35a, 0x35b, 0x35d, 0x35e, 0x360, 0x361, 0x363,
    0x365, 0x366, 0x368, 0x369, 0x36b, 0x36c, 0x36e, 0x370,   // 16
    0x371, 0x373, 0x374, 0x376, 0x378, 0x379, 0x37b, 0x37c,
    0x37e, 0x380, 0x381, 0x383, 0x384, 0x386, 0x388, 0x389,
    0x38b, 0x38d, 0x38e, 0x390, 0x392, 0x393, 0x395, 0x397,
    0x398, 0x39a, 0x39c, 0x39d, 0x39f, 0x3a1, 0x3a2, 0x3a4,   // 17
    0x3a6, 0x3a7, 0x3a9, 0x3ab, 0x3ac, 0x3ae, 0x3b0, 0x3b1,
    0x3b3, 0x3b5, 0x3b7, 0x3b8, 0x3ba, 0x3bc, 0x3bd, 0x3bf,
    0x3c1, 0x3c3, 0x3c4, 0x3c6, 0x3c8, 0x3ca, 0x3cb, 0x3cd,
    // The last note has an incomplete range, and loops round back to
    // the start.  Note that the last value is actually a buffer overrun
    // and does not fit with the other values.
    0x3cf, 0x3d1, 0x3d2, 0x3d4, 0x3d6, 0x3d8, 0x3da, 0x3db,   // 18
    0x3dd, 0x3df, 0x3e1, 0x3e3, 0x3e4, 0x3e6, 0x3e8, 0x3ea,
    0x3ec, 0x3ed, 0x3ef, 0x3f1, 0x3f3, 0x3f5, 0x3f6, 0x3f8,
    0x3fa, 0x3fc, 0x3fe, 0x36c,
};

// Mapping from MIDI volume level to OPL level value.
static const unsigned int volume_mapping_table[] = {
    0, 1, 3, 5, 6, 8, 10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127
};

//static opl_driver_ver_t opl_drv_ver = opl_doom_1_9;
static int opl_drv_ver = 119;

// GENMIDI lump instrument data:
static genmidi_instr_t *main_instrs;
static genmidi_instr_t *percussion_instrs;
static char (*main_instr_names)[32];
static char (*percussion_names)[32];

// Voices:
static opl_voice_t voices[OPL_NUM_VOICES * 2];
static opl_voice_t *voice_free_list[OPL_NUM_VOICES * 2];
static opl_voice_t *voice_alloced_list[OPL_NUM_VOICES * 2];
static int voice_free_num;
static int voice_alloced_num;
static int opl_opl3mode;
static int num_opl_voices;

// Data for each channel.
static opl_channel_data_t channels[MIDI_CHANNELS_PER_TRACK];

// Mini-log of recently played percussion instruments:
static uint8_t last_perc[PERCUSSION_LOG_LEN];
static unsigned int last_perc_count;

// Configuration file variable, containing the port number for the adlib chip.
static int opl_io_port = 0x388;
static boolean opl_stereo_correct = false;
static int start_music_volume;
static int current_music_volume;

// Load instrument table from GENMIDI lump:
static boolean LoadInstrumentTable(void)
{
    byte *lump;

    lump = W_CacheLumpName("GENMIDI", PU_STATIC);

    // DMX does not check header
    main_instrs = (genmidi_instr_t *) (lump + strlen(GENMIDI_HEADER));
    percussion_instrs = main_instrs + GENMIDI_NUM_INSTRS;
    main_instr_names =
        (char (*)[32]) (percussion_instrs + GENMIDI_NUM_PERCUSSION);
    percussion_names = main_instr_names + GENMIDI_NUM_INSTRS;

    return true;
}

// Get the next available voice from the freelist.
static opl_voice_t *GetFreeVoice(void)
{
    opl_voice_t *result;
    int i;

    // None available?
    if (voice_free_num == 0) {
        return NULL;
    }

    // Remove from free list
    result = voice_free_list[0];
    voice_free_num--;
    for (i = 0; i < voice_free_num; i++) {
        voice_free_list[i] = voice_free_list[i + 1];
    }

    // Add to allocated list
    voice_alloced_list[voice_alloced_num++] = result;
    return result;
}

// Release a voice back to the freelist.
static void VoiceKeyOff(opl_voice_t *voice);

static void ReleaseVoice(int index)
{
    opl_voice_t *voice;
    boolean double_voice;
    int i;

    // Doom 2 1.666 OPL crash emulation.
    if (index >= voice_alloced_num) {

        voice_alloced_num = 0;
        voice_free_num = 0;
        return;
    }

    voice = voice_alloced_list[index];
    VoiceKeyOff(voice);
    voice->channel = NULL;
    voice->note = 0;
    double_voice = voice->current_instr_voice != 0;
    
    // Remove from alloced list.
    voice_alloced_num--;
    for (i = index; i < voice_alloced_num; i++) {
        voice_alloced_list[i] = voice_alloced_list[i + 1];
    }

    // Search to the end of the freelist (This is how Doom behaves!)
    voice_free_list[voice_free_num++] = voice;
    if (double_voice && opl_drv_ver < 119) {
        ReleaseVoice(index);
    }
}

// Load data to the specified operator
static void LoadOperatorData(int operator, genmidi_op_t *data,
                             boolean max_level, unsigned int *volume)
{
    int level;

    // The scale and level fields must be combined for the level register.
    // For the carrier wave we always set the maximum level.
    level = data->scale;
    if (max_level) {
        level |= 0x3f;
    } else {
        level |= data->level;
    }

    *volume = level;
    OPL_WriteRegister(OPL_REGS_LEVEL + operator, level);
    OPL_WriteRegister(OPL_REGS_TREMOLO + operator, data->tremolo);
    OPL_WriteRegister(OPL_REGS_ATTACK + operator, data->attack);
    OPL_WriteRegister(OPL_REGS_SUSTAIN + operator, data->sustain);
    OPL_WriteRegister(OPL_REGS_WAVEFORM + operator, data->waveform);
}

// Set the instrument for a particular voice.
static void SetVoiceInstrument(opl_voice_t *voice,
                               genmidi_instr_t *instr,
                               unsigned int instr_voice)
{
    genmidi_voice_t *data;
    unsigned int modulating;

    // Instrument already set for this channel?
    if (voice->current_instr == instr && voice->current_instr_voice == instr_voice) {
        return;
    }

    voice->current_instr = instr;
    voice->current_instr_voice = instr_voice;
    data = &instr->voices[instr_voice];

    // Are we usind modulated feedback mode?
    modulating = (data->feedback & 0x01) == 0;

    // Doom loads the second operator first, then the first.
    // The carrier is set to minimum volume until the voice volume
    // is set in SetVoiceVolume (below).  If we are not using
    // modulating mode, we must set both to minimum volume.
    LoadOperatorData(voice->op2 | voice->array, &data->carrier, true, &voice->car_volume);
    LoadOperatorData(voice->op1 | voice->array, &data->modulator, !modulating, &voice->mod_volume);

    // Set feedback register that control the connection between the
    // two operators.  Turn on bits in the upper nybble; I think this
    // is for OPL3, where it turns on channel A/B.
    OPL_WriteRegister((OPL_REGS_FEEDBACK + voice->index) | voice->array,
                      data->feedback | voice->reg_pan);

    // Calculate voice priority.
    voice->priority = 0x0f - (data->carrier.attack >> 4)
                    + 0x0f - (data->carrier.sustain & 0x0f);
}

static void SetVoiceVolume(opl_voice_t *voice, unsigned int volume)
{
    genmidi_voice_t *opl_voice;
    unsigned int midi_volume;
    unsigned int full_volume;
    unsigned int car_volume;
    unsigned int mod_volume;

    voice->note_volume = volume;
    opl_voice = &voice->current_instr->voices[voice->current_instr_voice];

    // Multiply note volume and channel volume to get the actual volume.
    midi_volume = 2 * (volume_mapping_table[voice->channel->volume] + 1);
    full_volume = (volume_mapping_table[voice->note_volume] * midi_volume) >> 9;
    if (full_volume >= 0x3f) {
        full_volume = 0x3f;
    }

    // The volume value to use in the register:
    car_volume = 0x3f - full_volume;

    // Update the volume register(s) if necessary.
    if (car_volume != (voice->car_volume & 0x3f))
    {
        voice->car_volume = car_volume | (voice->car_volume & 0xc0);
        OPL_WriteRegister((OPL_REGS_LEVEL + voice->op2) | voice->array, voice->car_volume);

        // If we are using non-modulated feedback mode, we must set the volume for both voices.
        if ((opl_voice->feedback & 0x01) != 0 && opl_voice->modulator.level != 0x3f)
        {
            mod_volume = opl_voice->modulator.level;
            if (mod_volume < car_volume) {
                mod_volume = car_volume;
            }

            mod_volume |= voice->mod_volume & 0xc0;
            if(mod_volume != voice->mod_volume) {
                voice->mod_volume = mod_volume;
                OPL_WriteRegister((OPL_REGS_LEVEL + voice->op1) | voice->array, mod_volume | (opl_voice->modulator.scale & 0xc0));
            }
        }
    }
}

static void SetVoicePan(opl_voice_t *voice, unsigned int pan)
{
    genmidi_voice_t *opl_voice;
    voice->reg_pan = pan;
    opl_voice = &voice->current_instr->voices[voice->current_instr_voice];;
    OPL_WriteRegister((OPL_REGS_FEEDBACK + voice->index) | voice->array, opl_voice->feedback | pan);
}

// Initialize the voice table and freelist
static void InitVoices(void)
{
    int i;

    // Start with an empty free list.
    voice_free_num = num_opl_voices;
    voice_alloced_num = 0;

    // Initialize each voice.
    for (i = 0; i < num_opl_voices; ++i) {
        voices[i].index = i % OPL_NUM_VOICES;
        voices[i].op1 = voice_operators[0][i % OPL_NUM_VOICES];
        voices[i].op2 = voice_operators[1][i % OPL_NUM_VOICES];
        voices[i].array = (i / OPL_NUM_VOICES) << 8;
        voices[i].current_instr = NULL;
        voice_free_list[i] = &voices[i];
    }
}


static void VoiceKeyOff(opl_voice_t *voice)
{
    OPL_WriteRegister((OPL_REGS_FREQ_2 + voice->index) | voice->array,
                      voice->freq >> 8);
}

static opl_channel_data_t *TrackChannelForEvent(MD_midi_event *event)
{
    unsigned int channel_num = event->channel;
    // MIDI uses track #9 for percussion, but for MUS it's track #15
    // instead. Because DMX works on MUS data internally, we need to
    // swap back to the MUS version of the channel number.
    if (channel_num == 9) {
        channel_num = 15;
    } else if (channel_num == 15) {
        channel_num = 9;
    }
    return &channels[channel_num];
}

// Get the frequency that we should be using for a voice.
static void KeyOffEvent(MD_midi_event *event)
{
    opl_channel_data_t* channel = TrackChannelForEvent(event);
    unsigned int key = event->data[1];

    // Turn off voices being used to play this key.
    // If it is a double voice instrument there will be two.
    for (int i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel == channel
         && voice_alloced_list[i]->key == key)
        {
            // Finished with this voice now.
            ReleaseVoice(i);
            i--;
        }
    }
}

// When all voices are in use, we must discard an existing voice to
// play a new note.  Find and free an existing voice.  The channel
// passed to the function is the channel for the new note to be
// played.

static void ReplaceExistingVoice(void)
{
    int i;
    int result;

    // Check the allocated voices, if we find an instrument that is
    // of a lower priority to the new instrument, discard it.
    // If a voice is being used to play the second voice of an instrument,
    // use that, as second voices are non-essential.
    // Lower numbered MIDI channels implicitly have a higher priority
    // than higher-numbered channels, eg. MIDI channel 1 is never
    // discarded for MIDI channel 2.

    result = 0;
    for (i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->current_instr_voice != 0
         || voice_alloced_list[i]->channel
         >= voice_alloced_list[result]->channel)
        {
            result = i;
        }
    }

    ReleaseVoice(result);
}

// Alternate versions of ReplaceExistingVoice() used when emulating old
// versions of the DMX library used in Doom 1.666, Heretic and Hexen.
static void ReplaceExistingVoiceDoom1(void)
{
    int i;
    int result;

    result = 0;
    for (i = 0; i < voice_alloced_num; i++)
    {
        if (voice_alloced_list[i]->channel
          > voice_alloced_list[result]->channel)
        {
            result = i;
        }
    }

    ReleaseVoice(result);
}

static void ReplaceExistingVoiceDoom2(opl_channel_data_t *channel)
{
    int i;
    int result;
    int priority;

    result = 0;

    priority = 0x8000;

    for (i = 0; i < voice_alloced_num - 3; i++) {
        if (voice_alloced_list[i]->priority < priority && voice_alloced_list[i]->channel >= channel) {
            priority = voice_alloced_list[i]->priority;
            result = i;
        }
    }

    ReleaseVoice(result);
}


static unsigned int FrequencyForVoice(opl_voice_t *voice)
{
    genmidi_voice_t *gm_voice;
    signed int freq_index;
    unsigned int octave;
    unsigned int sub_index;
    signed int note;

    note = voice->note;

    // Apply note offset.
    // Don't apply offset if the instrument is a fixed note instrument.
    gm_voice = &voice->current_instr->voices[voice->current_instr_voice];
    if ((SHORT(voice->current_instr->flags) & GENMIDI_FLAG_FIXED) == 0) {
        note += (signed short) SHORT(gm_voice->base_note_offset);
    }

    // Avoid possible overflow due to base note offset:
    while (note < 0) {
        note += 12;
    }
    while (note > 95) {
        note -= 12;
    }

    freq_index = 64 + 32 * note + voice->channel->bend;

    // If this is the second voice of a double voice instrument, the
    // frequency index can be adjusted by the fine tuning field.
    if (voice->current_instr_voice != 0) {
        freq_index += (voice->current_instr->fine_tuning / 2) - 64;
    }

    if (freq_index < 0) {
        freq_index = 0;
    }

    // The first 7 notes use the start of the table, while
    // consecutive notes loop around the latter part.
    if (freq_index < 284) {
        return frequency_curve[freq_index];
    }
    sub_index = (freq_index - 284) % (12 * 32);
    octave = (freq_index - 284) / (12 * 32);

    // Once the seventh octave is reached, things break down.
    // We can only go up to octave 7 as a maximum anyway (the OPL
    // register only has three bits for octave number), but for the
    // notes in octave 7, the first five bits have octave=7, the
    // following notes have octave=6.  This 7/6 pattern repeats in
    // following octaves (which are technically impossible to
    // represent anyway).
    if (octave >= 7) {
        octave = 7;
    }

    // Calculate the resulting register value to use for the frequency.
    return frequency_curve[sub_index + 284] | (octave << 10);
}

// Update the frequency that a voice is programmed to use.

static void UpdateVoiceFrequency(opl_voice_t *voice)
{
    // Calculate the frequency to use for this voice and update it
    // if neccessary.
    unsigned int freq = FrequencyForVoice(voice);
    if (voice->freq != freq) {
        OPL_WriteRegister((OPL_REGS_FREQ_1 + voice->index) | voice->array, freq & 0xff);
        OPL_WriteRegister((OPL_REGS_FREQ_2 + voice->index) | voice->array, (freq >> 8) | 0x20);
        voice->freq = freq;
    }
}

// Program a single voice for an instrument.  For a double voice
// instrument (GENMIDI_FLAG_2VOICE), this is called twice for each
// key on event.
static void VoiceKeyOn(opl_channel_data_t *channel,
                       genmidi_instr_t *instrument,
                       unsigned int instrument_voice,
                       unsigned int note,
                       unsigned int key,
                       unsigned int volume)
{
    opl_voice_t *voice;
    if (!opl_opl3mode && opl_drv_ver == 116) {
        instrument_voice = 0;
    }

    // Find a voice to use for this new note.
    voice = GetFreeVoice();
    if (voice == NULL) {
        return;
    }

    voice->channel = channel;
    voice->key = key;

    // Work out the note to use.  This is normally the same as
    // the key, unless it is a fixed pitch instrument.
    if ((SHORT(instrument->flags) & GENMIDI_FLAG_FIXED) != 0) {
        voice->note = instrument->fixed_note;
    } else {
        voice->note = note;
    }

    voice->reg_pan = channel->pan;

    // Program the voice with the instrument data:
    SetVoiceInstrument(voice, instrument, instrument_voice);

    // Set the volume level.
    SetVoiceVolume(voice, volume);

    // Write the frequency value to turn the note on.

    voice->freq = 0;
    UpdateVoiceFrequency(voice);
}

static void KeyOnEvent(MD_midi_event *event)
{
    genmidi_instr_t *instrument;
    opl_channel_data_t *channel;
    unsigned int note, key, volume, voicenum;
    boolean double_voice;

    note = event->data[1];   // event->data.channel.param1;
    key = event->data[1];    // event->data.channel.param1;
    volume = event->data[2]; // event->data.channel.param2;

    // A volume of zero means key off. Some MIDI tracks, eg. the ones
    // in AV.wad, use a second key on with a volume of zero to mean
    // key off.
    if (volume <= 0) {
        KeyOffEvent(event);
        return;
    }

    // The channel.
    channel = TrackChannelForEvent(event);

    // Percussion channel is treated differently.
    if (event->channel == 9) {
        if (key < 35 || key > 81) {
            return;
        }

        instrument = &percussion_instrs[key - 35];
        last_perc[last_perc_count] = key;
        last_perc_count = (last_perc_count + 1) % PERCUSSION_LOG_LEN;
        note = 60;
    } else {
        instrument = channel->instrument;
    }

    double_voice = (SHORT(instrument->flags) & GENMIDI_FLAG_2VOICE) != 0;

    switch (opl_drv_ver)
    {
        case 116:
            voicenum = double_voice + 1;
            if (!opl_opl3mode) {
                voicenum = 1;
            }
            while (voice_alloced_num > num_opl_voices - voicenum) {
                ReplaceExistingVoiceDoom1();
            }

            // Find and program a voice for this instrument.  If this
            // is a double voice instrument, we must do this twice.
            if (double_voice) {
                VoiceKeyOn(channel, instrument, 1, note, key, volume);
            }
            VoiceKeyOn(channel, instrument, 0, note, key, volume);
            break;
        case 216:
            if (voice_alloced_num == num_opl_voices) {
                ReplaceExistingVoiceDoom2(channel);
            }
            if (voice_alloced_num == num_opl_voices - 1 && double_voice) {
                ReplaceExistingVoiceDoom2(channel);
            }

            // Find and program a voice for this instrument.  If this
            // is a double voice instrument, we must do this twice.
            if (double_voice) {
                VoiceKeyOn(channel, instrument, 1, note, key, volume);
            }
            VoiceKeyOn(channel, instrument, 0, note, key, volume);
            break;
        default:
        case 119:
            if (voice_free_num == 0) {
                ReplaceExistingVoice();
            }
            // Find and program a voice for this instrument.  If this
            // is a double voice instrument, we must do this twice.
            VoiceKeyOn(channel, instrument, 0, note, key, volume);
            if (double_voice) {
                VoiceKeyOn(channel, instrument, 1, note, key, volume);
            }
            break;
    }
}

static void ProgramChangeEvent(MD_midi_event *event)
{
    opl_channel_data_t *channel;
    int instrument;
    // Set the instrument used on this channel.
    channel = TrackChannelForEvent(event);
    instrument = event->data[1];// event->data.channel.param1;
    channel->instrument = &main_instrs[instrument];
    // TODO: Look through existing voices that are turned on on this
    // channel, and change the instrument.
}

static void SetChannelVolume(opl_channel_data_t *channel, unsigned int volume,
                             boolean clip_start)
{
    unsigned int i;
    channel->volume_base = volume;
    if (volume > current_music_volume) {
        volume = current_music_volume;
    }

    if (clip_start && volume > start_music_volume) {
        volume = start_music_volume;
    }

    channel->volume = (volume << 3);

    // Update all voices that this channel is using.
    for (i = 0; i < num_opl_voices; ++i) {
        if (voices[i].channel == channel) {
            SetVoiceVolume(&voices[i], voices[i].note_volume);
        }
    }
}

static void SetChannelPan(opl_channel_data_t *channel, unsigned int pan)
{
    unsigned int reg_pan;
    unsigned int i;

    // The DMX library has the stereo channels backwards, maybe because
    // Paul Radek had a Soundblaster card with the channels reversed, or
    // perhaps it was just a bug in the OPL3 support that was never
    // finished. By default we preserve this bug, but we also provide a
    // secret DMXOPTION to fix it.
    if (opl_stereo_correct) {
        pan = 144 - pan;
    }

    if (opl_opl3mode) {
        if (pan >= 96) {
            reg_pan = 0x10;
        } else if (pan <= 48) {
            reg_pan = 0x20;
        } else {
            reg_pan = 0x30;
        }

        if (channel->pan != reg_pan) {
            channel->pan = reg_pan;
            for (i = 0; i < num_opl_voices; i++) {
                if (voices[i].channel == channel) {
                    SetVoicePan(&voices[i], reg_pan);
                }
            }
        }
    }
}

// Handler for the MIDI_CONTROLLER_ALL_NOTES_OFF channel event.
static void AllNotesOff(opl_channel_data_t *channel, unsigned int param)
{
    for (int i = 0; i < voice_alloced_num; i++) {
        if (voice_alloced_list[i]->channel == channel) {
            ReleaseVoice(i); i--;
        }
    }
}

static void ControllerEvent(MD_midi_event *event)
{
    opl_channel_data_t *channel;
    unsigned int controller;
    unsigned int param;

    channel = TrackChannelForEvent(event);
    controller = event->data[1];// event->data.channel.param1;
    param = event->data[2];// event->data.channel.param2;

    switch (controller)
    {
        case MIDI_CONTROLLER_VOLUME_MSB:
            SetChannelVolume(channel, param, true);
            break;

        case MIDI_CONTROLLER_PAN:
            SetChannelPan(channel, param);
            break;

        case MIDI_CONTROLLER_ALL_NOTES_OFF:
            AllNotesOff(channel, param);
            break;

        default:
#ifdef OPL_MIDI_DEBUG
            fprintf(stderr, "Unknown MIDI controller type: %u\n", controller);
#endif
            break;
    }
}

// Process a pitch bend event.
static void PitchBendEvent(MD_midi_event *event)
{
    opl_channel_data_t *channel;
    int i;
    opl_voice_t *voice_updated_list[OPL_NUM_VOICES * 2];
    unsigned int voice_updated_num = 0;
    opl_voice_t *voice_not_updated_list[OPL_NUM_VOICES * 2];
    unsigned int voice_not_updated_num = 0;

    // Update the channel bend value.  Only the MSB of the pitch bend
    // value is considered: this is what Doom does.
    channel = TrackChannelForEvent(event);
    //channel->bend = event->data.channel.param2 - 64;
    channel->bend = ((int)event->data[2]) - 64;

    // Update all voices for this channel.
	for (i = 0; i < voice_alloced_num; ++i) {
        if (voice_alloced_list[i]->channel == channel) {
            UpdateVoiceFrequency(voice_alloced_list[i]);
            voice_updated_list[voice_updated_num++] = voice_alloced_list[i];
        } else {
            voice_not_updated_list[voice_not_updated_num++] =
            voice_alloced_list[i];
        }
    }

    for (i = 0; i < voice_not_updated_num; i++) {
        voice_alloced_list[i] = voice_not_updated_list[i];
    }

    for (i = 0; i < voice_updated_num; i++) {
        voice_alloced_list[i + voice_not_updated_num] = voice_updated_list[i];
    }
}

// --------------------------------------------------------------------------------

static void midiEventHandler(MD_midi_event* event) {

    uint8_t type = event->data[0] & 0xf0;
    switch (type)
    {
        case MIDI_EVENT_NOTE_OFF:
            KeyOffEvent(event);
            break;
        case MIDI_EVENT_NOTE_ON:
            KeyOnEvent(event);
            break;
        case MIDI_EVENT_CONTROLLER:
            ControllerEvent(event);
            break;
        case MIDI_EVENT_PROGRAM_CHANGE:
            ProgramChangeEvent(event);
            break;
        case MIDI_EVENT_PITCH_BEND:
            PitchBendEvent(event);
            break;
        default:
            break;
    }
}

static void midiSysexHandler(MD_sysex_event* event) {
    /* can be ignore */
}

static void midiMetaHandler(const MD_meta_event* event) {
    /* can be ignored */
}

// Initialize a channel.
static void InitChannel(opl_channel_data_t *channel)
{
    // TODO: Work out sensible defaults?
    channel->instrument = &main_instrs[0];
    channel->volume = current_music_volume;
    channel->volume_base = 100;
    if (channel->volume > channel->volume_base)
    {
        channel->volume = channel->volume_base;
    }
    channel->pan = 0x30;
    channel->bend = 0;
}

// --------------------------------------------------------------------------------

int I_InitMusic_OPL(void)
{ 
    opl_init_result_t chip_type;
    chip_type = OPL_Init(opl_io_port);
    if (chip_type == OPL_INIT_NONE) {
        printf("Dude.  The Adlib isn't responding.\n");
        return false;
    }

    if (chip_type == OPL_INIT_OPL3) {
        opl_opl3mode = 1;
        num_opl_voices = OPL_NUM_VOICES * 2;
    } else {
        opl_opl3mode = 0;
        num_opl_voices = OPL_NUM_VOICES;
    }

    opl_stereo_correct = false;
    OPL_InitRegisters(opl_opl3mode);

    if (!LoadInstrumentTable()) {
        OPL_Shutdown();
        return false;
    }

    InitVoices();

    if (!I_InitMusic_MIDI()) {
        OPL_Shutdown();
        return false;
    }

    return true;
}

void I_ShutdownMusic_OPL(void) {
    I_ShutdownMusic_MIDI();
}

void I_PlaySong_OPL(int handle, int looping) {

    I_StopSong_OPL(handle);
    for (int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i) {
        InitChannel(&channels[i]);
    }

    start_music_volume = current_music_volume;
    I_PlaySong_MIDI(handle, looping);
}

void I_SetMusicVolume_OPL(int volume)
{
    if (current_music_volume != volume) {
        current_music_volume = volume;
        for (unsigned int i = 0; i < MIDI_CHANNELS_PER_TRACK; ++i) {
            if (i == 15) {
                SetChannelVolume(&channels[i], volume, false);
            } else {
                SetChannelVolume(&channels[i], channels[i].volume_base, false);
            }
        }    
    }
}

void I_PauseSong_OPL(int handle) {
    I_PauseSong_MIDI(handle);
}

void I_ResumeSong_OPL(int handle) {
    I_ResumeSong_MIDI(handle);
}

void I_StopSong_OPL(int handle) {
    I_StopSong_MIDI(handle);
}

int I_RegisterSong_OPL(void* data, int len) {
    MD_MIDIFile* midi = (MD_MIDIFile*) I_RegisterSong_MIDI(data, len);
    if (midi) {    
        midi->_midiHandler = midiEventHandler;
        midi->_sysexHandler = midiSysexHandler;
        midi->_metaHandler = midiMetaHandler;
    }
    return (int) midi;
}

void I_UnRegisterSong_OPL(int handle) {
    I_UnRegisterSong_MIDI(handle);
}

int I_QrySongPlaying_OPL(int handle) {
    return I_QrySongPlaying_MIDI(handle);
}

void I_UpdateMusic_OPL(void *unused, uint8_t *stream, int len) {
    I_UpdateMusic_MIDI(unused, stream, len);
}

