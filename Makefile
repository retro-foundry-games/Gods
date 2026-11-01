# Gods (projects/gods/native): the C of the intro and of the menu program, built with m68k-amigaos-gcc.
#
#   make              both programs linked: build/intro, build/menu (with their maps and symbol lists)
#   make intro | menu one of them
#   make menu-blob    the menu as a blob at $(MENU_BASE) for the checks against the original (build/menu.blob, menu_syms.lua)
#   make adf          the disk whose game is the intro's C (build/..., local/gods-c-disk1.adf); needs the original disks in local/
#   make regen        write the generated sources in src/ again (needs the recordings in build/, see ../PLAN.md)
#   make clean
#
# The toolchain is in the flake of this directory: outside its shell every target is run again inside it.  Variables:
# GODS_CFLAGS, GODS_OPT (the intro's optimisation, default -O2), MENU_BASE (default C0000, hex), PYTHON.

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
MENU_BASE ?= C0000

ifeq ($(shell command -v $(CC) 2>/dev/null),)
# not in the toolchain's shell: run the same target there
.PHONY: all $(MAKECMDGOALS)
all $(MAKECMDGOALS):
	@nix develop $(CURDIR) --command $(MAKE) --no-print-directory $@
else

COMMON := -m68000 -ffreestanding -fno-tree-loop-distribute-patterns -fno-tree-forwprop -fno-ivopts -nostdlib -fdata-sections
INTRO_CFLAGS := $(GODS_CFLAGS) $(COMMON) $(GODS_OPT)
MENU_CFLAGS := $(COMMON) -O2 -ffunction-sections
WARN := -Wall -Wextra

# the menu program: one object for each module (the sources are split by what the code does, see src/menu.h)
MENU_MODULES := menu disk alloc decrunch input screen draw display message ui file boot game game_a game_b game_c menu_data
MENU_OBJS := $(MENU_MODULES:%=$(BUILD)/menu_%.o) $(BUILD)/menu_amiga.o
INTRO_OBJS := $(BUILD)/intro.o $(BUILD)/intro_amiga.o $(BUILD)/intro_data.o

.PHONY: all intro menu menu-blob adf regen clean
all: intro menu
intro: $(BUILD)/intro
menu: $(BUILD)/menu

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

# menu
$(BUILD)/menu_menu_data.o: $(SRC)/menu_data.c | $(BUILD)
	$(CC) $(MENU_CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/menu_%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(MENU_CFLAGS) $(WARN) -MMD -MP -c $< -o $@
$(BUILD)/menu_amiga.o: $(AMIGA)/menu_amiga.asm | $(BUILD)
	$(AS) -Fhunk -no-opt -quiet -nowarn=62 -I$(AMIGA) -o $@ $<
$(BUILD)/menu: $(MENU_OBJS) $(SRC)/menu.ld
	$(LD) -m amiga -T $(SRC)/menu.ld -Map $(BUILD)/menu.map -o $@ $(MENU_OBJS)
	$(NM) $@ > $(BUILD)/menu.sym

# what is made from the executables: the blob, the disk
menu-blob:
	$(PYTHON) ../scripts/build_menu.py $(MENU_BASE)
adf:
	$(PYTHON) ../scripts/build_adf.py

# the generated sources (they are kept in src/); their generators read the recordings
regen:
	$(PYTHON) ../scripts/intro_offsets.py
	$(PYTHON) ../scripts/gen_intro_amiga_asm.py
	$(PYTHON) ../scripts/gen_menu_symbols.py
	$(PYTHON) ../scripts/gen_menu_amiga_asm.py

clean:
	rm -f $(BUILD)/*.o $(BUILD)/*.d $(BUILD)/intro $(BUILD)/menu $(BUILD)/intro.map $(BUILD)/menu.map $(BUILD)/intro.sym $(BUILD)/menu.sym

-include $(BUILD)/*.d
endif
