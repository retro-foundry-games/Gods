/* Gods, the game: what runs while a level is played.  The loop that calls it all is at $2C20; the routines are written in the
 * order they were found, named by what they do (a provisional name is `fn_` and the address). */
#include "menu.h"

/* orig $00363A: this frame's input: the joystick byte and the button bits are kept for the game; when the input is being recorded
 * (demo_mode positive) one byte of them is put in the record (the buttons in bits 6 and 7, the stick below) */
void game_read_input(void)
{
    game_stick = joystick;
    game_buttons = button_bits[0];
    if (demo_mode == 0)
        return;
    if (demo_mode < 0)
        return;   /* playing a demo back: the original goes on at $3668, which did not run */
    *game_record_ptr++ = (uint8_t)((game_buttons << 6) | game_stick);
}

/* orig $004952: a thing at the screen position (x, y) is counted when it is near the screen: x from -16 to 320 and y, less the
 * scroll, above -32 and below 192 */
void count_if_visible(int16_t x, int16_t y)
{
    y -= scroll_y;
    if (x > 320 || x < -16 || y <= -32 || y >= 192)
        return;
    onscreen_count++;
}

/* orig $006136: the first of the twenty records of 8 bytes ptr_0cce points at whose fourth word is not negative (the others are
 * free), or 0.  The original goes on with the record at $6186, which did not run */
struct Slot8 *find_record_cce(void)
{
    struct Slot8 *record = (struct Slot8 *)ptr_0cce;

    for (int i = 0; i < 20; i++, record++)
        if (record->w[3] >= 0)
            return record;
    return 0;
}

/* orig $00D314: the first of the ten records of 12 bytes at $113C whose third word is not 0, or 0 (the original goes on at $D32A) */
struct Slot12 *find_record_113c(void)
{
    for (int i = 0; i < 10; i++)
        if (records_113c[i].w[2] != 0)
            return &records_113c[i];
    return 0;
}

/* orig $01924C: the first of the ten records of 8 bytes at $19334 whose third word is not negative, or 0 (the original goes on at $19262) */
struct Slot8 *find_record_19334(void)
{
    for (int i = 0; i < 10; i++)
        if (records_19334[i].w[2] >= 0)
            return &records_19334[i];
    return 0;
}

/* orig $018EF6: the first free (third word negative) of the two hundred records of 8 bytes at $19B14, or 0 when all are in use */
struct Slot8 *find_free_record_19b14(void)
{
    for (int i = 0; i < 200; i++)
        if (records_19b14[i].w[2] < 0)
            return &records_19b14[i];
    return 0;
}

/* orig $00FF1E: whether the words at +0, +8 and +16 of the record are all 0 (otherwise the original goes on at $FF30) */
int words_all_zero(const uint16_t *record)
{
    return record[0] == 0 && record[4] == 0 && record[8] == 0;
}

/* orig $017DDC: `count` + 1 entries of 6 bytes (an address and a word) are undone: where the address is not 0 the low byte of the
 * word is put there, and the entry is cleared; returns where the list ends */
struct SavedByte *restore_saved_bytes(struct SavedByte *list, int16_t count)
{
    for (int i = 0; i <= count; i++, list++) {
        if (list->address != 0) {
            *list->address = (uint8_t)list->value;
            list->address = 0;
            list->value = 0;
        }
    }
    return list;
}

/* orig $018A10: the routine the kind (0, 1, ...) names in dispatch_18be0 is called */
void call_by_kind(int16_t kind)
{
    dispatch_18be0[kind]();
}

/* orig $012A80: the first message line, or the one after it when it is the one on show (message_text or message_current) */
uint8_t *next_message_slot(void)
{
    uint8_t *slot = message_slots;

    if (slot == message_text || slot == message_current)
        slot += 88;
    return slot;
}

/* orig $001D2FC: wait until more than `frames` frames have passed since the last flip (vbl_saturating counts them), then flip */
static void wait_frames_then_flip(uint16_t frames)
{
    while ((int16_t)(frames - vbl_saturating) >= 0)
        ;
    screens_flip();
}

