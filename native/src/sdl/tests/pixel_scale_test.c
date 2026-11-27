/* Adapted from F/A-18's native_pixel_scale_test.c.  Check image output, not
 * the implementation: solid integer pixels, fractional boundary coverage,
 * source-edge isolation and the game's non-square pixel presentation. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "pixel_scale.h"
#include <SDL3/SDL_main.h>
#include <assert.h>
#include <stdio.h>

static unsigned grey(const SDL_Surface *image, int x, int y)
{
    const uint32_t *row = (const uint32_t *)((const uint8_t *)image->pixels + y * image->pitch);
    const uint32_t pixel = row[x];
    assert(((pixel >> 16) & 255) == ((pixel >> 8) & 255) && ((pixel >> 8) & 255) == (pixel & 255));
    return pixel & 255;
}

int main(void)
{
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("Gods pixel resampling check", 32, 24, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    assert(renderer);
    SDL_Texture *source = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 8, 6);
    assert(source);
    uint32_t pixels[8 * 6];
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 8; x++)
            pixels[y * 8 + x] = x < 4 ? ((x & 1) ? 0xffffffffu : 0xff000000u) : 0xffff00ffu;
    assert(SDL_UpdateTexture(source, NULL, pixels, 8 * 4));
    assert(SDL_SetRenderLogicalPresentation(renderer, 4, 6, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    PixelScale scale;
    assert(pixel_scale_open(&scale, renderer, source, 8, 6));
    assert(scale.factor == 12);
    const int sizes[][2] = {{12, 18}, {10, 15}, {24, 36}, {5, 8}, {2, 3}, {1, 1}};
    for (unsigned test = 0; test < sizeof sizes / sizeof *sizes; test++) {
        assert(SDL_SetWindowSize(window, sizes[test][0], sizes[test][1]));
        SDL_PumpEvents();
        assert(pixel_scale_draw(&scale, source, 4));
        assert(SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
        SDL_Surface *read = SDL_RenderReadPixels(renderer, NULL);
        assert(read);
        SDL_Surface *image = SDL_ConvertSurface(read, SDL_PIXELFORMAT_ARGB8888);
        assert(image);
        if (test == 0 || test == 2) {
            const int repeat = test == 0 ? 3 : 6;
            assert(image->w == repeat * 4 && image->h == repeat * 6);
            for (int y = 0; y < image->h; y++)
                for (int x = 0; x < image->w; x++)
                    assert(grey(image, x, y) == ((x / repeat) & 1 ? 255u : 0u));
        } else if (test == 1) {
            /* At 2.5x, pixel interiors stay black/white; boundaries cover
             * half a destination pixel. Plain nearest or linear fails this. */
            assert(image->w == 10 && image->h == 15);
            assert(grey(image, 0, 7) == 0 && grey(image, 4, 7) > 225);
            assert(grey(image, 5, 7) < 30 && grey(image, 9, 7) == 255);
            unsigned a = grey(image, 2, 7), b = grey(image, 7, 7);
            assert(a > 90 && a < 165 && b > 90 && b < 165);
        }
        /* Cropped spare columns are magenta and must never leak, even when
         * the entire display is one pixel. */
        for (int y = 0; y < image->h; y++)
            for (int x = 0; x < image->w; x++)
                (void)grey(image, x, y);
        SDL_DestroySurface(image);
        SDL_DestroySurface(read);
        assert(SDL_RenderPresent(renderer));
        assert(SDL_SetRenderLogicalPresentation(renderer, 4, 6, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    }
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 4; x++)
            pixels[y * 8 + x] = ((x + y) & 1) ? 0xffffffffu : 0xff000000u;
    assert(SDL_UpdateTexture(source, NULL, pixels, 8 * 4));
    assert(SDL_SetWindowSize(window, 24, 30));
    SDL_PumpEvents();
    assert(SDL_SetRenderLogicalPresentation(renderer, 24, 30, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    assert(pixel_scale_draw(&scale, source, 4));
    assert(SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
    SDL_Surface *read = SDL_RenderReadPixels(renderer, NULL);
    assert(read);
    SDL_Surface *image = SDL_ConvertSurface(read, SDL_PIXELFORMAT_ARGB8888);
    assert(image && image->w == 24 && image->h == 30);
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 24; x++)
            assert(grey(image, x, y) == (((x / 6 + y / 5) & 1) ? 255u : 0u));
    SDL_DestroySurface(image);
    SDL_DestroySurface(read);
    printf("%s: integer pixels, fractional boundaries, cropped edges, tiny outputs and non-square pixels passed\n", SDL_GetRendererName(renderer));
    pixel_scale_close(&scale);
    SDL_DestroyTexture(source);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
