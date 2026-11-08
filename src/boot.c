/* Gods, the menu program: the start (the loader enters it at $84 with the stack at $78000): the chips, the memory map, the
 * sound, the first asset set.  What follows (the part at $2400 that loads the copy protection's stub and the code it
 * releases) is not here. */
#include "menu.h"

/* amiga.asm: the status register (a value of $2000 is the supervisor state with all interrupts enabled) */
void sr_set(uint16_t value);
void keyboard_irq(void);
void audio_irq(void);
void vbl_irq(void);
/* the stack is set to `stack` and the code at `address` is entered (it does not come back) */
void jump_with_stack(void (*address)(void), uint8_t *stack);

/* orig $039FA6: the two screens and the display registers: 320 x 192, four bitplanes */
void display_setup(void)
{
    guest.screen_buffer = guest.screen_1;
    guest.screen_other = guest.screen_2;
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
    uint32_t *state = (uint32_t *)guest.voice;
    uint8_t *silence = guest.sound_silence;   /* the silent sample, in the data the loader brings */

    guest.sound_lock = 0xff;
    CIAA_PRA |= 0x02;
    *(uint32_t *)guest.sound_state_2079e = 0;
    *(uint16_t *)(guest.sound_state_2079e + 4) = 0;
    *(uint32_t *)(guest.sound_state_2079e + 6) = 0;
    *(uint16_t *)(guest.sound_state_2079e + 10) = 0;
    custom->dmacon = 0x820f;
    custom->intena = 0x0780;
    custom->intreq = 0x0780;
    custom->adkcon = 0x00ff;
    for (int i = 0; i < 277; i++)   /* the voices and the state after them */
        state[i] = 0;
    for (int i = 0; i < 4; i++)
        guest.voice[i].number = i;
    for (int i = 0; i < 4; i++) {
        custom->aud[i].vol = 0;
        custom->aud[i].len = 1;
        custom->aud[i].per = 1;
        guest.voice[i].sample_a = silence;
        guest.voice[i].sample_b = silence;
        custom->aud[i].lc = ADDR(silence);
        guest.voice[i].field_28 = 8;
        custom->aud[i].len = 8;
        guest.voice[i].field_e = 2;
        custom->aud[i].per = 0x100;
    }
    custom->dmacon = 0x000f;
    for (int i = 0; i < 4; i++) {
        guest.voice[i].buffer = guest.voice_buffers + 0x80 * i;
        guest.voice[i].channel = (struct AudioChannel *)&custom->aud[i];
        guest.voice[i].dma_bit = 1 << i;
        guest.voice[i].dma_on = 0x8000 | (1 << i);
        guest.voice[i].int_bit = 0x80 << i;
        guest.voice[i].int_on = 0x8000 | (0x80 << i);
    }
    *(uint16_t *)&guest.sound_lock = 0;
}

/* orig $039FDC: the colours are black, the interrupt vectors are set (keyboard, vertical blank, audio), the sound is set up,
 * the copper list is started and the DMA and the interrupts are enabled; then the interrupts are let through */
void chips_setup(void)
{
    for (int i = 0; i < 16; i++)
        custom->color[i] = 0;
    guest.irq_vectors[0] = (uint32_t)(uintptr_t)keyboard_irq;
    guest.irq_vectors[1] = (uint32_t)(uintptr_t)vbl_irq;
    guest.irq_vectors[2] = (uint32_t)(uintptr_t)audio_irq;
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
    CIAB_ICR = 0x6f;   /* CIA B: the interrupts of the disk index and the others off, then the one on */
    CIAB_ICR = 0x90;
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
    guest.unit_scripts = guest.unit_script_data;
    guest.cell_entry_table = (struct EntryRef *)guest.cell_entry_data;
    guest.creature_entries = (struct CreatureEntry *)guest.creature_entry_data;
    guest.monster_types = guest.monster_type_data;
    guest.path_description_base = guest.path_descriptions;
}

/* orig $03A060: the memory map: each buffer of the program is a pointer into one block that begins at program_end, a buffer
 * after the other, and the pointers are kept in these variables */
void memory_map_init(void)
{
    uint8_t *p = guest.program_end + 0x300;

    guest.stack_top = p;
    p += 0x32;
    guest.asset_memory = p;
    p += 0x21d68;
    guest.line_text_default = p;
    guest.line_text = p;
    p += 0x640;
    guest.target_map_default = p;
    guest.target_map = p;
    p += 0x190;
    guest.asset_directory = p;
    guest.asset_directory_default = p;
    p += 0x960;
    guest.memory_base = p;
    guest.background = p;
    p += 0x8500;
    guest.entry_list = (struct Entry8 *)p;
    p += 0x1518;
    guest.copper_save_first = p;
    guest.copper_save = (uint16_t *)p;
    p += 0xc8;
    guest.monster_part_blocks = p;
    p += 0x960;
    *(uint32_t *)guest.creature_types = (uint32_t)(uintptr_t)p;
    p += 0x578;
    guest.random_words = p;
    p += 0x200;
    guest.image_table = (struct Image *)p;
    p += 0x1450;
    guest.work_area = (uint32_t *)p;
    clear_work_area();
    guest.copper_message_5254[2] = (uint16_t)(uintptr_t)p;          /* the copper list's bitplane pointer for the message: the work area */
    guest.copper_message_5254[0] = (uint16_t)((uintptr_t)p >> 16);
    p += 0x140;
    guest.level_map = p;
    p += 0x4000;
    guest.bytes_dest = p;   /* (the 0x200 bytes after the map: narrow_words_to_bytes makes its bytes there) */
    p += 0x200;
    guest.copper_colours = p;
    p += 0xc0;
    guest.unit_member_blocks[0] = p;
    p += 0x3e8;
    guest.script_records = (struct ScriptRecord *)p;
    p += 0x960;
    guest.message_offsets = p;
    p += 0x50;
    guest.unused_buffer_10 = p;
    p += 0xa;
    guest.bolt_slots = p;
    p += 0xa0;
    guest.path_records = (struct PathRecord *)p;
    p += 0x258;
    guest.unused_scratch_alias = p;
    guest.scratch_buffer = p;
    p += 0x300;
    guest.message_text_block = p;
    data_pointers_init();
    guest.hint_list = guest.hint_data;
    guest.restart_address = (uint8_t *)(uintptr_t)menu_boot;
}

