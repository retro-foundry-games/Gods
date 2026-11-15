/* The original glyph_draw ($17686), sprite_draw ($173C4) and image_draw_clipped
 * ($1EFCC) still perform every blit.  Record their actual masked writes so that
 * presentation can move the drawings without executing any gameplay code.
 * A frame is published before screens_flip ($1D302), then read only after that
 * screen becomes visible.  The program thread never shares a mutable capture
 * with the renderer. */
#include "presentation.h"
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 352, ROW = WIDTH / 8, HEIGHT = 192, PLANE = ROW * HEIGHT,
       BYTES = PLANE * 4, VIEW_WIDTH = 320,
       VIEW_HEIGHT = HEIGHT - 1, TOP = 60 - 44 };

struct Write {
    uint16_t offset, mask, before, value;
};

struct Drawing {
    uintptr_t caller, object;
    int kind, ordinal, group_count, x, y, fixed, bottom;
    size_t first, count;
};

struct Frame {
    uint8_t background[BYTES];
    struct Write *writes;
    struct Drawing *drawings;
    size_t write_count, write_capacity, draw_count, draw_capacity;
    uint64_t serial;
    uintptr_t scene;
    int x, y, fine;
    uint16_t tick;
};

static int enabled, drawing, overlay;
static uint8_t *capture_screen;
static uintptr_t capture_object;
static int capture_bottom = INT_MAX;
static struct Frame capture, published[2], previous, current;
static uint64_t serial, shown_at;
static pthread_mutex_t publication = PTHREAD_MUTEX_INITIALIZER;

static void *reserve(void *data, size_t *capacity, size_t needed, size_t size)
{
    if (*capacity >= needed)
        return data;
    size_t count = *capacity ? *capacity * 2 : 64;
    if (count < needed)
        count = needed;
    void *next = realloc(data, count * size);
    if (!next) {
        fprintf(stderr, "gods-sdl: cannot allocate %zu bytes for interpolation\n", count * size);
        abort();
    }
    *capacity = count;
    return next;
}

static void copy_frame(struct Frame *to, const struct Frame *from)
{
    to->writes = reserve(to->writes, &to->write_capacity, from->write_count, sizeof *to->writes);
    to->drawings = reserve(to->drawings, &to->draw_capacity, from->draw_count, sizeof *to->drawings);
    memcpy(to->background, from->background, BYTES);
    if (from->write_count)
        memcpy(to->writes, from->writes, from->write_count * sizeof *to->writes);
    if (from->draw_count)
        memcpy(to->drawings, from->drawings, from->draw_count * sizeof *to->drawings);
    to->write_count = from->write_count;
    to->draw_count = from->draw_count;
    to->serial = from->serial;
    to->scene = from->scene;
    to->x = from->x;
    to->y = from->y;
    to->fine = from->fine;
    to->tick = from->tick;
}

void presentation_reset(void)
{
    capture_screen = 0;
    capture_object = 0;
    capture_bottom = INT_MAX;
    drawing = overlay = 0;
    pthread_mutex_lock(&publication);
    /* Only the renderer owns previous/current.  An empty publication tells it
     * to discard its history, including after a save-state load. */
    published[0].serial = published[1].serial = 0;
    serial++;
    pthread_mutex_unlock(&publication);
}

void presentation_enable(int on)
{
    enabled = on;
    presentation_reset();
}

void presentation_discard_history(void)
{
    current.serial = previous.serial = 0;
}

void presentation_begin(uint8_t *screen, int x, int y, uintptr_t scene)
{
    if (!enabled)
        return;
    capture_screen = screen;
    capture_object = 0;
    capture_bottom = INT_MAX;
    drawing = overlay = 0;
    capture.draw_count = capture.write_count = 0;
    capture.x = x;
    capture.y = y;
    capture.scene = scene;
}

void presentation_draw_begin(uintptr_t caller, int kind, int x, int y)
{
    if (!enabled || !capture_screen || overlay)
        return;
    capture.drawings = reserve(capture.drawings, &capture.draw_capacity, capture.draw_count + 1, sizeof *capture.drawings);
    int ordinal = 0;
    for (size_t i = 0; i < capture.draw_count; i++)
        if (capture.drawings[i].caller == caller && capture.drawings[i].kind == kind && capture.drawings[i].object == capture_object)
            ordinal++;
    capture.drawings[capture.draw_count++] = (struct Drawing){
        .caller = caller, .object = capture_object, .kind = kind, .ordinal = ordinal,
        .x = x, .y = y, .bottom = capture_bottom, .first = capture.write_count
    };
    drawing = 1;
}

