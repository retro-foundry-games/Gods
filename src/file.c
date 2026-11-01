/* Gods, the menu program: loading files from the disks */
#include "menu.h"

/* orig $01D3A6: read the file `name` to `dest`.  A file cannot be read twice onto the screen that is on show while the colours
 * are lit: then it goes to address 0.  When the disk does not give it, the screen flashes for ever. */
void load_file(const char *name, void *dest)
{
    if (palette_lit && (uint8_t *)dest == screen_other)
        dest = 0;
    if (file_read(name, dest) < 0)
        for (;;)
            custom->color[0] = (uint16_t)tick_count;
}

/* orig $002702 (and $002718, the part after the name is made): load the packed file whose name is "c" and then `name`, and unpack it to `dest`.  The packed file is read to
 * `dest` first, unpacked on to the screen buffer and copied back (at most 64 KB come back).  The name is put together in the buffer line_text points at */
void load_unpacked(const char *name, void *dest)
{
    char *buffer = (char *)line_text;

    *buffer++ = 'c';
    while ((*buffer = *name++) != 0)
        buffer++;
    load_file((const char *)line_text, dest);

    uint8_t *packed_end = unpack(screen_buffer, dest);
    int32_t size = packed_end - screen_buffer;
    const uint8_t *from = screen_buffer;
    uint8_t *to = dest;

    uint16_t n = (uint16_t)(size - 1);   /* the count is a 16-bit number: a result of 64 KB or more is cut, as the original's is */

    do {
        *to++ = *from++;
    } while (n-- != 0);
}

/* orig $002988: set up the memory lists and read the directory of the disks (tocache.bin, 2048 bytes) into the first list */
void load_directory(void)
{
    alloc_init();
    load_file((const char *)tocache_name, background);

    const uint32_t *from = (const uint32_t *)background;
    uint32_t *to = (uint32_t *)mem_lists.a_head;

    for (int i = 0; i < 512; i++)
        *to++ = *from++;
}

/* orig $01D36E: the scroll of the second screen's copper list is reset (twice), the colour moves of the copper list are as
 * the program starts them */
void display_reset(void)
{
    x_offset = 0;
    set_scroll(copper_list_b, 0);
    set_scroll(copper_list_b, 0);
    copper_move_a = 0x00840000;
    copper_move_b = 0x00865246;
}

/* orig $01ECA8: the list of file names for an asset name (assets_load).  The name is compared with the names of
 * asset_names as far as the table's name goes; the number of the name found picks the list.  0 when none matches */
static const uint8_t *asset_list(const uint8_t *name)
{
    const uint8_t *k = asset_names;
    uint32_t index = 0;

    for (;;) {
        const uint8_t *n = name;

        while (*k != 0 && (int8_t)*k >= 0) {
            if (*n++ != *k++)
                goto mismatch;
        }
        {
            const uint8_t *entry = asset_names + 86 + 4 * index;   /* the long of the table, big endian */

            return (const uint8_t *)(uintptr_t)(((uint32_t)entry[0] << 24) | (entry[1] << 16) | (entry[2] << 8) | entry[3]);
        }
mismatch:
        for (;;) {
            uint8_t c = *k++;

            if ((int8_t)c < 0)
                return 0;
            if (c == 0)
                break;
        }
        index++;
    }
}

/* orig $01ECA2 and $01EE3C: load the pictures of an asset set.  `keyword` names the set (asset_names); each
 * of its files is read as a picture (name + ext_picture, unpacked: the resolution word, 16 colours) and a table (name + ext_data):
 * a count and records of 10 bytes (width, height less one, ?, x, y) of sprites to cut out of the picture.  The colours go to
 * `palette`; the picture to the screen buffer; each sprite is made into four planes at `memory` and gets an entry in `table`.
 * asset_mem_end and asset_table_end say where they ended.  With asset_flag not negative a bit of shift_bits picks for each
 * record whether it is cut out (otherwise it only gets its entry) */
void assets_load(const char *keyword, uint16_t *palette, uint8_t *memory, struct AssetEntry *table)
{
    const uint8_t *list;

    asset_mem_end = memory;
    asset_table_end = table;
    list = asset_list((const uint8_t *)keyword);
    if (list == 0)
        return;
    asset_mem_end = memory;
    asset_table_end = table;
    if (*list == 0)
        return;

    do {
        char *name = asset_name;
        char *base_end;
        const uint8_t *ext;

        do {
            *name++ = *list++;
        } while (*list != 0);
        base_end = name;
        list++;
        ext = asset_extensions;
        do {
            *name++ = *ext++;
        } while (ext[-1] != 0);
        load_unpacked(asset_name, picture_buffer);

        const uint16_t *picture = (const uint16_t *)picture_buffer;

        picture_header = *picture++;
        for (int i = 0; i < 16; i++)
            palette[i] = *picture++;

        name = base_end;
        ext = asset_extensions + 6;
        do {
            *name++ = *ext++;
        } while (ext[-1] != 0);
        load_file(asset_name, asset_directory + 20);

        picture = (const uint16_t *)(picture_buffer + 34);
        for (int i = 0; i < 16000; i++)
            ((uint16_t *)screen_buffer)[i] = *picture++;

        const uint16_t *record = (const uint16_t *)(asset_directory + 20);
        uint16_t count = *record++;
        uint16_t records = (uint16_t)(count - 1);

        do {
            int16_t width = record[0];
            int16_t height = record[1];
            /* the sprite's place in the picture: 160 bytes a row (320 pixels of four planes), a word of arithmetic as in the original */
            int16_t offset = (int16_t)(((uint16_t)record[3] >> 1) + (uint16_t)(((uint16_t)record[4] << 5) * 5));
            int copy = 1;

            if (asset_flag >= 0) {
                uint32_t bits = shift_bits;

                shift_bits = bits << 1;
                copy = (bits & 0x80000000u) != 0;
                if (--shift_count < 0) {
                    shift_bits = *shift_source++;
                    shift_count = 31;
                }
            }
            table->width_negative = -width;
            table->height_less_one = height;
            table->planes = memory;
            table++;
            if (copy) {
                const uint16_t *from = (const uint16_t *)(screen_buffer + offset);
                uint16_t columns = (uint16_t)width >> 4;
                uint16_t skip = (uint16_t)((19 - columns) << 3);
                uint16_t plane = (uint16_t)(((columns + 1) << 3) * (uint16_t)(height + 1)) >> 2;   /* bytes of one plane */
                uint16_t *to = (uint16_t *)memory;

                for (int row = 0; row <= height; row++) {
                    for (int c = 0; c <= columns; c++) {
                        *to = *from++;
                        *(uint16_t *)((uint8_t *)to + (int16_t)plane) = *from++;
                        *(uint16_t *)((uint8_t *)to + (int16_t)(uint16_t)(plane * 2)) = *from++;
                        *(uint16_t *)((uint8_t *)to + (int16_t)(uint16_t)(plane * 3)) = *from++;
                        to++;
                    }
                    from = (const uint16_t *)((const uint8_t *)from + skip);
                }
                memory = (uint8_t *)to + (int16_t)(uint16_t)(plane * 3);
            }
            record += 5;
        } while (records-- != 0);
        asset_mem_end = memory;
        asset_table_end = table;
    } while (*list != 0);
}
