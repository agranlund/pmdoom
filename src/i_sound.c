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

// The number of internal mixing channels,
//  the samples calculated for each mixing step,
//  the size of the 16bit, 2 hardware channel (stereo)
//  mixing buffer, and the samplerate of the raw data.

typedef struct {
	// The channel data pointers, start and end.
	unsigned char *startp, *end;

	Uint32 length, position;

	// The channel step amount...
	Uint32 step;
	// ... and a 0.16 bit remainder of last step.
	Uint32 stepremainder;	/* or position.frac for m68k asm rout */

	// Time/gametic that the channel started playing,
	//  used to determine oldest, which automatically
	//  has lowest priority.
	// In case number of active sounds exceeds
	//  available channels.
	int start;

	// The sound in channel handles,
	//  determined on registration,
	//  might be used to unregister/stop/modify,
	//  currently unused.
	int 		handle;

	// SFX id of the playing sound effect.
	// Used to catch duplicates (like chainsaw).
	int		id;			

	// Hardware left and right channel volume lookup.
	int*		leftvol_lookup;
	int*		rightvol_lookup;

} channel_t;

static channel_t	channels[NUM_CHANNELS];


// Pitch to stepping lookup, unused.
static int		steptable[256];

// Volume lookups.
static int *vol_lookup=NULL;

static Sint32 *tmpMixBuffer = NULL;	/* 32bit mixing buffer for n voices */
static Sint16 *tmpMixBuffer2 = NULL;	/* 16bit clipped mixing buffer for conv */
static int tmpMixBuffLen = 0;
static SDL_bool quit = SDL_FALSE;

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
			if ( (channels[i].startp) && (channels[i].id == sfxid) ) {
				// Reset.
				channels[i].startp = NULL;
				// We are sure that iff,
				//  there will only be one.
				break;
			}
		}
	}

	// Loop all channels to find oldest SFX.
	for (i=0; (i<NUM_CHANNELS) && (channels[i].startp); i++) {
		if (channels[i].start < oldest) {
			oldestnum = i;
			oldest = channels[i].start;
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
	if (channels[slot].startp) {
		S_sfx[channels[slot].id].usefulness--;
	}

	// Okay, in the less recent channel,
	//  we will handle the new SFX.
	// Set pointer to raw data.
	channels[slot].startp = (unsigned char *) S_sfx[sfxid].data;
	// Set pointer to end of raw data.
	channels[slot].end = channels[slot].startp + S_sfx[sfxid].length;

	channels[slot].position = 0;
	channels[slot].length = S_sfx[sfxid].length;

	// Reset current handle number, limited to 0..100.
	if (!handlenums)
		handlenums = 100;

	// Assign current handle number.
	// Preserved so sounds could be stopped (unused).
	channels[slot].handle = rc = handlenums++;

	// Set stepping???
	// Kinda getting the impression this is never used.
	channels[slot].step = step;
	// ???
	channels[slot].stepremainder = 0;
	// Should be gametic, I presume.
	channels[slot].start = gametic;

	// Separation, that is, orientation/stereo.
	//  range is: 1 - 256
	seperation += 1;

	// Per left/right channel.
	//  x^2 seperation,
	//  adjust volume properly.
	leftvol = volume - ((volume*seperation*seperation) >> 16); ///(256*256);
	seperation = seperation - 257;
	rightvol = volume - ((volume*seperation*seperation) >> 16);	

	// Sanity check, clamp volume.
	if (rightvol < 0 || rightvol > 127)
		I_Error("rightvol out of bounds");

	if (leftvol < 0 || leftvol > 127)
		I_Error("leftvol out of bounds");

	// Get the proper lookup table piece
	//  for this volume level???
	channels[slot].leftvol_lookup = &vol_lookup[leftvol*256];
	channels[slot].rightvol_lookup = &vol_lookup[rightvol*256];

	// Preserve sound SFX id,
	//  e.g. for avoiding duplicates of chainsaw.
	channels[slot].id = sfxid;

	// You tell me.
	return rc;
}

void I_UpdateSounds(void)
{
	int i;

	for (i=0;i<NUM_CHANNELS;i++) {
		int sfxid;

		sfxid = channels[i].id;
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
	// Init internal lookups (raw data, mixing buffer, channels).
	// This function sets up internal lookups used during
	//  the mixing process. 
	int		i;
	int		j;

	if (!sysaudio.sound_enabled)
		return;

	// Okay, reset internal mixing channels to zero.
	for (i=0; i<NUM_CHANNELS; i++)
	{
		channels[i].startp = NULL;
	}

	// This table provides step widths for pitch parameters.
	// I fail to see that this is currently used.
	for (i=-128 ; i<128 ; i++) {
		int newstep;

		newstep = (int)(pow(2.0, (i/64.0))*65536.0);
		/* FIXME: are all samples 11025Hz ? */
		newstep = (newstep*11025)/sysaudio.obtained.freq;
		steptable[i+128] = newstep;
	}


	// Generates volume lookup tables
	//  which also turn the unsigned samples
	//  into signed samples.
	vol_lookup = Z_Malloc(128*256*sizeof(int), PU_STATIC, NULL);

	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<256 ; j++)
			vol_lookup[i*256+j] = (i*(j-128)*256)/127;
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
		if (!channels[ chan ].startp) {
			continue;
		}

		source = tmpMixBuffer;
		sample = channels[chan].startp;
		position = channels[ chan ].position;
		stepremainder = channels[chan].stepremainder;
		step = channels[chan].step;
		leftvol = channels[chan].leftvol_lookup;
		rightvol = channels[chan].rightvol_lookup;

		maxlen = FixedDiv(channels[chan].length-position, step);
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
			channels[ chan ].startp = NULL;
			S_sfx[channels[chan].id].usefulness--;
		}
		channels[ chan ].position = position;
		channels[ chan ].stepremainder = stepremainder;
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
	int i;
	int done = 0;

	quit = SDL_TRUE;

	// Wait till all pending sounds are finished.
	while ( !done ) {
		for( i=0 ; i<8 && !(channels[i].startp) ; i++) {
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
}

int I_InitSound(void)
{ 
    if (!sysaudio.pcm_available)
        return false;

    tmpMixBuffLen = sysaudio.obtained.samples * 2 * sizeof(Sint32);
    tmpMixBuffer = Z_Malloc(tmpMixBuffLen, PU_STATIC, 0);
    if (sysaudio.convert) {
        tmpMixBuffer2 = Z_Malloc(tmpMixBuffLen>>1, PU_STATIC, 0);
    }
    
    return true;
}
