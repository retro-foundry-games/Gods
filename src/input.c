/* Gods, the menu program: the fire buttons, the joystick and the mouse */
#include "input.h"

/* how far the mouse moved: the old and the new counter (8 bits, they wrap) give the step, taken from the smaller of the two
 * ways round; added to the position, which stays between 0 and `limit` */
static int16_t move_pointer(int16_t position, uint8_t old, uint8_t now, int16_t limit)
{
    uint8_t step = old - now;
    int borrow = old < now;
    int negative = (step & 0x80) != 0;

    if (borrow ? !negative : negative)
        step += 0xff;
    position = position - (int8_t)step;
    if (position < 0)
        position = 0;
    if (position > limit)
        position = limit;
    return position;
}

/* orig $01E39C: read the fire buttons, the joystick, the mouse counters and the pot lines: the button bits, the joystick
 * byte and the pointer position */
void input_poll(void)
{
    uint8_t pra;
    uint8_t joy = 0;
    uint16_t stick;
    int16_t position;

    guest.button_bits[0] = 0;
    guest.button_bits[1] = 0;
    pra = CIAA_PRA;
    if (!(pra & 0x80)) {
        joy |= 0x80;
        guest.button_bits[0] |= 1;
        guest.button_presses |= 1;
    }
    if (!(pra & 0x40)) {
        guest.button_bits[0] |= 2;
        guest.button_presses |= 4;
    }
    stick = custom->joy1dat;
    if (stick & 0x0200) {   /* left */
        joy |= 4;
        stick ^= 0x0100;
    }
    if (stick & 0x0002) {   /* right */
        joy |= 8;
        stick ^= 0x0001;
    }
    if (stick & 0x0100)   /* up */
        joy |= 1;
    if (stick & 0x0001)   /* down */
        joy |= 2;
    /* the keyboard as a second joystick: the cursor keys (raw key codes $4C up, $4D down, $4F left and $4E right) move the selection as the stick does and the space bar is fire;
     * this is not in the original program */
    if (guest.key_down[0x4c])
        joy |= 1;
    if (guest.key_down[0x4d])
        joy |= 2;
    if (guest.key_down[0x4f])
        joy |= 4;
    if (guest.key_down[0x4e])
        joy |= 8;
    /* the space bar is the first fire button (raw key code $40); not in the original */
    if (guest.key_down[0x40]) {
        joy |= 0x80;
        guest.button_bits[0] |= 1;
        guest.button_presses |= 1;
    }
    guest.joystick = joy;

    guest.mouse_counter = custom->joy0dat;
    position = move_pointer(guest.mouse_y, guest.mouse_counter_old >> 8, guest.mouse_counter >> 8, 0xc7);
    guest.mouse_y = position;
    guest.mouse_y_copy = position;
    position = move_pointer(guest.mouse_x, guest.mouse_counter_old & 0xff, guest.mouse_counter & 0xff, 0x200);
    guest.mouse_x = position;
    guest.mouse_x_copy = position;
    guest.mouse_counter_old = guest.mouse_counter;

    if (custom->pot0dat & 0xfc00) {
        guest.button_bits[0] |= 1;
        guest.button_presses |= 4;
        guest.joystick |= 0x80;
    }
    custom->potgo = 0xffff;
}

/* orig $01CEF0: the next request, as a debounce: nothing waiting is 0; a request that is new is returned at once and starts
 * a delay of 10; one that is held is returned again only when the delay (counted down elsewhere) has run out, and
 * then it is returned and the delay is 1 */
uint16_t next_request(void)
{
    uint16_t request = (uint16_t)take_pending_request();

    if (request == 0) {
        guest.last_request = 0;
        return 0;
    }
    if (request != guest.last_request) {
        guest.last_request = request;
        guest.repeat_delay = 10;
        return request;
    }
    if (guest.repeat_delay >= 0)
        return 0;
    guest.repeat_delay = 1;
    return guest.last_request;
}
