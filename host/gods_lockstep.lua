-- Gods: the original against the SDL port, side by side, frame-locked and given the same keys (tools/ami9000_host/lua/amiga/lockstep.lua, as fa18.lua does for the native port of FA-18).
--
--   tools/engine9000/ami9000_host --scale 2 projects/gods/host/gods_lockstep.lua [--script-args "[state=NAME.state] [extra=N] [continue=all|N] [trace]"]
--
-- The original starts from the saved state of level 1 (128 iterations into the level) and you play it with the keyboard (arrows and space); every frame the keys are given to the SDL
-- port (native/build/sdl/gods-sdl --lockstep: no window, the protocol on stdin/stdout), which boots to the same place by itself, is stepped one video frame, and what both show is
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
--   nosync        the port models the original's CPU time itself (host_iteration_cpu); by default the port's flips that end an iteration wait for the original's own (flipsync: `flips N` to the port)
--   flipoff=N / nolag  tuning of that sync (the port's flip k goes with the original's k+N; nolag: the count of the same step, not the step before)
--   watch         (with continue=all) the first frame at which each of a few game variables differs between the two sides is logged (read from the port with `mem`)
--   pause=F       pause both at emulator frame F (for lockstep_dump.py; the first difference does not stop the run when continue= is given)
--   dump=PATH     the port's image and the original's at the first frame that differs are written to PATH.native.ppm and PATH.emulated.ppm
local args = ami.args or ""
local tick_synced = false
local hashlog = args:match("hashlog=(%d+)-(%d+)") and { tonumber(args:match("hashlog=(%d+)")), tonumber(args:match("hashlog=%d+-(%d+)")) } or nil
local meanlog = args:match("meanlog=(%d+)-(%d+)") and { tonumber(args:match("meanlog=(%d+)")), tonumber(args:match("meanlog=%d+-(%d+)")) } or nil
local STATE = args:match("state=(%S+)") or "gods-level1-1m.state"
local EXTRA = tonumber(args:match("extra=(%d+)")) or 2   -- (found by trying: with 3, the comparison starting at frame 5, the idle level is pixel for pixel the same)

ami.machine{
    name = "Gods lockstep", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },   -- 1 MB chip RAM as the saved states have it, the joystick in port 1
    peers = {
        native = { ami.script_dir .. "/../native/build/sdl/gods-sdl", "--disks", ami.script_dir .. "/../local", "--skip-intro", "--lockstep",
                   "--lockstep-extra", tostring(EXTRA), "--keys", "300:40:1,320:40:0" },
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
ami.hook(0x363a, function()
    local stick = ami.read(0x1e5ac, 1):byte()
    for raw, bit in pairs(STICK_BITS) do
        local down = (stick >> bit) & 1 == 1
        if down ~= (last_keys[raw] or false) then
            last_keys[raw] = down
            pending[#pending + 1] = ("key %d %d\n"):format(raw, down and 1 or 0)
            if input_log then ami.append_file(input_log, ("%d k %d %d\n"):format(steps - 1, SYM[raw], down and 1 or 0)) end
        end
    end
end)
-- flipsync: the port's flips wait for the original's (the original's CPU time is not modelled): `flips N`, the times the original has reached its flip ($1D302) since the sync began,
-- goes to the port before every step; `flipoff=N` shifts which flip of the port goes with which of the original's (nosync: the port models the CPU time itself)
local flip_sync = not args:find("%f[%w]nosync%f[%W]")
local flip_offset = tonumber(args:match("flipoff=(-?%d+)")) or 0
local orig_flips, flips_at_sync, sync_on, last_flips = 0, 0, false, 0
local flip_lag = not args:find("%f[%w]nolag%f[%W]")   -- (the count the port gets is the one of the step before: the original's flip shows a frame after the one the count reaches)
ami.hook(0x1d302, function() orig_flips = orig_flips + 1 end)
local delayed = setmetatable({
    send = function(_, text)
        for line in text:gmatch("[^\n]+") do
            if not line:match("^key ") then
                if line == "step" then
                    steps = steps + 1
                    if sync_on then
                        real:send(("flips %d\n"):format((flip_lag and last_flips or orig_flips) - flips_at_sync))
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
local WATCH = { player_x = 0x1e0, player_y = 0x1e2, player_state = 0x19c, player_frame = 0x19a, counter_204 = 0x204, counter_260 = 0x260, scroll_x = 0x358, scroll_y = 0x35a, word_39e = 0x39e,
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
-- drawlog=FIRST-LAST: the original's calls of its draw routines (glyph $17686, sprite $173C4) at the top of the screen (y below 6) in those emulator frames, with who called
local draw_first, draw_last = args:match("drawlog=(%d+)-(%d+)")
if draw_first then
    for _, at in ipairs({ 0x17686, 0x173c4 }) do
        ami.hook(at, function()
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
lockstep.run {
    peer = delayed,
    width = 320, height = 256,
    window = { x = 40, y = 18, sx = 2 },   -- where the port's image sits in the original's 720x287 frame (tools/lockstep_align.py)
    start = function() ami.wait(4) return true end,   -- the saved state is the start (a frame later it has been drawn)
    keymap = function(key) return KEYS[key] end,
    on_frame = function(count, native, emulated)
        if false then ami.log(("endframe %d: %d"):format(ami.frame(), ami.cycles())) end
        if not tick_synced and ami.frame() >= 8 then
            -- the original's tick_count ($CE2: the frames since the level began) includes the frames of its disk loading, which the port does not wait for: the port is given the
            -- original's count (it matters where the script looks at the seconds, as the bonus that is given only in the first minute)
            tick_synced = true
            local p = lockstep.peer
            p:send("mem ce2 4\n")
            local n = tonumber(p:readline():match("mem (%d+)"))
            local port = string.unpack("<I4", p:read(n))
            local orig = ami.peek32(0xce2)
            p:send(("tick %d\n"):format(orig - port))
            if flip_sync then
                sync_on, flips_at_sync = true, orig_flips
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
        if watching then watch_variables() end
        if track_at then track_long() end
        if color_n then track_color() end
        if trace then ami.log(("different pixels: %d"):format(count)) end
        if pause_at and ami.frame() >= pause_at then return true end
        if dump and count > 0 and not dumped and ami.frame() >= (tonumber(args:match("dumpfrom=(%d+)")) or 0) then
            dumped = true
            ami.save_file(dump .. ".native.ppm", "P6\n320 256\n255\n" .. native)
            ami.save_file(dump .. ".emulated.ppm", "P6\n320 256\n255\n" .. emulated)
        end
    end or nil,
    -- the message band: a message shows in the port one frame before the original's CPU gets to draw it (the raster has passed the band by then), a one frame difference each time
    ignore = { { 0, 209, 320, 7 } },
    schedule = schedule,
    continue_for = args:find("continue=all") and -1 or tonumber(args:match("continue=(%d+)")) or 0,
}
