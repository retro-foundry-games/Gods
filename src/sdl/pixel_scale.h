#ifndef GODS_PIXEL_SCALE_H
#define GODS_PIXEL_SCALE_H
#include <SDL3/SDL.h>

enum { PIXEL_SCALE_LEVELS = 16 };
typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *levels[PIXEL_SCALE_LEVELS];
    int width, height, factor, count, integral_level;
} PixelScale;

/* F/A-18's ZXSC-style nearest supersampling and trilinear mip resampling. */
int pixel_scale_open(PixelScale *scale, SDL_Renderer *renderer,
                     SDL_Texture *source, int width, int height);
int pixel_scale_draw(PixelScale *scale, SDL_Texture *source, int width);
void pixel_scale_close(PixelScale *scale);
#endif
