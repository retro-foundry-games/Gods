# Gods, SDL3 port of the C intro and menu program for a Linux desktop.
#
#   make -C projects/gods/native/src/sdl            build projects/gods/native/build/sdl/gods-sdl (intro, menu and game in one program)
#   make -C projects/gods/native/src/sdl run        build and run (disks from projects/gods/local)
#   make -C projects/gods/native/src/sdl clean
#
# It compiles the Amiga program's C unchanged, as a 32-bit executable (the program keeps 32-bit pointers in its data and
# structs of the 68000's layout) with the chips, the disk layer, the sound and the asm routines replaced by the C in this
# folder.  Outside the flake's shell every target is run again inside it.
#
#   DEBUG=1      -O0 -ggdb everywhere (the objects are made again when the flags change)
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
ifdef DEBUG
OPT_GAME := -O0 -ggdb
OPT_HOST := -O0 -ggdb
else
OPT_GAME := -O1 -g
OPT_HOST := -O2 -g
endif
GAME_CFLAGS := -m32 -fpack-struct=2 -std=gnu11 $(OPT_GAME) -fno-strict-aliasing -ffunction-sections -fdata-sections \
               -DGODS_SDL -I$(SRC) -I$(HERE) -Wall -Wno-unused-function
HOST_CFLAGS := -m32 -std=gnu11 $(OPT_HOST) -Wall $(shell pkg-config --cflags sdl3)
HOST_LIBS   := -m32 $(shell pkg-config --libs sdl3) -lpthread -lm

FLAGS_STAMP := $(BUILD)/.cflags
$(shell mkdir -p $(BUILD); echo '$(GAME_CFLAGS) | $(HOST_CFLAGS)' | cmp -s - $(FLAGS_STAMP) || echo '$(GAME_CFLAGS) | $(HOST_CFLAGS)' > $(FLAGS_STAMP))

# the program's modules (as native/Makefile's MENU_MODULES) and the C that replaces the Amiga parts
MENU_MODULES := menu disk alloc decrunch input draw message ui file boot game menu_data
GAME_OBJS := $(BUILD)/g_shims.o $(BUILD)/g_display.o $(MENU_MODULES:%=$(BUILD)/g_%.o) $(BUILD)/g_stubs.o
HOST_OBJS := $(BUILD)/h_main.o $(BUILD)/h_guest.o $(BUILD)/h_files.o $(BUILD)/h_render.o $(BUILD)/h_blitter.o $(BUILD)/h_blitter_regs.o \
             $(BUILD)/h_irq.o $(BUILD)/h_sound.o

.PHONY: all run clean
all: $(BUILD)/gods-sdl

$(BUILD):
	mkdir -p $@

# the linker script and the stubs come from menu.symbols
$(BUILD)/guest.ld $(BUILD)/g_stubs.c $(BUILD)/guest_tables.h: $(HERE)/gen_guest.py $(SRC)/menu.symbols $(SRC)/menu.ld | $(BUILD)
	$(PYTHON) $(HERE)/gen_guest.py $(SRC) $(BUILD)
$(BUILD)/guest_fixups.h: $(HERE)/gen_fixups.py $(SRC)/menu.symbols | $(BUILD)
	$(PYTHON) $(HERE)/gen_fixups.py $(SRC) $(BUILD)

$(BUILD)/g_menu_data.o: $(SRC)/menu_data.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_stubs.o: $(BUILD)/g_stubs.c
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_shims.o: $(HERE)/shims.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_display.o: $(HERE)/display.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_draw.o: $(SRC)/draw.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -fno-inline -MMD -MP -c $< -o $@   # (blit_masked_image is replaced by blitter.c: no copy of it inside)
$(BUILD)/g_%.o: $(SRC)/%.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/h_%.o: $(HERE)/%.c $(BUILD)/guest_tables.h $(BUILD)/guest_fixups.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(HOST_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@

# the intro: its C and its data are compiled with the program's flags and linked into one object, in which every symbol but the few entry
# points is made local (the intro's names are those of the menu program's: glyph_draw, screens_flip, blit_image ...)
INTRO_OBJS := $(BUILD)/g_intro_host.o $(BUILD)/g_intro.o $(BUILD)/g_intro_data.o
INTRO_EXPORTS := game_main ih_vbl ih_set_fire ih_shown_screen ih_color ih_music_mix
$(BUILD)/g_intro_host.o: $(HERE)/intro_host.c $(HERE)/intro_host.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_intro_data.o: $(SRC)/intro_data.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/intro_program.o: $(INTRO_OBJS)
	ld -m elf_i386 -r -o $(BUILD)/intro_all.o $(INTRO_OBJS)
	nm --defined-only -g $(BUILD)/intro_all.o | awk '{print $$3}' | grep -v '^__x86' $(foreach s,$(INTRO_EXPORTS),| grep -vx $(s)) > $(BUILD)/intro_local.txt
	objcopy --localize-symbols=$(BUILD)/intro_local.txt $(BUILD)/intro_all.o $@

# one program: the intro, the menu and the game, and the machine around them
$(BUILD)/gods-sdl: $(HOST_OBJS) $(BUILD)/h_intro_view.o $(BUILD)/intro_program.o $(GAME_OBJS) $(BUILD)/guest.ld
	$(CC) -m32 -no-pie -Wl,--allow-multiple-definition -o $@ $(HOST_OBJS) $(BUILD)/h_intro_view.o $(BUILD)/intro_program.o $(GAME_OBJS) $(BUILD)/guest.ld $(HOST_LIBS)

run: $(BUILD)/gods-sdl
	$(BUILD)/gods-sdl --disks $(GODS_DISKS)

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.d
endif
