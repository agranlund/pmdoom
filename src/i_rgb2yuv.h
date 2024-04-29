/*
	RGB2YUV conversion functions
	taken from SDL test program testoverlay.c

*/

#ifndef I_RGB2YUV_H
#define I_RGB2YUV_H

#include <SDL.h>

void I_Pal2Yuv(SDL_Color *palette);

void I_RGB8toYV12(SDL_Surface *s, SDL_Overlay *o);
void I_RGB8toIYUV(SDL_Surface *s, SDL_Overlay *o);
void I_RGB8toUYVY(SDL_Surface *s, SDL_Overlay *o);
void I_RGB8toYVYU(SDL_Surface *s, SDL_Overlay *o);
void I_RGB8toYUY2(SDL_Surface *s, SDL_Overlay *o);

void I_RGB32toYV12(SDL_Surface *s, SDL_Overlay *o);
void I_RGB32toIYUV(SDL_Surface *s, SDL_Overlay *o);
void I_RGB32toUYVY(SDL_Surface *s, SDL_Overlay *o);
void I_RGB32toYVYU(SDL_Surface *s, SDL_Overlay *o);
void I_RGB32toYUY2(SDL_Surface *s, SDL_Overlay *o);

#endif /* I_RGB2YUV_H */
