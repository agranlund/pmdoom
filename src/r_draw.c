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
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include <SDL_endian.h>

#include "doomdef.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"
#include "i_video.h"

// State.
#include "doomstat.h"

#include "st_stuff.h"

// ?
static int maxwidth=0, maxheight=0;

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//


byte*		viewimage; 
int		viewwidth;
int		scaledviewwidth;
int		viewheight;
int		viewwindowx;
int		viewwindowy; 
byte*		*ylookup=NULL; 
int		*columnofs=NULL; 

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte		translations[3][256];	
 
 


//
// R_DrawColumn
// Source is the top of the column to scale.
//
lighttable_t*		dc_colormap; 
int			dc_x; 
int			dc_yl; 
int			dc_yh; 
fixed_t			dc_iscale; 
fixed_t			dc_texturemid;

// first pixel in a column (possibly virtual) 
byte*			dc_source;		

// just for profiling 
int			dccount;

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn (void) 
{ 
	unsigned short	count;
	byte*		dest;
	fixed_t		frac, fracstep;

	// Zero length, column does not exceed a pixel.
	if (dc_yh < dc_yl) 
		return; 
				 
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height) 
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

	count = dc_yh - dc_yl; 

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	// Use columnofs LUT for subwindows? 
	dest = ylookup[dc_yl] + columnofs[dc_x];  

	// Determine scaling,
	//  which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
	//  e.g. a DDA-lile scaling.
	// This is as fast as it gets.

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

    __asm__ __volatile__ (
	"moveql	#127,d0\n"
"	swap	%1\n"
"	swap	%2\n"
"	andw	d0,%2\n"
"	moveql	#0,d1\n"
"	movew	%0,d2\n"	/* d2 = 3-(count&3) */
"	notw	d2\n"
"	andw	#3,d2\n"
"	lea		R_DrawColumn_loop,a0\n"
"	muluw	#R_DrawColumn_loop1-R_DrawColumn_loop,d2\n"
"	lsrw	#2,%0\n"
"	move	#4,ccr\n"
"	jmp		a0@(0,d2:w)\n"

"R_DrawColumn_loop:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"R_DrawColumn_loop1:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%4@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	d1,%5@\n"
"	addw	%6,%5\n"

/*"	subqw	#1,%0\n"
"	bpls	R_DrawColumn_loop"*/
"	dbra	%0,R_DrawColumn_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "d"(fracstep), "d"(frac), "a"(dc_source),
			"a"(dc_colormap), "a"(dest), "a"(sysvideo.pitch)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "a0", "cc", "memory" 
	);
#else
	// Re-map color indices from wall texture column
	//  using a lighting/special effects LUT.

# define RENDER_PIXEL	\
	*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];	\
	dest += sysvideo.pitch; 	\
	frac += fracstep;

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
						RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
					} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
} 

void R_DrawColumn060 (void) 
{ 
	int	count, rshift;
	byte	*dest;
	fixed_t	frac, fracstep;

	// Zero length, column does not exceed a pixel.
	if (dc_yh < dc_yl) 
		return; 
				 
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height) 
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

	count = dc_yh - dc_yl; 

	// Framebuffer destination address.
	// Use ylookup LUT to avoid multiply with ScreenWidth.
	// Use columnofs LUT for subwindows? 
	dest = ylookup[dc_yl] + columnofs[dc_x];  

	// Determine scaling,
	//  which is the only mapping to be done.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	fracstep <<= 16-7;
	frac <<= 16-7;
	rshift = 32-7;

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

    __asm__ __volatile__ (
	"moveql	#0,d1\n\t"
	"movel	%1,d0\n\t"
	"lsrl	%2,d0\n"

"R_DrawColumn060_loop:\n\t"
	"moveb	%3@(0,d0:w),d1\n\t"
	"addl	%0,%1\n\t"

	"moveb	%4@(0,d1:l),d2\n\t"
	"movel	%1,d0\n\t"

	"moveb	d2,%5@\n\t"
	"lsrl	%2,d0\n\t"

	"subqw	#1,%7\n\t"
	"addal	%6,%5\n\t"

	"bpls	R_DrawColumn060_loop\n"

	 	: /* no return value */
	 	: /* input */
	 		"d"(fracstep), "d"(frac), "d"(rshift),
			"a"(dc_source), "a"(dc_colormap), "a"(dest),
			"a"(sysvideo.pitch), "d"(count)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "cc", "memory" 
	);
