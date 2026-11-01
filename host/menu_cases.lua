-- The functions of menu.c that host/menu_oracle.lua checks against the original: where the original has it, the registers its
-- arguments come in (".w": only the low word counts, ".l" or a bare name: the whole register), the register its result is in,
-- and how many calls to compare.  A function is in this list from the moment it is decompiled.
return {
    { name = "clear_work_area",      orig = 0x00f518, args = {} },
    { name = "take_pending_request", orig = 0x01cf26, args = {},                       ret = "d0" },
    { name = "clear_screen_buffer",  orig = 0x01cfac, args = { "a0" } },
    { name = "copy_screen_buffer",   orig = 0x01cfe4, args = { "a0", "a1" } },
    { name = "find_color01_move",    orig = 0x01d22a, args = { "a2" },                 ret = "a2" },
    { name = "shadow_set",           orig = 0x01d2d8, args = { "d0.w", "d1.w" } },
    { name = "shadow_get",           orig = 0x01d2ea, args = { "d0.w" },               ret = "d1.w" },
    { name = "copy_sectors",         orig = 0x01d4e4, args = { "a0", "a1", "d0.w" } },
    { name = "clear_track_slots",    orig = 0x01de42, args = { "a5" } },
    { name = "mfm_decode_long",      orig = 0x01de76, args = { "a0" },                 ret = "d0" },
    { name = "mfm_checksum",         orig = 0x01de9c, args = { "a5" },                 ret = "d0" },
    { name = "mfm_encode_long",      orig = 0x01e03a, args = { "d0", "a0" },           ret = "a0" },
    { name = "unpack",               orig = 0x01e260, args = { "a0", "a2" },           ret = "a0" },
    { name = "blitter_wait",         orig = 0x01dfd8, args = {} },
    { name = "alloc_init",           orig = 0x01d48e, args = {} },
    { name = "input_poll",           orig = 0x01e39c, args = {} },
}
