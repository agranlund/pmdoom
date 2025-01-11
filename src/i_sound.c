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

#include "i_sound_sdl.h"
#include "i_sound_sb.h"

extern sysaudio_t sysaudio;

typedef struct 
{
    void (*I_UpdateSound)(void *unused, Uint8 *stream, int len);
    int  (*I_InitSound)(void);
    void (*I_ShutdownSound)(void);
} sound_drv_t;

static sound_drv_t drv;

i_sound_channel_t i_sound_channels[NUM_CHANNELS];

// Pitch to stepping lookup, unused.
static int		steptable[256];

// Volume lookups.
static int *vol_lookup=NULL;

//
// This function loads the sound data from the WAD lump,
//  for single sound.
//
void*
I_LoadSfx
( char*         sfxname,
  int*          len )
{
    unsigned char*      sfx;
    int                 size;
    char                name[20];
    int                 sfxlump;
    
	if (!sysaudio.sound_enabled)
		return NULL;

    // Get the sound data from the WAD, allocate lump
    //  in zone memory.
    sprintf(name, "ds%s", sfxname);

    // Now, there is a severe problem with the
    //  sound handling, in it is not (yet/anymore)
    //  gamemode aware. That means, sounds from
    //  DOOM II will be requested even with DOOM
    //  shareware.
    // The sound list is wired into sounds.c,
    //  which sets the external variable.
    // I do not do runtime patches to that
    //  variable. Instead, we will use a
    //  default sound for replacement.
    if ( W_CheckNumForName(name) == -1 )
      sfxlump = W_GetNumForName("dspistol");
    else
      sfxlump = W_GetNumForName(name);
    
    size = W_LumpLength( sfxlump );


    // Debug.
    // fprintf( stderr, "." );
    //fprintf( stderr, " -loading  %s (lump %d, %d bytes)\n",
    //	     sfxname, sfxlump, size );
    //fflush( stderr );
    
    sfx = (unsigned char*)W_CacheLumpNum( sfxlump, PU_STATIC );

	*len = size-8;

    return (sfx+8);
}

//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
int
addsfx
( int		sfxid,
  int		volume,
  int		step,
  int		seperation )
{
	static unsigned short	handlenums = 0;

	int		i;
	int		rc = -1;

	int		oldest = gametic;
	int		oldestnum = 0;
	int		slot;

	int		rightvol;
	int		leftvol;

	if (!sysaudio.sound_enabled)
		return -1;

	// Chainsaw troubles.
	// Play these sound effects only one at a time.
	if ( sfxid == sfx_sawup || sfxid == sfx_sawidl || sfxid == sfx_sawful
		|| sfxid == sfx_sawhit || sfxid == sfx_stnmov || sfxid == sfx_pistol) {
		// Loop all channels, check.
		for (i=0 ; i<NUM_CHANNELS ; i++) {
			// Active, and using the same SFX?
			if ( (i_sound_channels[i].startp) && (i_sound_channels[i].id == sfxid) ) {
				// Reset.
				i_sound_channels[i].startp = NULL;
				// We are sure that iff,
				//  there will only be one.
				break;
			}
		}
	}

	// Loop all channels to find oldest SFX.
	for (i=0; (i<NUM_CHANNELS) && (i_sound_channels[i].startp); i++) {
		if (i_sound_channels[i].start < oldest) {
			oldestnum = i;
			oldest = i_sound_channels[i].start;
		}
	}

	// Tales from the cryptic.
	// If we found a channel, fine.
	// If not, we simply overwrite the first one, 0.
	// Probably only happens at startup.
	if (i == NUM_CHANNELS)
		slot = oldestnum;
	else
		slot = i;

	/* Decrease usefulness of sample on channel 'slot' */
	if (i_sound_channels[slot].startp) {
		S_sfx[i_sound_channels[slot].id].usefulness--;
	}

	// Okay, in the less recent channel,
	//  we will handle the new SFX.
	// Set pointer to raw data.
	i_sound_channels[slot].startp = (unsigned char *) S_sfx[sfxid].data;

	// Set pointer to end of raw data.
	i_sound_channels[slot].end = S_sfx[sfxid].data + S_sfx[sfxid].length;

	i_sound_channels[slot].position = 0;
	i_sound_channels[slot].length = S_sfx[sfxid].length;

	// Reset current handle number, limited to 0..100.
	if (!handlenums)
		handlenums = 100;

	// Assign current handle number.
	// Preserved so sounds could be stopped (unused).
	i_sound_channels[slot].handle = rc = handlenums++;

	// Set stepping???
	// Kinda getting the impression this is never used.
	i_sound_channels[slot].step = step;
	// ???
	i_sound_channels[slot].stepremainder = 0;
	// Should be gametic, I presume.
	i_sound_channels[slot].start = gametic;

	// Separation, that is, orientation/stereo.
	//  range is: 1 - 256
	seperation += 1;

	// Per left/right channel.
	//  x^2 seperation,
	//  adjust volume properly.
	leftvol = volume - ((volume*seperation*seperation) >> 16); ///(256*256);
	seperation = seperation - 257;
	rightvol = volume - ((volume*seperation*seperation) >> 16);	

    if (leftvol < 0) leftvol = 0;
    else if (leftvol > 127) leftvol = 127;
    if (rightvol < 0) rightvol = 0;
    else if (rightvol > 127) rightvol = 127;
#if 0
	// Sanity check, clamp volume.
	if (rightvol < 0 || rightvol > 127)
		I_Error("rightvol out of bounds");

	if (leftvol < 0 || leftvol > 127)
		I_Error("leftvol out of bounds");
#endif

	i_sound_channels[slot].leftvol_lookup = &vol_lookup[leftvol*256];
	i_sound_channels[slot].rightvol_lookup = &vol_lookup[rightvol*256];

	// Preserve sound SFX id,
	//  e.g. for avoiding duplicates of chainsaw.
	i_sound_channels[slot].id = sfxid;

	// You tell me.
	return rc;
}