#endif
} 

void R_DrawColumnLow (void) 
{ 
	int	count, n; 
	byte*		dest; 
	fixed_t		frac, fracstep;	 

	// Zero length.
	if (dc_yh < dc_yl) 
		return; 

#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif 

	count = dc_yh - dc_yl; 

	dest = ylookup[dc_yl] + columnofs[dc_x<<1];

	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep;
    
	// Re-map color indices from wall texture column
	//  using a lighting/special effects LUT.

#define RENDER_PIXEL	\
	{	\
		int spot;	\
		spot = dc_colormap[dc_source[(frac>>FRACBITS)&127]];	\
		*(unsigned short *)dest = spot|(spot<<8);	\
		dest += sysvideo.pitch; 	\
		frac += fracstep;	\
	}	

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
}


//
// Spectre/Invisibility.
//
#define FUZZTABLE		64 


int	fuzzoffset[FUZZTABLE] =
{
	 1,-1, 1,-1, 1, 1,-1, 1,
	 1, 1,-1, 1, 1, 1,-1,-1,
	 1, 1, 1,-1,-1,-1,-1, 1,
	 1,-1,-1, 1, 1, 1, 1,-1,
	 1,-1, 1, 1,-1,-1, 1, 1,
	 1,-1,-1,-1,-1, 1, 1,-1,
	 1, 1,-1, 1, 1,-1, 1, 1,
	-1, 1, -1, 1, 1, -1, -1
}; 

static int fuzzcalcoffset[FUZZTABLE];

static int	fuzzpos = 0; 


