/* Optional SDL presentation.  None of this state belongs to the Amiga machine. */
#ifndef GODS_PRESENTATION_H
#define GODS_PRESENTATION_H

#include <stdint.h>

/* 400 / (270 * 5/6) = 16/9, preserving the original pixel proportions.
 * The unchanged 320x256 raster is centred in this presentation surface. */
enum { WIDE_WIDTH = 400, WIDE_HEIGHT = 270, WIDE_LEFT = 40, WIDE_TOP = 7 };

void presentation_enable(int enabled);
void presentation_widescreen(int enabled);
void presentation_reset(void);
void presentation_discard_history(void); /* renderer only, after a clock discontinuity */
void presentation_begin(uint8_t *screen, int x, int y, uintptr_t scene);
void presentation_world(const uint8_t *map, const uint8_t *tiles, int frozen);
void presentation_image(int x, int y, int width, int height, const uint8_t *source,
                        int plane_bytes, int fine, int last_row, const int16_t *clip, int opaque);
int presentation_recording(void);
uintptr_t presentation_object(const void *object); /* returns the previous drawing owner */
int presentation_bottom_clip(int bottom); /* fixed base of a revealed image; returns the previous clip */
void presentation_draw_begin(uintptr_t caller, int kind, int x, int y);
void presentation_draw_end(void);
void presentation_overlay_begin(void);
void presentation_overlay_end(void);
void presentation_write(uint16_t *destination, uint16_t mask, uint16_t value);
void presentation_publish(int slot, const uint8_t *screen, int fine_scroll, uint16_t tick);
void presentation_render(uint32_t *pixels, const uint16_t colors[][16], int slot, uint64_t now);
void presentation_fit_wide(uint32_t *wide, const uint32_t *original);
void presentation_render_wide(uint32_t *wide, const uint32_t *original,
                              const uint16_t colors[][16], int slot, uint64_t now);

#endif
