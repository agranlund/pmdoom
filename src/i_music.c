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
#include "i_qmus2mid.h"
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

int I_InitMusic(void)
{ 
    if (sysaudio.music_enabled) {
	    if (M_CheckParm ("-adlib")) {
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

        if (1) {
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
#if 0
        if (1) {
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

/* Export MUS files as MIDI */
void I_ExportMusic(void)
{
	int i, lumpnum, mus_length, mus_from, mus_to;
	char namebuf[9], dstname[16];
	void *mus_data;
	SDL_RWops *src, *dst;

	/* Doom 1 */
	mus_from = mus_e1m1;
	mus_to = mus_introa;
	if (gamemode == commercial) {
		/* Doom 2 */
		mus_from = mus_runnin;
		mus_to = mus_dm2int;
	}

	for (i=mus_from; i<=mus_to; i++) {
		sprintf(namebuf, "d_%s", S_music[i].name);
		sprintf(dstname, "%s.mid", namebuf);
		printf("Exporting %s to %s ... ", namebuf, dstname);

		lumpnum = W_GetNumForName(namebuf);
		mus_data = (void *) W_CacheLumpNum(lumpnum, PU_MUSIC);
		mus_length = W_LumpLength(lumpnum);

		src = SDL_RWFromMem(mus_data, mus_length);
		mus_length = qmus2mid(mus_data, mus_length, src, 1,0,0,0);
		if (mus_length<0) {
			printf("failed\n");
		} else {
			dst = SDL_RWFromFile(dstname, "w");
			if (!dst) {
				printf("failed\n");
			} else {
				SDL_RWwrite(dst, mus_data, mus_length, 1);
				printf("ok\n");
			}
		}
		SDL_FreeRW(src);

		Z_ChangeTag(mus_data, PU_CACHE);
	}
}
