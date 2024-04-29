/*
	RGB2YUV conversion functions
	taken from SDL test program testoverlay.c

*/

#include <SDL.h>

static Uint8 p2y[256];
static Uint8 p2u[256];
static Uint8 p2v[256];

void I_Pal2Yuv(SDL_Color *palette)
{
	int i, r,g,b, y,u,v;

	for (i=0; i<256; i++) {
		r = palette[i].r;
		g = palette[i].g;
		b = palette[i].b;
#if 0
		y = 0.299*r + 0.587*g + 0.114*b;
		u = (b-y)*0.565 + 128;
		v = (r-y)*0.713 + 128;
#else
		y = (77*r + 150*g +29*b)>>8;
		u = ((b-y)*145 + 256*128)>>8;
		v = ((r-y)*182 + 256*128)>>8;
#endif
		p2y[i] = y;
		p2u[i] = u;
		p2v[i] = v;
	}
}

static inline void RGBtoYUV(Uint8 *rgb, int *yuv)
{
#if 0
	yuv[0] = 0.299*rgb[0] + 0.587*rgb[1] + 0.114*rgb[2];
	yuv[1] = (rgb[2]-yuv[0])*0.565 + 128;
	yuv[2] = (rgb[0]-yuv[0])*0.713 + 128;
#else
	yuv[0] = (77*rgb[0] + 150*rgb[1] +29*rgb[2])>>8;
	yuv[1] = ((rgb[2]-yuv[0])*145 + 256*128)>>8;
	yuv[2] = ((rgb[0]-yuv[0])*182 + 256*128)>>8;
#endif
}

/*--- 8bits paletted to overlay ---*/

void I_RGB8toYV12(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			int c = *p++;
			*(op[0]++)=p2y[c];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=p2v[c];
				*(op[2]++)=p2u[c];
			}
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}
 
void I_RGB8toIYUV(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			int c = *p++;
			*(op[0]++)=p2y[c];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=p2u[c];
				*(op[2]++)=p2v[c];
			}
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB8toUYVY(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			int c = *p++;
			if(x%2==0)
			{
				*(op++)=p2u[c];
				*(op++)=p2y[c];
				*(op++)=p2v[c];
			}
			else
				*(op++)=p2y[c];
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB8toYVYU(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			int c = *p++;
			if(x%2==0)
			{
				*(op++)=p2y[c];
				*(op++)=p2v[c];
				op[1]=p2u[c];
			}
			else
			{
				*op=p2y[c];
				op+=2;
			}
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB8toYUY2(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			int c = *p++;
			if(x%2==0)
			{
				*(op++)=p2y[c];
				*(op++)=p2u[c];
				op[1]=p2v[c];
			}
			else
			{
				*op=p2y[c];
				op+=2;
			}
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

/*--- 32bits RGBA to overlay ---*/

void I_RGB32toYV12(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p, yuv);
			*(op[0]++)=yuv[0];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=yuv[2];
				*(op[2]++)=yuv[1];
			}
			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}
 
void I_RGB32toIYUV(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op[3];

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	/* Black initialization */
	/*
	memset(o->pixels[0],0,o->pitches[0]*o->h);
	memset(o->pixels[1],128,o->pitches[1]*((o->h+1)/2));
	memset(o->pixels[2],128,o->pitches[2]*((o->h+1)/2));
	*/

	/* Convert */
	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op[0]=o->pixels[0]+o->pitches[0]*y;
		op[1]=o->pixels[1]+o->pitches[1]*(y/2);
		op[2]=o->pixels[2]+o->pitches[2]*(y/2);
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			*(op[0]++)=yuv[0];
			if(x%2==0 && y%2==0)
			{
				*(op[1]++)=yuv[1];
				*(op[2]++)=yuv[2];
			}
			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB32toUYVY(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p, yuv);
			if(x%2==0)
			{
				*(op++)=yuv[1];
				*(op++)=yuv[0];
				*(op++)=yuv[2];
			}
			else
				*(op++)=yuv[0];

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB32toYVYU(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			if(x%2==0)
			{
				*(op++)=yuv[0];
				*(op++)=yuv[2];
				op[1]=yuv[1];
			}
			else
			{
				*op=yuv[0];
				op+=2;
			}

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}

void I_RGB32toYUY2(SDL_Surface *s, SDL_Overlay *o)
{
	int x,y;
	int yuv[3];
	Uint8 *p,*op;

	SDL_LockSurface(s);
	SDL_LockYUVOverlay(o);

	for(y=0; y<s->h && y<o->h; y++)
	{
		p=((Uint8 *) s->pixels)+s->pitch*y;
		op=o->pixels[0]+o->pitches[0]*y;
		for(x=0; x<s->w && x<o->w; x++)
		{
			RGBtoYUV(p,yuv);
			if(x%2==0)
			{
				*(op++)=yuv[0];
				*(op++)=yuv[1];
				op[1]=yuv[2];
			}
			else
			{
				*op=yuv[0];
				op+=2;
			}

			p+=s->format->BytesPerPixel;
		}
	}

	SDL_UnlockYUVOverlay(o);
	SDL_UnlockSurface(s);
}