void I_UpdateSounds(void)
{
    if (!sysaudio.sound_enabled) {
        return;
    }

	for (int i=0;i<NUM_CHANNELS;i++) {
		int sfxid = i_sound_channels[i].id;
		if ((S_sfx[sfxid].usefulness <= 0) && S_sfx[sfxid].data) {
		    Z_ChangeTag(S_sfx[sfxid].data - 8, PU_CACHE);
			S_sfx[sfxid].usefulness = 0;
		    S_sfx[sfxid].data = NULL;
	    }
	}
}



//
// SFX API
// Note: this was called by S_Init.
// However, whatever they did in the
// old DPMS based DOS version, this
// were simply dummies in the Linux
// version.
// See soundserver initdata().
//
void I_SetChannels()
{
    if (!sysaudio.sound_enabled) {
        return;
    }

	for (int i=0; i<NUM_CHANNELS; i++) {
		i_sound_channels[i].startp = NULL;
    }

	// This table provides step widths for pitch parameters.
	// I fail to see that this is currently used.
	for (int i=-128 ; i<128 ; i++) {
		int newstep = (int)(pow(2.0, (i/64.0))*65536.0);
		newstep = (newstep*11025)/sysaudio.obtained.freq;
		steptable[i+128] = newstep;
	}

	// Generates volume lookup tables
	//  which also turn the unsigned samples
	//  into signed samples.
	vol_lookup = Z_Malloc(128*256*sizeof(int), PU_STATIC, NULL);
	for (int i=0 ; i<128 ; i++) {
		for (int j=0 ; j<256 ; j++) {
			vol_lookup[i*256+j] = (i*(j-128)*256)/127;
        }
    }
}	

 
void I_SetSfxVolume(int volume)
{
  // Identical to DOS.
  // Basically, this should propagate
  //  the menu/config file setting
  //  to the state variable used in
  //  the mixing.
  snd_SfxVolume = volume;
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//
int I_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    sprintf(namebuf, "ds%s", sfx->name);
    return W_GetNumForName(namebuf);
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//
int
I_StartSound
( int		id,
  int		vol,
  int		sep,
  int		pitch,
  int		priority )
{
	// UNUSED
	priority = 0;

	// Returns a handle (not used).
	id = addsfx( id, vol, steptable[pitch], sep );

	return id;
}



void I_StopSound (int handle)
{
	// You need the handle returned by StartSound.
	// Would be looping all channels,
	//  tracking down the handle,
	//  an setting the channel to zero.

	// UNUSED.
	handle = 0;
}


int I_SoundIsPlaying(int handle)
{
	// Ouch.
	return gametic < handle;
}


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
void I_UpdateSound(void *unused, Uint8 *stream, int len)
{
	if (sysaudio.sound_enabled)
        drv.I_UpdateSound(unused, stream, len);
}


void
I_UpdateSoundParams
( int	handle,
  int	vol,
  int	sep,
  int	pitch)
{
  // I fail too see that this is used.
  // Would be using the handle to identify
  //  on which channel the sound might be active,
  //  and resetting the channel parameters.

  // UNUSED.
  handle = vol = sep = pitch = 0;
}

void I_ShutdownSound(void)
{    
	if (sysaudio.sound_enabled) {
        drv.I_ShutdownSound();
    }
}

#define SOUND_DRV_OFF   0
#define SOUND_DRV_AUTO  (1<<0)
#define SOUND_DRV_SDL   (1<<1)
#define SOUND_DRV_SB    (1<<2)

int I_InitSound(void)
{
    sysaudio.sound_enabled = false;

    uint16_t sound = SOUND_DRV_AUTO;
    int p = M_CheckParm ("-sound");
    if (p && (p<myargc-1)) {
        if (strcmp(myargv[p+1],"sb")==0) {
            sound = SOUND_DRV_SB;
        }
        else if (strcmp(myargv[p+1],"sdl")==0) {
            sound = SOUND_DRV_SDL;
        }
    }

    if (sound & (SOUND_DRV_AUTO | SOUND_DRV_SB)) {
        drv.I_InitSound = I_InitSound_SB;
        drv.I_UpdateSound = I_UpdateSound_SB;
        drv.I_ShutdownSound = I_ShutdownSound_SB;
        if (drv.I_InitSound()) {
            sysaudio.sound_enabled = true;
            return true;
        }
    }

    if (sound & (SOUND_DRV_AUTO | SOUND_DRV_SDL)) {
        drv.I_InitSound = I_InitSound_SDL;
        drv.I_UpdateSound = I_UpdateSound_SDL;
        drv.I_ShutdownSound = I_ShutdownSound_SDL;
        if (drv.I_InitSound()) {
            sysaudio.sound_enabled = true;
            return true;
        }
    }
    return false;
}
