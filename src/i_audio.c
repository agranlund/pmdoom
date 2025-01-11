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

#include <string.h>

#include "z_zone.h"

#include "i_system.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_sound.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"

#include "doomdef.h"

sysaudio_t	sysaudio;

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

void I_UpdateAudio(void *unused, Uint8 *stream, int len)
{
    if (sysaudio.music_enabled) {
	    I_UpdateMusic(unused, stream, len);
    }

    if (sysaudio.sound_enabled) {
	    I_UpdateSound(unused, stream, len);
    }
}

void I_ShutdownAudio(void)
{    
    if (sysaudio.music_enabled) {
	    I_ShutdownMusic();
    }

	if (sysaudio.sound_enabled) {
    	I_ShutdownSound();
    }
}

void I_InitAudio(void)
{ 
    sysaudio.sdl_available = false;
	sysaudio.obtained.freq = sysaudio.desired.freq = SAMPLERATE;
	sysaudio.obtained.format = sysaudio.desired.format = AUDIO_S16SYS;
	sysaudio.obtained.channels = sysaudio.desired.channels = 2;
	sysaudio.obtained.samples = sysaudio.desired.samples = SAMPLECOUNT;
	sysaudio.obtained.size = sysaudio.desired.size = (SAMPLECOUNT*sysaudio.obtained.channels*((sysaudio.obtained.format&0xff)>>3));
	sysaudio.convert = false;

    if (sysaudio.sound_enabled) {
        if (!I_InitSound()) {
            sysaudio.sound_enabled = false;
        }
    }

    if (sysaudio.music_enabled) {
	    if (!I_InitMusic()) {
            sysaudio.music_enabled = false;
        }
    }
}
