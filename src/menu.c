/* Gods, the menu program: what is not placed in a module of its own yet.  The modules are disk.c, alloc.c, decrunch.c,
 * input.c and screen.c (menu.h includes their headers). */
#include "menu.h"

/* orig $00F518: clear the 80 longs the work area pointer points at */
void clear_work_area(void)
{
    uint32_t *p = work_area;
    int i;

    for (i = 0; i < 80; i++)
        *p++ = 0;
}

/* orig $01CF26: the request that was waiting, or 0; it is taken, so it is gone afterwards */
uint32_t take_pending_request(void)
{
    uint32_t request = pending_request;

    if (request != 0)
        pending_request = 0;
    return request;
}

/* orig $0053EC and $016D74: nothing; two places of the program where an empty routine is called */
void do_nothing(void)
{
}

/* orig $017CAC: mark the hundred entries of the table as empty */
void reset_marker_table(void)
{
    int i;

    for (i = 0; i < 100; i++)
        marker_table[i] = 0xffffffff;
}

/* orig $00F474: copy 200 records of 6 bytes into the record table */
void copy_record_table(const struct Rec6 *from)
{
    int i;

    for (i = 0; i < 200; i++) {
        record_table[i].w[0] = file_word(&from[i].w[0]);
        record_table[i].w[1] = file_word(&from[i].w[1]);
        record_table[i].w[2] = file_word(&from[i].w[2]);
    }
}

/* orig $017DF8: clear the 50 entries of the entry table */
void clear_entry_table(void)
{
    int i;

    for (i = 0; i < 50; i++) {
        entry_table[i].w[0] = 0;
        entry_table[i].w[1] = 0;
        entry_table[i].w[2] = 0;
    }
}

/* orig $00F382: make bytes of 506 words: the low byte of each */
void narrow_words_to_bytes(void)
{
    const uint16_t *from = words_source;
    uint8_t *to = bytes_dest;
    int i;

    for (i = 0; i < 506; i++)
        *to++ = (uint8_t)file_word(from++);
}

/* orig $00F3C0: give the entries of the list their addresses, one after the other, from 400 bytes after the start of the
 * list on, each as far on as the size of the one before; the first with a negative first long ends the list */
void assign_addresses(void)
{
    struct Entry8 *entry = entry_list;
    uint32_t next = (uint32_t)(uintptr_t)entry + 400;

    for (;;) {
        uint32_t first = entry->address;
        uint32_t size = entry->size;

        if ((int32_t)first < 0)
            return;
        entry->address = next;
        next += size;
        entry++;
    }
}

/* orig $00F488: fill the 2400-byte buffer: first clear it, then copy the bytes in */
void load_buffer_2400(const uint8_t *from)
{
    uint8_t *to = buffer_cb6;
    int i;

    for (i = 0; i < 2400; i++)
        *to++ = 0;
    to = buffer_cb6;
    for (i = 0; i < 2400; i++)
        *to++ = *from++;
}

/* orig $0029B4: take the row of param_table that param_index names (the first is 1) into param_a to param_d */
void select_params(void)
{
    int16_t row = (int16_t)(param_index - 1);
    const uint16_t *at = param_table;

    at += (int16_t)(row * 4) / 2;
    at += (int16_t)(row * 8) / 2;
    param_a = ((uint32_t)at[0] << 16) | at[1];
    param_b = ((uint32_t)at[2] << 16) | at[3];
    param_c = at[4];
    param_d = at[5];
}

/* orig $004C90: build the 132 entries of the table: the first word counts up from 170, the third is the signed byte from
 * the table of offsets */
void build_table_14cf8(void)
{
    uint16_t count = 170;
    int i;

    for (i = 0; i < 132; i++) {
        table_14cf8[i].w[0] = count;
        table_14cf8[i].w[2] = (uint16_t)(int16_t)(int8_t)offsets_4cba[i];
        count++;
    }
}

/* orig $01494C: put away two words of each of the eleven slots */
void save_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        saved_slot_words[i * 2] = object_slots[i].value_c;
        saved_slot_words[i * 2 + 1] = object_slots[i].value_1e;
    }
}

/* orig $01496A: and put them back */
void restore_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        object_slots[i].value_c = saved_slot_words[i * 2];
        object_slots[i].value_1e = saved_slot_words[i * 2 + 1];
    }
}

/* orig $006A82: the background picture is read from the disk (the file is packed: backscr.pi1, to 256 bytes into the memory block
 * memory_base is), the colours are faded out and the display is reset while it is shown, and the flag says it is there.  The
 * disk drives are put to rest.  (The original has a path for a failed load too; load_file does not return then) */
void load_background(void)
{
    if (background_loaded)
        return;
    background = memory_base + 256;
    load_unpacked((const char *)background_name, background);
    fade_out(1, 0xffff);
    display_reset();
    background_loaded = 1;
    drives_idle();
}

/* orig $006B06: the background image on the screen: 192 rows of 20 words of each of the four planes */
void background_draw(void)
{
    background = memory_base + 256;
    load_background();

    const uint16_t *src = (const uint16_t *)(background + 34);
    uint16_t *dst = (uint16_t *)screen_buffer;

    for (int row = 0; row < 192; row++) {
        for (int col = 0; col < 20; col++) {
            dst[0] = *src++;
            dst[4224] = *src++;
            dst[8448] = *src++;
            dst[12672] = *src++;
            dst++;
        }
        dst += 2;
    }
}

/* orig $00659C: set up the password screen, once: the background is drawn and shown, the colours fade in, the high score file
 * is read onto the screen buffer and copied to record_text (216 bytes), and the three saved codes at its end are unscrambled
 * (each long that is not 0 is exclusive-or'ed with a constant) to password_slots; then the colours fade out */
