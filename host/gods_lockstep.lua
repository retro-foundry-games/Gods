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
--   delay=N       the keys reach the port N steps after the original (the keyboard of the emulated machine delivers a key a little later than the port takes it)
--   replay=FILE   the keys of a recording (captures/input/level1-run1.txt: "frame k keysym state", by frame from the state) are given to both sides at the same steps, as if played
--   dumpfrom=N    (with dump=) the first difference at emulator frame N or later
--   dump=PATH     the port's image and the original's at the first frame that differs are written to PATH.native.ppm and PATH.emulated.ppm
local args = ami.args or ""
local STATE = args:match("state=(%S+)") or "gods-level1-1m.state"
local DELAY = tonumber(args:match("delay=(%d+)")) or 2   -- (found by trying: with 2 the recorded play of level 1 is the same on both)
local EXTRA = tonumber(args:match("extra=(%d+)")) or 1   -- (found by trying: with 1 the idle level is pixel for pixel the same)

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
-- the peer, with its key lines held back DELAY steps
local real = ami.peer("native")
local steps, held = 0, {}
local delayed = setmetatable({
    send = function(_, text)
        for line in text:gmatch("[^\n]+") do
            if line:match("^key ") then
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
            if args:find("%f[%w]keylog%f[%W]") then ami.log(("step %d: key %d %s; table %02x"):format(step, e[1], tostring(e[2]), ami.read(0x1e63a + 0x4e, 1):byte())) end
            local raw = ({ [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [32] = 0x40 })[e[1]]
            if raw then lines[#lines + 1] = ("key %d %d\n"):format(raw, e[2] and 1 or 0) end
        end
        return #lines > 0 and table.concat(lines) or nil
    end })
end
-- the keyboard: libretro key -> the Amiga raw code the port's keyboard takes (up, down, right, left, space)
local KEYS = { [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [32] = 0x40 }
local trace = args:find("%f[%w]trace%f[%W]")
local dump = args:match("dump=(%S+)")
local dumped = false
lockstep.run {
    peer = delayed,
    width = 320, height = 256,
    window = { x = 40, y = 18, sx = 2 },   -- where the port's image sits in the original's 720x287 frame (tools/lockstep_align.py)
    start = function() ami.wait(2) return true end,   -- the saved state is the start (a frame later it has been drawn)
    keymap = function(key) return KEYS[key] end,
    on_frame = (trace or dump or args:find("%f[%w]keylog%f[%W]")) and function(count, native, emulated)
        if args:find("%f[%w]keylog%f[%W]") then ami.log(("frame %d: px %d table %02x"):format(ami.frame(), ami.read(0x1e0, 2):byte(1) * 256 + ami.read(0x1e0, 2):byte(2), ami.read(0x1e63a + 0x4e, 1):byte())) end
        if trace then ami.log(("different pixels: %d"):format(count)) end
        if dump and count > 0 and not dumped and ami.frame() >= (tonumber(args:match("dumpfrom=(%d+)")) or 0) then
            dumped = true
            ami.save_file(dump .. ".native.ppm", "P6\n320 256\n255\n" .. native)
            ami.save_file(dump .. ".emulated.ppm", "P6\n320 256\n255\n" .. emulated)
        end
    end or nil,
    schedule = schedule,
    continue_for = args:find("continue=all") and -1 or tonumber(args:match("continue=(%d+)")) or 0,
}