int presentation_recording(void)
{
    return enabled && capture_screen && !overlay;
}

uintptr_t presentation_object(const void *object)
{
    uintptr_t previous_object = capture_object;
    capture_object = (uintptr_t)object;
    return previous_object;
}

int presentation_bottom_clip(int bottom)
{
    int previous_bottom = capture_bottom;
    capture_bottom = bottom;
    return previous_bottom;
}

void presentation_draw_end(void)
{
    if (!overlay)
        drawing = 0;
}

/* The HUD shares the bitmap with the level: its transparent pixels still
 * contain scrolling scenery.  Capture only its actual blit masks as a fixed
 * drawing, so that scenery underneath can use the same camera as every other
 * row.  Calls to glyph_draw inside this scope belong to the fixed drawing. */
void presentation_overlay_begin(void)
{
    if (!presentation_recording())
        return;
    presentation_draw_begin(0, -1, 0, 0);
    capture.drawings[capture.draw_count - 1].fixed = 1;
    overlay = 1;
}

void presentation_overlay_end(void)
{
    if (overlay) {
        overlay = 0;
        presentation_draw_end();
    }
}

void presentation_write(uint16_t *destination, uint16_t mask, uint16_t value)
{
    if (!enabled || !capture_screen || !drawing || !mask)
        return;
    uintptr_t offset = (uintptr_t)destination - (uintptr_t)capture_screen;
    if (offset >= BYTES || (offset & 1))
        return;
    capture.writes = reserve(capture.writes, &capture.write_capacity, capture.write_count + 1, sizeof *capture.writes);
    capture.writes[capture.write_count++] = (struct Write){ offset, mask, *destination, value };
    capture.drawings[capture.draw_count - 1].count++;
}

void presentation_publish(int slot, const uint8_t *screen, int fine, uint16_t tick)
{
    if (!enabled)
        return;
    pthread_mutex_lock(&publication);
    if (capture_screen != screen) {
        published[slot].serial = 0;
    } else {
        memcpy(capture.background, screen, BYTES);
        /* Undo the captured blits in reverse order, including opaque pixels
         * which happened to have the same colour as the old background. */
        for (size_t i = capture.write_count; i; i--) {
            const struct Write *w = &capture.writes[i - 1];
            uint16_t *to = (uint16_t *)(capture.background + w->offset);
            *to = (*to & ~w->mask) | (w->before & w->mask);
        }
        for (size_t i = 0; i < capture.draw_count; i++) {
            struct Drawing *d = &capture.drawings[i];
            d->group_count = 0;
            for (size_t j = 0; j < capture.draw_count; j++)
                if (capture.drawings[j].caller == d->caller && capture.drawings[j].kind == d->kind && capture.drawings[j].object == d->object)
                    d->group_count++;
        }
        capture.fine = fine & 31;
        capture.tick = tick;
        capture.serial = ++serial;
        copy_frame(&published[slot], &capture);
    }
    pthread_mutex_unlock(&publication);
    capture_screen = 0;
    drawing = overlay = 0;
}

static int pixel(const struct Frame *frame, int x, int y)
{
    int index = 0, at = y * ROW + (x >> 4) * 2, bit = 15 - (x & 15);
    for (int plane = 0; plane < 4; plane++)
        index |= ((*(const uint16_t *)(frame->background + at + plane * PLANE) >> bit) & 1) << plane;
    return index;
}

static uint32_t argb(uint16_t color)
{
    return 0xff000000u | ((color >> 8 & 15) * 17 << 16) |
           ((color >> 4 & 15) * 17 << 8) | ((color & 15) * 17);
}

