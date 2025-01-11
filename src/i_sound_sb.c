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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_audio.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "m_fixed.h"

#include "doomdef.h"


/* -----------------------------------------------------------------------------------
 * 
 * Soundblaster
 * 
 * ---------------------------------------------------------------------------------*/
#include <mint/osbind.h>
#include "isa.h"

#define _soundChunkSize     512

typedef struct
{
    volatile uint8_t*   play;               /* playback pointer */
    volatile uint8_t*   write;              /* write pointer */
    volatile uint8_t*   dsp;                /* dsp write register pointer */
    uint8_t*            mem;                /* memory buffer */
    uint16_t            port;
    uint16_t            freq;
    void (*update)(void);
} blaster_t;

static blaster_t    blaster;
static uint32_t     _timerA_oldvec;
static uint32_t     _timerA_freq;
static uint8_t      _timerA_ctrl;
static uint8_t      _timerA_data;


static uint16_t disable_interrupts() {
    uint16_t ret;
    __asm__ volatile (
        "   move.w  sr,%0\n\t"
        "   or.w    #0x0700,sr\n\t"
        : "=d"(ret) : : "cc" );
    return ret;
}

static void restore_interrupts(uint16_t oldsr) {
    __asm__ volatile (
        "   move.w  %0,sr\n\t"
        : : "d"(oldsr) : "cc" );
}

static void __attribute__ ((interrupt)) timerA_blaster(void) {
    if (blaster.play != blaster.write) {
        *blaster.dsp = 0x10;
        while(*blaster.dsp & 0x80);
        *blaster.dsp = *blaster.play;
        ((uint16_t*)&blaster.play)[1]++;
    }
    __asm__ volatile (" move.b #0xdf,0xfa0f.w\n" : : : "cc" );
}

void timerA_enable(bool enable) {
    uint16_t ipl = disable_interrupts();
    *((volatile uint8_t*)0x00fffa19) = enable ? _timerA_ctrl : 0;
    restore_interrupts(ipl);
}

static uint16_t timerA_calc(uint16_t hz, uint8_t* ctrl, uint8_t* data) {
    uint16_t best_hz = 0; uint8_t best_ctrl = 1; uint8_t best_data = 1;
	const uint32_t mfp_dividers[7] = {4, 10, 16, 50, 64, 100, 200};
	const uint32_t mfp_clock = 2457600;
    int best_diff = 0xFFFFFF;
    for (int i=7; i!=0; i--)
	{
		uint32_t val0 = mfp_clock / mfp_dividers[i-1];
		for (int j=1; j<256; j++)
		{
			int val = val0 / j;
			int diff = (hz > val) ? (hz - val) : (val - hz);
			if (diff < best_diff)
			{
                best_hz = val;
				best_diff = diff;
				best_ctrl = i;
				best_data = j;
			}
			if (val < hz)
				break;
		}
	}
    if (ctrl) { *ctrl = best_ctrl; }
    if (data) { *data = best_data; }
    return best_hz;
}


static inline void blaster_write(uint16_t reg, uint8_t data) { outp(blaster.port + reg, data); }
static inline uint8_t blaster_read(uint16_t reg) { return inp(blaster.port + reg); }
static inline void blaster_write_dsp(uint8_t data) {
    while ((blaster_read(0xC) & 0x80) != 0) { }
    blaster_write(0xC, data);
}

static uint16_t blaster_detect() {
    blaster.port = 0;
    for (uint16_t i = 0x220; (i<=0x280) && (blaster.port == 0); i += 0x20) {
        uint16_t temp = inp(i + 0x6);
        outp(i + 0x6, 0x1); isa_delay(3000);
        outp(i + 0x6, 0x0); isa_delay(3000);
        if (inp(i + 0xA) == 0xAA) {
            blaster.port = i;
        } else {
            outp(i + 0x6, temp); isa_delay(3000);
        }
    }
    if (blaster.port) {
        blaster.dsp = (volatile uint8_t*) (isa_if->iobase + blaster.port + 0xC);
    }
    return blaster.port;
}

static void blaster_deinit(void) {
    if (blaster.port) {
        uint16_t ipl = disable_interrupts();

        /* vbl interrupt */
        uint32_t* vblqueue = (uint32_t*) *((uint32_t*)0x456);
        uint32_t  nvbls = *((uint32_t*)0x454);
        for (unsigned long i = 0; i < nvbls; i++) {
            if (vblqueue[i] == (uint32_t)blaster.update) {
                vblqueue[i] = 0;
                break;
            }
        }

        /* timer interrupt */
        *((volatile uint8_t*)0x00fffa19) = 0;
        uint32_t base = *((volatile uint8_t*)0x00fffa17) & 0xF0;
        uint32_t vecp = ((base + 13) * 4);
        *((volatile uint32_t*)vecp) = _timerA_oldvec;

        if (blaster.mem) {
            free(blaster.mem);
            blaster.mem = NULL;
            blaster.play = NULL;
            blaster.write = NULL;
        }

        restore_interrupts(ipl);        
    }
}

