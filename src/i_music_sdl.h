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

#ifndef __I_MUSIC_SDL__
#define __I_MUSIC_SDL__

void I_UpdateMusic_SDL(void *unused, Uint8 *stream, int len);
int  I_InitMusic_SDL(void);
void I_ShutdownMusic_SDL(void);
void I_SetMusicVolume_SDL(int volume);
void I_PauseSong_SDL(int handle);
void I_ResumeSong_SDL(int handle);
int  I_RegisterSong_SDL(void *data, int length);
void I_PlaySong_SDL(int handle, int looping);
void I_StopSong_SDL(int handle);
void I_UnRegisterSong_SDL(int handle);
int  I_QrySongPlaying_SDL(int handle);


#endif
