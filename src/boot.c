/* Gods, the menu program: the start (the loader enters it at $84 with the stack at $78000): the chips, the memory map, the
 * sound, the first asset set.  What follows (the part at $2400 that loads the copy protection's stub and the code it
 * releases) is not here. */
#include "menu.h"

/* menu_amiga.asm: the status register (a value of $2000 is the supervisor state with all interrupts enabled) */
void sr_set(uint16_t value);
void keyboard_irq(void);
void audio_irq(void);
void vbl_irq(void);
/* the stack is set to `stack` and the code at `address` is entered (it does not come back) */
void jump_with_stack(void (*address)(void), uint8_t *stack);

/* orig $039FA6: the two screens and the display registers: 320 x 192, four bitplanes */
void display_setup(void)
{
    screen_buffer = screen_1;
    screen_other = screen_2;
    custom->bplcon0 = 0x4200;
    custom->diwstrt = 0x3c81;
    custom->diwstop = 0x04c1;
    custom->ddfstrt = 0x0038;
    custom->ddfstop = 0x00d0;
}

/* orig $03A18A: the sound system: the voices are cleared, each gets its channel and its bits for DMA and interrupt, and the
 * four audio channels are set to play a short silent buffer; then the sound DMA is off until a sound is played */
void sound_init(void)
{
    uint32_t *state = (uint32_t *)voice;
    uint8_t *silence = (uint8_t *)0x21e5e;   /* the silent sample, in the data the loader brings */

    sound_lock = 0xff;
    CIAA_PRA |= 0x02;
    *(uint32_t *)0x2079e = 0;
    *(uint16_t *)0x207a2 = 0;
    *(uint32_t *)0x207a4 = 0;
    *(uint16_t *)0x207a8 = 0;
    custom->dmacon = 0x820f;
    custom->intena = 0x0780;
    custom->intreq = 0x0780;
    custom->adkcon = 0x00ff;
    for (int i = 0; i < 277; i++)   /* the voices and the state after them */
        state[i] = 0;
    for (int i = 0; i < 4; i++)
        voice[i].number = i;
    for (int i = 0; i < 4; i++) {
        custom->aud[i].vol = 0;
        custom->aud[i].len = 1;
        custom->aud[i].per = 1;
        voice[i].sample_a = silence;
        voice[i].sample_b = silence;
        custom->aud[i].lc = ADDR(silence);
        voice[i].field_28 = 8;
        custom->aud[i].len = 8;
        voice[i].field_e = 2;
        custom->aud[i].per = 0x100;
    }
    custom->dmacon = 0x000f;
    for (int i = 0; i < 4; i++) {
        voice[i].buffer = voice_buffers + 0x80 * i;
        voice[i].channel = (struct AudioChannel *)&custom->aud[i];
        voice[i].dma_bit = 1 << i;
        voice[i].dma_on = 0x8000 | (1 << i);
        voice[i].int_bit = 0x80 << i;
        voice[i].int_on = 0x8000 | (0x80 << i);
    }
    *(uint16_t *)&sound_lock = 0;
}

/* orig $039FDC: the colours are black, the interrupt vectors are set (keyboard, vertical blank, audio), the sound is set up,
 * the copper list is started and the DMA and the interrupts are enabled; then the interrupts are let through */
void chips_setup(void)
{
    for (int i = 0; i < 16; i++)
        custom->color[i] = 0;
    *(uint32_t *)0x68 = (uint32_t)(uintptr_t)keyboard_irq;
    *(uint32_t *)0x6c = (uint32_t)(uintptr_t)vbl_irq;
    *(uint32_t *)0x70 = (uint32_t)(uintptr_t)audio_irq;
    sound_init();
    custom->spr[0][1] = 0;
    custom->cop1lc = 0x5302;
    custom->cop2lc = 0x5302;
    custom->copjmp1 = 0;
    custom->dmacon = 0x0020;
    custom->dmacon = 0x83d0;
    custom->adkcon = 0x0200;
    custom->adkcon = 0x9500;
    custom->intena = 0x3fd7;
    custom->intena = 0xc028;
    *(volatile uint8_t *)0xbfdd00 = 0x6f;   /* CIA B: the interrupts of the disk index and the others off, then the one on */
    *(volatile uint8_t *)0xbfdd00 = 0x90;
    sr_set(0x2000);
}

/* orig $031FC0: the chips */
void hardware_setup(void)
{
    chips_setup();
    display_setup();
}

/* orig $01AA3C: five pointers into the data the loader brings */
void data_pointers_init(void)
{
    ptr_0cb2 = data_1aa66;
    ptr_0cbe = data_1ad8a;
    ptr_0d0e = data_1b17e;
    ptr_0ce6 = data_1b7be;
    ptr_0c9a = data_1c302;
}

/* orig $03A060: the memory map: each buffer of the program is a pointer into one block that begins at program_end, a buffer
 * after the other, and the pointers are kept in these variables */
