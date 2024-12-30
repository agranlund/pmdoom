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
#if 0

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

#include "i_system.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_qmus2mid.h"
#include "sounds.h"
#include "w_wad.h"

#ifdef ENABLE_SDLMIXER
static Mix_Music *music=NULL;
static SDL_RWops *rw=NULL;
static void *midifile=NULL;
static int midifile_length=0;
#endif

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
void I_UpdateMusic_SDL(void *unused, Uint8 *stream, int len)
{
}

// MUSIC API - dummy. Some code from DOS version.
void I_SetMusicVolume_SDL(int volume)
{
#ifdef ENABLE_SDLMIXER
	if (sysaudio.enabled)
		Mix_VolumeMusic((volume*128)/15);
#endif
}

void I_ShutdownMusic_SDL(void)
{    
#ifdef ENABLE_SDLMIXER
	I_StopSong(-1);
	while (I_QrySongPlaying(-1)) {
		SDL_Delay(100);
	}

	I_UnRegisterSong(-1);
#endif
}

int I_InitMusic_SDL(void)
{ 
    if (!sysaudio.pcm_available)
        return 0;
    return 1;
}

//
// MUSIC API.
// Still no music done.
// Remains. Dummies.
//

static int	looping=0;
static int	musicdies=-1;

void I_PlaySong_SDL(int handle, int looping)
{
	// UNUSED.
	handle = 0;
	musicdies = gametic + TICRATE*30;
#ifdef ENABLE_SDLMIXER
	if (music && sysaudio.enabled) {
		Mix_FadeInMusic(music,looping ? -1 : 0,1000);
	}
#else
	looping = 0;
#endif
}

void I_PauseSong_SDL (int handle)
{
	// UNUSED.
	handle = 0;
#ifdef ENABLE_SDLMIXER
	if (sysaudio.enabled)
		Mix_PauseMusic();
#endif
}

void I_ResumeSong_SDL (int handle)
{
	// UNUSED.
	handle = 0;
#ifdef ENABLE_SDLMIXER
	if (sysaudio.enabled)
		Mix_ResumeMusic();
#endif
}

void I_StopSong_SDL(int handle)
{
	// UNUSED.
	handle = 0;
  
	looping = 0;
	musicdies = 0;
#ifdef ENABLE_SDLMIXER
	if (sysaudio.enabled)
		Mix_HaltMusic();
#endif
}

void I_UnRegisterSong_SDL(int handle)
{
	// UNUSED.
	handle = 0;
#ifdef ENABLE_SDLMIXER
	if (!sysaudio.enabled)
		return;

	if (music) {
		Mix_FreeMusic(music);
		music=NULL;
	}
	if (rw) {
		SDL_FreeRW(rw);
		rw=NULL;
	}
	if (midifile) {
		Z_Free(midifile);
		midifile=NULL;
		midifile_length=0;
	}
#endif
}

int I_RegisterSong_SDL(void* data, int length)
{
#ifdef ENABLE_SDLMIXER
	if (!sysaudio.music_enabled)
		return 0;

	/* Create rw on it */
	if (rw) {
		SDL_FreeRW(rw);
		rw=NULL;
	}

	/* Convert MUS_Music *data to MIDI music */
	midifile_length = 65536;
	midifile = Z_Malloc(midifile_length, PU_STATIC, NULL);
	rw = SDL_RWFromMem(midifile, midifile_length);

	midifile_length = qmus2mid(data, length, rw, 1,0,0,0);
	
	if (midifile_length<0) {
		I_UnRegisterSong(-1);
		return 0;
	}

	/* Create a new rwops with new midifile length */
	SDL_FreeRW(rw);
	rw = SDL_RWFromMem(midifile, midifile_length);

	if (midifile && midifile_length) {
		SDL_RWseek(rw, 0, SEEK_SET);
		music = Mix_LoadMUS_RW(rw);
		if (music==NULL) {
			I_UnRegisterSong(-1);
			return 0;
		}
	}
#else

	data = NULL;
	length = 0;

#endif

	return 1;
}

// Is the song playing?
int I_QrySongPlaying_SDL(int handle)
{
	// UNUSED.
	handle = 0;
#ifdef ENABLE_SDLMIXER
	if (sysaudio.music_enabled)
		return Mix_PlayingMusic();
	else
		return 0;
#else
	return looping || musicdies > gametic;
#endif
}

#endif
