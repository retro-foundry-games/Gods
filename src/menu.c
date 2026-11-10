/* Gods, the menu program: what is not placed in a module of its own yet.  The modules are disk.c, alloc.c, decrunch.c,
 * input.c and screen.c (menu.h includes their headers), and the screens of the menu (the main menu, the password screen, the top ten). */
#include "menu.h"

/* orig $00F518: clear the 80 longs the work area pointer points at */
void clear_work_area(void)
{
    uint32_t *p = guest.work_area;
    int i;

    for (i = 0; i < 80; i++)
        *p++ = 0;
}

/* orig $01CF26: the request that was waiting, or 0; it is taken, so it is gone afterwards */
uint32_t take_pending_request(void)
{
    uint32_t request = guest.pending_request;

    if (request != 0)
        guest.pending_request = 0;
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
        guest.marker_table[i] = 0xffffffff;
}

/* orig $00F474: copy 200 records of 6 bytes into the record table */
void copy_record_table(const struct PlacedThing *from)
{
    int i;

    for (i = 0; i < 200; i++) {
        guest.record_table[i].x = file_word(&from[i].x);
        guest.record_table[i].y = file_word(&from[i].y);
        guest.record_table[i].kind = file_word(&from[i].kind);
    }
}

/* orig $017DF8: clear the 50 entries of the entry table */
void clear_entry_table(void)
{
    int i;

    for (i = 0; i < 50; i++) {
        guest.entry_table[i].w[0] = 0;
        guest.entry_table[i].w[1] = 0;
        guest.entry_table[i].w[2] = 0;
    }
}

/* orig $00F382: make bytes of 506 words: the low byte of each */
void narrow_words_to_bytes(void)
{
    const uint16_t *from = (const uint16_t *)guest.cell_entry_table;
    uint8_t *to = guest.bytes_dest;
    int i;

    for (i = 0; i < 506; i++)
        *to++ = (uint8_t)*from++;
}

/* orig $00F3C0: give the entries of the list their addresses, one after the other, from 400 bytes after the start of the
 * list on, each as far on as the size of the one before; the first with a negative first long ends the list */
void assign_addresses(void)
{
    struct Entry8 *entry = guest.entry_list;
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
void load_script_records(const uint8_t *from)
{
    uint8_t *to = (uint8_t *)guest.script_records;
    int i;

    for (i = 0; i < 2400; i++)
        *to++ = 0;
    to = (uint8_t *)guest.script_records;
    for (i = 0; i < 2400; i++)
        *to++ = *from++;
    for (i = 0; i < 100; i++)   /* (loaded from the disk: the words of the records are made the machine's own) */
        file_words(&guest.script_records[i], 12);
}

/* orig $0029B4: take the row of param_table that param_index names (the first is 1) into param_a to param_d */
void select_params(void)
{
    int16_t row = (int16_t)(guest.param_index - 1);
    const uint16_t *at = param_table;

    at += (int16_t)(row * 4) / 2;
    at += (int16_t)(row * 8) / 2;
    guest.param_a = ((uint32_t)at[0] << 16) | at[1];
    guest.param_b = ((uint32_t)at[2] << 16) | at[3];
    guest.param_c = at[4];
    guest.param_d = at[5];
}

/* orig $004C90: build the 132 entries of the table: the first word counts up from 170, the third is the signed byte from
 * the table of offsets */
void build_thing_kinds(void)
{
    uint16_t count = 170;
    int i;

    for (i = 0; i < 132; i++) {
        guest.thing_kinds[i].glyph = count;
        guest.thing_kinds[i].kind = (uint16_t)(int16_t)(int8_t)offsets_4cba[i];
        count++;
    }
}

/* orig $01494C: put away two words of each of the eleven slots */
void save_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        guest.saved_slot_words[i * 2] = guest.object_slots[i].value_c;
        guest.saved_slot_words[i * 2 + 1] = guest.object_slots[i].value_1e;
    }
}

/* orig $01496A: and put them back */
void restore_slot_words(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        guest.object_slots[i].value_c = guest.saved_slot_words[i * 2];
        guest.object_slots[i].value_1e = guest.saved_slot_words[i * 2 + 1];
    }
}