//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//
void R_DrawFuzzColumn (void) 
{ 
	int		count, n; 
	byte*		dest;
	byte *fuzzcolormap = &colormaps[6*256];
	fixed_t		frac, fracstep;	 

	// Adjust borders. Low... 
	if (!dc_yl) 
		dc_yl = 1;

	// .. and high.
	if (dc_yh == viewheight-1) 
		dc_yh = viewheight - 2; 

	// Zero length.
	if (dc_yh < dc_yl) 
		return; 
    
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	count = dc_yh - dc_yl; 

	// Does not work with blocky mode.
	dest = ylookup[dc_yl] + columnofs[dc_x];

	// Looks familiar.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	// Looks like an attempt at dithering,
	//  using the colormap #6 (of 0-31, a bit
	//  brighter than average).

	// Lookup framebuffer, and retrieve
	//  a pixel that is either one column
	//  left or right of the current one.
	// Add index from colormap to index.
#define RENDER_PIXEL	\
	{	\
		*dest = fuzzcolormap[dest[fuzzcalcoffset[fuzzpos]]]; 	\
		\
		fuzzpos++;	/* Clamp table lookup index. */	\
		fuzzpos &= FUZZTABLE-1;	\
		\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
} 

void R_DrawFuzzColumnLow (void) 
{ 
	int	count, n;
	byte		*dest;
	byte *fuzzcolormap = &colormaps[6*256];
	fixed_t		frac, fracstep;

	/*  Adjust borders. Low...  */
	if (!dc_yl) 
		dc_yl = 1;

	/*  .. and high. */
	if (dc_yh == viewheight-1) 
		dc_yh = viewheight - 2; 

	/*  Zero length. */
	if (dc_yh < dc_yl) 
		return; 
    
#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ("R_DrawFuzzColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

	count = dc_yh - dc_yl; 

	/*  Does not work with blocky mode. */
	dest = ylookup[dc_yl] + columnofs[dc_x << 1];

	/*  Looks familiar. */
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	// Looks like an attempt at dithering,
	//  using the colormap #6 (of 0-31, a bit
	//  brighter than average).

	// Lookup framebuffer, and retrieve
	//  a pixel that is either one column
	//  left or right of the current one.
	// Add index from colormap to index.
#define RENDER_PIXEL	\
	{	\
		int spot;	\
		\
		spot = fuzzcolormap[dest[fuzzcalcoffset[fuzzpos]]]; 	\
		\
		*((unsigned short *) dest) = spot|(spot<<8);	\
		\
		fuzzpos++;	/* Clamp table lookup index. */	\
		fuzzpos &= FUZZTABLE-1;	\
		\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
}  
  
 

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
byte*	dc_translation;
byte*	translationtables;

void R_DrawTranslatedColumn (void) 
{ 
	unsigned short	count;
	byte*		dest; 
	fixed_t		frac, fracstep;	 

	if (dc_yh < dc_yl) 
		return; 

#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ( "R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif 

	count = dc_yh - dc_yl; 

	// FIXME. As above.
	dest = ylookup[dc_yl] + columnofs[dc_x]; 

	// Looks familiar.
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

    __asm__ __volatile__ (
	"moveql	#127,d0\n"
"	swap	%1\n"
"	swap	%2\n"
"	andw	d0,%2\n"
"	moveql	#0,d1\n"
"	movew	%0,d2\n"	/* d2 = 3-(count&3) */
"	notw	d2\n"
"	andw	#3,d2\n"
"	lea		R_DrawTranslatedColumn_loop,a0\n"
"	muluw	#R_DrawTranslatedColumn_loop1-R_DrawTranslatedColumn_loop,d2\n"
"	lsrw	#2,%0\n"
"	move	#4,ccr\n"
"	jmp		a0@(0,d2:w)\n"

"R_DrawTranslatedColumn_loop:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"R_DrawTranslatedColumn_loop1:\n"
"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

"	moveb	%3@(0,%2:w),d1\n"
"	addxl	%1,%2\n"
"	moveb	%7@(0,d1:l),d1\n"
"	andw	d0,%2\n"
"	moveb	%4@(0,d1:l),%5@\n"
"	addw	%6,%5\n"

/*"	subqw	#1,%0\n"
"	bpls	R_DrawTranslatedColumn_loop"*/
"	dbra	%0,R_DrawTranslatedColumn_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "d"(fracstep), "d"(frac), "a"(dc_source),
			"a"(dc_colormap), "a"(dest), "a"(sysvideo.pitch),
			"a"(dc_translation)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "a0", "cc", "memory" 
	);
#else
	// Translation tables are used
	//  to map certain colorramps to other ones,
	//  used with PLAY sprites.
	// Thus the "green" ramp of the player 0 sprite
	//  is mapped to gray, red, black/indigo. 

#define RENDER_PIXEL	\
	*dest = dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&127]]];	\
	dest += sysvideo.pitch; 	\
	frac += fracstep;

	{
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
} 

void R_DrawTranslatedColumnLow (void) 
{ 
	int	count, n; 
	byte		*dest; 
	fixed_t		frac, fracstep;	 

	if (dc_yh < dc_yl) 
		return; 

#ifdef RANGECHECK 
	if ((unsigned)dc_x >= sysvideo.width
		|| dc_yl < 0 || dc_yh >= sysvideo.height)
		I_Error ( "R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif 

	count = dc_yh - dc_yl; 

	/*  FIXME. As above. */
	dest = ylookup[dc_yl] + columnofs[dc_x << 1]; 

	/*  Looks familiar. */
	fracstep = dc_iscale; 
	frac = dc_texturemid + (dc_yl-centery)*fracstep; 

	// Translation tables are used
	//  to map certain colorramps to other ones,
	//  used with PLAY sprites.
	// Thus the "green" ramp of the player 0 sprite
	//  is mapped to gray, red, black/indigo. 

# define RENDER_PIXEL	\
	{	\
		int spot;	\
		\
		spot = dc_colormap[dc_translation[dc_source[(frac>>FRACBITS)&127]]]; 	\
		\
		*((unsigned short *) dest) = spot|(spot<<8);	\
		\
		dest += sysvideo.pitch;	\
		frac += fracstep;	\
	}

	n = count>>2;
	switch (count & 3) {
		case 3: do {
				RENDER_PIXEL;
		case 2:		RENDER_PIXEL;
		case 1:		RENDER_PIXEL;
		case 0:		RENDER_PIXEL;
			} while (--n>=0);
	}
#undef RENDER_PIXEL
} 



//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
	int		i;

	translationtables = Z_Malloc (256*3+255, PU_STATIC, 0);
	translationtables = (byte *)(( (int)translationtables + 255 )& ~255);

	// translate just the 16 green colors
	for (i=0 ; i<256 ; i++) {
		if (i >= 0x70 && i<= 0x7f) {
			// map green ramp to gray, brown, red
			translationtables[i] = 0x60 + (i&0xf);
			translationtables [i+256] = 0x40 + (i&0xf);
			translationtables [i+512] = 0x20 + (i&0xf);
		} else {
			// Keep all other colors as is.
			translationtables[i] = translationtables[i+256] 
				= translationtables[i+512] = i;
		}
	}
}




