#include "pixel_scale.h"
#include <math.h>
#include <limits.h>
#include <string.h>

/* Ported from fa18-interceptor/native/src/platform/pixel_scale.c.
 * Technique: binaryfoundry/zxsc, src/speccy/Render.cpp (MIT, 2019).
 * See third_party/ZXSC-LICENSE.txt. SDL target textures implement the mip
 * chain without backend-specific GL handles. Integral levels are reproduced
 * directly from the source: 12 -> 6 -> 3 nearest blocks are identical after
 * box mip filtering, so unused large levels need no per-frame rendering. */
static uint64_t storage(int width, int height, int factor)
{
    uint64_t bytes = 0;
    for (int w = width * factor, h = height * factor; ; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {
        bytes += (uint64_t)w * h * 4;
        if (w == 1 && h == 1)
            return bytes;
    }
}

void pixel_scale_close(PixelScale *scale)
{
    for (int i = 0; i < scale->count; ++i)
        SDL_DestroyTexture(scale->levels[i]);
    memset(scale, 0, sizeof *scale);
}

static int plain(SDL_Texture *texture, SDL_ScaleMode mode)
{
    return SDL_SetTextureScaleMode(texture, mode) &&
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) && SDL_SetTextureAlphaMod(texture, 255);
}

int pixel_scale_open(PixelScale *scale, SDL_Renderer *renderer, SDL_Texture *source, int width, int height)
{
    memset(scale, 0, sizeof *scale);
    if (width < 1 || height < 1 || width > INT_MAX / 12 || height > INT_MAX / 12)
        return SDL_SetError("Invalid pixel resampler source size"), 0;
    const SDL_PropertiesID properties = SDL_GetRendererProperties(renderer);
    const Sint64 limit = SDL_GetNumberProperty(properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    const char *name = SDL_GetRendererName(renderer);
    /* Preserve F/A-18's explicit host/GPU texture-storage budgets. */
    const uint64_t budget = (uint64_t)(name && !strcmp(name, "software") ? 16 : 256) * 1024 * 1024;
    int factor = 12;
    while (factor > 1 && ((limit > 0 && ((Sint64)width * factor > limit || (Sint64)height * factor > limit)) ||
                         storage(width, height, factor) > budget))
        factor /= 2;
    if ((limit > 0 && (width > limit || height > limit)) || storage(width, height, factor) > budget)
        return SDL_SetError("Pixel resampler exceeds renderer texture/storage limits"), 0;
    scale->renderer = renderer;
    scale->width = width;
    scale->height = height;
    scale->factor = factor;
    for (int divisor = 2; factor % divisor == 0; divisor *= 2)
        ++scale->integral_level;
    for (int w = width * factor, h = height * factor; ; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) {
        if (scale->count == PIXEL_SCALE_LEVELS) {
            SDL_SetError("Pixel resampler mip capacity exceeded");
            goto error;
        }
        SDL_Texture *level = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if (!level)
            goto error;
        scale->levels[scale->count++] = level;
        if (!plain(source, SDL_SCALEMODE_NEAREST) || !SDL_SetRenderTarget(renderer, level) ||
            !SDL_RenderTexture(renderer, source, NULL, NULL))
            goto error;
        if (w == 1 && h == 1)
            break;
    }
    /* Complete startup GPU work before starting the gameplay/audio clocks. */
    SDL_Surface *completed = SDL_RenderReadPixels(renderer, NULL);
    if (!completed)
        goto error;
    SDL_DestroySurface(completed);
    if (!SDL_SetRenderTarget(renderer, NULL) || !pixel_scale_draw(scale, source, width) || !SDL_FlushRenderer(renderer))
        goto error;
    return 1;
error:
    SDL_SetRenderTarget(renderer, NULL);
    pixel_scale_close(scale);
    return 0;
}

int pixel_scale_draw(PixelScale *scale, SDL_Texture *source, int width)
{
    SDL_Renderer *renderer = scale->renderer;
    SDL_FRect destination;
    if (width < 1 || width > scale->width)
        return SDL_SetError("Pixel resampler source exceeds reserved width"), 0;
    if (!SDL_GetRenderLogicalPresentationRect(renderer, &destination))
        return 0;
    const float sx = destination.w / width, sy = destination.h / scale->height;
    const SDL_FRect image = { 0, 0, (float)width, (float)scale->height };
    if (sx >= 1 && sy >= 1 && fabsf(sx - roundf(sx)) < 0.0001f && fabsf(sy - roundf(sy)) < 0.0001f)
        return plain(source, SDL_SCALEMODE_NEAREST) && SDL_RenderClear(renderer) &&
            SDL_RenderTexture(renderer, source, &image, NULL);

    /* Traverse target/draw even for a subpixel letterbox to restore the
     * backend viewport after a canvas resize. */
    const float rho = sx > 0 && sy > 0 ? fmaxf(scale->factor / sx, scale->factor / sy) : INFINITY;
    const float lod = fminf((float)(scale->count - 1), fmaxf(0, log2f(rho)));
    const int low = (int)lod;
    const Uint8 weight = (Uint8)lroundf((lod - low) * 255);
    const int high = weight && low + 1 < scale->count ? low + 1 : low;
    const int first = low < scale->integral_level ? low : scale->integral_level;
    SDL_Texture *initial = scale->levels[first];
    const int multiplier = scale->factor / (1 << first);
    const SDL_FRect enlarged = { 0, 0, (float)(width * multiplier), (float)(scale->height * multiplier) };
    if (!plain(source, SDL_SCALEMODE_NEAREST) || !SDL_SetRenderTarget(renderer, initial) ||
        !SDL_RenderTexture(renderer, source, &image, &enlarged))
        return 0;
    if (width < scale->width) {
        const SDL_FRect edge = { (float)(width - 1), 0, 1, (float)scale->height };
        const SDL_FRect padding = { enlarged.w, 0, (float)((scale->width - width) * multiplier), enlarged.h };
        if (!SDL_RenderTexture(renderer, source, &edge, &padding))
            return 0;
    }
    for (int i = first + 1; i <= high; ++i) {
        if (!plain(scale->levels[i - 1], SDL_SCALEMODE_LINEAR) || !SDL_SetRenderTarget(renderer, scale->levels[i]) ||
            !SDL_RenderTexture(renderer, scale->levels[i - 1], NULL, NULL))
            return 0;
    }
    if (!SDL_SetRenderTarget(renderer, NULL) || !SDL_RenderClear(renderer))
        return 0;
    for (int i = low; i <= high; ++i) {
        SDL_Texture *level = scale->levels[i];
        const SDL_FRect crop = { 0, 0, level->w * (float)width / scale->width, (float)level->h };
        if (!plain(level, rho < 1 ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR))
            return 0;
        if (i != low && (!SDL_SetTextureBlendMode(level, SDL_BLENDMODE_BLEND) || !SDL_SetTextureAlphaMod(level, weight)))
            return 0;
        if (!SDL_RenderTexture(renderer, level, &crop, NULL))
            return 0;
        if (high == low || rho <= 1)
            break;
    }
    return 1;
}
