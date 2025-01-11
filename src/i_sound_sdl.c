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

extern sysaudio_t sysaudio;

static Sint32 *tmpMixBuffer = NULL;	/* 32bit mixing buffer for n voices */
static Sint16 *tmpMixBuffer2 = NULL;	/* 16bit clipped mixing buffer for conv */
static int tmpMixBuffLen = 0;
static SDL_bool quit = SDL_FALSE;


//
// This function loops all active (internal) sound
//  channels, retrieves a given number of samples
//  from the raw sound data, modifies it according
//  to the current (internal) channel parameters,
//  mixes the per channel samples into the global
//  mixbuffer, clamping it to the allowed range,
//  and sets up everything for transferring the
//  contents of the mixbuffer to the (two)
//  hardware channels (left and right, that is).
//
// This function currently supports only 16bit.
//
void I_UpdateSound_SDL(void *unused, Uint8 *stream, int len)
{
	int i, chan, srclen;
	boolean mixToFinal = false;
	Sint32 *source;
	Sint16 *dest;

	if (quit) {
		return;
	}

	memset(tmpMixBuffer, 0, tmpMixBuffLen);
	srclen = len;
	if (sysaudio.convert) {
		srclen = (int) (len / sysaudio.audioCvt.len_ratio);
	}

	/* Add each channel to tmp mix buffer */
	for ( chan = 0; chan < NUM_CHANNELS; chan++ ) {
		Uint8 *sample;
		Uint32 position, stepremainder, step;
		int *leftvol, *rightvol;
		Sint32 maxlen;
		SDL_bool end_of_sample;

		// Check channel, if active.
		if (!i_sound_channels[ chan ].startp) {
			continue;
		}

		source = tmpMixBuffer;
		sample = i_sound_channels[chan].startp;
		position = i_sound_channels[ chan ].position;
		stepremainder = i_sound_channels[chan].stepremainder;
		step = i_sound_channels[chan].step;
		leftvol = i_sound_channels[chan].leftvol_lookup;
		rightvol = i_sound_channels[chan].rightvol_lookup;

		maxlen = FixedDiv(i_sound_channels[chan].length-position, step);
		end_of_sample = SDL_FALSE;
		if ((srclen>>2) <= maxlen) {
			maxlen = srclen>>2;
		} else {
			end_of_sample = SDL_TRUE;
		}

		{
#if defined(__GNUC__) && defined(__m68k__)
			Uint32	step_int = step>>16;
			Uint32	step_frac = step<<16;
#endif
			for (i=0; i<maxlen; i++) {
				unsigned int val;

				// Get the raw data from the channel. 
				val = sample[position];

				// Add left and right part
				//  for this channel (sound)
				//  to the current data.
				// Adjust volume accordingly.
				*source++ += leftvol[val];
				*source++ += rightvol[val];

#if defined(__GNUC__) && defined(__m68k__)
				__asm__ __volatile__ (
						"addl	%3,%1\n"	\
					"	addxl	%2,%0"	\
				 	: /* output */
						"=d"(position), "=d"(stepremainder)
				 	: /* input */
						"d"(step_int), "r"(step_frac), "d"(position), "d"(stepremainder)
				 	: /* clobbered registers */
				 		"cc"
				);
#else
				// Increment index ???
				stepremainder += step;

				// MSB is next sample???
				position += stepremainder >> 16;

				// Limit to LSB???
				stepremainder &= 65536-1;
#endif
			}
		}

		if (end_of_sample) {
			i_sound_channels[ chan ].startp = NULL;
			S_sfx[i_sound_channels[chan].id].usefulness--;
		}
		i_sound_channels[ chan ].position = position;
		i_sound_channels[ chan ].stepremainder = stepremainder;
	}

	/* Now clip values for final buffer */
	source = tmpMixBuffer;	
	if (sysaudio.convert) {
		dest = (Sint16 *) tmpMixBuffer2;
	} else {
		dest = (Sint16 *) stream;
#ifdef ENABLE_SDLMIXER
		mixToFinal = true;
#endif
	}

	if (mixToFinal) {
		for (i=0; i<srclen>>2; i++) {
			Sint32 dl, dr;

			dl = *source++ + dest[0];
			dr = *source++ + dest[1];

			if (dl > 0x7fff)
				dl = 0x7fff;
			else if (dl < -0x8000)
				dl = -0x8000;

			*dest++ = dl;

			if (dr > 0x7fff)
				dr = 0x7fff;
			else if (dr < -0x8000)
				dr = -0x8000;

			*dest++ = dr;
		}
	} else {
		for (i=0; i<srclen>>2; i++) {
			Sint32 dl, dr;

			dl = *source++;
			dr = *source++;

			if (dl > 0x7fff)
				dl = 0x7fff;
			else if (dl < -0x8000)
				dl = -0x8000;

			*dest++ = dl;

			if (dr > 0x7fff)
				dr = 0x7fff;
			else if (dr < -0x8000)
				dr = -0x8000;

			*dest++ = dr;
		}
	}

	/* Conversion if needed */
	if (sysaudio.convert) {
		sysaudio.audioCvt.buf = (Uint8 *) tmpMixBuffer2;
		sysaudio.audioCvt.len = srclen;
		SDL_ConvertAudio(&sysaudio.audioCvt);

		SDL_MixAudio(stream, sysaudio.audioCvt.buf, len, SDL_MIX_MAXVOLUME);
	}
}



