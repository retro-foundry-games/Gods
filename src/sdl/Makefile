# Gods, SDL3 port of the C intro and menu program for a Linux desktop.
#
#   make -C projects/gods/native/src/sdl            build projects/gods/native/build/sdl/gods-sdl (the intro, which starts
#                                                   gods-sdl-menu, the menu program, when fire is pressed)
#   make -C projects/gods/native/src/sdl run        build and run (disks from projects/gods/local)
#   make -C projects/gods/native/src/sdl clean
#
# It compiles the Amiga program's C unchanged, as a 32-bit executable (the program keeps 32-bit pointers in its data and
# structs of the 68000's layout) with the chips, the disk layer, the sound and the asm routines replaced by the C in this
# folder.  Outside the flake's shell every target is run again inside it.
#
#   GODS_DISKS   the folder with gods-disk1.adf and gods-disk2.adf (default: ../../../local)

HERE   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
NATIVE := $(abspath $(HERE)/../..)
SRC    := $(NATIVE)/src
BUILD  := $(NATIVE)/build/sdl
GODS_DISKS ?= $(abspath $(NATIVE)/../local)
PYTHON ?= python3
CC := gcc   # the host compiler, not the Amiga cross compiler the flake shell also has

ifeq ($(IN_NIX_SHELL),)
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(NATIVE) --command $(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) $@
else

# the game's C is the 68000's: 16-bit alignment of structs, 32-bit pointers
GAME_CFLAGS := -m32 -fpack-struct=2 -std=gnu11 -O1 -g -fno-strict-aliasing -ffunction-sections -fdata-sections \
               -DGODS_SDL -I$(SRC) -I$(HERE) -Wall -Wno-unused-function
HOST_CFLAGS := -m32 -std=gnu11 -O2 -g -Wall $(shell pkg-config --cflags sdl3)
HOST_LIBS   := -m32 $(shell pkg-config --libs sdl3) -lpthread -lm

# the program's modules (as native/Makefile's MENU_MODULES) and the C that replaces the Amiga parts
MENU_MODULES := menu disk alloc decrunch input screen draw message ui file boot game game_a game_b game_c game_d game_e game_f game_g menu_data
GAME_OBJS := $(BUILD)/g_shims.o $(BUILD)/g_display_sdl.o $(MENU_MODULES:%=$(BUILD)/g_%.o) $(BUILD)/g_stubs.o
HOST_OBJS := $(BUILD)/h_main.o $(BUILD)/h_guest.o $(BUILD)/h_files.o $(BUILD)/h_render.o $(BUILD)/h_blitter.o $(BUILD)/h_blitter_regs.o \
             $(BUILD)/h_irq.o $(BUILD)/h_sound.o

.PHONY: all run clean
all: $(BUILD)/gods-sdl $(BUILD)/gods-sdl-menu

$(BUILD):
	mkdir -p $@

# the linker script and the stubs come from menu.symbols
$(BUILD)/guest.ld $(BUILD)/g_stubs.c $(BUILD)/guest_tables.h: $(HERE)/gen_guest.py $(SRC)/menu.symbols $(SRC)/menu.ld | $(BUILD)
	$(PYTHON) $(HERE)/gen_guest.py $(SRC) $(BUILD)
$(BUILD)/guest_fixups.h: $(HERE)/gen_fixups.py $(SRC)/menu.symbols | $(BUILD)
	$(PYTHON) $(HERE)/gen_fixups.py $(SRC) $(BUILD)

$(BUILD)/g_menu_data.o: $(SRC)/menu_data.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_stubs.o: $(BUILD)/g_stubs.c
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_shims.o: $(HERE)/shims.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_display_sdl.o: $(HERE)/display_sdl.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_draw.o: $(SRC)/draw.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -fno-inline -MMD -MP -c $< -o $@   # (blit_masked_image is replaced by blitter.c: no copy of it inside)
$(BUILD)/g_%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/h_%.o: $(HERE)/%.c $(BUILD)/guest_tables.h $(BUILD)/guest_fixups.h | $(BUILD)
	$(CC) $(HOST_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@

# the host objects come first, so their routines are the ones the program calls (the Amiga's asm is not linked)
$(BUILD)/gods-sdl-menu: $(HOST_OBJS) $(GAME_OBJS) $(BUILD)/guest.ld
	$(CC) -m32 -no-pie -Wl,--allow-multiple-definition -o $@ $(HOST_OBJS) $(GAME_OBJS) $(BUILD)/guest.ld $(HOST_LIBS)

# the intro: its data is C, no guest memory layout is needed
INTRO_OBJS := $(BUILD)/h_intro_sdl.o $(BUILD)/g_intro_host.o $(BUILD)/g_intro.o $(BUILD)/g_intro_data.o
$(BUILD)/g_intro_host.o: $(HERE)/intro_host.c $(HERE)/intro_host.h | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_intro_data.o: $(SRC)/intro_data.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/gods-sdl: $(INTRO_OBJS)
	$(CC) -m32 -no-pie -o $@ $(INTRO_OBJS) $(HOST_LIBS)

run: $(BUILD)/gods-sdl $(BUILD)/gods-sdl-menu
	$(BUILD)/gods-sdl --disks $(GODS_DISKS)

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.d
endif
