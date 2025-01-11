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
//	System interface, sound.
//
//-----------------------------------------------------------------------------

#ifndef __I_SOUND__
#define __I_SOUND__

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "i_audio.h"

int I_InitSound();
void I_UpdateSound(void *unused, Uint8 *stream, int len);
void I_ShutdownSound(void);
void I_SetChannels();
int I_GetSfxLumpNum (sfxinfo_t* sfxinfo );
int I_StartSound(int id, int vol, int sep, int pitch, int priority);
void I_StopSound(int handle);
int I_SoundIsPlaying(int handle);
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch);
void* I_LoadSfx(char* sfxname, int* len);
void I_UpdateSounds(void);


typedef struct {
    Uint8*  startp;
    Uint8*  end;
	Uint32  length;
    Uint32  position;

	Uint32  step;
	Uint32  stepremainder;	// or position.frac for m68k asm rout
	int     start;          // Time/gametic that the channel started playing,
	int     handle;
	int		id;			

    int*    leftvol_lookup;
    int*    rightvol_lookup;

} i_sound_channel_t;

extern i_sound_channel_t i_sound_channels[NUM_CHANNELS];

#endif