//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int			ds_y; 
int			ds_x1; 
int			ds_x2;

lighttable_t*		ds_colormap; 

fixed_t			ds_xfrac; 
fixed_t			ds_yfrac; 
fixed_t			ds_xstep; 
fixed_t			ds_ystep;

// start of a 64*64 tile image 
byte*			ds_source;	


//
// Draws the actual span.
void R_DrawSpan (void) 
{ 
	unsigned short		count;
	byte*		dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpan: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1];

	// We do not check for zero spans here?
	count = ds_x2 - ds_x1; 

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

    __asm__ __volatile__ (
	"moveql	#0,d1\n"
"	moveql	#10,d2\n"
"	moveql	#6,d3\n"
"	movel	%5,d0\n"

"	movew	%0,d4\n"
"	notw	d4\n"
"	andw	#3,d4\n"
"	lea		R_DrawSpan_loop,a0\n"
"	muluw	#R_DrawSpan_loop1-R_DrawSpan_loop,d4\n"
"	lsrw	#2,%0\n"
"	jmp		a0@(0,d4:w)\n"

"R_DrawSpan_loop:\n"
"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"R_DrawSpan_loop1:\n"
"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	lsrw	d2,d0\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"

"	subqw	#1,%0\n"
"	bpls	R_DrawSpan_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "a"(ds_source), "d"(uvstep), "a"(ds_colormap),
			"a"(dest), "d"(uv)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "a0", "cc", "memory" 
	);
	}
#else
# define RENDER_PIXEL	\
	{	\
	    int spot;	\
		\
		/* Current texture index in u,v. */	\
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); \
		\
		/* Lookup pixel from flat texture tile, */ \
		/*  re-index using light/colormap. */ \
		*dest++ = ds_colormap[ds_source[spot]];	\
		\
		/* Next step in u,v. */ \
		xfrac += ds_xstep;	\
		yfrac += ds_ystep;	\
	}

	{
		fixed_t xfrac = ds_xfrac, yfrac = ds_yfrac;

		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
} 

void R_DrawSpan060 (void) 
{ 
	unsigned short		count;
	byte*		dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpan: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1];

	// We do not check for zero spans here?
	count = ds_x2 - ds_x1 + 1; 

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

		/* Align dest on multiple of 4 */
		if (((Uint32) dest) & 3) {
			int num_pix, num_pix_start;

			num_pix = 4-(((Uint32) dest) & 3);
			if (num_pix>count) {
				num_pix = count;
			}
			num_pix_start = num_pix;

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_num_pix	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"0:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4," ASM_dest "@+\n\t"
	"roll	#6,d0\n\t"

	"subqw	#1," ASM_num_pix "\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"bgts	0b\n\t"

	"subal	" ASM_uvstep "," ASM_uv "\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(num_pix)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_num_pix
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

			count -= num_pix_start;
		}

		/* Main part */
		if (count>=4) {
			int count4, count4start;

			count4 = count4start = count>>2;
			--count4;

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_count4	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"1:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 0 */
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"lsll	#8,d4\n\t"
	"roll	#6,d0\n\t"

	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 1 */
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"lsll	#8,d4\n\t"
	"lsrw	d2,d0\n\t"

	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"

	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 2 */
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"lsll	#8,d4\n\t"
	"roll	#6,d0\n\t"

	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 3 */
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"movel	d4," ASM_dest "@+\n\t"
	"lsrw	d2,d0\n\t"

	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"

	"dbra	" ASM_count4 ",1b\n\t"

	"subal	" ASM_uvstep "," ASM_uv "\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(count4)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_count4
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

			count -= count4start<<2;
		}

		/* Draw remaining pixels */
		if (count>0) {

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_num_pix	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"2:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4," ASM_dest "@+\n\t"
	"roll	#6,d0\n\t"

	"subqw	#1," ASM_num_pix "\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"bgts	2b\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(count)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_num_pix
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

		}

	}

