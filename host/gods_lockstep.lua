-- Gods: the original against the SDL port, side by side, frame-locked and given the same keys (tools/ami9000_host/lua/amiga/lockstep.lua, as fa18.lua does for the native port of FA-18).
--
--   tools/engine9000/ami9000_host --scale 2 projects/gods/host/gods_lockstep.lua [--script-args "[state=NAME.state] [extra=N] [continue=all|N] [trace]"]
--
-- The original starts from the saved state of level 1 (128 iterations into the level) and you play it with the keyboard (arrows and space); every frame the keys are given to the SDL
-- port (native/build/sdl/gods-sdl-dev --lockstep: no window, the protocol on stdin/stdout), which boots to the same place by itself, is stepped one video frame, and what both show is
-- compared exactly.  The panels right of the original's frame show the port's image and the difference.  At the first difference the original is paused (silent, frozen) and the port
-- is not stepped again: the control socket (--control PATH, tools/ami9000_host/ctl.py) can look at both.
--
--   extra=N       the port is stepped N more frames before it is compared (the phase of the video frame the saved state is in; 0 to 2 for a state saved at the top of an iteration)
--   continue=all  do not stop: the differences are logged as they start and stop, with a summary now and then
--   continue=N    go on N frames past the first difference
--   trace         log the number of differing pixels of every frame
--   input=PATH    the keys you play are written to PATH ("step k keysym state", the format replay= reads) as they happen: a play can be given again
--   replay=FILE   the keys of a recording (captures/input/level1-run1.txt: "frame k keysym state", by frame from the state) are given to both sides at the same steps, as if played
--   dumpfrom=N    (with dump=) the first difference at emulator frame N or later
--   from=N        the first difference that stops the run is at emulator frame N or later (a level above 1: 12)
--   level=N       the original starts from gods-levelN-1m.state and the port begins level N (the game's levels have worlds: WORLD ONE COMPLETED is in the middle of level 1) (iter=N: the iteration of the game loop the saved state is at; world 2: 124)
--   break=ADDR    pause at the end of the frame in which the original reaches the address (hex, several with commas; with --control PATH the sockets can then look at both sides)
--   ram=PATH      (with state=ABSOLUTE.state) the port begins from that chip RAM of the original (gods_ramdump.lua; gods-sdl-dev --quickload) instead of Begin Quest; the original starts from the state
--                 with a PATH of f5-K.port (the port's own memory, saved by F5) the port begins exactly as it was, not from the original's RAM: state=f5-K.state ram=f5-K.port
--   F5            (in the window) saves both sides: captures/quicksave/f5-K.state and .ram (the original) and f5-K.port (the port's memory), at the frame after the original's next flip
--   control socket (--control SOCK, tools/ami9000_host/ctl.py): port_read, port_poke, orig_poke, mem_diff and port_save (below, before `replay`) read, compare and change the memory of both sides, paused or not
--   blitlog=F=PATH the original's custom register writes (with the pc) of the emulator frame F are written to PATH
--   mask=X,Y,W,H[;..] image regions (in the 320x256 picture) left out of the comparison
--   autosave=SECONDS the original's state is saved every SECONDS seconds (default 30; noautosave: off; not with replay=) to captures/quicksave/auto/auto-K.state/.ram, 5 slots going round (latest.txt)
--   afterend      with break=: only once a level end record was used (ptr_0bc6 past the first), so break=374a stops at the teleport after "WORLD ONE COMPLETED"
--   nosync        the port models the original's CPU time itself (host_iteration_cpu); by default the port's flips that end an iteration wait for the original's own (flipsync: `flips N` to the port)
--   flipoff=N / nolag  tuning of that sync (the port's flip k goes with the original's k+N; nolag: the count of the same step, not the step before)
--   watch         (with continue=all) the first frame at which each of a few game variables differs between the two sides is logged (read from the port with `mem`)
--   pause=F       pause both at emulator frame F (for lockstep_dump.py; the first difference does not stop the run when continue= is given)
--   memcmp[=FROM] [memstop]  the record tables of the game (monster/walker groups, archers, things, bolts...) in the original and the port at every frame: logs the first frame each differs, with the words; memstop pauses there
--   highlight=NAME  every run of different frames is saved when it ends (captures/highlights/NAME/: original, port and the differing pixels in red) and listed in runs.txt; use with continue=all
--   weapons=ITEM:N,ITEM:N  both sides pick up the weapon ITEM (0 knife .. 5 throwing star, 4 lightning bolt, ..., 10 mace) N times at the top of the first iteration (not a regular play)
--   wordlog=F1-F2:ADDR,ADDR  (hex) the words at those addresses, original/port, at each emulator frame F1 to F2
--   peeklog=F:ADDR,ADDR  (hex) the longs at those addresses, in the original and in the port, at emulator frame F
--   peekrange=F1-F2:ADDR (hex) the byte at ADDR in the original and the port at each emulator frame F1 to F2
--   dump=PATH     the port's image and the original's at the first frame that differs are written to PATH.native.ppm and PATH.emulated.ppm
local args = ami.args or ""
local tick_synced = false
-- highlight=NAME: every run of different frames (the play goes on, as with continue=all) is saved when it ends to captures/highlights/NAME/runNNN-fFIRST-LAST.ppm: the original, the port and the
-- differing pixels (red, on the original dimmed), of the frame of the run that differed the most; captures/highlights/NAME/runs.txt lists the runs (first and last frame, frames, most pixels, where)
local highlight = args:match("highlight=(%S+)")
local highlight_run, highlight_runs = nil, {}
local function highlight_regions()
    local list = { { 0, 209, 320, 7 } }
    for x, y, w, h in (args:match("mask=(%S+)") or ""):gmatch("(%d+),(%d+),(%d+),(%d+)") do list[#list + 1] = { tonumber(x), tonumber(y), tonumber(w), tonumber(h) } end
    return list
end
local function highlight_finish(run)
    local ignore = highlight_regions()
    local rows, x0, y0, x1, y1 = {}, 320, 256, -1, -1
    local a, b = run.emulated, run.native
    for y = 0, 255 do
        local original, port, mask = {}, {}, {}
        for x = 0, 319 do
            local i = (y * 320 + x) * 3 + 1
            local pa, pb = a:sub(i, i + 2), b:sub(i, i + 2)
            local skip = false
            for _, r in ipairs(ignore) do if x >= r[1] and x < r[1] + r[3] and y >= r[2] and y < r[2] + r[4] then skip = true break end end
            original[#original + 1], port[#port + 1] = pa, pb
            if pa ~= pb and not skip then
                mask[#mask + 1] = "\255\0\0"
                if x < x0 then x0 = x end if x > x1 then x1 = x end if y < y0 then y0 = y end if y > y1 then y1 = y end
            else
                mask[#mask + 1] = string.char(pa:byte(1) // 4, pa:byte(2) // 4, pa:byte(3) // 4)
            end
        end
        rows[#rows + 1] = table.concat(original) .. table.concat(port) .. table.concat(mask)
    end
    local n = #highlight_runs + 1
    local path = ("%s/../captures/highlights/%s/run%03d-f%d-%d.ppm"):format(ami.script_dir, highlight, n, run.first, run.last)
    ami.save_file(path, "P6\n960 256\n255\n" .. table.concat(rows))
    highlight_runs[n] = ("run %d: frames %d-%d (%d frames), most %d pixels at frame %d, in x %d-%d y %d-%d: %s"):format(n, run.first, run.last, run.frames, run.max, run.frame, x0, x1, y0, y1, path:match("[^/]+$"))
    ami.save_file(("%s/../captures/highlights/%s/runs.txt"):format(ami.script_dir, highlight), table.concat(highlight_runs, "\n") .. "\n")
    ami.log("HIGHLIGHT " .. highlight_runs[n])
end
local memcmp_on = args:find("memcmp") ~= nil
local memcmp_from = tonumber(args:match("memcmp=(%d+)")) or 0
local memcmp_stop = args:find("memstop") ~= nil   -- (memstop: the lockstep pauses on the first frame a table differs)
local memcmp_hit = false
-- the tables compared (address, record count and size in bytes, unit 2 words or 1 bytes, skip: the bytes of a record that hold pointers)
MEMCMP_REGIONS = {
    { name = "records_1354 (monster groups)", addr = 0x1354, count = 10, size = 428, unit = 2, skip = { 2, 6 }, bytes = { 16, 20 } },
    { name = "records_12a0 (walker groups)", addr = 0x12a0, count = 10, size = 18, unit = 2, skip = { 2, 6 }, bytes = { 16, 18 } },
    { name = "table_ed0 (world 2: things the archers threw)", addr = 0xed0, count = 16, size = 16, unit = 2 },
    { name = "records_113c", addr = 0x113c, count = 10, size = 12, unit = 2 },
    { name = "slots_fd0", addr = 0xfd0, count = 10, size = 10, unit = 2 },
    { name = "slots_11b4 (archers)", addr = 0x11b4, count = 10, size = 10, unit = 2 },
    { name = "records_1036", addr = 0x1036, count = 120, size = 1, unit = 1 },
    { name = "records_19334 (pick-ups)", addr = 0x19334, count = 20, size = 8, unit = 2 },
    { name = "records_19b14 (things)", addr = 0x19b14, count = 200, size = 8, unit = 2 },
    { name = "records_721e (shards)", addr = 0x721e, count = 10, size = 6, unit = 2 },
    { name = "records_3a0a6", addr = 0x3a0a6, count = 20, size = 12, unit = 2 },
    { name = "records_316", addr = 0x316, count = 4, size = 6, unit = 2 },
    { name = "records_86c (particles)", addr = 0x86c, count = 80, size = 6, unit = 2 },
    { name = "slots_30c", addr = 0x30c, count = 1, size = 8, unit = 2 },
    { name = "records_7544", addr = 0x7544, count = 42, size = 10, unit = 2 },
    { name = "records_1a154 (bolts)", addr = 0x1a154, count = 20, size = 22, unit = 2, skip = { 0, 4 } },
    { name = "player x,y", addr = 0x1e0, count = 1, size = 4, unit = 2 },
    { name = "energy", addr = 0xc96, count = 1, size = 2, unit = 2 },
    { name = "counter_204", addr = 0x204, count = 1, size = 2, unit = 2 },
    { name = "counter_20c", addr = 0x20c, count = 1, size = 2, unit = 2 },
    { name = "counter_262", addr = 0x262, count = 1, size = 2, unit = 2 },
    { name = "counter_2b4", addr = 0x2b4, count = 1, size = 2, unit = 2 },
}
local hashlog = args:match("hashlog=(%d+)-(%d+)") and { tonumber(args:match("hashlog=(%d+)")), tonumber(args:match("hashlog=%d+-(%d+)")) } or nil
local meanlog = args:match("meanlog=(%d+)-(%d+)") and { tonumber(args:match("meanlog=(%d+)")), tonumber(args:match("meanlog=%d+-(%d+)")) } or nil
local LEVEL = tonumber(args:match("%f[%w]level=(%d)")) or 1   -- level=N: the original starts from gods-levelN-1m.state and the port begins level N (param_index; a level has worlds: the end texts are WORLD ONE, WORLD TWO, ... and LEVEL ONE COMPLETED) (iter=N: the iteration of the game loop the saved state is at; world 2: 124)
local START_SKIP = tonumber(args:match("from=(%d+)")) or (LEVEL > 1 and 12 or 0)   -- from=N: differences before the emulator frame N are only logged (a state saved in the middle of an iteration: the first frames differ a little)
local STATE = args:match("%f[%w]state=(%S+)") or ("gods-level" .. LEVEL .. "-1m.state")
local EXTRA = tonumber(args:match("extra=(%d+)")) or (args:find("ram=%S*%.port") and 0 or args:find("ram=") and 1 or 2)   -- (ram=X.port: the port's own memory, its phase is the one it was saved in; ram=X.ram: the original's RAM, the port is a step behind with 0: found by trying, f5-3 resumed equal for 1250 frames with 1)   -- (a quickload from a state saved at a flip: the port begins its first iteration with the first step)   -- (found by trying: with 3, the comparison starting at frame 5, the idle level is pixel for pixel the same)

ami.machine{
    name = "Gods lockstep", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },   -- 1 MB chip RAM as the saved states have it, the joystick in port 1
    peers = {
        native = { ami.script_dir .. "/../native/build/sdl/gods-sdl-dev", "--disks", ami.script_dir .. "/../local", "--skip-intro", "--lockstep",
                   "--lockstep-extra", tostring(EXTRA), "--lockstep-level", tostring(LEVEL), "--lockstep-iteration", args:match("iter=(%d+)") or tostring(args:find("ram=") and 1 or ({ [2] = 124 })[LEVEL] or 129), "--keys", "300:40:1,320:40:0",
                   table.unpack(args:match("ram=(%S+)") and { "--quickload", args:match("ram=(%S+)") } or {}) },
    },
}
ami.boot()
ami.wait(1)
assert(ami.snapshot_load(STATE), "cannot load " .. STATE)
-- the original takes only the joystick: the keyboard patch (native/src/amiga/keyboard_patch.asm) makes the arrows and the space bar its stick and fire button, as in the other host scripts
do
    local name = "native/build/keyboard_patch.bin"
    ami.write(0x1e608, ami.file_read(name, 0, ami.file_size(name)))
    ami.write(0x1e420, "\x4e\xb9\x00\x01\xe6\x08")
    ami.write(0x1e2f8, "\x4e\x71\x4e\x71")
end
-- the game's instruction at $39EE (the death is over) is `subq.w #1,$224` (the lives: a death takes one, and the player comes back while it is not negative). Our disk dump (md5 ecacb3d4...) has `tst.w $224` there and so never loses a life; every other Gods disk 1 dump we compared ([a], [cr], [cr CSL], [t +n]) has the subq. That the subq is the original is an assumption (decided by the user), so the lockstep puts it back.
-- The original's instruction is put back (the same length, the same flags for the branch after it); the port does what the game does
ami.write(0x39ee, "\x53\x78")
-- the same for the shop: the two instructions that take the price of what is bought (`sub.l d4,$1FC.w` at $13AB6 and `sub.l d0,$1FC.w` at $13BF4) are `bra.b *+2; nop` on our disk dump and the real ones on the others (WHDLoad's Disk.1 too)
ami.write(0x13ab6, "\x99\xb8\x01\xfc")
ami.write(0x13bf4, "\x91\xb8\x01\xfc")

-- The copy protection (Rob Northen's copylock, see analysis/media_variants.md) fails on our ADF (the lock's track is not in it) and leaves
-- values that make the game harder: the long $4B79C278 at $D158, $F6C0 and $107F4 (armour 1, the extra life does nothing) and $4B79C854 at $B04C
-- (the guardian's energy).  The WHDLoad slave (GodsHD.s) writes what a passing lock leaves, and so does the lockstep; the port has both
-- (-DPIRATED for the failed one), by default the passing one.  pirated in the script args keeps the failed values in the original.
if not (args or ""):find("pirated") then
    ami.write(0xd158, "\0\0\0\0")
    ami.write(0xf6c0, "\0\0\0\0")
    ami.write(0x107f4, "\0\0\0\0")
    ami.write(0xb04c, "\0\0\5\xdc")
    ami.write(0x282, "\0\0\0\0")
end

-- break=ADDR[,ADDR...] (hex): the run pauses (both sides frozen, the control socket usable) at the end of the frame in which the original reaches one of those addresses
local break_hit = nil
for hex in (args:match("break=([%x,]+)") or ""):gmatch("%x+") do
    local at = tonumber(hex, 16)
    ami.hook(at, function()
        if not break_hit and (not args:find("afterend") or ami.peek32(0xbc6) > 0x143d8) then   -- afterend: only once a level end record has been used (ptr_0bc6 past the first), e.g. the teleport that follows world one
            break_hit = at
            ami.log(("break at %x, frame %d: player %d,%d"):format(at, ami.frame(), ami.peek16(0x1e0), ami.peek16(0x1e2)))
        end
    end)
end
local lockstep = ami.import("amiga/lockstep")
-- The port is given the keys at the frame the original's game reads them (the hook at its input routine, $363A, sees the key table as the game takes it; what changed since the last read is
-- sent before the port's next step, which is the step of that same iteration: the port's logic and the original's are in the same phase).  The keys the window forwards are not used.  The
-- keys are recorded from those reads, as the frame before the read, which is when `replay=` has to press them for the original to read them again at the same iteration.
local real = ami.peer("native")
local steps, pending = 0, {}
local input_log = args:match("input=(%S+)")
if input_log then ami.save_file(input_log, "") end
local SYM = { [0x4c] = 273, [0x4d] = 274, [0x4e] = 275, [0x4f] = 276, [0x40] = 32 }   -- the raw codes the port takes, as the keys of the recordings
local last_keys = {}
-- what the game reads at $363A is the joystick byte ($1E5AC: bit 0 up, 1 down, 2 left, 3 right, 7 fire), which the keyboard patch makes from the key table at the vertical blank poll
local STICK_BITS = { [0x4c] = 0, [0x4d] = 1, [0x4f] = 2, [0x4e] = 3, [0x40] = 7 }
local function sample_stick()
    local stick = ami.read(0x1e5ac, 1):byte()
    for raw, bit in pairs(STICK_BITS) do
        local down = (stick >> bit) & 1 == 1
        if down ~= (last_keys[raw] or false) then
            last_keys[raw] = down
            pending[#pending + 1] = ("key %d %d\n"):format(raw, down and 1 or 0)
            if input_log then ami.append_file(input_log, ("%d k %d %d\n"):format(steps - 1, SYM[raw], down and 1 or 0)) end
        end
    end
end
local game_read_frame = -100   -- the emulator frame of the game's last read of the keys
local orig_reads, reads_at_sync = 0, 0   -- the original's reads of the keys ($363A): the port's read of an iteration waits for the original's (host_read_wait), so that it has the keys the original's read had
ami.hook(0x363a, function()
    game_read_frame = ami.frame()
    orig_reads = orig_reads + 1
    sample_stick()
end)
-- the shop's loop reads the stick byte at its top ($13840): the same as a read of the keys (the port's shop waits for it, host_read_wait, and has the byte the original's read had)
ami.hook(0x13840, function()
    game_read_frame = ami.frame()
    orig_reads = orig_reads + 1
    sample_stick()
end)
-- ... and the fire button at $1390A, a little later in the same iteration (the port's shop waits for that read too, with the fire bit of the byte)
ami.hook(0x1390a, function()
    orig_reads = orig_reads + 1
    sample_stick()
end)
-- ... and the end of the wait for the button's release at $13A2C (the port's shop goes on from there when the original has)
ami.hook(0x13a2c, function()
    orig_reads = orig_reads + 1
    sample_stick()
end)
-- (outside the game loop - the menu screens, the game over - nothing reads at $363A: the keys are taken from the joystick byte that the original's poll made in the frame it has just run,
-- before the port's step of that frame, so that the port's own poll of the frame has them as the original's did)
local function sample_stick_outside_game()
    if ami.frame() - game_read_frame > 3 then sample_stick() end
end
-- flipsync: the port's flips wait for the original's (the original's CPU time is not modelled): `flips N`, the times the original has reached its flip ($1D302) since the sync began,
-- goes to the port before every step; `flipoff=N` shifts which flip of the port goes with which of the original's (nosync: the port models the CPU time itself)
local flip_sync = not args:find("%f[%w]nosync%f[%W]")
local flip_offset = tonumber(args:match("flipoff=(-?%d+)")) or 0
local orig_flips, flips_at_sync, sync_on, last_flips = 0, 0, false, 0
local orig_fades, fades_at_sync = 0, 0   -- the original's calls of its fade-in ($1D0C0): the port's teleport waits for them (host_fade_wait)
local orig_world_loads, world_loads_at_sync = 0, 0   -- the original's arrivals at the end of the loading of a world ($0135C0, where it puts back the tick count): the port's load waits for them (host_world_load_wait)
local flip_lag = args:find("%f[%w]lag%f[%W]") ~= nil   -- (the port's flips are let go by the count of the same step and shown at the next blank: the original shows its flip a frame after the call; `lag`: the old way, the count of the step before and shown at once)   -- (the count the port gets is the one of the step before: the original's flip shows a frame after the one the count reaches)
-- the disk: the original's read of the background ($6A8A on, until its fade out at $1D23E) takes as long as the drive takes; the port waits for the original to have done (`diskdone N`, the reads finished)
local disk_pending, disk_done = false, 0
ami.hook(0x6a8a, function() disk_pending = true end)
-- the shop's reads: shop.pi1 ($136EC, until the fade out at $13734, which is $1D23E) and the sound file ($13768, until $1377A)
ami.hook(0x136ec, function() disk_pending = true end)
ami.hook(0x13768, function() disk_pending = true end)
ami.hook(0x1377a, function() if disk_pending then disk_pending = false disk_done = disk_done + 1 end end)
-- the guardian's start ($0030DA: the loading screen at $30E8, then the guardian's sprites and the sound files; the game loop begins again at $31CA)
ami.hook(0x30e8, function() disk_pending = true if args:find("diskdbg") then ami.log(("DISKDBG start frame %d"):format(ami.frame())) end end)
ami.hook(0x31ca, function() if disk_pending then disk_pending = false disk_done = disk_done + 1 if args:find("diskdbg") then ami.log(("DISKDBG done frame %d"):format(ami.frame())) end end end)
local frame_end_cycles = 0
local last_fade_frame = -100   -- (the frame of the original's last fade step: fadetol=N, default 600, lets a difference of that many pixels or fewer pass while a fade goes on: the band of the list edit is a row or two off)
local fade_line = -1   -- the raster line the original's thread was at when it began the copper fade of its step ($1D1C2 loops over the bands of the list: what lies above that line was shown before the edit)
ami.hook(0x1d1c2, function() last_fade_frame = ami.frame(); fade_line = math.floor((ami.cycles() - frame_end_cycles) / 454) if args:find("fadelog") then ami.log(("FADELINE frame %d line %d"):format(ami.frame(), fade_line)) end end)
ami.hook(0x1d168, function() last_fade_frame = ami.frame(); fade_line = math.floor((ami.cycles() - frame_end_cycles) / 454) if args:find("fadelog") then ami.log(("FADELINE-IN frame %d line %d"):format(ami.frame(), fade_line)) end end)
ami.hook(0x1d23e, function() if next_level_event then next_level_event(3, "start faded out") end if disk_pending then disk_pending = false disk_done = disk_done + 1 if args:find("diskdbg") then ami.log(("DISKDBG done at 1d23e frame %d (%d)"):format(ami.frame(), disk_done)) end end end)
-- the shop's way out ($1392A): the game's sprites and the sound file are read again (they take what the drive takes) before the fade out at $1D23E
ami.hook(0x1392a, function() disk_pending = true end)
-- the start of the next level ($2452, from $13646): three events the port's loading waits for: its loading screen is faded in ($1D0C0), its files are read (until $2AAA) and its start fades out ($1D23E)
next_level_stage = 0
function next_level_event(stage, what)
    if next_level_stage == stage then
        next_level_stage = (stage + 1) % 4
        disk_done = disk_done + 1
        if args:find("diskdbg") then ami.log(("DISKDBG next level %s at frame %d (%d)"):format(what, ami.frame(), disk_done)) end
    end
end
ami.hook(0x2452, function() next_level_stage = 1 end)
ami.hook(0x2aaa, function() next_level_event(2, "files read") end)

local flip_in_frame = false   -- (the autosave looks for a frame in which the original flipped)
ami.hook(0x1d302, function() orig_flips = orig_flips + 1; flip_in_frame = true end)
ami.hook(0x1d0c0, function() if next_level_event then next_level_event(1, "loading screen faded in") end orig_fades = orig_fades + 1; if args:find("fadelog") then ami.log(("FADE call %d at frame %d"):format(orig_fades, ami.frame())) end end)
ami.hook(0x135c0, function() orig_world_loads = orig_world_loads + 1 end)
-- autosave=SECONDS (default 30; 0 or `noautosave` off; not while replaying): every SECONDS seconds of play (50 frames each) the original's state is saved, the frame after one in which it flipped (so its iteration has
-- just ended: a quickload of the port begins one at its top), with its chip RAM: captures/quicksave/auto/auto-K.state and .ram, K going round 0 to 4 (latest.txt names the newest).  A play that stops at a
-- difference is then started again from the last one: ... state=PATH/auto-K.state ram=PATH/auto-K.ram
local AUTO_SECONDS = args:find("noautosave") and 0 or tonumber(args:match("autosave=(%d+)")) or (args:find("replay=") and 0 or 30)
local AUTO_DIR = ami.script_dir .. "/../captures/quicksave/auto"
local auto_next = AUTO_SECONDS * 50
local auto_slot = 0
-- F5 in the window saves both sides: the original's state and chip RAM (captures/quicksave/f5-K.state and .ram, as the autosave does) and the port's memory
-- (f5-K.port: its quicksave, the file --quickload reads), at the frame after the original flipped.  (ami.input is wrapped to see the key before the lockstep module takes the events.)
local F5_KEY = 286   -- libretro RETROK_F5
local f5_requested, f5_slot = false, 0
local libretro_input = ami.input
ami.input = function()
    local events = libretro_input()
    for _, e in ipairs(events) do
        if e[1] == "k" and e[2] == F5_KEY and e[3] ~= 0 then f5_requested = true end
    end
    return events
end
local function f5_step()
    if not (f5_requested and flip_in_frame) then return end
    f5_requested = false
    while ami.file_size(("captures/quicksave/f5-%d.state"):format(f5_slot)) do f5_slot = f5_slot + 1 end   -- (the first number not used: a save is never overwritten)
    local base = ("%s/../captures/quicksave/f5-%d"):format(ami.script_dir, f5_slot)
    f5_slot = f5_slot + 1
    if ami.snapshot_save(base .. ".state") then
        ami.save_file(base .. ".ram", ami.read(0, 0x100000))
        pending[#pending + 1] = ("quicksave %s.port\n"):format(base)
        ami.log(("F5: the original's state and RAM at emulator frame %d: %s.state and .ram; the port's memory: %s.port"):format(ami.frame(), base, base))
    else
        ami.log("F5: the original's state could not be saved")
    end
end
-- bosshp=F:N (a test): at emulator frame F the guardian's energy ($50C, the low word of its long) is set to N on both sides
local BOSSHP_FRAME, BOSSHP_VALUE = args:match("bosshp=(%d+):(%d+)")
BOSSHP_FRAME, BOSSHP_VALUE = tonumber(BOSSHP_FRAME), tonumber(BOSSHP_VALUE)
local function autosave_step()
    if BOSSHP_FRAME and ami.frame() == BOSSHP_FRAME then
        orig_poke(0x50C, BOSSHP_VALUE, 2)
        pending[#pending + 1] = ("poke %x %x 2\n"):format(0x50C, BOSSHP_VALUE)   -- (sent with the step: both sides see it at the same iteration)
    end
    f5_step()
    if AUTO_SECONDS <= 0 then flip_in_frame = false return end
    local f = ami.frame()
    if f >= auto_next and flip_in_frame then   -- (this frame is over; the original flipped in it and waits in the flip for the next blank: its next iteration has not begun)
        local base = ("%s/auto-%d"):format(AUTO_DIR, auto_slot)
        if ami.snapshot_save(base .. ".state") then
            ami.save_file(base .. ".ram", ami.read(0, 0x100000))
            ami.save_file(AUTO_DIR .. "/latest.txt", ("auto-%d frame %d\n"):format(auto_slot, f))
            ami.log(("autosave: %s.state and .ram at emulator frame %d"):format(base, f))
            auto_slot = (auto_slot + 1) % 5
        end
        auto_next = f + AUTO_SECONDS * 50
    end
    flip_in_frame = false
end
local delayed = setmetatable({
    send = function(_, text)
        for line in text:gmatch("[^\n]+") do
            if not line:match("^key ") then
                if line == "step" then
                    steps = steps + 1
                    sample_stick_outside_game()
                    real:send(("diskdone %d\n"):format(disk_done))
                    if sync_on then
                        real:send(("flips %d\n"):format((flip_lag and last_flips or orig_flips) - flips_at_sync))
                        real:send(("reads %d\n"):format(orig_reads - reads_at_sync))
                        real:send(("fadeline %d\n"):format(fade_line))
                        fade_line = -1
                        real:send(("fades %d\n"):format(orig_fades - fades_at_sync))
                        real:send(("worldloads %d\n"):format(orig_world_loads - world_loads_at_sync))
                        last_flips = orig_flips
                    end
                    for _, p in ipairs(pending) do real:send(p) end
                    pending = {}
                end
                real:send(line .. "\n")
            end
        end
    end,
}, { __index = function(_, k) local v = real[k] return type(v) == "function" and function(_, ...) return v(real, ...) end or v end })
-- Helpers for the control socket (tools/ami9000_host/ctl.py SOCK 'port_read(0x16bb4, 24)'), while the run is paused or going: the port's memory next to the original's.
--   port_read(addr, len)           the port's bytes (as it holds them: the words and longs native, little endian), read in pieces of 4096
--   port_poke(addr, value, size)   size 1, 2 (default) or 4 bytes of the port's memory are set (value as the program reads it, the number you mean)
--   orig_poke(addr, value, size)   the same on the original (big endian)
--   mem_diff(addr, len, size)      where the two differ over len bytes: size 2 (default) compares words (the port's swapped), 4 longs, 1 bytes; returns lines
--                                  "ADDR original port" with the neighbours joined into runs (at most 60 lines)
--   port_save(path)                the port's memory to PATH (its quicksave: f5-K.port, which ram= takes to resume it)
function port_read(addr, len)
    local out = {}
    while len > 0 do
        local n = math.min(len, 4096)
        real:send(("mem %x %x\n"):format(addr, n))   -- (the port reads both as hex)
        local got = tonumber(real:readline():match("mem (%d+)"))
        out[#out + 1] = real:read(got)
        addr, len = addr + n, len - n
    end
    return table.concat(out)
end
function port_poke(addr, value, size)
    size = size or 2
    real:send(("poke %x %x %d\n"):format(addr, value & ((1 << (8 * size)) - 1), size))
end
function orig_poke(addr, value, size)
    size = size or 2
    ami.write(addr, string.pack(">I" .. size, value & ((1 << (8 * size)) - 1)))
end
function port_save(path)
    real:send(("quicksave %s\n"):format(path))
end
function mem_diff(addr, len, size)
    size = size or 2
    local port, orig = port_read(addr, len), ami.read(addr, len)
    local lines, last = {}, nil
    for i = 0, len - size, size do
        local o = string.unpack(">I" .. size, orig, i + 1)
        local p = string.unpack("<I" .. size, port, i + 1)
        if o ~= p then
            if last and i - last <= size * 2 and #lines > 0 then
                lines[#lines] = lines[#lines] .. (" %x/%x"):format(o, p)
            elseif #lines < 60 then
                lines[#lines + 1] = ("%x: %x/%x"):format(addr + i, o, p)
            end
            last = i
        end
    end
    return table.concat(lines, "\n")
end
local stop_saved = false
local replay = args:match("replay=(%S+)")
local schedule = nil
if replay then
    local events = {}
    local text = ami.file_read(replay, 0, ami.file_size(replay))
    for line in text:gmatch("[^\n]+") do
        local f, kind, a, b = line:match("(%d+) (%a) (-?%d+) (-?%d+)")
        if f and kind == "k" then
            events[tonumber(f)] = events[tonumber(f)] or {}
            table.insert(events[tonumber(f)], { tonumber(a), tonumber(b) ~= 0 })
        end
    end
    -- at step N: the keys of the frame N are pressed on the original and sent to the port
    schedule = setmetatable({}, { __index = function(_, step)
        local lines = {}
        for _, e in ipairs(events[step] or {}) do
            ami.key(e[1], e[2])

            if args:find("%f[%w]keylog%f[%W]") then ami.log(("step %d: key %d %s; table %02x"):format(step, e[1], tostring(e[2]), ami.read(0x1e63a + 0x4e, 1):byte())) end

        end
        return #lines > 0 and table.concat(lines) or nil
    end })
end
-- the keyboard: libretro key -> the Amiga raw code the port's keyboard takes (up, down, right, left, space)
local KEYS = { [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [32] = 0x40 }
local trace = args:find("%f[%w]trace%f[%W]")
local dump = args:match("dump=(%S+)")
local watching = args:find("%f[%w]watch%f[%W]")
local pause_at = tonumber(args:match("pause=(%d+)"))
local WATCH = { frame_toggle = 0x196, frame_phase = 0x1e4, player_x = 0x1e0, player_y = 0x1e2, player_state = 0x19c, player_frame = 0x19a, counter_204 = 0x204, counter_260 = 0x260, scroll_x = 0x358, scroll_y = 0x35a, word_39e = 0x39e,
                onscreen_count = 0x2b8, flag_23a = 0x23a, timer_306 = 0x306, energy_shown = 0x216, flag_230 = 0x230, word_3b0 = 0x3b0, player_energy = 0xc96, hit = 0x3a6, word_2ba = 0x2ba, tick_hold = 0x26c, word_226 = 0x226, word_228 = 0x228, item_count = 0x20a }
local watch_found = {}
-- track=ADDR (hex): the long at ADDR on both sides is logged whenever either changes (to find where two states part)
local track_at = tonumber(args:match("track=(%x+)") or "", 16)
local track_size = tonumber(args:match("track=%x+:(%d)") or "4")
local track_last_o, track_last_p
-- trackcolor=N: the colour register N (0 to 31) of both sides is logged whenever either changes
local color_n = tonumber(args:match("trackcolor=(%d+)"))
local color_last_o, color_last_p
local function track_color()
    delayed:send(("mem dff%03x 2\n"):format(0x180 + 2 * color_n))
    local n = tonumber(delayed:readline():match("mem (%d+)"))
    local port = string.unpack("<I2", delayed:read(n))
    local orig = ami.custom(0x180 + 2 * color_n) & 0xffff
    if orig ~= color_last_o or port ~= color_last_p then
        color_last_o, color_last_p = orig, port
        ami.log(("color %d: frame %d: original %03x, port %03x"):format(color_n, ami.frame(), orig, port))
    end
end
local function track_long()
    delayed:send(("mem %x %d\n"):format(track_at, track_size))
    local n = tonumber(delayed:readline():match("mem (%d+)"))
    local fmt = track_size == 2 and "I2" or "I4"
    local port = string.unpack("<" .. fmt, delayed:read(n))
    local orig = string.unpack(">" .. fmt, ami.read(track_at, track_size))
    if orig ~= track_last_o or port ~= track_last_p then
        track_last_o, track_last_p = orig, port
        ami.log(("track %x: frame %d: original %d, port %d"):format(track_at, ami.frame(), orig, port))
    end
end
local function watch_variables()
    for name, address in pairs(WATCH) do
        if not watch_found[name] then
            delayed:send(("mem %x 2\n"):format(address))
            local header = delayed:readline()
            local n = tonumber(header:match("mem (%d+)"))
            local bytes = delayed:read(n)
            local port = string.unpack("<I2", bytes)
            local orig = string.unpack(">I2", ami.read(address, 2))
            if port ~= orig then
                watch_found[name] = true
                ami.log(("watch: %s differs from emulator frame %d: original %04x, port %04x"):format(name, ami.frame(), orig, port))
            end
        end
    end
end
local dumped = false
local peeklog_frame, peeklog = args:match('peeklog=(%d+):(%x[%x,]*)')
peeklog_frame = tonumber(peeklog_frame)
-- drawlog=FIRST-LAST: the original's calls of its draw routines (glyph $17686, sprite $173C4) at the top of the screen (y below 6) in those emulator frames, with who called
local draw_first, draw_last = args:match("drawlog=(%d+)-(%d+)")
if draw_first then
    for _, at in ipairs({ 0x17686, 0x173c4 }) do
        table.insert(draw_listeners[at], function()
            local f = ami.frame()
            if f >= tonumber(draw_first) and f <= tonumber(draw_last) then
                local y = ami.reg("d1") & 0xffff
                if y >= 0x8000 then y = y - 0x10000 end
                if y < 6 then
                    local x = ami.reg("d0") & 0xffff
                    if x >= 0x8000 then x = x - 0x10000 end
                    ami.log(("draw %x frame %d: x %d y %d image %d, called from %06x"):format(at, f, x, y, ami.reg("d2") & 0xffff, ami.peek32(ami.reg("a7"))))
                end
            end
        end)
    end
end
if draw_first then   -- and the draw routine the creatures call at $E660 (the one of the table at $6EA8 the creature's template names)
    ami.hook(0xe660, function()
        local f = ami.frame()
        if f >= tonumber(draw_first) and f <= tonumber(draw_last) then
            local a5 = ami.reg("a5")
            ami.log(("creature draw frame %d: routine %06x, record %06x (%d %d %d %d %d), d0 %d d1 %d"):format(f, ami.reg("a4"), a5, ami.peek16(a5), ami.peek16(a5 + 2), ami.peek16(a5 + 4), ami.peek16(a5 + 6), ami.peek16(a5 + 8), ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff))
        end
    end)
end
-- drawbox=FIRST-LAST,X0,Y0,X1,Y1: the original's calls of glyph_draw and sprite_draw whose screen place is in the box, in those emulator frames, with who called
-- the original's draw routines (glyph $17686, sprite $173C4) are hooked once: the listeners (drawbox, the log below) are called from that hook.  The log keeps the last draws with the
-- flip interval they were made in; every stop saves them with the port's (`draws`, the same for its draw hook) in captures/stops/stop-fN.draws (scripts/draws_diff.py compares them)
local draw_listeners = { [0x17686] = {}, [0x173c4] = {} }
for at, list in pairs(draw_listeners) do
    ami.hook(at, function() for _, f in ipairs(list) do f(at) end end)
end
local draw_ring = {}
local function signed16(v) v = v & 0xffff if v >= 0x8000 then v = v - 0x10000 end return v end
for at, list in pairs(draw_listeners) do
    list[#list + 1] = function()
        draw_ring[#draw_ring + 1] = { orig_flips, at == 0x17686 and 0 or 1, signed16(ami.reg("d0")), signed16(ami.reg("d1")), ami.reg("d2") & 0xffff, ami.peek32(ami.reg("a7")) }
        if #draw_ring > 12000 then table.move(draw_ring, 6001, #draw_ring, 1) for k = #draw_ring, #draw_ring - 5999, -1 do draw_ring[k] = nil end end
    end
end
local function save_draws(path)
    local lines = {}
    for _, e in ipairs(draw_ring) do
        if e[1] >= orig_flips - 1 then lines[#lines + 1] = ("O %d %d %d %d %d %06x"):format(e[1] - (orig_flips - 1), e[2], e[3], e[4], e[5], e[6]) end
    end
    real:send("draws 2\n")
    while true do
        local line = real:readline()
        if not line or line:match("^end") then break end
        local it, kind, x, y, index = line:match("^(%d+) (%-?%d+) (%-?%d+) (%-?%d+) (%d+)")
        if it then lines[#lines + 1] = ("P %s %s %s %s %s"):format(it, kind, x, y, index) end
    end
    ami.save_file(path, table.concat(lines, "\n") .. "\n")
end
local box_first, box_last, bx0, by0, bx1, by1 = args:match("drawbox=(%d+)-(%d+),(%-?%d+),(%-?%d+),(%-?%d+),(%-?%d+)")
if box_first then
    for _, at in ipairs({ 0x17686, 0x173c4 }) do
        ami.hook(at, function()
            local f = ami.frame()
            if f >= tonumber(box_first) and f <= tonumber(box_last) then
                local function signed(v) v = v & 0xffff if v >= 0x8000 then v = v - 0x10000 end return v end
                local x, y = signed(ami.reg("d0")), signed(ami.reg("d1"))
                if x >= tonumber(bx0) and x <= tonumber(bx1) and y >= tonumber(by0) and y <= tonumber(by1) then
                    local image = ami.reg("d2") & 0xffff
                    ami.log(("drawbox %x frame %d: x %d y %d image %d data %x, called from %06x"):format(at, f, x, y, image, ami.peek32(ami.peek32(0xe3c) + image * 8 + 4), ami.peek32(ami.reg("a7"))))
                end
            end
        end)
    end
end
-- hitlog=FIRST-LAST: the original's calls of its player damage routine ($D270: d0, d1 the place, d2 the strength) in those emulator frames, with who called and the energy
local hit_first, hit_last = args:match("hitlog=(%d+)-(%d+)")
if hit_first then
    for _, at in ipairs({ 0x80a2, 0xd00a, 0x7eae }) do
        ami.hook(at, function()
            local f = ami.frame()
            if f >= tonumber(hit_first) and f <= tonumber(hit_last) then
                ami.log(("call %x frame %d: d0 %d d1 %d"):format(at, f, ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff))
            end
        end)
    end
    ami.hook(0xd270, function()
        local f = ami.frame()
        if f >= tonumber(hit_first) and f <= tonumber(hit_last) then
            local function signed(v) v = v & 0xffff if v >= 0x8000 then v = v - 0x10000 end return v end
            ami.log(("damage $d270 frame %d: x %d y %d strength %d, energy %d, called from %06x, player %d,%d"):format(f, signed(ami.reg("d0")), signed(ami.reg("d1")), signed(ami.reg("d2")), ami.peek16(0xc96), ami.peek32(ami.reg("a7")), ami.peek16(0x1e0), ami.peek16(0x1e2)))
        end
    end)
end
-- calllog=ADDR[,ADDR...] (hex): the emulator frame and the registers d0, d1, d7 of every time the original reaches those addresses (once per call, whatever frames)
for hex in (args:match("calllog=([%x,]+)") or ""):gmatch("%x+") do
    local at = tonumber(hex, 16)
    ami.hook(at, function() ami.log(("reached %x at frame %d (cycles %d): d0 %x d1 %x d7 %x player %d,%d"):format(at, ami.frame(), ami.cycles(), ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("d7") & 0xffff, ami.peek16(0x1e0), ami.peek16(0x1e2))) end)
end
-- randlog=FIRST-LAST: the original's calls of its random routine ($633E) in those emulator frames, with who called and the table index
local rl_first, rl_last = args:match("randlog=(%d+)-(%d+)")
if rl_first then
    ami.hook(0x633e, function()
        local f = ami.frame()
        if f >= tonumber(rl_first) and f <= tonumber(rl_last) then ami.log(("rand frame %d: index %d, called from %06x"):format(f, ami.peek16(0x2e6), ami.peek32(ami.reg("a7")))) end
    end)
end
-- reglog=ADDR[,ADDR...] (hex): every register d0-d7, a0-a6 of every time the original reaches those addresses
for hex in (args:match("reglog=([%x,]+)") or ""):gmatch("%x+") do
    local at = tonumber(hex, 16)
    ami.hook(at, function()
        local t = {}
        for _, r in ipairs({ "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6" }) do t[#t + 1] = ("%s=%x"):format(r, ami.reg(r)) end
        ami.log(("regs %x frame %d: %s ret %06x"):format(at, ami.frame(), table.concat(t, " "), ami.peek32(ami.reg("a7"))))
    end)
end
if args:find("%f[%w]itlog%f[%W]") then   -- itlog: the emulator frame of the top of every iteration of the game loop (at $2C20), 20 at a time, on the log
    local last, line = 0, {}
    ami.hook(0x2c20, function()
        local f = ami.frame()
        line[#line + 1] = ("%d(+%d)"):format(f, f - last)
        last = f
        if #line >= 20 then ami.log("iterations: " .. table.concat(line, " ")) line = {} end
    end)
end
-- weapons=ITEM:N,ITEM:N (the number of the weapon in the weapons file: 0 knife, 1 time bomb, 2 fireball, 3 flame, 4 lightning bolt, 5 throwing star, 6 spear, 7 axe, 8 hunter, 9 magic axe, 10 mace; N pickups of
-- each): at the top of the first iteration the original picks each up by its own routine ($015282, as the player does) and the port is told to do the same at the top of its next
if args:match("weapons=") then
    local list = {}
    for item, n in args:match("weapons=([%d:,]+)"):gmatch("(%d+):(%d+)") do list[#list + 1] = { tonumber(item), tonumber(n) } end
    local given = false
    ami.hook(0x2c20, function()
        if given or ami.frame() < 6 then return end
        given = true
        for _, w in ipairs(list) do
            for _ = 1, w[2] do
                ami.poke16(0x2b2, w[1])
                ami.call(0x15282, { d2 = w[1] })
                delayed:send(("weapon %d\n"):format(w[1]))
            end
        end
        ami.log(("weapons given at frame %d"):format(ami.frame()))
    end)
end
lockstep.run {
    peer = delayed,
    width = 320, height = 256,
    window = { x = 40, y = 18, sx = 2 },   -- where the port's image sits in the original's 720x287 frame (tools/lockstep_align.py)
    start = function() if not args:find("ram=") then ami.wait(4) end return true end,   -- (a quickload begins with the first step: the port's first iteration goes with the original's next)   -- the saved state is the start (a frame later it has been drawn)
    keymap = function(key) return KEYS[key] end,
    on_frame = function(count, native, emulated)
        frame_end_cycles = ami.cycles()
        if count > 0 and count <= (tonumber(args:match("fadetol=(%d+)")) or 600) and ami.frame() - last_fade_frame <= 3 then count = 0 end
        -- A stop (the first difference, from=N, pause=F, break=ADDR) saves both sides to captures/stops/stop-fFRAME[-N].*: the original's state and RAM, the port's memory
        -- from 0 to $80000 (.portmem: as it holds it, words native) and the two pictures; scripts/stop_diff.py compares them offline, however the play got there.
        local stops_here = (pause_at and ami.frame() >= pause_at) or break_hit
            or (count > 0 and not args:find("continue=all") and ami.frame() >= START_SKIP)
        if stops_here and not stop_saved then
            stop_saved = true
            local n, base = 0, nil
            repeat
                base = ("%s/../captures/stops/stop-f%d%s"):format(ami.script_dir, ami.frame(), n > 0 and ("-" .. n) or "")
                n = n + 1
            until not ami.file_size(("captures/stops/stop-f%d%s.state"):format(ami.frame(), n > 1 and ("-" .. (n - 1)) or ""))
            ami.snapshot_save(base .. ".state")
            ami.save_file(base .. ".ram", ami.read(0, 0x100000))
            ami.save_file(base .. ".portmem", port_read(0, 0x80000))
            ami.save_file(base .. ".native.ppm", "P6\n320 256\n255\n" .. native)
            ami.save_file(base .. ".emulated.ppm", "P6\n320 256\n255\n" .. emulated)
            save_draws(base .. ".draws")
            ami.log(("stop saved: %s.state .ram .portmem (the port's memory 0-$7FFFF) .native.ppm .emulated.ppm"):format(base))
        end
        autosave_step()
        if false then ami.log(("endframe %d: %d"):format(ami.frame(), ami.cycles())) end
        -- audlog=PATH: every write of the original to a sound register (AUDxLC/LEN/PER/VOL, DMACON) with the emulator frame, written to PATH every 100 frames (the port's sound.c logs its notes the same way)
        do
            local ap = args:match("audlog=(%S+)")
            if ap then
                audlog_lines = audlog_lines or {}
                local cur = ami.read(0x207a0, 16)
                if cur ~= audlog_prev then
                    audlog_prev = cur
                    local hex = {}
                    for k = 1, #cur do hex[#hex + 1] = ("%02x"):format(cur:byte(k)) end
                    audlog_lines[#audlog_lines + 1] = ("f%d busy/ids %s"):format(ami.frame(), table.concat(hex, " "))
                end
                if ami.frame() % 100 == 0 then ami.save_file(ap, table.concat(audlog_lines, "\n")) end
            end
        end
        -- blitlog=FRAME=PATH: the custom register writes of the emulator frame FRAME (the blitter's pointers and sizes, with the program counter) are written to PATH
        do
            local bf, bp = args:match("blitlog=(%d+)=(%S+)")
            if bf and ami.frame() == tonumber(bf) then ami.trace_writes(true) end
            if bf and ami.frame() == tonumber(bf) + 1 then ami.save_file(bp, ami.take_writes()); ami.trace_writes(false) end
        end
        if not tick_synced and ami.frame() >= (args:find("ram=") and 3 or 8) then
            -- the original's tick_count ($CE2: the frames since the level began) includes the frames of its disk loading, which the port does not wait for: the port is given the
            -- original's count (it matters where the script looks at the seconds, as the bonus that is given only in the first minute)
            tick_synced = true
            local p = lockstep.peer
            p:send("mem ce2 4\n")
            local n = tonumber(p:readline():match("mem (%d+)"))
            local port = string.unpack("<I4", p:read(n))
            local orig = ami.peek32(0xce2)
            p:send(("tick %d\n"):format(orig - port))
            -- the counters that count each frame (the bat's flap, the heads of the flyers...) are in the phase the original's loading gave them, as tick_count is
            for _, addr in ipairs({ 0x204, 0x20c, 0x262, 0x2b4 }) do p:send(("poke16 %x %x\n"):format(addr, (ami.peek16(addr) + (addr == 0x204 and tonumber(args:match("c204off=(-?%d+)")) or 1)) & 0xffff)) end
            if flip_sync then
                sync_on, flips_at_sync, fades_at_sync, world_loads_at_sync, reads_at_sync = true, orig_flips, orig_fades, orig_world_loads, orig_reads
                p:send(("flipsync %d\n"):format(flip_offset))
            end
            ami.log(("tick_count: the original's %d, the port's %d: %d added to the port's"):format(orig, port, orig - port))
        end
        if args:find("%f[%w]keylog%f[%W]") then ami.log(("frame %d: px %d table %02x"):format(ami.frame(), ami.read(0x1e0, 2):byte(1) * 256 + ami.read(0x1e0, 2):byte(2), ami.read(0x1e63a + 0x4e, 1):byte())) end
        if hashlog then
            local function hash(img) local h = 5381 for i = 1, #img, 7 do h = (h * 33 + img:byte(i)) % 4294967296 end return h end
            if ami.frame() >= hashlog[1] and ami.frame() <= hashlog[2] then ami.log(("hash frame %d: original %x port %x"):format(ami.frame(), hash(emulated), hash(native))) end
        end
        if meanlog then
            local function mean(img) local t = 0 for i = 1, #img, 61 do t = t + img:byte(i) end return t // (#img // 61) end
            if ami.frame() >= meanlog[1] and ami.frame() <= meanlog[2] then ami.log(("mean frame %d: original %d port %d"):format(ami.frame(), mean(emulated), mean(native))) end
        end
        if peeklog and ami.frame() == peeklog_frame then   -- peeklog=F:ADDR,ADDR: the longs at those addresses in the original and the port at emulator frame F
            for address in peeklog:gmatch("%x+") do
                address = tonumber(address, 16)
                delayed:send(("mem %x 4\n"):format(address))
                local header = delayed:readline()
                local bytes = delayed:read(tonumber(header:match("mem (%d+)")))
                ami.log(("peek %x at frame %d: original %08x, port %08x"):format(address, ami.frame(), string.unpack(">I4", ami.read(address, 4)), string.unpack("<I4", bytes)))
            end
        end
        do   -- peekrange=F1-F2:ADDR (hex): the byte at ADDR in the original and the port at each emulator frame F1 to F2
            local f1, f2, a = args:match("peekrange=(%d+)-(%d+):(%x+)")
            if f1 and ami.frame() >= tonumber(f1) and ami.frame() <= tonumber(f2) then
                local address = tonumber(a, 16)
                delayed:send(("mem %x 1\n"):format(address))
                local header = delayed:readline()
                local bytes = delayed:read(tonumber(header:match("mem (%d+)")))
                ami.log(("peek %x at frame %d: original %02x, port %02x"):format(address, ami.frame(), ami.read(address, 1):byte(), bytes:byte()))
            end
        end
        if memcmp_on and ami.frame() >= memcmp_from then   -- memcmp[=FROM]: the game's record tables of the original and the port, compared word by word at every frame (the first frame each table differs, with the record and the words)
            for _, r in ipairs(MEMCMP_REGIONS) do
                local total = r.count * r.size
                delayed:send(("mem %x %x\n"):format(r.addr, total))
                local header = delayed:readline()
                local port = delayed:read(tonumber(header:match("mem (%d+)")))
                local orig = ami.read(r.addr, total)
                local first, nd = nil, 0
                for i = 0, total - 1, r.unit do
                    local off = i % r.size
                    if not (r.skip and off >= r.skip[1] and off < r.skip[2]) then
                        local same
                        if r.bytes and off >= r.bytes[1] and off < r.bytes[2] then same = orig:byte(i + 1) == port:byte(i + 1) and orig:byte(i + 2) == port:byte(i + 2)
                        elseif r.unit == 2 then same = orig:byte(i + 1) == port:byte(i + 2) and orig:byte(i + 2) == port:byte(i + 1)
                        else same = orig:byte(i + 1) == port:byte(i + 1) end
                        if not same then
                            nd = nd + 1
                            if not first then first = i end
                        end
                    end
                end
                if nd > 0 and not r.differs then
                    r.differs = true
                    local rec, off = first // r.size, first % r.size
                    local o, q = {}, {}
                    for i = rec * r.size, rec * r.size + r.size - 1, r.unit do
                        if r.unit == 2 then
                            o[#o + 1] = ("%02x%02x"):format(orig:byte(i + 1), orig:byte(i + 2)); q[#q + 1] = ("%02x%02x"):format(port:byte(i + 2), port:byte(i + 1))
                        else o[#o + 1] = ("%02x"):format(orig:byte(i + 1)); q[#q + 1] = ("%02x"):format(port:byte(i + 1)) end
                        if #o >= 24 then break end
                    end
                    ami.log(("memcmp frame %d: %s differs (%d words), first in record %d at +%d (%x)"):format(ami.frame(), r.name, nd, rec, off, r.addr + first))
                    ami.log(("  original: %s"):format(table.concat(o, " ")))
                    ami.log(("  port:     %s"):format(table.concat(q, " ")))
                    if memcmp_stop then memcmp_hit = true end
                elseif nd == 0 and r.differs then
                    r.differs = false
                    ami.log(("memcmp frame %d: %s equal again"):format(ami.frame(), r.name))
                end
            end
        end
        if memcmp_hit then return true end
        do   -- wordlog=F1-F2:ADDR,ADDR (hex): the words at those addresses in the original and the port at each emulator frame F1 to F2
            local f1, f2, list = args:match("wordlog=(%d+)-(%d+):(%x[%x,]*)")
            if f1 and ami.frame() >= tonumber(f1) and ami.frame() <= tonumber(f2) then
                local out = {}
                for a in list:gmatch("%x+") do
                    local address = tonumber(a, 16)
                    delayed:send(("mem %x 2\n"):format(address))
                    local header = delayed:readline()
                    local bytes = delayed:read(tonumber(header:match("mem (%d+)")))
                    out[#out + 1] = ("%x: %04x/%04x"):format(address, string.unpack(">I2", ami.read(address, 2)), string.unpack("<I2", bytes))
                end
                ami.log(("words frame %d (original/port): %s"):format(ami.frame(), table.concat(out, "  ")))
            end
        end
        if highlight then
            if count > 0 then
                highlight_run = highlight_run or { first = ami.frame(), max = 0, frames = 0 }
                highlight_run.frames = highlight_run.frames + 1
                highlight_run.last = ami.frame()
                if count > highlight_run.max then
                    highlight_run.max, highlight_run.frame, highlight_run.native, highlight_run.emulated = count, ami.frame(), native, emulated
                end
            elseif highlight_run then
                local run = highlight_run
                highlight_run = nil
                highlight_finish(run)
            end
        end
        if watching then watch_variables() end
        if track_at then track_long() end
        if color_n then track_color() end
        if trace then ami.log(("different pixels: %d"):format(count)) end
        if dump and count > 0 and not dumped and ami.frame() >= (tonumber(args:match("dumpfrom=(%d+)")) or 0) then
            dumped = true
            ami.save_file(dump .. ".native.ppm", "P6\n320 256\n255\n" .. native)
            ami.save_file(dump .. ".emulated.ppm", "P6\n320 256\n255\n" .. emulated)
        end
        if pause_at and ami.frame() >= pause_at then return true end
        if break_hit then return true end
        if START_SKIP > 0 and not args:find("continue=all") and count > 0 and ami.frame() >= START_SKIP then return true end   -- (the first differences are not looked at, then the first stops)
    end or nil,
    -- the message band: a message shows in the port one frame before the original's CPU gets to draw it (the raster has passed the band by then), a one frame difference each time
    ignore = (function()   -- the strip at the foot, and mask=X,Y,W,H[;X,Y,W,H...]: image regions left out of the comparison
        local list = { { 0, 209, 320, 7 } }
        for x, y, w, h in (args:match("mask=(%S+)") or ""):gmatch("(%d+),(%d+),(%d+),(%d+)") do list[#list + 1] = { tonumber(x), tonumber(y), tonumber(w), tonumber(h) } end
        return list
    end)(),
    schedule = schedule,
    continue_for = (args:find("continue=all") or START_SKIP > 0) and -1 or tonumber(args:match("continue=(%d+)")) or 0,
}
