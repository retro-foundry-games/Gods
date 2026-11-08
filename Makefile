# Gods (projects/gods/native): the C of the intro and of the game, built with m68k-amigaos-gcc.
#
#   make              both programs linked: build/intro, build/game (with their maps and symbol lists)
#   make intro | game one of them
#   make game-blob    the game as a blob at $(GAME_BASE) for the checks against the original (build/game.blob, game_syms.lua)
#   make adf          the disk whose game is the intro's C (build/..., local/gods-c-disk1.adf); needs the original disks in local/
#   make regen        write the generated sources in src/ again (needs the recordings in build/, see ../PLAN.md)
#   make sdl          the SDL3 port (src/sdl/), build/sdl/gods-sdl and gods-sdl-dev; make sdl-run builds and runs gods-sdl
#   make clean
#
# The toolchain is in the flake of this directory: outside its shell every target is run again inside it.  Variables:
# GODS_CFLAGS, GODS_OPT (the intro's optimisation, default -O2), GAME_BASE (default C0000, hex), PYTHON, PIRATED (any value: the game as
# it runs when the copylock fails, -DPIRATED: no extra life, less armour; without it the lock has passed).

CC     := m68k-amigaos-gcc
AS     := vasmm68k_mot
LD     := m68k-amigaos-ld
NM     := m68k-amigaos-nm
SRC    := src
# the Amiga-specific files: the assembly and the custom chip registers
AMIGA  := $(SRC)/amiga
BUILD  := build
PYTHON ?= uv run python
GODS_OPT ?= -O2
GAME_BASE ?= C0000

ifeq ($(shell command -v $(CC) 2>/dev/null),)
# not in the toolchain's shell: run the same target there
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(CURDIR) --command $(MAKE) --no-print-directory $@
else

COMMON := -m68000 -ffreestanding -fno-tree-loop-distribute-patterns -fno-tree-forwprop -fno-ivopts -nostdlib -fdata-sections
INTRO_CFLAGS := $(GODS_CFLAGS) $(COMMON) $(GODS_OPT)
GAME_CFLAGS := $(COMMON) -O2 -ffunction-sections -DMATCH_MEMORY_LAYOUT $(if $(PIRATED),-DPIRATED)
WARN := -Wall -Wextra

# the game program: one object for each module (the sources are split by what the code does, see src/menu.h)
GAME_MODULES := menu disk alloc decrunch input draw message file boot game data
GAME_OBJS := $(GAME_MODULES:%=$(BUILD)/game_%.o) $(BUILD)/game_display.o $(BUILD)/game_memory.o $(BUILD)/game_amiga.o
INTRO_OBJS := $(BUILD)/intro.o $(BUILD)/intro_amiga.o $(BUILD)/intro_data.o

.PHONY: all intro game game-blob adf regen clean sdl sdl-run
all: intro game
intro: $(BUILD)/intro
game: $(BUILD)/game

$(BUILD):
	mkdir -p $@

# intro
$(BUILD)/intro.o: $(SRC)/intro.c | $(BUILD)
	$(CC) $(INTRO_CFLAGS) $(WARN) -MMD -MP -c $< -o $@
$(BUILD)/intro_data.o: $(SRC)/intro_data.c | $(BUILD)
	$(CC) $(INTRO_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/intro_amiga.o: $(AMIGA)/intro_amiga.asm $(AMIGA)/intro_offsets.i | $(BUILD)
	$(AS) -Fhunk -no-opt -quiet -nowarn=62 -I$(AMIGA) -o $@ $<
$(BUILD)/intro: $(INTRO_OBJS) $(SRC)/intro.ld
	$(LD) -m amiga -T $(SRC)/intro.ld -Map $(BUILD)/intro.map -o $@ $(INTRO_OBJS)
	$(NM) $@ > $(BUILD)/intro.sym

# game
$(BUILD)/game_data.o: $(SRC)/data.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/game_%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) $(WARN) -MMD -MP -c $< -o $@
$(BUILD)/game_display.o: $(AMIGA)/display.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) $(WARN) -MMD -MP -c $< -o $@
$(BUILD)/game_memory.o: $(AMIGA)/memory.c | $(BUILD)
	$(CC) $(GAME_CFLAGS) $(WARN) -MMD -MP -c $< -o $@
$(BUILD)/game_amiga.o: $(AMIGA)/amiga.asm | $(BUILD)
	$(AS) -Fhunk -no-opt -quiet -nowarn=62 -I$(AMIGA) -o $@ $<
$(BUILD)/game: $(GAME_OBJS) $(SRC)/game.ld
	$(LD) -m amiga -T $(SRC)/game.ld -Map $(BUILD)/game.map -o $@ $(GAME_OBJS)
	$(NM) $@ > $(BUILD)/game.sym

# what is made from the executables: the blob, the disk
game-blob:
	$(PYTHON) ../scripts/build_game.py $(GAME_BASE)
adf:
	$(PYTHON) ../scripts/build_adf.py

# the generated sources (they are kept in src/); their generators read the recordings
regen:
	$(PYTHON) ../scripts/intro_offsets.py
	$(PYTHON) ../scripts/gen_intro_amiga_asm.py
	$(PYTHON) ../scripts/gen_game_symbols.py
	$(PYTHON) ../scripts/gen_game_amiga_asm.py

# the SDL3 port for a Linux desktop (src/sdl/): make sdl builds build/sdl/gods-sdl (the game) and gods-sdl-dev (the tests), make sdl-run runs the game
sdl:
	$(MAKE) --no-print-directory -C $(SRC)/sdl
sdl-run:
	$(MAKE) --no-print-directory -C $(SRC)/sdl run

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/intro $(BUILD)/game $(BUILD)/intro.map $(BUILD)/game.map $(BUILD)/intro.sym $(BUILD)/game.sym

-include $(BUILD)/*.d
endif
