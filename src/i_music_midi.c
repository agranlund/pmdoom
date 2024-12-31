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

#include <mint/osbind.h>
#include <SDL.h>

#include "z_zone.h"
#include "m_misc.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_music_midi.h"
#include "md_midi.h"
#include "mus2mid.h"
#include "memio.h"

#define MAXMIDLENGTH (96 * 1024)

static int current_music_volume;

// Track data for playing tracks:
static boolean song_looping = 0;
static boolean song_paused = 0;
static boolean song_playing = 0;
static unsigned int currentmicros = 0;
static MD_MIDIFile* midi = 0;
static void* midibuf = 0;

// --------------------------------------------------------------------------------
static int32_t (*biosMidiOut)(uint32_t data) = 0;
static void midiWrite_bios(uint8_t* buf, uint16_t size) {
    if (biosMidiOut) {
        uint8_t* end = &buf[size];
        while (buf != end) {
            int16_t c = (int8_t)*buf++;
            biosMidiOut((3<<16) | c);
        }
    }
}

// --------------------------------------------------------------------------------
static void midiEventHandler(MD_midi_event* event) {

    if (event->data[0] & 0x90) {
        int vel = (current_music_volume * event->data[2]) / 15;
        vel = (vel > 127) ? 127 : (vel < 0) ? 0 : vel;
        event->data[2] = (uint8_t)vel;
    }
    midiWrite_bios(event->data, event->size);
}

static void midiSysexHandler(MD_sysex_event* event) {
    midiWrite_bios(event->data, event->size);
}

static void midiMetaHandler(const MD_meta_event* event) {
    /* can be ignored */
}


// --------------------------------------------------------------------------------
static void PeriodicUpdate(unsigned int elapsed) {
    if (midi && song_playing && !song_paused) {
        currentmicros += elapsed;
        MD_Update(midi, currentmicros);
        if (MD_isEOF(midi)) {
            if (song_looping) {
                MD_Restart(midi);
            } else {
                song_playing = false;
            }
        }
    }
}

static void TimerCallback() {

    static uint32_t last200hz = 0;
    // assume 20ms per frame (50hz)
    uint32_t microseconds = 20 * 1000UL;
    // get more accurate reading from 200hz timer if possible
    uint32_t this200hz = *((volatile uint32_t*)0x4ba);
    if (last200hz && (this200hz > last200hz)) {
        // 5ms per 200hz tick
        microseconds = (this200hz - last200hz) * 5000UL;
    }
    PeriodicUpdate(microseconds);
    last200hz = this200hz;
}

static uint16_t DisableInterrupts() {
    register uint16_t ret __asm__ ("d0");
    __asm__ volatile (
        "   move.w  sr,%0\n\t"
        "   or.w    #0x0700,sr\n\t"
        : "=r"(ret) : : __CLOBBER_RETURN("d0") "cc" );
    return ret;
}

static void RestoreInterrupts(uint16_t oldsr) {
    __asm__ volatile (
        "   move.w  sr,d0\n\t"
        "   and.w   #0xF0FF,d0\n\t"
        "   and.w   #0x0F00,%0\n\t"
        "   or.w    %0,d0\n\t"
        "   move.w  d0,sr\n\t"
        : : "d"(oldsr) : "d0", "cc" );
}

static void InstallTimer() {
    /* todo: use real timer interrupt */
    uint16_t sr = DisableInterrupts();
    biosMidiOut = (int32_t(*)(uint32_t)) *(volatile uint32_t*)(0x57e + (3 * 4));
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (int i = 0; i < nvbls; i++) {
        if (vblqueue[i] == 0) {
            vblqueue[i] = (unsigned long) &TimerCallback;
            return;
        }
    }
    RestoreInterrupts(sr);
}

static void UninstallTimer() {
    /* todo: use real timer interrupt */
    uint16_t sr = DisableInterrupts();
    unsigned long* vblqueue = (unsigned long*) *((unsigned long*)0x456);
    unsigned long nvbls = *((unsigned long*)0x454);
    for (int i = 0; i < nvbls; i++) {
        if (vblqueue[i] == (unsigned long) &TimerCallback) {
            vblqueue[i] = 0;
            return;
        }
    }
    RestoreInterrupts(sr);
}

// --------------------------------------------------------------------------------

int I_InitMusic_MIDI(void) { 
    Supexec(InstallTimer);
    return true;    
}

void I_ShutdownMusic_MIDI(void) {
    I_StopSong_MIDI(0);
    Supexec(UninstallTimer);
}

void I_PlaySong_MIDI(int handle, int looping) {
    if (midi) {
        I_StopSong_MIDI(handle);
        MD_Restart(midi);
        midi->_looping = looping;
        currentmicros = 0;
        song_looping = looping;
        song_paused = false;
        song_playing = true;
    }
}

void I_SetMusicVolume_MIDI(int volume) {
    current_music_volume = volume;
}

void I_PauseSong_MIDI(int handle) {
    if (midi) {
        song_paused = true;
        MD_Pause(midi, true);
    }
}

void I_ResumeSong_MIDI(int handle) {
    if (midi) {
        MD_Pause(midi, false);
        song_paused = false;
    }
}

void I_StopSong_MIDI(int handle) {
    if (midi) {
        song_paused = true;
        song_playing = false;
        MD_Pause(midi, true);
    }
}

int I_RegisterSong_MIDI(void* data, int len) {

    if (midi) {
        I_UnRegisterSong_MIDI(-1);
        midi = NULL;
    }

    if ((len > 4) && (len < MAXMIDLENGTH) && !memcmp(data, "MThd", 4)) {
        /* midi data */
        midibuf = Z_Malloc(len, PU_STATIC, data);
        if (midibuf) {
            memcpy(midibuf, data, len);
            midi = MD_OpenBuffer(midibuf);
        }
    } else {
        /* mus data */
        MEMFILE *instream = mem_fopen_read(data, len);
        MEMFILE* outstream = mem_fopen_write();
        if (instream && outstream) {
            if (mus2mid(instream, outstream) == 0)
            {
                void *outbuf; size_t outbuf_len;
                mem_get_buf(outstream, &outbuf, &outbuf_len);
                if (outbuf_len > 4) {
                    midibuf = Z_Malloc(outbuf_len, PU_STATIC, data);
                    if (midibuf) {
                        memcpy(midibuf, outbuf, outbuf_len);
                        midi = MD_OpenBuffer(midibuf);
                    }
                }
            }
            mem_fclose(instream);
            mem_fclose(outstream);
        }
    }

    if (midi) {
        midi->_midiHandler = midiEventHandler;
        midi->_sysexHandler = midiSysexHandler;
        midi->_metaHandler = midiMetaHandler;
    }

    return (int) midi;
}

void I_UnRegisterSong_MIDI(int handle) {
    if (midi) {
        song_paused = true;
        song_playing = false;
        MD_Close(midi);
        midi = NULL;
        if (midibuf) {
            Z_Free(midibuf);
            midibuf = 0;
        }
    }
}

int I_QrySongPlaying_MIDI(int handle) {
    return (midi != NULL) && song_playing;
}

void I_UpdateMusic_MIDI(void *unused, uint8_t *stream, int len) {
    (void) unused; (void) stream; (void) len;
}
