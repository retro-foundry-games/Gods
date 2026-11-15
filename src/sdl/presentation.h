/* Optional SDL presentation.  None of this state belongs to the Amiga machine. */
#ifndef GODS_PRESENTATION_H
#define GODS_PRESENTATION_H

#include <stdint.h>

void presentation_enable(int enabled);
void presentation_reset(void);
void presentation_discard_history(void); /* renderer only, after a clock discontinuity */
void presentation_begin(uint8_t *screen, int x, int y, uintptr_t scene);
int presentation_recording(void);
uintptr_t presentation_object(const void *object); /* returns the previous drawing owner */
void presentation_draw_begin(uintptr_t caller, int kind, int x, int y);
void presentation_draw_end(void);
void presentation_overlay_begin(void);
void presentation_overlay_end(void);
void presentation_write(uint16_t *destination, uint16_t mask, uint16_t value);
void presentation_publish(int slot, const uint8_t *screen, int fine_scroll, uint16_t tick);
void presentation_render(uint32_t *pixels, const uint16_t colors[][16], int slot, uint64_t now);

#endif
