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
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>

#include <SDL.h>
#ifdef __MINT__
#include <mint/osbind.h>
#include <mint/cookie.h>
#endif

#include "doomdef.h"
#include "m_misc.h"
#include "m_fixed.h"
#include "i_video.h"
#include "i_audio.h"
#include "i_net.h"
#include "i_cdmus.h"

#include "d_net.h"
#include "g_game.h"

#include "i_system.h"

sysgame_t	sysgame={DEFAULT_HEAP_SIZE,NULL,false};

static void I_InitFpu(void);

void
I_Tactile
( int	on,
  int	off,
  int	total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t	emptycmd;
ticcmd_t*	I_BaseTiccmd(void)
{
    return &emptycmd;
}

byte* I_ZoneBase (int*	size)
{
#ifdef __MINT__
	int mxalloc_present;
	long maximal_heap_size=0;

	mxalloc_present = ((Sversion()&0xFF)>=0x01)|(Sversion()>=0x1900);
	if (mxalloc_present) {
		maximal_heap_size = Mxalloc(-1,MX_PREFTTRAM);
	} else {
		maximal_heap_size = Malloc(-1);
	}
	maximal_heap_size >>= 10;
	maximal_heap_size -= 256;	/* Keep some KB */
	if (sysgame.kb_used>maximal_heap_size)
		sysgame.kb_used=maximal_heap_size;
#endif

    *size = sysgame.kb_used<<10;

	printf(" %d Kbytes allocated for zone\n",sysgame.kb_used);

#ifdef __MINT__
	if (mxalloc_present) {
		sysgame.zone = (void *)Mxalloc(*size, MX_PREFTTRAM);
		maximal_heap_size = Mxalloc(-1,MX_STRAM);
	} else {
		sysgame.zone = (void *)Malloc(*size);
		maximal_heap_size = Malloc(-1);
	}
	printf(" (%d Kbytes left for audio/video subsystem)\n", maximal_heap_size>>10);
#else
	sysgame.zone = malloc (*size);
#endif

    return (byte *) sysgame.zone;
}



//
// I_GetTime
// returns time in 1/70th second tics
//
int  I_GetTime (void)
{
    int			newtics;
    static int		basetime=0;
  
    if (!basetime)
	basetime = SDL_GetTicks();
    newtics = ((SDL_GetTicks()-basetime)*TICRATE)/1000;
    return newtics;
}



//
// I_Init
//
void I_Init (void)
{
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)<0) {
		fprintf(stderr, "Can not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	if (sysaudio.enabled) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO)<0) {
			sysaudio.enabled = false;
		}
	}

#ifdef __MINT__
	if (sysnetwork.layer==NETWORK_STING) {
		I_InitNetwork = I_InitNetwork_sting;
		I_ShutdownNetwork = I_ShutdownNetwork_sting;
	}
#endif

	I_InitFpu();
	I_InitAudio();
	//  I_InitGraphics();
	if (i_CDMusic) {
		if (I_CDMusInit() == -1) {
			fprintf(stderr, "Can not use CD for music replay\n");
			i_CDMusic = false;
		}
	}
}

static void I_InitFpu(void)
{
#if defined(__MINT__) && !defined(__mcoldfire__)
	long cpu_cookie;

	if (Getcookie(C__CPU, &cpu_cookie) != C_FOUND) {
		return;
	}

	cpu_cookie &= 0xffff;
	if ((cpu_cookie<20) || (cpu_cookie>60)) {
		return;
	}

	FixedMul = FixedMul020;
	FixedDiv2 = FixedDiv2020;

	if (cpu_cookie==60) {
	    __asm__ __volatile__ (
				".chip	68060\n"
			"	fmove%.l	fpcr,d0\n"
			"	andl	#~0x30,d0\n"
			"	orb		#0x20,d0\n"
			"	fmove%.l	d0,fpcr\n"
#if defined(__mc68020__)
			"	.chip	68020"
#else
			"	.chip	68000"
#endif
			: /* no return value */
			: /* no input */
			: /* clobbered registers */
				"d0", "cc"
		);

		FixedMul = FixedMul060;
		FixedDiv2 = FixedDiv2060;
		sysgame.cpu060 = true;
	}
#endif
}

static void I_Shutdown(void)
{
	if (i_CDMusic) {
		I_CDMusShutdown();
	}
	I_ShutdownNetwork();
	I_ShutdownAudio();
	I_ShutdownGraphics();

	if (sysgame.zone) {
#ifdef __MINT__
		Mfree(sysgame.zone);
#else
		free(sysgame.zone);
#endif
		sysgame.zone=NULL;
	}
	SDL_Quit();
}

//
// I_Quit
//
void I_Quit (void)
{
	D_QuitNetGame ();
	M_SaveDefaults ();
	I_Shutdown();
	exit(0);
}

void I_WaitVBL(int count)
{
	SDL_Delay((count*1000)/(TICRATE<<1));
}


//
// I_Error
//
extern boolean demorecording;

void I_Error (char *error, ...)
{
    va_list	argptr;

	static int firsttime = 1;
	if (!firsttime) {	/* Avoid infinite error loop */
		SDL_Quit();
		exit(1);
	}

    // Message first.
    va_start (argptr,error);
    fprintf (stderr, "Error: ");
    vfprintf (stderr,error,argptr);
    fprintf (stderr, "\n");
    va_end (argptr);

    fflush( stderr );

	firsttime = 0;

    // Shutdown. Here might be other errors.
	if (demorecording)
		G_CheckDemoStatus();

	D_QuitNetGame ();
	I_Shutdown();
#ifdef __MINT__
	Cconws("- Press a key to exit -");
	while (Cconis()==0) {
	}
#endif
    exit(-1);
}
