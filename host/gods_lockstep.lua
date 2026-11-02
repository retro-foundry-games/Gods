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
--   delay=N       the keys reach the port N steps after the original (default 2)
--   input=PATH    the keys you play are written to PATH ("step k keysym state", the format replay= reads) as they happen: a play can be given again
--   replay=FILE   the keys of a recording (captures/input/level1-run1.txt: "frame k keysym state", by frame from the state) are given to both sides at the same steps, as if played
--   dumpfrom=N    (with dump=) the first difference at emulator frame N or later
--   watch         (with continue=all) the first frame at which each of a few game variables differs between the two sides is logged (read from the port with `mem`)
--   pause=F       pause both at emulator frame F (for lockstep_dump.py; the first difference does not stop the run when continue= is given)
--   dump=PATH     the port's image and the original's at the first frame that differs are written to PATH.native.ppm and PATH.emulated.ppm
local args = ami.args or ""
local STATE = args:match("state=(%S+)") or "gods-level1-1m.state"
local EXTRA = tonumber(args:match("extra=(%d+)")) or 3   -- (found by trying: with 3, the comparison starting at frame 5, the idle level is pixel for pixel the same)

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
-- the peer, with its key lines held back DELAY steps (the keyboard of the emulated machine takes a key a little later than the port: with 2 the recorded play of level 1 is the same on both)
local DELAY = tonumber(args:match("delay=(%d+)")) or 2
local real = ami.peer("native")
local steps, held = 0, {}
local input_log = args:match("input=(%S+)")
if input_log then ami.save_file(input_log, "") end
local SYM = { [0x4c] = 273, [0x4d] = 274, [0x4e] = 275, [0x4f] = 276, [0x40] = 32 }   -- the raw codes the port takes, as the keys of the recordings
local delayed = setmetatable({
    send = function(_, text)
        for line in text:gmatch("[^\n]+") do
            if line:match("^key ") then
                local code, down = line:match("^key (%d+) (%d+)")
                if input_log and SYM[tonumber(code)] then ami.append_file(input_log, ("%d k %d %s\n"):format(steps, SYM[tonumber(code)], down)) end
                held[#held + 1] = { steps + DELAY, line .. "\n" }
            else
                if line == "step" then
                    steps = steps + 1
                    local keep = {}
                    for _, h in ipairs(held) do
                        if h[1] < steps then real:send(h[2]) else keep[#keep + 1] = h end
                    end
                    held = keep
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
            local raw = ({ [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [32] = 0x40 })[e[1]]
            if raw then lines[#lines + 1] = ("key %d %d\n"):format(raw, e[2] and 1 or 0) end
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
    ami.hook(at, function() ami.log(("reached %x at frame %d (cycles %d): d0 %x d1 %x d7 %x"):format(at, ami.frame(), ami.cycles(), ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("d7") & 0xffff)) end)
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
    on_frame = (trace or dump or watching or track_at or color_n or pause_at or args:find("%f[%w]keylog%f[%W]")) and function(count, native, emulated)
        if args:find("%f[%w]keylog%f[%W]") then ami.log(("frame %d: px %d table %02x"):format(ami.frame(), ami.read(0x1e0, 2):byte(1) * 256 + ami.read(0x1e0, 2):byte(2), ami.read(0x1e63a + 0x4e, 1):byte())) end
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