/* orig $01F5C4: the state of the two screens starts again: the first has the saved colours' buffer, the second the buffer e58 points to */
void display_state_reset(void)
{
    uint32_t first = (uint32_t)(uintptr_t)guest.copper_save;
    uint32_t second = (uint32_t)(uintptr_t)guest.copper_save_second;

    guest.buffer_state[0].w[2] = 0;
    guest.buffer_state[0].w[0] = first >> 16;
    guest.buffer_state[0].w[1] = (uint16_t)first;
    guest.buffer_state[1].w[2] = 0;
    guest.buffer_state[1].w[0] = second >> 16;
    guest.buffer_state[1].w[1] = (uint16_t)second;
}

/* orig $039E50: the start.  The chips, the memory map and the sound are set up (the original moves the stack to stack_top here;
 * the C program keeps the stack it has), the screens are flipped twice, the message "loading game please wait" is asked for, the
 * first asset set is read, and where its data ended is noted.  The original goes on at $240C: the code that loads the copy
 * protection's stub; that is not decompiled */
void menu_boot(void)
{
    custom->dmacon = 0x7fff;
    guest.message_shown = 0;
    guest.scroll_ease_phase = 0;
    guest.message_arg = 0;
    guest.message_current = 0;
    guest.unused_5418 = 0;
    guest.tick_count = 0;
    guest.unused_240 = 0;
    guest.sound_enabled = 0;
    guest.screen_prepared = 0;
    guest.frame_wait = 2;
    guest.asset_flag = -1;
    guest.play_starting = 1;
    play_sound(0, 0);
    hardware_setup();
    memory_map_init();
    display_state_reset();
    guest.random_seed = 0x1234;
    screens_flip();
    screens_flip();
    guest.message_next = guest.loading_text;
    guest.tick_hold_26c = 1;
    message_request(20);

    for (int k = 1; k <= 4; k++)
        guest.unit_member_blocks[k] = guest.unit_member_blocks[k - 1] + 0xc8;
    for (int k = 1; k <= 9; k++)
        *(uint32_t *)(guest.creature_types + 0x24 * k) = *(uint32_t *)guest.creature_types + 0x8c * k;
    guest.param_index = 0;
    guest.unused_25c = 0;
    guest.world_b_files = 0;
    guest.world_loading = 0;
    guest.menu_active = 0;
    guest.has_continue = 0;
    guest.param_index = 1;
    guest.image_draw_flags |= 0x10;
    guest.asset_flag = -1;
    assets_load(guest.first_asset_name, guest.palette_words, guest.asset_memory, (struct AssetEntry *)guest.image_table);
    guest.asset_end_28ec = guest.asset_mem_end;
    guest.asset_table_28f0 = (uint8_t *)guest.asset_table_end;
    guest.asset_end_0ca2 = guest.asset_mem_end;
    guest.asset_table_0ca6 = (uint8_t *)guest.asset_table_end;
    guest.unused_28ea = 0;

    /* the memory end is moved back to the start of the last sprite's data */
    uint8_t *last = (uint8_t *)(uintptr_t)((uint32_t *)guest.asset_table_end)[-1];

    guest.asset_mem_end = last;
    guest.asset_end_28ec = guest.asset_mem_end;
}

/* orig $000084: the entry of the menu program (the loader has put its image in memory and runs this): the start-up in C, then the
 * original's code goes on at $240C on the stack the memory map made.  The original's menu start ($6388, which that code reaches
 * once the copy protection has run) is made to enter menu_start, so the menu runs in C */
void menu_entry(void)
{
    menu_boot();
    guest.menu_start_code[0] = 0x4e;   /* jmp menu_start */
    guest.menu_start_code[1] = 0xf9;
    guest.menu_start_code[2] = (uint8_t)((uintptr_t)menu_start >> 24);
    guest.menu_start_code[3] = (uint8_t)((uintptr_t)menu_start >> 16);
    guest.menu_start_code[4] = (uint8_t)((uintptr_t)menu_start >> 8);
    guest.menu_start_code[5] = (uint8_t)(uintptr_t)menu_start;
    jump_with_stack(original_start, guest.stack_top);
}