void presentation_render(uint32_t *pixels, const uint16_t colors[][16], int slot, uint64_t now)
{
    if (!enabled)
        return;
    pthread_mutex_lock(&publication);
    const struct Frame *shown = &published[slot];
    if (!shown->serial) {
        current.serial = previous.serial = 0;
    } else if (shown->serial != current.serial) {
        struct Frame swap = previous;
        previous = current;
        current = swap;
        copy_frame(&current, shown);
        shown_at = now;
    }
    pthread_mutex_unlock(&publication);
    unsigned ticks = (uint16_t)(current.tick - previous.tick);
    /* A missing capture, scene change, long stall or teleport starts with a
     * complete frame.  Never interpolate across unrelated scenes. */
    if (!current.serial || !previous.serial || current.serial != previous.serial + 1 ||
        current.scene != previous.scene || !ticks || ticks > 5 ||
        abs(current.x - previous.x) >= VIEW_WIDTH || abs(current.y - previous.y) >= VIEW_HEIGHT)
        return;
    double alpha = (double)(now - shown_at) / (ticks * 20000000.0);
    if (alpha >= 1.0)
        return;
    int camera_x = (int)lround((previous.x - current.x) * (1.0 - alpha));
    int camera_y = (int)lround((previous.y - current.y) * (1.0 - alpha));
    uint8_t indices[VIEW_WIDTH * VIEW_HEIGHT];
    for (int y = 0; y < VIEW_HEIGHT; y++) {
        for (int x = 0; x < VIEW_WIDTH; x++) {
            int sx = x + current.fine + camera_x, sy = y + camera_y;
            const struct Frame *source = &current;
            if (sx < 0 || sx >= WIDTH || sy < 0 || sy >= HEIGHT) {
                sx += current.x - previous.x + previous.fine - current.fine;
                sy += current.y - previous.y;
                source = &previous;
            }
            indices[y * VIEW_WIDTH + x] = sx >= 0 && sx < WIDTH && sy >= 0 && sy < HEIGHT ? pixel(source, sx, sy) : 0;
        }
    }
    for (size_t i = 0; i < current.draw_count; i++) {
        const struct Drawing *d = &current.drawings[i], *old = 0;
        for (size_t j = 0; !d->fixed && j < previous.draw_count; j++) {
            const struct Drawing *candidate = &previous.drawings[j];
            if (candidate->caller == d->caller && candidate->object == d->object && candidate->kind == d->kind && candidate->ordinal == d->ordinal &&
                candidate->group_count == d->group_count) {
                old = candidate;
                break;
            }
        }
        /* Without an old pose, retain the drawing's current world position.
         * It must still follow the interpolated camera: leaving it at the
         * current screen position separates foreground masks from scenery. */
        int dx = d->fixed ? 0 : old ? (int)lround((old->x - d->x) * (1.0 - alpha)) : -camera_x;
        int dy = d->fixed ? 0 : old ? (int)lround((old->y - d->y) * (1.0 - alpha)) : -camera_y;
        const struct Drawing *source = d;
        const struct Frame *frame = &current;
        int bottom = VIEW_HEIGHT - 1;
        if (d->bottom != INT_MAX) {
            bottom = d->bottom - camera_y;
            if (old && old->bottom != INT_MAX) {
                bottom = d->bottom + (int)lround((old->bottom - d->bottom) * (1.0 - alpha));
                /* $49F8 reveals a variable-height spike stem above its base.
                 * On retraction, keep the longer recorded stem and crop it
                 * at the interpolated base; translating the shorter pose
                 * would leave a gap above the base. */
                if (old->bottom - old->y > d->bottom - d->y) {
                    source = old;
                    frame = &previous;
                    dx += d->x - old->x;
                    dy += d->y - old->y;
                }
            }
        }
        for (size_t j = source->first; j < source->first + source->count; j++) {
            const struct Write *w = &frame->writes[j];
            int plane = w->offset / PLANE, at = w->offset % PLANE;
            int y = at / ROW + dy, x = (at % ROW) * 8 - frame->fine + dx;
            if (y < 0 || y >= VIEW_HEIGHT || y > bottom)
                continue;
            for (int bit = 15; bit >= 0; bit--, x++) {
                if (x >= 0 && x < VIEW_WIDTH && (w->mask & (1u << bit))) {
                    uint8_t *to = &indices[y * VIEW_WIDTH + x];
                    *to = (*to & ~(1 << plane)) | (((w->value >> bit) & 1) << plane);
                }
            }
        }
    }
    for (int y = 0; y < VIEW_HEIGHT; y++)
        for (int x = 0; x < VIEW_WIDTH; x++)
            pixels[(y + TOP) * VIEW_WIDTH + x] = argb(colors[y + 60][indices[y * VIEW_WIDTH + x]]);
}
