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

struct EdgePixel {
    int16_t x, y;
    uint8_t index;
};

struct Drawing {
    uintptr_t caller, object;
    int kind, ordinal, group_count, x, y, fixed, bottom;
    size_t first, count;
    size_t edge_first, edge_count;
};

struct Frame {
    uint8_t background[BYTES];
    struct Write *writes;
    struct Drawing *drawings;
    struct EdgePixel *edges;
    size_t write_count, write_capacity, draw_count, draw_capacity;
    size_t edge_count, edge_capacity;
    uint8_t world[WIDE_WIDTH * HEIGHT];
    int world_valid, world_x;
    uint64_t serial;
    uintptr_t scene;
    int x, y, fine;
    uint16_t tick;
};

static int enabled, widescreen, drawing, overlay;
static uint8_t *capture_screen;
static uintptr_t capture_object;
static int capture_bottom = INT_MAX;
static struct Frame capture, published[2], previous, current;
static uintptr_t cached_scene;
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
    to->edges = reserve(to->edges, &to->edge_capacity, from->edge_count, sizeof *to->edges);
    memcpy(to->background, from->background, BYTES);
    if (from->write_count)
        memcpy(to->writes, from->writes, from->write_count * sizeof *to->writes);
    if (from->draw_count)
        memcpy(to->drawings, from->drawings, from->draw_count * sizeof *to->drawings);
    if (from->edge_count)
        memcpy(to->edges, from->edges, from->edge_count * sizeof *to->edges);
    to->edge_count = from->edge_count;
    to->world_valid = from->world_valid;
    to->world_x = from->world_x;
    if (from->world_valid)
        memcpy(to->world, from->world, sizeof to->world);
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
    cached_scene = 0;
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

void presentation_widescreen(int on)
{
    widescreen = on;
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
    capture.draw_count = capture.write_count = capture.edge_count = 0;
    capture.world_valid = 0;
    capture.x = x;
    capture.y = y;
    capture.scene = scene;
}

/* $178C6 / $17972: the same 128x64 map of 32x16 tiles and four native-word
 * planes (64 bytes each). Read it on the game thread, before moving paths
 * restore/mark cells. Extra drawing never changes the map or guest memory. */
void presentation_world(const uint8_t *map, const uint8_t *tiles, int frozen)
{
    if (!enabled || !widescreen || !capture_screen)
        return;
    if (!frozen || cached_scene != capture.scene) {
        capture.world_x = capture.x - WIDE_LEFT;
        if (capture.world_x < 0)
            capture.world_x = 0;
        if (capture.world_x > 4096 - WIDE_WIDTH)
            capture.world_x = 4096 - WIDE_WIDTH;
        for (int y = 0; y < HEIGHT; y++) {
            int wy = capture.y + y;
            for (int x = 0; x < WIDE_WIDTH; x++) {
                int wx = capture.world_x + x, index = 0;
                if (wy >= 0 && wy < 1024) {
                    const uint8_t *tile = tiles + map[(wy / 16) * 128 + wx / 32] * 256;
                    int at = (wy & 15) * 4 + (wx & 31) / 16 * 2, bit = 15 - (wx & 15);
                    for (int p = 0; p < 4; p++)
                        index |= ((*(const uint16_t *)(tile + p * 64 + at) >> bit) & 1) << p;
                }
                capture.world[y * WIDE_WIDTH + x] = (uint8_t)index;
            }
        }
        cached_scene = capture.scene;
    }
    capture.world_valid = 1;
}

/* Record just the part of a real image blit beyond the hardware bitmap.
 * On-screen pixels still come from presentation_write, including its exact
 * masks and clipping. Never call an update or a draw routine a second time. */
void presentation_image(int x, int y, int width, int height, const uint8_t *source,
                        int plane_bytes, int fine, int last_row, const int16_t *clip, int opaque)
{
    if (!enabled || !widescreen || !capture_screen || !drawing || overlay)
        return;
    int left = clip ? clip[0] : 0, right = clip ? clip[1] : WIDTH - 1;
    int top = clip ? clip[2] : 0, bottom = clip ? clip[3] : last_row;
    if (left == 0)
        left = -WIDE_LEFT * 2;
    if (right == WIDTH - 1)
        right = VIEW_WIDTH + WIDE_LEFT * 2 + 31;
    if (x + width <= -WIDE_LEFT * 2 || x >= VIEW_WIDTH + WIDE_LEFT * 2)
        return;
    for (int row = 0; row < height; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= HEIGHT || dy < top || dy > bottom || dy > last_row)
            continue;
        for (int col = 0; col < width; col++) {
            int dx = x + col, bx = dx + fine;
            if ((bx >= 0 && bx < WIDTH) || bx < left || bx > right ||
                dx < -WIDE_LEFT * 2 || dx >= VIEW_WIDTH + WIDE_LEFT * 2)
                continue;
            int at = row * (width / 8) + col / 16 * 2, bit = 15 - (col & 15), index = 0;
            for (int p = 0; p < 4; p++)
                index |= ((*(const uint16_t *)(source + p * plane_bytes + at) >> bit) & 1) << p;
            if (!index && !opaque)
                continue;
            capture.edges = reserve(capture.edges, &capture.edge_capacity, capture.edge_count + 1, sizeof *capture.edges);
            capture.edges[capture.edge_count++] = (struct EdgePixel){ dx, dy, index };
            capture.drawings[capture.draw_count - 1].edge_count++;
        }
    }
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
        .x = x, .y = y, .bottom = capture_bottom, .first = capture.write_count,
        .edge_first = capture.edge_count
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

