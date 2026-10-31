/* Gods: the game's variables at the addresses the original keeps them at (chip RAM, set by the decrunched image).
 * The code is placed by the loader; only data is pinned here. */
palette_ptr     = 0x1af6;
palette_dirty   = 0x2cc2;
vbl_saturating  = 0x2cc4;
vbl_count       = 0x2cc6;
screen_front    = 0x2940;
screen_back     = 0x2944;
