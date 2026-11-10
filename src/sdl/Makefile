# Gods, SDL3 port of the C intro and menu program for a Linux desktop.
#
#   make -C projects/gods/native/src/sdl            build projects/gods/native/build/sdl/gods-sdl (the game as it is played: intro, menu and levels in one program, no options)
#                                                   and gods-sdl-dev (the same with the options, the lockstep with the original and the tests)
#   make -C projects/gods/native/src/sdl run        build and run gods-sdl (disks from GODS_DISKS)
#   make -C projects/gods/native/src/sdl clean
#
# It compiles the Amiga program's C unchanged, as a 32-bit executable (the program keeps 32-bit pointers in its data and
# structs of the 68000's layout) with the chips, the disk layer, the sound and the asm routines replaced by the C in this
# folder.  Outside the flake's shell every target is run again inside it.
#
#   DEBUG=1      -O0 -ggdb everywhere (the objects are made again when the flags change)
#   MATCH_MEMORY_LAYOUT=0   struct Gods without its padding (PAD() is nothing): its members are not at the original's addresses
#   GODS_DISKS   (make run) the folder with gods-disk1.adf and gods-disk2.adf (default: ../../../local); gods-sdl itself takes --disks DIR, default the working folder

HERE   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
NATIVE := $(abspath $(HERE)/../..)
SRC    := $(NATIVE)/src
BUILD  := $(NATIVE)/build/sdl
GODS_DISKS ?= $(abspath $(NATIVE)/../local)
PYTHON ?= python3
CC := gcc   # the host compiler, not the Amiga cross compiler the flake shell also has

# (the makefiles are not made by make's built-in rules: Makefile.web would make it try to tangle one)
.DEFAULT_GOAL := all
Makefile Makefile.web: ;

ifeq ($(IN_NIX_SHELL),)
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(NATIVE) --command $(MAKE) --no-print-directory -f $(lastword $(MAKEFILE_LIST)) $@
else

# the game's C is the 68000's: 16-bit alignment of structs, 32-bit pointers
ifdef DEBUG
OPT_GAME := -O0 -ggdb
OPT_HOST := -O0 -ggdb
# the compiler wrapper turns _FORTIFY_SOURCE on, which needs -O: not in a debug build
export NIX_HARDENING_ENABLE := $(filter-out fortify fortify3,$(NIX_HARDENING_ENABLE))
else
OPT_GAME := -O1 -g
OPT_HOST := -O2 -g
endif
GAME_CFLAGS := -m32 -fpack-struct=2 -std=gnu11 $(OPT_GAME) -fno-strict-aliasing -ffunction-sections -fdata-sections \
               -DGODS_SDL -DGODS_HEAP_IN_GUEST $(if $(filter 0,$(MATCH_MEMORY_LAYOUT)),,-DMATCH_MEMORY_LAYOUT) $(if $(PIRATED),-DPIRATED) -I$(SRC) -I$(HERE) -Wall -Wno-unused-function
HOST_CFLAGS := -m32 -std=gnu11 $(OPT_HOST) -Wall $(shell pkg-config --cflags sdl3)
HOST_LIBS   := -m32 $(shell pkg-config --libs sdl3) -lpthread -lm

FLAGS_STAMP := $(BUILD)/.cflags
$(shell mkdir -p $(BUILD); echo '$(GAME_CFLAGS) | $(HOST_CFLAGS)' | cmp -s - $(FLAGS_STAMP) || echo '$(GAME_CFLAGS) | $(HOST_CFLAGS)' > $(FLAGS_STAMP))

# the program's modules (as native/Makefile's MENU_MODULES) and the C that replaces the Amiga parts
MENU_MODULES := menu disk alloc decrunch input draw message file boot game data
GAME_OBJS := $(BUILD)/g_shims.o $(BUILD)/g_display.o $(BUILD)/g_sound.o $(BUILD)/g_guest.o $(BUILD)/g_guest_load.o $(BUILD)/g_irq.o $(BUILD)/g_files.o $(MENU_MODULES:%=$(BUILD)/g_%.o) $(BUILD)/g_stubs.o
HOST_OBJS := $(BUILD)/h_host.o

.PHONY: all run clean
all: $(BUILD)/gods-sdl $(BUILD)/gods-sdl-dev

$(BUILD):
	mkdir -p $@

# the stubs and the table list come from game.symbols
$(BUILD)/g_stubs.c $(BUILD)/guest_tables.h: $(HERE)/gen_guest.py $(SRC)/game.symbols $(SRC)/game.ld | $(BUILD)
	$(PYTHON) $(HERE)/gen_guest.py $(SRC) $(BUILD)