/* orig $002C20: one frame of the game.  The routines it calls are the original's (fn_ and the address) until they are written
 * in C.  Where the original goes on at an address that did not run in the recorded play ($3A36, $3A6C, $3A54, $39F6, $32B4,
 * $30DA, $35D2, $2F12) the frame ends here: those are the ends of a level, a death, the pause menu and the like */
void game_frame(void)
{
    message_tick();
    game_read_input();
    frame_toggle = ~frame_toggle;
    frame_phase++;
    if ((int16_t)frame_phase >= 2)
        frame_phase = 0;
    timer_306--;
    if ((int16_t)timer_306 < 0)
        timer_306 = 0;
    if ((int16_t)demo_mode < 0 && (button_bits[0] & 1)) {
        demo_mode = 0;
        return;   /* goes on at $3A36 */
    }
    if (!(game_buttons & 1))
        fire_latch = 0;
    camera_x_old = camera_x;
    if ((int16_t)life_state <= 0)
        return;   /* goes on at $3A6C */
    fn_6002();
    if (flag_26e)
        fn_30c6();
    fn_178c6();
    fn_1924c();
    fn_194d0();
    fn_17cc0();
    fn_ba34();
    if (flag_23a)
        callback_4e4();
    fn_17e9c();
    if (param_index == 2 || flag_23a != 1) {
        fn_3ea4();
        fn_417c();
        fn_7faa();
        fn_4070();
    }
    fn_15b04();
    fn_15cc4();
    fn_836c();
    value_c6c = 3030;
    flag_20e = -1;
    fn_6ed0();
    if (flag_23a == 0) {
        fn_73d2();
        fn_77e6();
        fn_7906();
        fn_3f8e();
        fn_18d14();
    }
    scroll_x_a = scroll_x;
    scroll_y_a = scroll_y;
    if (flag_290 >= 0)
        fn_33d4();
    else
        fn_10368();
    fn_9912();
    scroll_x_b = scroll_x;
    scroll_y_b = scroll_y;
    scroll_x = scroll_x_a;
    scroll_y = scroll_y_a;
    fn_16aee();
    if (flag_23a == 0) {
        fn_db04();
        fn_190f2();
        fn_e142();
        fn_de54();
        fn_9ae6();
        fn_3436();
        fn_3c56();
        fn_8a54();
        fn_7292();
        fn_e3a2();
        fn_e486();
        fn_e76c();
        fn_ea52();
        fn_ed38();
        fn_f01e();
        fn_d47e();
        fn_d314();
        fn_d6b8();
        fn_d898();
    } else {
        callback_4e0();
        fn_c58e();
        fn_cb0e();
        fn_c406();
    }
    fn_bc1a();
    fn_3dfc();
    fn_8452();
    fn_6136();
    fn_1843e();
    fn_c374();
    fn_5842();
    fn_57fc();
    fn_5608();
    fn_54c4();
    fn_5e02();
    fn_5d76();
    fn_3806();
    if (flag_268) {
        int16_t d = (int16_t)camera_x_old;

        if (d >= (int16_t)camera_x) {
            d -= (int16_t)camera_x;
            camera_x += d >> 1;
        }
    }
    scroll_x = scroll_x_b;
    scroll_y = scroll_y_b;

    int16_t request = (int16_t)take_pending_request();

    if (flag_23a == 0 && request == 27)
        return;   /* goes on at $3A54 */
    if (request == 32) {   /* the space bar: the game waits for a request or a button, and the clock goes on from where it was */
        uint32_t saved_ticks = tick_count;

        tick_hold_26c = 1;
        while ((uint16_t)take_pending_request() != 0)
            ;
        for (;;) {
            request = (int16_t)take_pending_request();
            if (request != 0 || button_bits[0] != 0)
                break;
        }
        tick_hold_26c = 0;
        tick_count = saved_ticks;
    }
    if (flag_2b0 == 18)
        request = 98;
    if (request == 70)
        return;   /* goes on at $39F6 */
    if (flag_230)
        fn_12e08();
    else
        wait_frames_then_flip(frame_wait);
    if ((int16_t)flag_290 >= 0)
        return;   /* goes on at $32B4 */
    if (flag_258)
        return;   /* goes on at $30DA */
    if (flag_1f0)
        return;   /* goes on at $35D2 */
    if (flags_3b0 & 0x40)
        return;   /* goes on at $2F12 */
}
