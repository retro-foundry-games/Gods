/* Exercise published rasters and the original planar image/tile formats.
 * Guest memory is compared before/after presentation; no gameplay runs here. */
#include "presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ROW = 44, PLANE = ROW * 192, BYTES = PLANE * 4 };
static uint16_t screens[2][BYTES / 2], colors[320][16], tiles[256 * 256 / 2];
static uint8_t map[8192];
static uint32_t original[320 * 256], wide[WIDE_WIDTH * WIDE_HEIGHT];
static const uint64_t clock_start = 1000000000ull;

static void setup(void)
{
    memset(screens, 0, sizeof screens);
    memset(tiles, 0, sizeof tiles);
    memset(map, 0, sizeof map);
    for (int i = 0; i < 32; i++)
        tiles[i] = 0xffff; /* tile zero is red */
    for (int y = 0; y < 320; y++) {
        colors[y][1] = 0xf00;
        colors[y][2] = 0x0f0;
        colors[y][3] = 0x00f;
    }
    presentation_widescreen(1);
    presentation_enable(1);
}

static void begin(int slot, int x, int y, int frozen)
{
    memset(screens[slot], 0, BYTES);
    for (int i = 0; i < PLANE / 2; i++)
        screens[slot][i] = 0xffff;
    presentation_begin((uint8_t *)screens[slot], x, y, 1);
    presentation_world(map, (const uint8_t *)tiles, frozen);
}

static void paint(int slot, int x, int y, int fine, int index)
{
    int bx = x + fine;
    if (bx < 0 || bx >= 352 || y < 0 || y >= 192)
        return;
    uint16_t mask = 1u << (15 - (bx & 15));
    for (int p = 0; p < 4; p++) {
        uint16_t *to = screens[slot] + (p * PLANE + y * ROW) / 2 + bx / 16;
        uint16_t value = (*to & ~mask) | ((index >> p & 1) ? mask : 0);
        presentation_write(to, mask, value);
        *to = value;
    }
}

static void image(int slot, int x, int y, int height, int fine, int index)
{
    uint16_t source[4 * 32] = {0};
    for (int p = 0; p < 4; p++)
        for (int r = 0; r < height; r++)
            source[p * 32 + r] = (index >> p & 1) ? 0xffff : 0;
    presentation_draw_begin(1, 0, x, y);
    presentation_image(x, y, 16, height, (const uint8_t *)source, 64, fine, 191, 0, 0);
    for (int row = 0; row < height; row++)
        for (int col = 0; col < 16; col++)
            paint(slot, x + col, y + row, fine, index);
    presentation_draw_end();
}

static void publish(int slot, int fine, int tick)
{
    presentation_publish(slot, (const uint8_t *)screens[slot], fine, tick);
}

static void show(int slot, int fine, uint64_t now)
{
    uint16_t before[2][BYTES / 2];
    memcpy(before, screens, sizeof before);
    for (int i = 0; i < 320 * 256; i++)
        original[i] = 0xff000000;
    for (int y = 0; y < 191; y++)
        for (int x = 0; x < 320; x++) {
            int bx = x + fine, index = 0;
            for (int p = 0; p < 4; p++)
                index |= ((screens[slot][(p * PLANE + y * ROW) / 2 + bx / 16] >> (15 - (bx & 15))) & 1) << p;
            static const uint32_t argb[] = { 0xff000000, 0xffff0000, 0xff00ff00, 0xff0000ff };
            assert(index < 4);
            original[(y + 16) * 320 + x] = argb[index];
        }
    presentation_render_wide(wide, original, colors, slot, now);
    assert(!memcmp(before, screens, sizeof before));
}

static uint32_t at(int x, int y)
{
    return wide[(y + 16 + WIDE_TOP) * WIDE_WIDTH + x];
}

static void scenery_and_level_limits(void)
{
    setup();
    const int cameras[] = { 0, 128, 3776 };
    const int origins[] = { 0, 40, 80 };
    for (int i = 0; i < 3; i++) {
        begin(0, cameras[i], 832, 0);
        image(0, 100, 30, 8, 0, 2);
        publish(0, 0, 100 + i * 20);
        /* Publication owns its tiles, including the last map column. */
        memset(tiles, 0, sizeof tiles);
        show(0, 0, 0);
        assert(at(0, 100) == 0xffff0000);
        assert(at(399, 100) == 0xffff0000);
        assert(at(origins[i] + 100, 30) == 0xff00ff00);
        for (int j = 0; j < 32; j++) tiles[j] = 0xffff;
    }
    /* The fixed HUD stays centred while the world view clamps at either end. */
    begin(1, 0, 832, 0);
    presentation_overlay_begin();
    paint(1, 8, 180, 0, 3);
    presentation_overlay_end();
    publish(1, 0, 180);
    show(1, 0, 0);
    assert(at(WIDE_LEFT + 8, 180) == 0xff0000ff);
    /* Menus and other missing publications keep every original pixel. */
    presentation_begin(0, 0, 0, 0);
    publish(0, 0, 200);
    show(0, 0, 0);
    for (int y = 0; y < 256; y++)
        assert(!memcmp(wide + (y + WIDE_TOP) * WIDE_WIDTH + WIDE_LEFT,
                       original + y * 320, 320 * sizeof *wide));
}

