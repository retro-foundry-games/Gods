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
