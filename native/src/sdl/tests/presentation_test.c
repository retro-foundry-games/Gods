/* Exercise the real presentation recorder/compositor with deterministic planar
 * drawings.  No original disk data or gameplay state is needed. */
#include "presentation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { ROW = 44, PLANE = ROW * 192, BYTES = PLANE * 4, TOP = 16 };
static uint16_t screens[2][BYTES / 2];
static uint16_t colors[320][16];
static uint32_t pixels[320 * 256];
static const uint32_t sentinel = 0x12345678;
static uint64_t now = 1000000000ull;

static void clear_output(void)
{
    for (int i = 0; i < 320 * 256; i++)
        pixels[i] = sentinel;
}

static void show(int slot, uint64_t elapsed)
{
    uint16_t before[2][BYTES / 2];
    memcpy(before, screens, sizeof before);
    clear_output();
    presentation_render(pixels, colors, slot, now + elapsed);
    assert(!memcmp(before, screens, sizeof before));
}

static uint32_t at(int x, int y)
{
    return pixels[(y + TOP) * 320 + x];
}

static void begin(int slot, int camera_x, int camera_y, uintptr_t scene)
{
    memset(screens[slot], 0, sizeof screens[slot]);
    presentation_begin((uint8_t *)screens[slot], camera_x, camera_y, scene);
}

static void paint_pixel(int slot, int x, int y, int fine, unsigned index)
{
    int bx = x + fine;
    uint16_t mask = 1u << (15 - (bx & 15));
    for (int plane = 0; plane < 4; plane++) {
        uint16_t *to = screens[slot] + (plane * PLANE + y * ROW) / 2 + (bx >> 4);
        uint16_t value = (*to & ~mask) | ((index >> plane & 1) ? mask : 0);
        presentation_write(to, mask, value);
        *to = value;
    }
}

static void dot(int slot, uintptr_t caller, int x, int y, int fine, unsigned index)
{
    presentation_draw_begin(caller, 1, x, y);
    paint_pixel(slot, x, y, fine, index);
    presentation_draw_end();
}

static void publish(int slot, int fine, uint16_t tick)
{
    presentation_publish(slot, (uint8_t *)screens[slot], fine, tick);
}

static void motion_and_timing(void)
{
    presentation_enable(1);
    begin(0, 0, 0, 1);
    dot(0, 1, 20, 30, 0, 1);
    publish(0, 0, 100);
    show(0, 0);
    assert(at(20, 30) == sentinel); /* one frame has no history */
    begin(1, 0, 0, 1);
    dot(1, 1, 28, 34, 0, 1);
    publish(1, 0, 102);
    show(1, 0);
    assert(at(20, 30) == 0xffff0000);
    assert(at(28, 34) == 0xff000000);
    show(1, 10000000);
    assert(at(22, 31) == 0xffff0000);
    show(1, 20000000);
    assert(at(24, 32) == 0xffff0000);
    assert(at(20, 30) == 0xff000000);
    assert(at(28, 34) == 0xff000000); /* no double image */
    assert(at(24, 191) == sentinel); /* final bitmap row is outside the display */
    assert(pixels[0] == sentinel);   /* border untouched */
    show(1, 39999999);
    assert(at(28, 34) == 0xffff0000);
    show(1, 40000000);
    assert(at(28, 34) == sentinel); /* exact current bitmap already rendered */
    presentation_enable(0);
    show(1, 20000000);
    assert(at(24, 32) == sentinel);
}