#endif
} 

void R_DrawSpanFlat (void) 
{ 
	byte*		dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpanFlat: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1];

	memset(dest, ds_colormap[*ds_source], ds_x2 - ds_x1 +1);
} 


//
// Again..
//
void R_DrawSpanLow (void) 
{ 
	unsigned short		count;
	byte *dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpan: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1<<1];

	count = ds_x2 - ds_x1; 

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

    __asm__ __volatile__ (
	"moveql	#0,d1\n"
"	moveql	#10,d2\n"
"	moveql	#6,d3\n"
"	movel	%5,d0\n"
"	lsrw	d2,d0\n"

"	movew	%0,d4\n"
"	notw	d4\n"
"	andw	#3,d4\n"
"	lea		R_DrawSpanLow_loop,a0\n"
"	muluw	#R_DrawSpanLow_loop1-R_DrawSpanLow_loop,d4\n"
"	lsrw	#2,%0\n"
"	jmp		a0@(0,d4:w)\n"

"R_DrawSpanLow_loop:\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"R_DrawSpanLow_loop1:\n"
"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	roll	d3,d0\n"
"	moveb	%1@(0,d0:w),d1\n"
"	addl	%2,%5\n"
"	moveb	%3@(0,d1:l),d1\n"
"	movel	%5,d0\n"
"	moveb	d1,%4@+\n"
"	lsrw	d2,d0\n"
"	moveb	d1,%4@+\n"

"	subqw	#1,%0\n"
"	bpls	R_DrawSpanLow_loop"
	 	: /* no return value */
	 	: /* input */
	 		"d"(count), "a"(ds_source), "d"(uvstep), "a"(ds_colormap),
			"a"(dest), "d"(uv)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "a0", "cc", "memory" 
	);
	}
#else
# define RENDER_PIXEL	\
	{	\
	    int spot;	\
		\
		/* Current texture index in u,v. */	\
		spot = ((yfrac>>(16-6))&(63*64)) + ((xfrac>>16)&63); \
		spot = ds_colormap[ds_source[spot]];	\
		\
		/* Lookup pixel from flat texture tile, */ \
		/*  re-index using light/colormap. */ \
		*(unsigned short *)dest = spot|(spot<<8);	\
		dest += 2;	\
		\
		/* Next step in u,v. */ \
		xfrac += ds_xstep;	\
		yfrac += ds_ystep;	\
	}

	{
		fixed_t		xfrac = ds_xfrac, yfrac = ds_yfrac;
		int n = count>>2;
		switch (count & 3) {
			case 3: do {
					RENDER_PIXEL;
			case 2:		RENDER_PIXEL;
			case 1:		RENDER_PIXEL;
			case 0:		RENDER_PIXEL;
				} while (--n>=0);
		}
	}
#undef RENDER_PIXEL
#endif
}

void R_DrawSpanLow060 (void) 
{ 
	unsigned short		count;
	byte*		dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpan: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1<<1];

	// We do not check for zero spans here?
	count = ds_x2 - ds_x1 + 1; 

