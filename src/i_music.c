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

#include "doomdef.h"
#include "doomstat.h"
#include "z_zone.h"
#include "m_argv.h"

#include "i_system.h"
#include "i_audio.h"
#include "i_music.h"
#include "i_music_sdl.h"
#include "i_music_opl.h"
#include "i_music_midi.h"
#include "sounds.h"
#include "w_wad.h"

typedef struct 
{
    void (*I_UpdateMusic)(void *unused, Uint8 *stream, int len);
    int  (*I_InitMusic)(void);
    void (*I_ShutdownMusic)(void);
    void (*I_SetMusicVolume)(int volume);
    void (*I_PauseSong)(int handle);
    void (*I_ResumeSong)(int handle);
    int  (*I_RegisterSong)(void *data, int length);
    void (*I_PlaySong)(int handle, int looping );
    void (*I_StopSong)(int handle);
    void (*I_UnRegisterSong)(int handle);
    int  (*I_QrySongPlaying)(int handle);
} music_drv_t;

static music_drv_t drv;

void I_UpdateMusic(void *unused, Uint8 *stream, int len)
{
    if (sysaudio.music_enabled) {
        drv.I_UpdateMusic(unused, stream, len);
    }
}

extern int snd_MusicVolume;
void I_SetMusicVolume(int volume)
{
	snd_MusicVolume = volume;
    if (sysaudio.music_enabled) {
        drv.I_SetMusicVolume(volume);
    }
}

void I_ShutdownMusic(void)
{
    if (sysaudio.music_enabled) {
        I_StopSong(-1);
        while (I_QrySongPlaying(-1)) {
            SDL_Delay(100);
        }
        I_UnRegisterSong(-1);
        drv.I_ShutdownMusic();
    }
}


#define MUSIC_DRV_OFF       0
#define MUSIC_DRV_AUTO      (1<<0)
#define MUSIC_DRV_SDL       (1<<1)
#define MUSIC_DRV_MIDI      (1<<2)
#define MUSIC_DRV_ADLIB     (1<<3)

int I_InitMusic(void)
{ 
    if (sysaudio.music_enabled) {
        uint16_t music = MUSIC_DRV_AUTO;
        int p = M_CheckParm ("-music");
        if (p && (p<myargc-1)) {
            if (strcmp(myargv[p+1],"adlib")==0) {
                music = MUSIC_DRV_ADLIB;
            }
            else if (strcmp(myargv[p+1],"midi")==0) {
                music = MUSIC_DRV_MIDI;
            }
#if 0
            else if (strcmp(myargv[p+1],"sdl")==0) {
                music = MUSIC_DRV_SDL;
            }
#endif                
        }

        if (music & (MUSIC_DRV_AUTO | MUSIC_DRV_MIDI)) {
            drv.I_InitMusic         = I_InitMusic_MIDI;
            drv.I_UpdateMusic       = I_UpdateMusic_MIDI;
            drv.I_ShutdownMusic     = I_ShutdownMusic_MIDI;
            drv.I_SetMusicVolume    = I_SetMusicVolume_MIDI;
            drv.I_PauseSong         = I_PauseSong_MIDI;
            drv.I_ResumeSong        = I_ResumeSong_MIDI;
            drv.I_RegisterSong      = I_RegisterSong_MIDI;
            drv.I_PlaySong          = I_PlaySong_MIDI;
            drv.I_StopSong          = I_StopSong_MIDI;
            drv.I_UnRegisterSong    = I_UnRegisterSong_MIDI;
            drv.I_QrySongPlaying    = I_QrySongPlaying_MIDI;
            if (drv.I_InitMusic())
                return true;
        }

	    if (music & (MUSIC_DRV_AUTO | MUSIC_DRV_ADLIB)) {
            drv.I_InitMusic         = I_InitMusic_OPL;
            drv.I_UpdateMusic       = I_UpdateMusic_OPL;
            drv.I_ShutdownMusic     = I_ShutdownMusic_OPL;
            drv.I_SetMusicVolume    = I_SetMusicVolume_OPL;
            drv.I_PauseSong         = I_PauseSong_OPL;
            drv.I_ResumeSong        = I_ResumeSong_OPL;
            drv.I_RegisterSong      = I_RegisterSong_OPL;
            drv.I_PlaySong          = I_PlaySong_OPL;
            drv.I_StopSong          = I_StopSong_OPL;
            drv.I_UnRegisterSong    = I_UnRegisterSong_OPL;
            drv.I_QrySongPlaying    = I_QrySongPlaying_OPL;
            if (drv.I_InitMusic())
                return true;
        }

#if 0
        if (MUSIC & (MUSIC_DRV_AUTO | MUSIC_DRV_SDL)) {
            drv.I_InitMusic         = I_InitMusic_SDL;
            drv.I_UpdateMusic       = I_UpdateMusic_SDL;
            drv.I_ShutdownMusic     = I_ShutdownMusic_SDL;
            drv.I_SetMusicVolume    = I_SetMusicVolume_SDL;
            drv.I_PauseSong         = I_PauseSong_SDL;
            drv.I_ResumeSong        = I_ResumeSong_SDL;
            drv.I_RegisterSong      = I_RegisterSong_SDL;
            drv.I_PlaySong          = I_PlaySong_SDL;
            drv.I_StopSong          = I_StopSong_SDL;
            drv.I_UnRegisterSong    = I_UnRegisterSong_SDL;
            drv.I_QrySongPlaying    = I_QrySongPlaying_SDL;
            if (drv.I_InitMusic())
                return true;
        }
#endif        
    }

    sysaudio.music_enabled = false;
    return false;
}

void I_PlaySong(int handle, int looping)
{
    if (sysaudio.music_enabled) {
        drv.I_PlaySong(handle, looping);
    }
}

void I_PauseSong (int handle)
{
    if (sysaudio.music_enabled) {
        drv.I_PauseSong(handle);
    }
}

void I_ResumeSong (int handle)
{
    if (sysaudio.music_enabled) {
        drv.I_ResumeSong(handle);
    }
}

void I_StopSong(int handle)
{
    if (sysaudio.music_enabled) {
        drv.I_StopSong(handle);
    }
}

void I_UnRegisterSong(int handle)
{
    if (sysaudio.music_enabled) {
        drv.I_UnRegisterSong(handle);
    }
}

int I_RegisterSong(void* data, int length)
{
    if (sysaudio.music_enabled) {
        return drv.I_RegisterSong(data, length);
    }
    return -1;
}

int I_QrySongPlaying(int handle)
{
    if (sysaudio.music_enabled) {
        return drv.I_QrySongPlaying(handle);
    }
    return false;
}