static long blaster_init() {
    if (!isa_init()) {
        printf("No ISA bus detected\n");
        return false;
    }

    if (blaster_detect() == 0) {
        printf("No Soundblaster detected\n");
        return false;
    }

    printf("Soundblaster detected on port %03x\n", blaster.port);
    blaster_write_dsp(0xD1);
    blaster_write_dsp(0xF0);
    blaster.mem = malloc(2*64*1024);
    blaster.play = (volatile uint8_t*) ((((uint32_t)blaster.mem) + 0xffffUL) & 0xffff0000UL);
    blaster.write = blaster.play;

    /* timer interrupt */
    uint16_t ipl = disable_interrupts();

    _timerA_freq = timerA_calc(blaster.freq, &_timerA_ctrl, &_timerA_data);
    *((volatile uint8_t*)0x00fffa19) = 0;
    *((volatile uint8_t*)0x00fffa07) |= (1 << 5);
    *((volatile uint8_t*)0x00fffa13) |= (1 << 5);
    *((volatile uint8_t*)0x00fffa1f) = _timerA_data;
    uint32_t base = *((volatile uint8_t*)0x00fffa17) & 0xF0;
    uint32_t vecp = ((base + 13) * 4);
    _timerA_oldvec = *((volatile uint32_t*)vecp);
    *((volatile uint32_t*)vecp) = (uint32_t)timerA_blaster;

    /* vbl interrupt */
    uint32_t* vblqueue = (uint32_t*) *((uint32_t*)0x456);
    uint32_t  nvbls = *((uint32_t*)0x454);
    for (unsigned long i = 0; i < nvbls; i++) {
        if (vblqueue[i] == 0) {
            vblqueue[i] = (unsigned long) blaster.update;
            break;
        }
    }
    restore_interrupts(ipl);
    timerA_enable(true);

    return true;
}



/* -----------------------------------------------------------------------------------
 * 
 * I_Sound
 * 
 * ---------------------------------------------------------------------------------*/

extern sysaudio_t sysaudio;

void vbl_blaster(void) {
    static int32_t tmpMixBuffer[_soundChunkSize];

    uint32_t start = (uint32_t)blaster.write;
    uint32_t end   = (uint32_t)blaster.play;
    uint32_t bfree = ((end > start) ? (end - start) : ((end + 0x10000) - start)) & ~3;
    uint32_t bused = (0x10000 - bfree);
    uint32_t howmuch = (bused < _soundChunkSize) ? (_soundChunkSize - bused) : 0;

    if (blaster.play == blaster.write) {
        timerA_enable(false);
    }

    if (howmuch >= 16)
    {
        memset(tmpMixBuffer, 0, howmuch<<2);

        uint16_t written = 0;
        for ( int chan = 0; chan < NUM_CHANNELS; chan++ ) {
            i_sound_channel_t* channel = &i_sound_channels[chan];
            if (channel->startp) {
                int32_t srclen = (channel->length - channel->position) - 4;
                if (srclen <= 0) {
                    channel->startp = NULL;
                    S_sfx[channel->id].usefulness--;
                    continue;
                }

                int32_t* leftvol = channel->leftvol_lookup;
                int32_t* rightvol = channel->rightvol_lookup;

                int32_t qu = howmuch > srclen ? srclen : howmuch;
                if (qu > written) { written = qu; }

                int32_t* writeptr = tmpMixBuffer;
                volatile uint8_t* readptr = channel->startp + channel->position;
                for (int32_t i = 0; i < qu; i++) {
                    uint8_t sample = (unsigned int) (*readptr++);
                    *writeptr++ += (leftvol[sample] + rightvol[sample]);
                }
                channel->position += qu;
            }
        }

        if (written) {
            int32_t* readptr = tmpMixBuffer;
            volatile uint8_t* writeptr = blaster.write;
            for (int i=0; i<written; i++)
            {
                int32_t dl = *readptr++;
                if (dl > 0x7fff) { dl = 0x7fff; }
                else if (dl < -0x8000) { dl = -0x8000; }
                *writeptr++ = (uint8_t) ((dl >> 8) ^ 0x80);
            }

            ((uint16_t*)&blaster.write)[1] += written;
            timerA_enable(true);
        }
    }
}

void I_UpdateSound_SB(void *unused, Uint8 *stream, int len) {
    (void)unused; (void)stream; (void)len;
}

void I_ShutdownSound_SB(void) {
    Supexec(blaster_deinit);
}

int I_InitSound_SB(void) { 
    blaster.freq = 11025;
    blaster.update = vbl_blaster;
    if (!Supexec(blaster_init)) {
        return false;
    }

    return true;
}