void memory_map_init(void)
{
    uint8_t *p = program_end + 0x300;

    stack_top = p;
    p += 0x32;
    asset_memory = p;
    p += 0x21d68;
    ptr_0b5a = p;
    line_text = p;
    p += 0x640;
    ptr_0b5e = p;
    ptr_03ac = p;
    p += 0x190;
    asset_directory = p;
    ptr_0b52 = p;
    p += 0x960;
    memory_base = p;
    background = p;
    p += 0x8500;
    entry_list = (struct Entry8 *)p;
    p += 0x1518;
    ptr_0e4c = p;
    copper_save = (uint16_t *)p;
    p += 0xc8;
    ptr_0cee = p;
    p += 0x960;
    *(uint32_t *)table_199ac = (uint32_t)(uintptr_t)p;
    p += 0x578;
    ptr_0cc6 = p;
    p += 0x200;
    image_table = (struct Image *)p;
    p += 0x1450;
    work_area = (uint32_t *)p;
    clear_work_area();
    *(uint16_t *)0x5258 = (uint16_t)(uintptr_t)p;          /* the copper list's bitplane pointer for the message: the work area */
    *(uint16_t *)0x5254 = (uint16_t)((uintptr_t)p >> 16);
    p += 0x140;
    ptr_0b86 = p;
    p += 0x4000;
    ptr_0cc2 = p;
    p += 0x200;
    ptr_0cda = p;
    p += 0xc0;
    ptr_0cfa[0] = p;
    p += 0x3e8;
    buffer_cb6 = p;
    p += 0x960;
    ptr_0cba = p;
    p += 0x50;
    ptr_0bce = p;
    p += 0xa;
    ptr_0cce = p;
    p += 0xa0;
    ptr_0c9e = p;
    p += 0x258;
    ptr_0bb2 = p;
    ptr_0e44 = p;
    p += 0x300;
    ptr_0e5c = p;
    data_pointers_init();
    ptr_0cf2 = data_1c5be;
    restart_address = (uint8_t *)(uintptr_t)menu_boot;
}

/* orig $01F5C4: the state of the two screens starts again: the first has the saved colours' buffer, the second the buffer e58 points to */
void display_state_reset(void)
{
    uint32_t first = (uint32_t)(uintptr_t)copper_save;
    uint32_t second = (uint32_t)(uintptr_t)ptr_0e58;

    buffer_state[0].w[2] = 0;
    buffer_state[0].w[0] = first >> 16;
    buffer_state[0].w[1] = (uint16_t)first;
    buffer_state[1].w[2] = 0;
    buffer_state[1].w[0] = second >> 16;
    buffer_state[1].w[1] = (uint16_t)second;
}

/* orig $039E50: the start.  The chips, the memory map and the sound are set up (the original moves the stack to stack_top here;
 * the C program keeps the stack it has), the screens are flipped twice, the message "loading game please wait" is asked for, the
 * first asset set is read, and where its data ended is noted.  The original goes on at $240C: the code that loads the copy
 * protection's stub; that is not decompiled */
void menu_boot(void)
{
    custom->dmacon = 0x7fff;
    message_shown = 0;
    word_037c = 0;
    message_arg = 0;
    message_current = 0;
    word_5418 = 0;
    tick_count = 0;
    word_0240 = 0;
    sound_enabled = 0;
    screen_prepared = 0;
    word_01dc = 2;
    asset_flag = -1;
    word_0248 = 1;
    play_sound(0, 0);
    hardware_setup();
    memory_map_init();
    display_state_reset();
    random_seed = 0x1234;
    screens_flip();
    screens_flip();
    message_next = loading_text;
    tick_hold_26c = 1;
    message_request(20);

    for (int k = 1; k <= 4; k++)
        ptr_0cfa[k] = ptr_0cfa[k - 1] + 0xc8;
    for (int k = 1; k <= 9; k++)
        *(uint32_t *)(table_199ac + 0x24 * k) = *(uint32_t *)table_199ac + 0x8c * k;
    param_index = 0;
    word_025c = 0;
    word_0266 = 0;
    word_0384 = 0;
    menu_active = 0;
    has_continue = 0;
    param_index = 1;
    flags_0ddd |= 0x10;
    asset_flag = -1;
    assets_load(first_asset_name, palette_words, asset_memory, (struct AssetEntry *)image_table);
    asset_end_28ec = asset_mem_end;
    asset_table_28f0 = (uint8_t *)asset_table_end;
    asset_end_0ca2 = asset_mem_end;
    asset_table_0ca6 = (uint8_t *)asset_table_end;
    word_28ea = 0;

    /* the memory end is moved back to the start of the last sprite's data */
    uint8_t *last = (uint8_t *)(uintptr_t)((uint32_t *)asset_table_end)[-1];

    asset_mem_end = last;
    asset_end_28ec = asset_mem_end;
}

/* orig $000084: the entry of the menu program (the loader has put its image in memory and runs this): the start-up in C, then the
 * original's code goes on at $240C on the stack the memory map made.  The original's menu start ($6388, which that code reaches
 * once the copy protection has run) is made to enter menu_start, so the menu runs in C */
void menu_entry(void)
{
    menu_boot();
    menu_start_code[0] = 0x4e;   /* jmp menu_start */
    menu_start_code[1] = 0xf9;
    menu_start_code[2] = (uint8_t)((uintptr_t)menu_start >> 24);
    menu_start_code[3] = (uint8_t)((uintptr_t)menu_start >> 16);
    menu_start_code[4] = (uint8_t)((uintptr_t)menu_start >> 8);
    menu_start_code[5] = (uint8_t)(uintptr_t)menu_start;
    jump_with_stack(original_start, stack_top);
}