/* orig $006A82: the background picture is read from the disk (the file is packed: backscr.pi1, to 256 bytes into the memory block
 * memory_base is), the colours are faded out and the display is reset while it is shown, and the flag says it is there.  The
 * disk drives are put to rest.  (The original has a path for a failed load too; load_file does not return then) */
void load_background(void)
{
    if (guest.background_loaded)
        return;
    guest.background = guest.memory_base + 256;
    load_unpacked((const char *)background_name, guest.background);
    host_disk_wait();   /* (the original's read of the file from the disk and its unpacking take what its drive takes before the colours fade) */
    fade_out(1, 0xffff);
    display_reset();
    guest.background_loaded = 1;
    drives_idle();
}

/* orig $006B06: the background image on the screen: 192 rows of 20 words of each of the four planes */
void background_draw(void)
{
    guest.background = guest.memory_base + 256;
    load_background();

    const uint16_t *src = (const uint16_t *)(guest.background + 34);
    uint16_t *dst = (uint16_t *)guest.screen_buffer;

    for (int row = 0; row < 192; row++) {
        for (int col = 0; col < 20; col++) {
            dst[0] = *src++;
            dst[4224] = *src++;
            dst[SCREEN_ROW * SCREEN_HEIGHT] = *src++;
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
    if (guest.screen_prepared)
        return;
    background_draw();
    screens_flip();
    guest.copper_setup_done = 1;
    fade_in((const uint16_t *)(guest.background + 2), 0);
    guest.drives_present &= ~0x40;
    guest.screen_prepared = 1;

    uint8_t *buffer = guest.screen_buffer + SCREEN_POS(2, 26, 8);

    guest.disk_digit = '2';
    file_read((const char *)(screen_texts + 51), buffer);
    for (int i = 0; i < 216; i++)
        guest.record_text[i] = buffer[i];

    const uint32_t *coded = (const uint32_t *)(guest.record_text + 200);
    static const uint32_t key[4] = { 0x4d616769, 0x63506f63, 0x6b657473, 0x436f6b65 };
    uint32_t value;

    value = coded[0];
    guest.password_slots[0] = value ? value ^ key[0] : 0;
    value = coded[1];
    guest.password_slots[0] = value ? value ^ key[1] : 0;   /* the same place again: the original stores both there */
    value = coded[2];
    guest.password_slots[1] = value ? value ^ key[2] : 0;
    value = coded[3];
    guest.password_slots[2] = value ? value ^ key[3] : 0;
    fade_out(0, 0xffff);
}

/* orig $0031CE: the small font is loaded once (state 3): the sound is stopped, glyph_base is where the font's images start in
 * the image table, and the asset set smallfnt is read to the memory the first set ended at */
void leave_load_screen(void)
{
    if (guest.menu_state_25a == 3)
        return;
    play_sound(0, 0);
    guest.glyph_base = (uint16_t)(((uint32_t)guest.asset_table_0ca6 - (uint32_t)guest.image_table) >> 3);
    guest.asset_flag = -1;
    guest.image_draw_flags |= 0x10;
    assets_load((const char *)small_font_name, guest.palette_words, guest.asset_end_0ca2, (struct AssetEntry *)guest.asset_table_0ca6);
    guest.palette_words[0] = 0;
    guest.menu_state_25a = 3;
    drives_idle();
}

/* orig $006DEA: sixteen characters of line_text in the large font at the bottom, 16 pixels apart */
void draw_line_text(void)
{
    const uint8_t *text = guest.line_text;
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
    int16_t x = ((guest.cursor_pos & 7) << 5) + 40;
    int16_t row = (int16_t)guest.cursor_pos >> 3;
    int16_t y = row * 24 + 40;

    glyph_draw(x, y, guest.cursor_frame + 87);
    guest.cursor_frame = (guest.cursor_frame + 1) & 3;
}

/* orig $006E5E: the password screen's background, its title and the grid of 5 rows of 8 glyphs (numbered on from row to row) */
void password_grid_draw(void)
{
    background_draw();
    text_draw_at(guest.title_text, guest.title_x, guest.title_y);
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
        uint32_t slot = guest.password_slots[i];

        guest.password_line[5] = '2' + i;
        if ((slot >> 24) != 0) {
            guest.password_line[8] = (uint8_t)(slot >> 24) + 96;
            guest.password_line[9] = (uint8_t)(slot >> 16) + 96;
            guest.password_line[10] = (uint8_t)(slot >> 8) + 96;
            guest.password_line[11] = (uint8_t)slot;
            draw_string_small((const char *)guest.password_line, x, 182);
        }
        x += 100;
    }
}

/* orig $006974: ten records, one a line 13 pixels below the one before: sixteen characters and the four bytes in hexadecimal */
void record_list_draw(void)
{
    const uint8_t *record = guest.record_text;
    int16_t y = 40;

    for (int i = 0; i < 10; i++) {
        int16_t x = draw_string_16(record, 20, y);

        record += 16;
        {   /* the score is a native long in the record: its four bytes are drawn as the big-endian ones that the original's record has */
            uint32_t score = *(const uint32_t *)record;
            const uint8_t bytes[4] = { (uint8_t)(score >> 24), (uint8_t)(score >> 16), (uint8_t)(score >> 8), (uint8_t)score };

            draw_hex_bytes(bytes, x + 40, y);
        }
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
        if (guest.button_bits[0] != 0) {
            while (guest.button_bits[0] != 0)
                ;
            return;
        }
    }
}

/* orig $006754: the title row: six sprites 32 pixels apart and a glyph after them */
void title_draw(void)
{
    uint16_t index = 123 + guest.glyph_base;

    for (int i = 0; i < 6; i++)
        sprite_draw(64 + 32 * i, 5, index + i);
    glyph_draw(256, 5, index + 6);
}

/* orig $012D5C: the text centred on the other screen (the one that is shown) */
void text_draw_centered_other(const char *text)
{
    uint8_t *saved = guest.screen_buffer;

    guest.screen_buffer = guest.screen_other;
    text_draw_centered(text);
    guest.screen_buffer = saved;
}

/* orig $01CF64: wait up to frames - 1 frames, or until the first button is down (1 then, 0 when the time ran out) */
int wait_frames_or_button(uint16_t frames)
{
    for (;;) {
        if (--frames == 0)
            return 0;
        wait_vblank();
        if (guest.button_bits[0] & 1)
            return 1;
    }
}

/* ======== the screens of the menu (was ui.c): the main menu, the password screen, the top ten, and what is done with the joystick and the keyboard on them ======== */

static int menu_draw_delay;   /* frames of the original's drawing that the idle loop of the menu waits for */

/* orig $0066CC: the main menu's screen: the background, the title, the lines of text (the third when there is a game to continue)
 * and the pointer, a glyph at each side of the line cursor_pos names; then it is shown */
void menu_screen_draw(void)
{
    background_draw();
    title_draw();
    guest.text_y = 82;
    text_draw_centered((const char *)menu_texts);
    guest.text_y = 114;
    text_draw_centered((const char *)menu_texts + 12);
    guest.menu_item_count = 1;
    guest.text_y = 146;
    if (guest.has_continue) {
        guest.menu_item_count++;
        text_draw_centered((const char *)menu_texts + 27);
        guest.text_y = 178;
    }

    int16_t y = 82 + (int16_t)(guest.cursor_pos << 5);

    glyph_draw(24, y, 168);
    glyph_draw(280, y, 166);
    host_cpu_frames(menu_draw_delay);   /* (the lockstep: the original's drawing of the menu takes about 3 frames before its flip) */
    screens_flip();
}

/* orig $0068DE: the top ten: the background, the title, the ten records and the three saved codes; the colours fade in, it
 * stays for 750 frames (or until a request or a button), and fades out again */
static int top_ten_delay;   /* frames of the original's CPU work that the next top ten waits for before its fade-in */

void top_ten_screen(void)
{
    leave_load_screen();
    background_draw();
    text_draw_at((const char *)(screen_texts + 38), 64, 8);
    record_list_draw();
    password_slots_draw();
    guest.copper_setup_done = 1;
    do_nothing();
    wait_vblank();
    screens_flip();
    copy_other_screen();
    host_cpu_frames(top_ten_delay);   /* (the lockstep: the original's drawing of the lists takes about 9 frames more, after the menu's time ran out) */
    top_ten_delay = 0;
    fade_in((const uint16_t *)(guest.background + 2), 1);
    wait_request(750);
    fade_out(1, 0);
    do_nothing();
}

/* orig $006B4C: the password screen: the grid of characters, the line of what has been picked so far (line_text, 16 characters
 * and a 0) and the cursor.  The joystick moves the cursor over the 40 cells and the button picks the one it is on (cell 38 is
 * the backspace, 39 is the end); the keyboard picks by key.  It returns when the end is picked or Return is pressed */
void password_screen(void)
{
    uint8_t *line = (uint8_t *)guest.line_text;
    int16_t length;

    for (int i = 0; i < 20; i++)
        line[i] = 0;
    fade_out(0, 0);
    guest.copper_setup_done = 1;
    do_nothing();
    wait_vblank();
    screens_flip();
    copy_other_screen();
    password_grid_draw();
    draw_line_text();
    screens_flip();
    guest.cursor_pos = 0;
    guest.game_buttons = 0;
    host_flip_late(1);
    host_cpu_frames(5);   /* (the original's copy of the screen and the drawing of the grid and the line take about 5 frames before the colours fade in) */
    fade_in((const uint16_t *)(guest.background + 2), 1);
    length = 0;

    for (;;) {
        password_grid_draw();
        draw_line_text();
        cursor_draw();
        host_cpu_frames(3);   /* (the original's copy of the background and its drawing of the grid, the line and the cursor take about 3 frames, then it reads the keys and the stick, and flips: a pass is 4 with the flip) */

        host_read_wait();   /* (the lockstep: the original's read of the key, $6BC2, is done) */
        uint16_t key = next_request();
        int16_t pick = -1;   /* the character the program numbers, to add to the line, or -1 */

        if (key != 0) {
            for (int i = 0; key_table[i] < 0x80; i++) {
                if (key_table[i] == (uint8_t)key) {
                    play_sound(22, 1);
                    guest.cursor_pos = i;
                    break;
                }
            }
            if (key >= 97 && key <= 122) {
                pick = key - 97 + 1;
            } else if (key == 8) {
                if (length != 0) {
                    length--;
                    line[length] = 0;
                }
            } else if (key >= 48 && key <= 57) {
                pick = key - 48 + 29;
            } else if (key == 46) {
                pick = 27;
            } else if (key == 13) {
                goto done;
            } else if (key == 32) {
                pick = 28;
            }
        } else if (guest.button_bits[0] & 1) {
            if (!guest.game_buttons) {
                guest.game_buttons = 1;
                play_sound(33, 2);

                int16_t cell = guest.cursor_pos;

                if (cell == 38) {
                    if (length != 0) {
                        length--;
                        line[length] = 0;
                    }
                } else if (cell > 38) {
                    goto done;
                } else {
                    pick = cell + 1;
                }
            }
        } else {
            guest.game_buttons = 0;
            if (guest.joystick != 0) {
                if (guest.joystick & 8) {
                    guest.cursor_pos++;
                    play_sound(22, 1);
                }
                if (guest.joystick & 4) {
                    guest.cursor_pos--;
                    play_sound(22, 1);
                }
                if ((guest.joystick & 1) && (int16_t)guest.cursor_pos >= 8) {
                    guest.cursor_pos -= 8;
                    play_sound(22, 1);
                }
                if ((guest.joystick & 2) && (int16_t)guest.cursor_pos < 32) {
                    guest.cursor_pos += 8;
                    play_sound(22, 1);
                }
                if ((int16_t)guest.cursor_pos > 39)
                    guest.cursor_pos = 39;
                if ((int16_t)guest.cursor_pos < 0)
                    guest.cursor_pos = 0;
            }
        }
        if (pick >= 0 && length != 16) {
            line[length++] = (uint8_t)pick;
            line[length] = 0;
        }
        screens_flip();
    }

done:
    host_flip_late(0);
    play_sound(33, 2);
    fade_out(1, 0);
    do_nothing();
    guest.copper_setup_done = 0;
}

/* orig $0067FA: the three saved codes are scrambled again (each long that is not 0 is exclusive-or'ed with the constant; the first slot is
 * written twice, as the setup reads it) to the end of record_text, and record_text is written to the file */
static void scores_store(void)
{
    static const uint32_t key[4] = { 0x4d616769, 0x63506f63, 0x6b657473, 0x436f6b65 };
    uint32_t *coded = (uint32_t *)(guest.record_text + 200);
    uint32_t slot[4] = { guest.password_slots[0], guest.password_slots[0], guest.password_slots[1], guest.password_slots[2] };

    for (int i = 0; i < 4; i++)
        coded[i] = slot[i] ? slot[i] ^ key[i] : 0;
    scores_save();
}

/* orig $006780: when there is no game to continue the score (as one number) is looked for in the ten records (each of 20 bytes, its
 * score the last four): under the first record it is under, the later ones move down one, the name is asked for in the password
 * screen (its title "ENTER YOUR NAME HERO") and put in the record, and the records are written to the disk (also when the
 * score is in none of them) */
void high_score_entry(void)
{
    uint32_t score = ((uint32_t)guest.score_words[3] << 24) | ((uint32_t)guest.score_words[2] << 16) | ((uint32_t)guest.score_words[1] << 8) | guest.score_words[0];

    if (guest.has_continue)
        return;
    for (int i = 0; i < 10; i++) {
        uint32_t *value = (uint32_t *)(guest.record_text + 20 * i + 16);

        if ((int32_t)score > (int32_t)*value) {
            uint8_t *record = guest.record_text + 20 * i;

            for (uint8_t *to = guest.record_text + 180; to != record; to -= 20)
                for (int b = 0; b < 20; b++)
                    to[b] = to[b - 20];
            *value = score;
            load_background();
            guest.title_text = (const char *)(screen_texts + 17);
            guest.title_x = 0;
            guest.title_y = 8;
            password_screen();
            for (int b = 0; b < 16; b++)
                record[b] = ((const uint8_t *)guest.line_text)[b];
            guest.scores_changed = 1;
            scores_store();
            return;
        }
    }
    guest.scores_changed = 1;
    scores_store();
}

/* orig $006388: the start of the menu program's main part: the screen the background image is read to, the sound on, and then
 * the main menu */
void menu_start(void)
{
    guest.background = guest.memory_base;
    guest.sound_enabled = 1;
    guest.menu_active = 1;
    guest.message_shown = 0;
    leave_load_screen();
    menu_main();
}

/* orig $0063A2: the main menu, from the start until a line is chosen: the three lines, the pointer moved by the joystick, and
 * after 750 frames without a move the top ten (then the menu again).  The second line is the password screen; the first
 * (Begin Quest), the third (Continue) and what follows the password screen go on in the original's code, which is not in C yet */
void menu_main(void)
{
    for (;;) {
        guest.has_continue = 0;
        guest.message_shown = 0;
        clear_work_area();
        do_nothing();
        guest.cursor_pos = guest.has_continue * 2;
        password_screen_setup();
        menu_screen_draw();
        guest.copper_setup_done = 1;
        guest.disk_digit = '2';
        fade_in((const uint16_t *)(guest.background + 2), 1);
        guest.tick_count = 0;

        for (;;) {
            guest.message_shown = 0;
            clear_work_area();
            menu_draw_delay = 3;
            menu_screen_draw();
            menu_draw_delay = 0;
            if ((int32_t)guest.tick_count >= 750) {
                fade_out(1, 0);
                do_nothing();
                top_ten_delay = 9;
                top_ten_screen();
                break;
            }
            host_read_wait();   /* (the lockstep: the original's read of the button and the stick, $6422, is done) */
            if (guest.button_bits[0] & 1) {
                play_sound(33, 2);
                if ((int16_t)guest.cursor_pos < 1) {
                    game_loop_install();
                    begin_quest();
                    return;
                }
                if ((int16_t)guest.cursor_pos > 1) {
                    game_loop_install();
                    original_continue();
                    return;
                }
                guest.title_text = (const char *)screen_texts;
                guest.title_x = 32;
                guest.title_y = 8;
                password_screen();
                game_loop_install();
                password_entered();
                return;
            }
            if (guest.joystick & 1) {
                guest.tick_count = 0;
                guest.cursor_pos--;
                if ((int16_t)guest.cursor_pos < 0) {
                    guest.cursor_pos = 0;
                    continue;
                }
                play_sound(22, 1);
            } else if (guest.joystick & 2) {
                guest.cursor_pos++;
                guest.tick_count = 0;
                if (guest.menu_item_count < (int16_t)guest.cursor_pos) {
                    guest.cursor_pos--;
                    continue;
                }
                play_sound(22, 1);
            }
        }
    }
}