static void mask_and_order(void)
{
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        begin(slot, 0, 0, 1);
        /* One opaque red pixel and one opaque black pixel, both over black.
         * Black must still overwrite the red drawing after interpolation. */
        dot(slot, 1, 20 + slot * 8, 30, 0, 1);
        dot(slot, 2, 20 + slot * 8, 30, 0, 0);
        publish(slot, 0, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(24, 30) == 0xff000000);
    assert(at(25, 30) == 0xff000000);
    assert(at(20, 30) == 0xff000000);
    presentation_reset();
    for (int slot = 0; slot < 2; slot++) {
        begin(slot, 0, 0, 1);
        screens[slot][30 * ROW / 2 + 1] = 1u << 7; /* red background at x=24 */
        dot(slot, 1, 20 + slot * 8, 30, 0, 0); /* black on black, still opaque */
        publish(slot, 0, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(24, 30) == 0xff000000);
}

static void reverse_motion_and_variable_cadence(void)
{
    presentation_enable(1);
    begin(0, 0, 0, 1);
    dot(0, 1, 28, 34, 0, 1);
    publish(0, 0, 100);
    show(0, 0);
    begin(1, 0, 0, 1);
    dot(1, 1, 16, 28, 0, 1);
    publish(1, 0, 103); /* 60 ms between drawings */
    show(1, 0);
    show(1, 30000000);
    assert(at(22, 31) == 0xffff0000);
    show(1, 45000000);
    assert(at(19, 30) == 0xffff0000); /* round to the nearest source pixel */
}

static void camera_and_scroll_wrap(void)
{
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        int camera = slot ? 34 : 30, fine = camera & 31;
        begin(slot, camera, 0, 1);
        /* A world-space background marker at x=60.  Its buffer position jumps
         * by 32 when the fine scroll wraps. */
        int bx = 60 - camera + fine;
        screens[slot][10 * ROW / 2 + (bx >> 4)] = 1u << (15 - (bx & 15));
        dot(slot, 1, 80 - camera, 20, fine, 1);
        publish(slot, fine, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(28, 10) == 0xffff0000);
    assert(at(48, 20) == 0xffff0000);
    assert(at(26, 10) == 0xff000000);
    assert(at(50, 20) == 0xff000000);
}

static void lower_playfield_scrolling(void)
{
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        int camera = slot ? 34 : 30, fine = camera & 31;
        begin(slot, camera, 0, 1);
        int bx = 60 - camera + fine;
        for (int y = 0; y < 192; y++)
            screens[slot][y * ROW / 2 + (bx >> 4)] = 1u << (15 - (bx & 15));
        publish(slot, fine, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    /* Scenery extends behind the HUD to the bottom of the bitmap.  Both
     * sides of the old row-160 boundary must use the same camera position. */
    assert(at(28, 159) == 0xffff0000);
    assert(at(28, 160) == 0xffff0000);
    assert(at(28, 183) == 0xffff0000);
    assert(at(28, 190) == 0xffff0000);
}

static void fixed_hud_over_scrolling_scenery(void)
{
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        int camera = slot ? 34 : 30, fine = camera & 31;
        begin(slot, camera, slot ? 6 : 2, 1);
        int bx = 60 - camera + fine;
        for (int y = 0; y < 192; y++)
            screens[slot][y * ROW / 2 + (bx >> 4)] = 1u << (15 - (bx & 15));
        /* A horizontal floor marker scrolls vertically through the lower
         * playfield, while a moving sprite crosses row 160. */
        int floor = 188 - (slot ? 6 : 2);
        for (int word = 0; word < ROW / 2; word++)
            screens[slot][(PLANE + floor * ROW) / 2 + word] = 0xffff;
        dot(slot, 1, 100, slot ? 166 : 158, fine, 1);
        presentation_overlay_begin();
        assert(!presentation_recording());
        dot(slot, 2, 28, 170, fine, 3); /* nested glyph of the fixed HUD */
        dot(slot, 3, 250, 160, fine, 3);
        presentation_overlay_end();
        assert(presentation_recording());
        publish(slot, fine, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(28, 159) == 0xffff0000);
    assert(at(28, 160) == 0xffff0000);
    assert(at(28, 169) == 0xffff0000);
    assert(at(28, 170) == 0xff0000ff); /* HUD remains fixed and opaque */
    assert(at(28, 171) == 0xffff0000);
    assert(at(250, 160) == 0xff0000ff);
    assert(at(100, 162) == 0xffff0000); /* entire sprite interpolates below 160 */
    assert(at(100, 184) == 0xff00ff00); /* continuous vertical floor scrolling */
    assert(at(30, 170) == 0xff000000); /* no HUD copied into the background */
    assert(at(28, 191) == sentinel);
    assert(pixels[209 * 320] == sentinel); /* message band untouched */
}

static void batches_and_discontinuities(void)
{
    presentation_enable(1);
    begin(0, 0, 0, 1);
    dot(0, 1, 20, 30, 0, 1);
    publish(0, 0, 65534);
    show(0, 0);
    begin(1, 0, 0, 1);
    dot(1, 1, 28, 30, 0, 1);
    dot(1, 1, 70, 30, 0, 1);
    publish(1, 0, 0); /* 16-bit machine counter wraps */
    show(1, 0);
    show(1, 20000000);
    assert(at(28, 30) == 0xffff0000); /* changed batch snaps */
    assert(at(70, 30) == 0xffff0000);
    presentation_reset();
    begin(0, 0, 0, 1);
    dot(0, 1, 100, 30, 0, 1);
    publish(0, 0, 2);
    show(0, 0);
    assert(at(100, 30) == sentinel); /* reset even if no empty frame was shown */
    begin(1, 0, 0, 2);
    dot(1, 1, 104, 30, 0, 1);
    publish(1, 0, 4);
    show(1, 0);
    assert(at(104, 30) == sentinel); /* new scene */
    begin(0, 1000, 0, 2);
    dot(0, 1, 108, 30, 8, 1);
    publish(0, 8, 6);
    show(0, 0);
    assert(at(108, 30) == sentinel); /* teleport */
    presentation_discard_history();
    show(0, 0);
    assert(at(108, 30) == sentinel);
    presentation_begin(0, 0, 0, 0);
    publish(1, 0, 8);
    show(1, 0);
    assert(at(108, 30) == sentinel); /* menu/non-game frame */
}

static void foreground_occlusion_when_batch_changes(void)
{
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        int camera = slot ? 34 : 30, fine = camera & 31;
        begin(slot, camera, slot ? 6 : 2, 1);
        /* The moving drawing stays at the same screen position.  Scenery
         * drawn after it is stationary in world coordinates, so it should
         * cover the moving drawing halfway through the camera pan. */
        dot(slot, 1, 28, 26, fine, 1);
        dot(slot, 2, slot ? 26 : 30, slot ? 24 : 28, fine, 2);
        if (slot)
            dot(slot, 2, 80, 80, fine, 2); /* another scenery draw enters view */
        publish(slot, fine, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(28, 26) == 0xff00ff00); /* foreground still covers the actor */
    assert(at(26, 24) == 0xff000000);
    assert(at(82, 82) == 0xff00ff00); /* new drawings follow the camera too */
}

static void persistent_object_matching_and_draw_order(void)
{
    static const int actor_a = 1, actor_b = 2, foreground = 3;
    presentation_enable(1);
    for (int slot = 0; slot < 2; slot++) {
        begin(slot, 0, 0, 1);
        /* The same draw site is shared by two records whose draw order
         * changes. Match each to its own old position, never its old ordinal. */
        presentation_object(slot ? &actor_b : &actor_a);
        dot(slot, 1, slot ? 104 : 10, 30, 0, slot ? 2 : 1);
        presentation_object(slot ? &actor_a : &actor_b);
        dot(slot, 1, slot ? 14 : 100, 30, 0, slot ? 1 : 2);
        presentation_object(&foreground);
        dot(slot, 2, 12, 30, 0, 3); /* scenery is drawn after the actors */
        presentation_object(0);
        publish(slot, 0, 100 + slot * 2);
        show(slot, 0);
    }
    show(1, 20000000);
    assert(at(12, 30) == 0xff0000ff); /* actor A remains behind the scenery */
    assert(at(102, 30) == 0xff00ff00); /* actor B follows its own motion */
    assert(at(57, 30) == 0xff000000); /* no interpolation between unrelated records */
}

static void spike_stem_preserves_fixed_base(void)
{
    for (int retracting = 0; retracting < 2; retracting++) {
        presentation_enable(1);
        for (int slot = 0; slot < 2; slot++) {
            int top = slot == retracting ? 150 : 148;
            begin(slot, 0, 0, 1);
            /* The grey/green sill beneath the trap is part of the level. */
            screens[slot][(PLANE + 160 * ROW) / 2 + (120 >> 4)] = 1u << (15 - (120 & 15));
            int saved_bottom = presentation_bottom_clip(159);
            presentation_draw_begin(1, 0, 120, top);
            for (int y = top; y <= 159; y++)
                paint_pixel(slot, 120, y, 0, 1);
            presentation_draw_end();
            presentation_bottom_clip(saved_bottom);
            publish(slot, 0, 100 + slot * 2);
            show(slot, 0);
        }
        show(1, 20000000);
        assert(at(120, 149) == 0xffff0000);
        assert(at(120, 159) == 0xffff0000); /* stem stays connected to its base */
        assert(at(120, 160) == 0xff00ff00); /* it never paints over the sill */
    }
}

int main(void)
{
    for (int y = 0; y < 320; y++) {
        colors[y][1] = 0xf00;
        colors[y][2] = 0x0f0;
        colors[y][3] = 0x00f;
    }
    motion_and_timing();
    mask_and_order();
    reverse_motion_and_variable_cadence();
    camera_and_scroll_wrap();
    lower_playfield_scrolling();
    fixed_hud_over_scrolling_scenery();
    batches_and_discontinuities();
    foreground_occlusion_when_batch_changes();
    persistent_object_matching_and_draw_order();
    spike_stem_preserves_fixed_base();
    puts("presentation: motion, timing, masks, layering, camera, HUD, occlusion, object identity, spike extension/retraction and discontinuities passed");
    return 0;
}
