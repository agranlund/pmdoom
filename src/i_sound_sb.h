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

#ifndef __I_SOUND_SB__
#define __I_SOUND_SB__

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"

int I_InitSound_SB();
void I_UpdateSound_SB(void *unused, Uint8 *stream, int len);
void I_ShutdownSound_SB(void);
void I_SetChannels_SB();

#endif