static void edge_masks_and_scroll_wrap(void)
{
    setup();
    for (int slot = 0; slot < 2; slot++) {
        int camera = slot ? 132 : 124, fine = camera & 31;
        begin(slot, camera, 832, 0);
        image(slot, 100 - camera, 20, 16, fine, 2);
        /* A later foreground image masks an object in the new left margin. */
        presentation_draw_begin(2, 0, 100 - camera, 24);
        uint16_t source[4 * 32] = {0};
        for (int r = 0; r < 8; r++) {
            source[r] = 0x7fff;
            source[32 + r] = 0x7fff;
        }
        presentation_image(100 - camera, 24, 16, 8, (const uint8_t *)source, 64, fine, 191, 0, 0);
        for (int y = 24; y < 32; y++)
            for (int x = 101 - camera; x < 116 - camera; x++)
                paint(slot, x, y, fine, 3);
        presentation_draw_end();
        publish(slot, fine, 100 + slot * 2);
        show(slot, fine, clock_start + slot * 40000000ull);
    }
    show(1, 4, clock_start + 60000000ull);
    assert(at(12, 20) == 0xff00ff00); /* stationary world object, camera halfway */
    assert(at(12, 25) == 0xff00ff00); /* transparent foreground pixel */
    assert(at(13, 25) == 0xff0000ff); /* opaque foreground covers it */
    assert(at(11, 20) == 0xffff0000);
}

static void spike_edges_and_frozen_background(void)
{
    setup();
    for (int slot = 0; slot < 2; slot++) {
        begin(slot, 128, 832, slot);
        int top = slot ? 14 : 10;
        int saved = presentation_bottom_clip(29);
        image(slot, -20, top, 30 - top, 0, 2);
        presentation_bottom_clip(saved);
        publish(slot, 0, 100 + slot * 2);
        show(slot, 0, clock_start + slot * 40000000ull);
        memset(tiles, 0, sizeof tiles);
    }
    show(1, 0, clock_start + 60000000ull);
    assert(at(20, 12) == 0xff00ff00);
    assert(at(20, 29) == 0xff00ff00);
    assert(at(20, 30) == 0xffff0000); /* stem cannot cross the fixed scenery base */
    assert(at(0, 100) == 0xffff0000); /* cached backdrop survives a frozen map */
}

static void clipped_edges_and_opaque_zero(void)
{
    setup();
    begin(0, 128, 832, 0);
    uint16_t source[4 * 32] = {0};
    for (int i = 0; i < 32; i++) source[32 + i] = 0xffff;
    const int16_t clip[] = { 0, 351, 2, 5 };
    presentation_draw_begin(2, 2, -20, -3);
    presentation_image(-20, -3, 16, 12, (const uint8_t *)source, 64, 0, 191, clip, 0);
    presentation_draw_end();
    memset(source, 0, sizeof source);
    presentation_draw_begin(3, 2, -20, 3);
    presentation_image(-20, 3, 16, 1, (const uint8_t *)source, 64, 0, 191, clip, 1);
    presentation_draw_end();
    publish(0, 0, 100);
    show(0, 0, 0);
    assert(at(20, 1) == 0xffff0000);
    assert(at(20, 2) == 0xff00ff00);
    assert(at(20, 3) == 0xff000000); /* plain blit writes colour zero too */
    assert(at(20, 5) == 0xff00ff00);
    assert(at(20, 6) == 0xffff0000);
}

int main(void)
{
    assert(WIDE_WIDTH * 9 * 6 == WIDE_HEIGHT * 5 * 16);
    scenery_and_level_limits();
    edge_masks_and_scroll_wrap();
    spike_edges_and_frozen_background();
    clipped_edges_and_opaque_zero();
    puts("widescreen: 16:9, immutable scenery, level limits, masks, HUD, scroll wrap, spike bases and frozen backgrounds passed");
    return 0;
}