#if defined(__GNUC__) && (defined(__m68k__) && !defined(__mcoldfire__))

	{
		long uv, uvstep;

		uv = (ds_yfrac >> 6) & 0xffffUL;
		uv |= (ds_xfrac<<10) & 0xffff0000UL;

		uvstep = (ds_ystep>>6) & 0xffffUL;
		uvstep |= (ds_xstep<<10) & 0xffff0000UL;

		/* Align dest on multiple of 4 */
		if (((Uint32) dest) & 3) {
			int num_pix, num_pix_start;

			num_pix = (4-(((Uint32) dest) & 3))>>1;
			if (num_pix>count) {
				num_pix = count;
			}
			num_pix_start = num_pix;

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_num_pix	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"0:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4,d3\n\t"
	"lslw	#8,d4\n\t"

	"moveb	d3,d4\n\t"

	"move	d4," ASM_dest "@+\n\t"
	"roll	#6,d0\n\t"

	"subqw	#1," ASM_num_pix "\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"bgts	0b\n\t"

	"subal	" ASM_uvstep "," ASM_uv "\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(num_pix)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_num_pix
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

			count -= num_pix_start;
		}

		/* Main part */
		if (count>=2) {
			int count4, count4start;

			count4 = count4start = (count>>1);
			--count4;

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_count4	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"1:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 0 */
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4,d3\n\t"
	"lsll	#8,d4\n\t"

	"roll	#6,d0\n\t"
	"moveb	d3,d4\n\t"

	"lsll	#8,d4\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"	/* pixel 1 */
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4,d3\n\t"
	"lsll	#8,d4\n\t"

	"roll	#6,d0\n\t"
	"moveb	d3,d4\n\t"

	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"movel	d4," ASM_dest "@+\n\t"

	"dbra	" ASM_count4 ",1b\n\t"

	"subal	" ASM_uvstep "," ASM_uv "\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(count4)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_count4
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

			count -= count4start<<1;
		}

		/* Draw remaining pixels */
		if (count>0) {

#define ASM_dest	"%0"
#define ASM_uv	"%1"
#define ASM_num_pix	"%2"
#define ASM_ds_source	"%3"
#define ASM_uvstep	"%4"
#define ASM_ds_colormap	"%5"

    __asm__ __volatile__ (
	"movel	" ASM_uv ",d0\n\t"
	"moveql	#10,d2\n\t"
	"lsrw	d2,d0\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"
	"roll	#6,d0\n\t"
	"moveql	#0,d1\n"

"2:\n\t"
	"moveb	" ASM_ds_source "@(0,d0:w),d1\n\t"
	"movel	" ASM_uv ",d0\n\t"

	"moveb	" ASM_ds_colormap "@(0,d1:l),d4\n\t"
	"lsrw	d2,d0\n\t"

	"moveb	d4,d3\n\t"
	"lslw	#8,d4\n\t"

	"moveb	d3,d4\n\t"
	"roll	#6,d0\n\t"

	"move	d4," ASM_dest "@+\n\t"

	"subqw	#1," ASM_num_pix "\n\t"
	"addal	" ASM_uvstep "," ASM_uv "\n\t"

	"bgts	2b\n"
	 	: /* return value */
			"+a"(dest), "+a"(uv), "+d"(count)
	 	: /* input */
	 		"a"(ds_source), "a"(uvstep), "a"(ds_colormap)
	 	: /* clobbered registers */
	 		"d0", "d1", "d2", "d3", "d4", "cc", "memory" 
	);

#undef ASM_dest
#undef ASM_uv
#undef ASM_num_pix
#undef ASM_ds_source
#undef ASM_uvstep
#undef ASM_ds_colormap

		}

	}

#endif
} 

void R_DrawSpanLowFlat (void) 
{ 
	byte*		dest; 

#ifdef RANGECHECK 
	if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=sysvideo.width  
		|| (unsigned)ds_y>sysvideo.height)
		I_Error( "R_DrawSpan: %i to %i at %i", ds_x1,ds_x2,ds_y);
#endif 

	dest = ylookup[ds_y] + columnofs[ds_x1<<1];

	memset(dest, ds_colormap[*ds_source], (ds_x2 - ds_x1 + 1)<<1);
}

