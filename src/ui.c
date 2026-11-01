/* Gods, the menu program: the screens of the menu (the main menu, the password screen, the top ten) and what is done with
 * the joystick and the keyboard on them */
#include "menu.h"

/* orig $0066CC: the main menu's screen: the background, the title, the lines of text (the third when there is a game to continue)
 * and the pointer, a glyph at each side of the line cursor_pos names; then it is shown */
void menu_screen_draw(void)
{
    background_draw();
    title_draw();
    text_y = 82;
    text_draw_centered((const char *)menu_texts);
    text_y = 114;
    text_draw_centered((const char *)menu_texts + 12);
    menu_item_count = 1;
    text_y = 146;
    if (has_continue) {
        menu_item_count++;
        text_draw_centered((const char *)menu_texts + 27);
        text_y = 178;
    }

    int16_t y = 82 + (int16_t)(cursor_pos << 5);

    glyph_draw(24, y, 168);
    glyph_draw(280, y, 166);
    screens_flip();
}

/* orig $0068DE: the top ten: the background, the title, the ten records and the three saved codes; the colours fade in, it
 * stays for 750 frames (or until a request or a button), and fades out again */
void top_ten_screen(void)
{
    leave_load_screen();
    background_draw();
    text_draw_at((const char *)text_top_ten, 64, 8);
    record_list_draw();
    password_slots_draw();
    copper_setup_done = 1;
    do_nothing();
    wait_vblank();
    screens_flip();
    copy_other_screen();
    fade_in((const uint16_t *)(background + 2), 1);
    wait_request(750);
    fade_out(1, 0);
    do_nothing();
}

/* orig $006B4C: the password screen: the grid of characters, the line of what has been picked so far (line_text, 16 characters
 * and a 0) and the cursor.  The joystick moves the cursor over the 40 cells and the button picks the one it is on (cell 38 is
 * the backspace, 39 is the end); the keyboard picks by key.  It returns when the end is picked or Return is pressed */
void password_screen(void)
{
    uint8_t *line = (uint8_t *)line_text;
    int16_t length;

    for (int i = 0; i < 20; i++)
        line[i] = 0;
    fade_out(0, 0);
    copper_setup_done = 1;
    do_nothing();
    wait_vblank();
    screens_flip();
    copy_other_screen();
    password_grid_draw();
    draw_line_text();
    screens_flip();
    cursor_pos = 0;
    key_held = 0;
    fade_in((const uint16_t *)(background + 2), 1);
    length = 0;

    for (;;) {
        password_grid_draw();
        draw_line_text();
        cursor_draw();

        uint16_t key = next_request();
        int16_t pick = -1;   /* the character the program numbers, to add to the line, or -1 */

        if (key != 0) {
            for (int i = 0; key_table[i] < 0x80; i++) {
                if (key_table[i] == (uint8_t)key) {
                    play_sound(22, 1);
                    cursor_pos = i;
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
        } else if (button_bits[0] & 1) {
            if (!key_held) {
                key_held = 1;
                play_sound(33, 2);

                int16_t cell = cursor_pos;

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
            key_held = 0;
            if (joystick != 0) {
                if (joystick & 8) {
                    cursor_pos++;
                    play_sound(22, 1);
                }
                if (joystick & 4) {
                    cursor_pos--;
                    play_sound(22, 1);
                }
                if ((joystick & 1) && (int16_t)cursor_pos >= 8) {
                    cursor_pos -= 8;
                    play_sound(22, 1);
                }
                if ((joystick & 2) && (int16_t)cursor_pos < 32) {
                    cursor_pos += 8;
                    play_sound(22, 1);
                }
                if ((int16_t)cursor_pos > 39)
                    cursor_pos = 39;
                if ((int16_t)cursor_pos < 0)
                    cursor_pos = 0;
            }
        }
        if (pick >= 0 && length != 16) {
            line[length++] = (uint8_t)pick;
            line[length] = 0;
        }
        screens_flip();
    }

done:
    play_sound(33, 2);
    fade_out(1, 0);
    do_nothing();
    copper_setup_done = 0;
}

/* orig $006388: the start of the menu program's main part: the screen the background image is read to, the sound on, and then
 * the main menu */
void menu_start(void)
{
    background = memory_base;
    sound_enabled = 1;
    menu_active = 1;
    message_shown = 0;
    leave_load_screen();
    menu_main();
}

/* orig $0063A2: the main menu, from the start until a line is chosen: the three lines, the pointer moved by the joystick, and
 * after 750 frames without a move the top ten (then the menu again).  The second line is the password screen; the first
 * (Begin Quest), the third (Continue) and what follows the password screen go on in the original's code, which is not in C yet */
void menu_main(void)
{
    for (;;) {
        has_continue = 0;
        message_shown = 0;
        clear_work_area();
        do_nothing();
        cursor_pos = has_continue * 2;
        password_screen_setup();
        menu_screen_draw();
        copper_setup_done = 1;
        disk_digit = '2';
        fade_in((const uint16_t *)(background + 2), 1);
        tick_count = 0;

        for (;;) {
            message_shown = 0;
            clear_work_area();
            menu_screen_draw();
            if ((int32_t)tick_count >= 750) {
                fade_out(1, 0);
                do_nothing();
                top_ten_screen();
                break;
            }
            if (button_bits[0] & 1) {
                play_sound(33, 2);
                if ((int16_t)cursor_pos < 1) {
                    original_begin();
                    return;
                }
                if ((int16_t)cursor_pos > 1) {
                    original_continue();
                    return;
                }
                title_text = (const char *)text_enter_a;
                title_x = 32;
                title_y = 8;
                password_screen();
                original_after_password();
                return;
            }
            if (joystick & 1) {
                tick_count = 0;
                cursor_pos--;
                if ((int16_t)cursor_pos < 0) {
                    cursor_pos = 0;
                    continue;
                }
                play_sound(22, 1);
            } else if (joystick & 2) {
                cursor_pos++;
                tick_count = 0;
                if (menu_item_count < (int16_t)cursor_pos) {
                    cursor_pos--;
                    continue;
                }
                play_sound(22, 1);
            }
        }
    }
}