void password_screen_setup(void)
{
    if (screen_prepared)
        return;
    background_draw();
    screens_flip();
    copper_setup_done = 1;
    fade_in((const uint16_t *)(background + 2), 0);
    drives_present &= ~0x40;
    screen_prepared = 1;

    uint8_t *buffer = screen_buffer + 18048;

    disk_digit = '2';
    file_read((const char *)(text_top_ten + 13), buffer);
    for (int i = 0; i < 216; i++)
        record_text[i] = buffer[i];

    const uint32_t *coded = (const uint32_t *)(record_text + 200);
    static const uint32_t key[4] = { 0x4d616769, 0x63506f63, 0x6b657473, 0x436f6b65 };
    uint32_t value;

    value = coded[0];
    password_slots[0] = value ? value ^ key[0] : 0;
    value = coded[1];
    password_slots[0] = value ? value ^ key[1] : 0;   /* the same place again: the original stores both there */
    value = coded[2];
    password_slots[1] = value ? value ^ key[2] : 0;
    value = coded[3];
    password_slots[2] = value ? value ^ key[3] : 0;
    fade_out(0, 0xffff);
}

/* orig $0031CE: the small font is loaded once (state 3): the sound is stopped, glyph_base is where the font's images start in
 * the image table, and the asset set smallfnt is read to the memory the first set ended at */
void leave_load_screen(void)
{
    if (menu_state_25a == 3)
        return;
    play_sound(0, 0);
    glyph_base = (uint16_t)(((uint32_t)asset_table_0ca6 - (uint32_t)image_table) >> 3);
    asset_flag = -1;
    flags_0ddd |= 0x10;
    assets_load((const char *)small_font_name, palette_words, asset_end_0ca2, (struct AssetEntry *)asset_table_0ca6);
    palette_words[0] = 0;
    menu_state_25a = 3;
    drives_idle();
}

/* orig $006DEA: sixteen characters of line_text in the large font at the bottom, 16 pixels apart */
void draw_line_text(void)
{
    const uint8_t *text = line_text;
    int16_t x = 32;

    for (int i = 0; i < 16; i++) {
        int16_t ch = (int8_t)*text++;

        if (ch == 0)
            ch = 28;
        glyph_draw(x, 176, ch + 128 - 1);
        x += 16;
    }
}

/* orig $006E1C: the cursor of the grid: a glyph of four frames at the cell cursor_pos names, then the next frame */
void cursor_draw(void)
{
    int16_t x = ((cursor_pos & 7) << 5) + 40;
    int16_t row = (int16_t)cursor_pos >> 3;
    int16_t y = row * 24 + 40;

    glyph_draw(x, y, cursor_frame + 87);
    cursor_frame = (cursor_frame + 1) & 3;
}

/* orig $006E5E: the password screen's background, its title and the grid of 5 rows of 8 glyphs (numbered on from row to row) */
void password_grid_draw(void)
{
    background_draw();
    text_draw_at(title_text, title_x, title_y);
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 8; col++)
            glyph_draw(40 + 32 * col, 40 + 24 * row, 128 + 8 * row + col);
}

/* orig $006994: the three saved codes on one line each, 100 pixels apart: "pass 2 :" and then the code's characters
 * (their codes plus 96), the ones that are there */
void password_slots_draw(void)
{
    int16_t x = 10;

    for (int i = 0; i < 3; i++) {
        uint32_t slot = password_slots[i];

        password_line[5] = '2' + i;
        if ((slot >> 24) != 0) {
            password_line[8] = (uint8_t)(slot >> 24) + 96;
            password_line[9] = (uint8_t)(slot >> 16) + 96;
            password_line[10] = (uint8_t)(slot >> 8) + 96;
            password_line[11] = (uint8_t)slot;
            draw_string_small((const char *)password_line, x, 182);
        }
        x += 100;
    }
}

/* orig $006974: ten records, one a line 13 pixels below the one before: sixteen characters and the four bytes in hexadecimal */
void record_list_draw(void)
{
    const uint8_t *record = record_text;
    int16_t y = 40;

    for (int i = 0; i < 10; i++) {
        int16_t x = draw_string_16(record, 20, y);

        record += 16;
        draw_hex_bytes(record, x + 40, y);
        record += 4;
        y += 13;
    }
}

/* orig $006948: wait `frames` frames (for ever when it is negative) for a request or a button; a button must be let go again */
void wait_request(int16_t frames)
{
    for (;;) {
        if (frames >= 0) {
            wait_vblank();
            if (--frames < 0)
                return;
        }
        if ((uint16_t)take_pending_request() != 0)
            return;
        if (button_bits[0] != 0) {
            while (button_bits[0] != 0)
                ;
            return;
        }
    }
}

/* orig $006754: the title row: six sprites 32 pixels apart and a glyph after them */
void title_draw(void)
{
    uint16_t index = 123 + glyph_base;

    for (int i = 0; i < 6; i++)
        sprite_draw(64 + 32 * i, 5, index + i);
    glyph_draw(256, 5, index + 6);
}

/* orig $012D5C: the text centred on the other screen (the one that is shown) */
void text_draw_centered_other(const char *text)
{
    uint8_t *saved = screen_buffer;

    screen_buffer = screen_other;
    text_draw_centered(text);
    screen_buffer = saved;
}

/* orig $01CF64: wait up to frames - 1 frames, or until the first button is down */
void wait_frames_or_button(uint16_t frames)
{
    for (;;) {
        if (--frames == 0)
            return;
        wait_vblank();
        if (button_bits[0] & 1)
            return;
    }
}