void I_ShutdownSound_SDL(void)
{    
	int i;
	int done = 0;

	quit = SDL_TRUE;

	// Wait till all pending sounds are finished.
	while ( !done ) {
		for( i=0 ; i<8 && !(i_sound_channels[i].startp) ; i++) {
		}

		// FIXME. No proper channel output.
		//if (i==8)
			done=1;
	}

	if (tmpMixBuffer) {
		Z_Free(tmpMixBuffer);
		tmpMixBuffer=NULL;
	}

	if (tmpMixBuffer2) {
		Z_Free(tmpMixBuffer2);
		tmpMixBuffer2=NULL;
	}

#ifdef ENABLE_SDLMIXER
    Mix_CloseAudio();
#else
    SDL_CloseAudio();
#endif
}

int I_InitSound_SDL(void)
{ 
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        return false;
    }

#ifdef ENABLE_SDLMIXER
    Uint16 format; int freq, channels;
    if (Mix_OpenAudio(sysaudio.desired.freq, sysaudio.desired.format, sysaudio.desired.channels, sysaudio.desired.samples) >= 0) {
        if (Mix_QuerySpec(&freq, &format, &channels)) {
            sysaudio.obtained.freq = freq;
            sysaudio.obtained.channels = channels;
            sysaudio.obtained.format = format;
            sysaudio.obtained.size = (SAMPLECOUNT*channels*((format&0xff)>>3));
        }
	}
#else
    sysaudio.desired.callback = I_UpdateAudio;
    sysaudio.desired.userdata = NULL;
    if (SDL_OpenAudio(&sysaudio.desired, &sysaudio.obtained) < 0) {
        return false;
    }
#endif

    if ((sysaudio.obtained.format != AUDIO_S16SYS) || (sysaudio.obtained.channels != 2)) {
        sysaudio.convert = true;
        if (SDL_BuildAudioCVT(&sysaudio.audioCvt, AUDIO_S16SYS, 2, sysaudio.obtained.freq, sysaudio.obtained.format, sysaudio.obtained.channels, sysaudio.obtained.freq) == -1) {
#ifdef ENABLE_SDLMIXER
            Mix_CloseAudio();
#else
            SDL_CloseAudio();
#endif
            return false;
        }
    }

#ifdef ENABLE_SDLMIXER
    Mix_SetPostMix(I_UpdateSound_SDL, NULL);
#else
    SDL_PauseAudio(0);
#endif
    char deviceName[32];
    if (SDL_AudioDriverName(deviceName, sizeof(deviceName))==NULL) {
        memset(deviceName, 0, sizeof(deviceName));
    }		
    deviceName[sizeof(deviceName)-1]='\0';
    printf("PCM Audio device: %s, %d Hz, %d bits, %d channels\n",
        deviceName,
        sysaudio.obtained.freq,
        sysaudio.obtained.format & 0xff,
        sysaudio.obtained.channels);


    tmpMixBuffLen = sysaudio.obtained.samples * 2 * sizeof(Sint32);
    tmpMixBuffer = Z_Malloc(tmpMixBuffLen, PU_STATIC, 0);
    if (sysaudio.convert) {
        tmpMixBuffer2 = Z_Malloc(tmpMixBuffLen>>1, PU_STATIC, 0);
    }

    sysaudio.sdl_available = true;
    return true;
}