static double advance_frame(int slot, uint64_t now)
{
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
        return 1.0;
    if (!now)
        return 1.0;
    double alpha = (double)(now - shown_at) / (ticks * 20000000.0);
    return alpha < 1.0 ? alpha : 1.0;
}

static int world_pixel(const struct Frame *frame, int wx, int wy, int wide)
{
    int x = wx - frame->x + frame->fine, y = wy - frame->y;
    if (y < 0 || y >= HEIGHT)
        return -1;
    if (x >= 0 && x < WIDTH)
        return pixel(frame, x, y);
    x = wx - frame->world_x;
    return wide && frame->world_valid && x >= 0 && x < WIDE_WIDTH ? frame->world[y * WIDE_WIDTH + x] : -1;
}

static void compose(uint8_t *indices, int width, int wide, double alpha)
{
    int camera_x = (int)lround((previous.x - current.x) * (1.0 - alpha));
    int camera_y = (int)lround((previous.y - current.y) * (1.0 - alpha));
    int view_x = wide ? current.world_x + (int)lround((previous.world_x - current.world_x) * (1.0 - alpha)) : current.x + camera_x;
    int origin = current.x + camera_x - view_x;
    for (int y = 0; y < VIEW_HEIGHT; y++) {
        for (int x = 0; x < width; x++) {
            int index = world_pixel(&current, x + view_x, y + current.y + camera_y, wide);
            if (index < 0 && alpha < 1.0)
                index = world_pixel(&previous, x + view_x, y + current.y + camera_y, wide);
            indices[y * width + x] = index >= 0 ? index : 0;
        }
    }
    for (size_t i = 0; i < current.draw_count; i++) {
        const struct Drawing *d = &current.drawings[i], *old = 0;
        for (size_t j = 0; alpha < 1.0 && !d->fixed && j < previous.draw_count; j++) {
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
        int offset = d->fixed ? (width - VIEW_WIDTH) / 2 : origin;
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
            int y = at / ROW + dy, x = (at % ROW) * 8 - frame->fine + dx + offset;
            if (y < 0 || y >= VIEW_HEIGHT || y > bottom)
                continue;
            for (int bit = 15; bit >= 0; bit--, x++) {
                if (x >= 0 && x < width && (w->mask & (1u << bit))) {
                    uint8_t *to = &indices[y * width + x];
                    *to = (*to & ~(1 << plane)) | (((w->value >> bit) & 1) << plane);
                }
            }
        }
        if (wide)
            for (size_t j = source->edge_first; j < source->edge_first + source->edge_count; j++) {
                const struct EdgePixel *p = &frame->edges[j];
                int x = p->x + dx + offset, y = p->y + dy;
                if (x >= 0 && x < width && y >= 0 && y < VIEW_HEIGHT && y <= bottom)
                    indices[y * width + x] = p->index;
            }
    }
}

void presentation_render(uint32_t *pixels, const uint16_t colors[][16], int slot, uint64_t now)
{
    if (!enabled)
        return;
    double alpha = advance_frame(slot, now);
    if (alpha >= 1.0)
        return;
    uint8_t indices[VIEW_WIDTH * VIEW_HEIGHT];
    compose(indices, VIEW_WIDTH, 0, alpha);
    for (int y = 0; y < VIEW_HEIGHT; y++)
        for (int x = 0; x < VIEW_WIDTH; x++)
            pixels[(y + TOP) * VIEW_WIDTH + x] = argb(colors[y + 60][indices[y * VIEW_WIDTH + x]]);
}

void presentation_fit_wide(uint32_t *wide, const uint32_t *original)
{
    for (int i = 0; i < WIDE_WIDTH * WIDE_HEIGHT; i++)
        wide[i] = original[0];
    for (int y = 0; y < 256; y++)
        memcpy(wide + (y + WIDE_TOP) * WIDE_WIDTH + WIDE_LEFT,
               original + y * VIEW_WIDTH, VIEW_WIDTH * sizeof *wide);
}

void presentation_render_wide(uint32_t *wide, const uint32_t *original,
                              const uint16_t colors[][16], int slot, uint64_t now)
{
    presentation_fit_wide(wide, original);
    if (!enabled || !widescreen)
        return;
    double alpha = advance_frame(slot, now);
    if (!current.serial || !current.world_valid)
        return; /* Intro, menus, shops and result screens retain their full original image. */
    uint8_t indices[WIDE_WIDTH * VIEW_HEIGHT];
    compose(indices, WIDE_WIDTH, 1, alpha);
    for (int y = 0; y < VIEW_HEIGHT; y++)
        for (int x = 0; x < WIDE_WIDTH; x++)
            wide[(y + TOP + WIDE_TOP) * WIDE_WIDTH + x] = argb(colors[y + 60][indices[y * WIDE_WIDTH + x]]);
    static const uint16_t band[7] = { 0x030, 0x042, 0x060, 0x082, 0x060, 0x042, 0x030 };
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < WIDE_LEFT; x++) {
            wide[(209 + y + WIDE_TOP) * WIDE_WIDTH + x] = argb(band[y]);
            wide[(209 + y + WIDE_TOP) * WIDE_WIDTH + WIDE_WIDTH - x - 1] = argb(band[y]);
        }
}