//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void
R_InitBuffer
( int		width,
  int		height ) 
{ 
	int		i; 

	if (sysvideo.width>maxwidth) {
		if (columnofs)
			Z_Free(columnofs);
		maxwidth = sysvideo.width;
		columnofs = Z_Malloc(maxwidth*sizeof(int), PU_STATIC, NULL);
	}

	if (sysvideo.height>maxheight) {
		if (ylookup)
			Z_Free(ylookup);
		maxheight = sysvideo.height;
		ylookup = Z_Malloc(maxheight*sizeof(byte *), PU_STATIC, NULL);
	}

	// Handle resize,
	//  e.g. smaller view windows
	//  with border and/or status bar.
	viewwindowx = (sysvideo.width-width) >> 1; 

	// Column offset. For windows.
	for (i=0 ; i<width ; i++) 
		columnofs[i] = viewwindowx + i;

	// Samw with base row offset.
	if (width == sysvideo.width) 
		viewwindowy = 0; 
	else 
		viewwindowy = (sysvideo.height-st_height-height) >> 1; 

	// Preclaculate all row offsets.
	for (i=0 ; i<height ; i++) 
		ylookup[i] = screens[0] + (i+viewwindowy)*sysvideo.pitch; 

	/* Recalc fuzz table */
	for (i=0; i<FUZZTABLE; i++) {
		fuzzcalcoffset[i] = fuzzoffset[i] * sysvideo.pitch;
	}
} 
 
 


//
// R_FillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen (void) 
{ 
	byte*	src;
	byte*	dest; 
	int		x;
	int		y; 
	patch_t*	patch;

	// DOOM border patch.
	char	name1[] = "FLOOR7_2";

	// DOOM II border patch.
	char	name2[] = "GRNROCK";	

	char*	name;

	if (scaledviewwidth == sysvideo.width)
		return;

	if ( gamemode == commercial)
		name = name2;
	else
		name = name1;

	src = W_CacheLumpName (name, PU_CACHE); 
	dest = screens[1]; 

	for (y=0 ; y<sysvideo.height-st_height ; y++) { 
		for (x=0 ; x<sysvideo.width/64 ; x++) { 
			memcpy (dest, src+((y&63)<<6), 64); 
			dest += 64; 
		} 

		if (sysvideo.width&63) { 
			memcpy (dest, src+((y&63)<<6), sysvideo.width&63); 
			dest += (sysvideo.width&63); 
		} 
	} 

	patch = W_CacheLumpName ("brdr_t",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy-8,1,patch);

	patch = W_CacheLumpName ("brdr_b",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy+viewheight,1,patch);

	patch = W_CacheLumpName ("brdr_l",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx-8,viewwindowy+y,1,patch);

	patch = W_CacheLumpName ("brdr_r",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx+scaledviewwidth,viewwindowy+y,1,patch);


	// Draw beveled edge. 
	V_DrawPatch (viewwindowx-8, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tl",PU_CACHE));

	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tr",PU_CACHE));

	V_DrawPatch (viewwindowx-8, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_bl",PU_CACHE));

	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_br",PU_CACHE));
} 
 

//
// Copy a screen buffer.
//
void
R_VideoErase
( int x, int y,
  int		count ) 
{ 
	// LFB copy.
	// This might not be a good idea if memcpy
	//  is not optiomal, e.g. byte by byte on
	//  a 32bit CPU, as GNU GCC/Linux libc did
	//  at one point.
	memcpy (screens[0]+y*sysvideo.pitch+x, screens[1]+y*sysvideo.width+x, count); 
} 


//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void
V_MarkRect
( int		x,
  int		y,
  int		width,
  int		height ); 
 
void R_DrawViewBorder (void) 
{ 
    int		top;
    int		side;
    int		i; 
 
	if (scaledviewwidth == sysvideo.width) 
		return; 
  
    top = ((sysvideo.height-st_height)-viewheight)/2; 
    side = (sysvideo.width-scaledviewwidth)/2; 
 
    // copy top
	for (i=0;i<top; i++) {
	    R_VideoErase (0, i, sysvideo.width); 
 	}
 
    // copy bottom 
	for (i=viewheight+top; i<sysvideo.height-st_height; i++) {
	    R_VideoErase (0, i, sysvideo.width); 
	}

    // copy sides
	for (i=top; i<top+viewheight; i++) {
		R_VideoErase (0, i, side);
		R_VideoErase (sysvideo.width-side, i, side);
	}

    // ? 
    V_MarkRect (0,0,sysvideo.width, sysvideo.height-st_height); 
} 