$(BUILD)/guest_fixups.h: $(HERE)/gen_fixups.py $(SRC)/game.symbols | $(BUILD)
	$(PYTHON) $(HERE)/gen_fixups.py $(SRC) $(BUILD)

$(BUILD)/g_data.o: $(SRC)/data.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_stubs.o: $(BUILD)/g_stubs.c
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_shims.o: $(HERE)/shims.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_display.o: $(HERE)/display.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_sound.o: $(HERE)/sound.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_sound_dev.o: $(HERE)/sound.c $(FLAGS_STAMP) | $(BUILD)   # (the development program's: the voices' log, GODS_SOUNDLOG)
	$(CC) $(GAME_CFLAGS) -DGODS_DEV -MMD -MP -c $< -o $@
$(BUILD)/g_guest.o: $(HERE)/guest.c $(BUILD)/guest_tables.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@
$(BUILD)/g_guest_load.o: $(HERE)/guest_load.c $(BUILD)/guest_tables.h $(BUILD)/guest_fixups.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@
$(BUILD)/g_irq.o: $(HERE)/irq.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_files.o: $(HERE)/files.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_draw.o: $(SRC)/draw.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -fno-inline -MMD -MP -c $< -o $@   # (blit_masked_image is replaced by display.c: no copy of it inside)
$(BUILD)/g_%.o: $(SRC)/%.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/h_%.o: $(HERE)/%.c $(BUILD)/guest_tables.h $(BUILD)/guest_tables.h $(BUILD)/guest_fixups.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(HOST_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@
# the regular program has --disks DIR (default: the working folder) and --help
$(BUILD)/h_main.o: $(HERE)/main.c $(HERE)/host.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(HOST_CFLAGS) -I$(BUILD) -MMD -MP -c $< -o $@
# what the game's thread does for the tests (the development program only)
$(BUILD)/g_dev_game.o: $(HERE)/dev_game.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@

# the intro: its C and its data are compiled with the program's flags and linked into one object, in which every symbol but the few entry
# points is made local (the intro's names are those of the menu program's: glyph_draw, screens_flip, blit_image ...)
INTRO_OBJS := $(BUILD)/g_intro_host.o $(BUILD)/g_intro.o $(BUILD)/g_intro_data.o $(BUILD)/g_intro_assets.o
INTRO_EXPORTS := game_main ih_vbl ih_set_fire ih_shown_screen ih_color ih_music_mix ih_load_assets
$(BUILD)/g_intro_host.o: $(HERE)/intro_host.c $(HERE)/intro_host.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/g_intro_data.o: $(SRC)/intro_data.c $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -w -MMD -MP -c $< -o $@
$(BUILD)/g_intro_assets.o: $(HERE)/intro_assets.c $(HERE)/intro_host.h $(FLAGS_STAMP) | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/intro_program.o: $(INTRO_OBJS)
	ld -m elf_i386 -r -o $(BUILD)/intro_all.o $(INTRO_OBJS)
	nm --defined-only -g $(BUILD)/intro_all.o | awk '{print $$3}' | grep -v '^__x86' $(foreach s,$(INTRO_EXPORTS),| grep -vx $(s)) > $(BUILD)/intro_local.txt
	objcopy --localize-symbols=$(BUILD)/intro_local.txt $(BUILD)/intro_all.o $@

# one program: the intro, the menu and the game, and the machine around them: gods-sdl is the game as it is played (main.c: no options), gods-sdl-dev has the options, the lockstep
# with the original and the other tests (devmain.c, dev_game.c)
COMMON_OBJS := $(HOST_OBJS) $(BUILD)/h_intro_view.o $(BUILD)/intro_program.o $(GAME_OBJS)
$(BUILD)/gods-sdl: $(BUILD)/h_main.o $(COMMON_OBJS)
	$(CC) -m32 -no-pie -Wl,--allow-multiple-definition -o $@ $(BUILD)/h_main.o $(COMMON_OBJS) $(HOST_LIBS)
DEV_OBJS := $(filter-out $(BUILD)/g_sound.o,$(COMMON_OBJS)) $(BUILD)/g_sound_dev.o
$(BUILD)/gods-sdl-dev: $(BUILD)/h_devmain.o $(BUILD)/g_dev_game.o $(DEV_OBJS)
	$(CC) -m32 -no-pie -Wl,--allow-multiple-definition -o $@ $(BUILD)/h_devmain.o $(BUILD)/g_dev_game.o $(DEV_OBJS) $(HOST_LIBS)

run: $(BUILD)/gods-sdl
	$(BUILD)/gods-sdl --disks $(GODS_DISKS)

clean:
	rm -rf $(BUILD)

-include $(BUILD)/*.d
endif
