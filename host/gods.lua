-- Gods (Bitmap Brothers, 1991) from the original PAL disk 1 on a plain Kickstart 1.3 A500.
-- No WHDLoad, no patches: the boot block and the game's own loader run unmodified.
--
--   play:   tools/engine9000/ami9000_host projects/gods/host/gods.lua
--   rebuilt             boot the disk whose game is the C in native/src/ (local/gods-c-disk1.adf, see scripts/build_adf.py)
--   flags:  see "Script flags" below (snapshots, screenshots)
--
-- Needs the A500 Kickstart 1.3 (CRC32 c4f0f55f) in ../local/system and ../local/gods-disk1.adf.
-- ../local/gods.m3u (create: printf 'gods-disk1.adf\ngods-disk2.adf\n' > local/gods.m3u) lists both disks: the core's multidrive option puts disk 1 in DF0 and disk 2 in DF1.
local script_args = ami.args or ""
local record_dir = script_args:match("record=(%S+)")
local compare_dir = script_args:match("compare=(%S+)")
local recorder
if record_dir or compare_dir then
    recorder = {
        "/usr/bin/env", "python3", ami.script_dir .. "/../../../tools/amiga/frame_recorder.py",
        record_dir and "--out" or "--compare", record_dir or compare_dir,
    }
    if compare_dir and script_args:match("%f[%w]survey%f[%W]") then recorder[#recorder + 1] = "--survey" end
    local mask = script_args:match("mask=(%S+)")
    if compare_dir and mask then
        recorder[#recorder + 1] = "--mask"
        recorder[#recorder + 1] = mask
    end
end

ami.machine{
    name = "Gods",
    model = "A500",
    system = "../local/system",
    save = "../local/save",
    files = "..",
    content = script_args:match("rebuilt") and "../local/gods-c.m3u" or "../local/gods.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = script_args:match("rebuilt") and { puae_chipmem_size = 2 } or nil,   -- the C runs from chip RAM above 512 KB
    peers = recorder and { recorder = recorder } or nil,
}
ami.boot()

-- Script flags (--script-args "..."):
--   keyboard            the original menu with the keyboard: cursor up/down, space = fire (native/src/amiga/keyboard_patch.asm; build it with scripts/build_keyboard_patch.py)
--   load=NAME            restore snapshot NAME (a file in ../local/save) before running; frames below count from it
--   snap=F[,F...]        save snapshot gods-F.state at frame F (relative to the start or the loaded snapshot)
--   snapname=NAME        save the snapshots as NAME.state (with one frame in snap=) and quit afterwards
--   click=F[,...]        press the left mouse button (the fire button the program polls) for 3 frames at frame F
--   move=F:DX:DY[,...]   move the mouse by DX, DY at frame F (the software pointer of the menu follows the counters)
--   joy=F:DIRS[,...]     set the joystick (port 1) at frame F: DIRS is up, down, left, right, fire joined by +, or none
--   press=KEY@F[,...]    press the key (a name from ami.keys, e.g. space) for 3 frames at frame F (to reach the menu with
--                        space and take a snapshot; the lockstep recordings never press a key)
--   record=DIR [screen=N]  stream every frame (hash, chip RAM, every Nth screen) to tools/amiga/frame_recorder.py
--   record=DIR hook=HEX [maxevents=N]  record at every execution of the instruction at HEX (the entry of the program's
--                       wait for the vertical blank) instead of every frame: event number, frame and chip RAM
--   compare=DIR [survey] [mask=FILE]  compare each frame with a recording; stops at the first difference, or with
--                       survey reports every differing RAM range; mask lists RAM ranges to ignore
--   until=N             quit after N frames (with record/compare/cover)
--   cover=PATH          hook chip RAM and write every executed instruction (pc + bytes) to PATH
--   ram=PATH            write chip RAM (512 KiB) at the end of the events, then quit
--   shots=DIR frames=F,. write DIR/gods-F.ppm at those frames, then quit (headless checks)
local args = ami.args or ""
local function frames_of(key)
    local list = {}
    for n in (args:match(key .. "=([%d,]+)") or ""):gmatch("%d+") do list[#list + 1] = tonumber(n) end
    return list
end
local load_name = args:match("load=(%S+)")
local shots_dir = args:match("shots=(%S+)")
if load_name then
    ami.wait(1)
    if not ami.snapshot_load(load_name) then ami.log("cannot load snapshot", load_name) ami.quit(2) end
end
-- keyboard: the original menu with the keyboard in its input (native/src/amiga/keyboard_patch.asm, a change of the original's code in
-- memory: cursor up and down are the stick, the space bar is fire).  Applied when the menu program starts its menu ($6388),
-- or at once when a snapshot of the menu is loaded.
if script_args:match("keyboard") then
    local function patch_menu()
        local code = ami.file_read("native/build/keyboard_patch.bin", 0, ami.file_size("native/build/keyboard_patch.bin"))
        ami.write(0x1e608, code)
        ami.write(0x1e420, "\x4e\xb9\x00\x01\xe6\x08")
        ami.log("keyboard patch applied")
    end
    if load_name then patch_menu() else
        local done = false
        ami.hook(0x6388, function() if not done then done = true; patch_menu() end end)
    end
end
local base = ami.frame()
local until_frame = tonumber(args:match("until=(%d+)"))
local screen_every = tonumber(args:match("screen=(%d+)")) or 0
local cover_path = args:match("cover=(%S+)")

-- Coverage: every even chip RAM address is hooked; a hook records the instruction's bytes and removes itself, and
-- the executed set is re-armed every REARM frames so that code overlaid at the same address is seen again.
local covered, cover_order = {}, {}
local REARM = 1000
local function on_cover()
    local pc = ami.reg("pc")
    local bytes = ami.read(pc, 10)
    local seen = covered[pc]
    if not seen then
        covered[pc] = { bytes }
        cover_order[#cover_order + 1] = pc
    else
        for _, b in ipairs(seen) do
            if b == bytes then ami.unhook(pc) return end
        end
        seen[#seen + 1] = bytes
    end
    ami.unhook(pc)
end
if cover_path then
    for a = 0, 0x7fffe, 2 do ami.hook(a, on_cover) end
end

local function write_cover()
    table.sort(cover_order)
    local out = {}
    for _, pc in ipairs(cover_order) do
        for _, b in ipairs(covered[pc]) do
            out[#out + 1] = ("%06x %s"):format(pc, (b:gsub(".", function(c) return ("%02x"):format(c:byte()) end)))
        end
    end
    ami.save_file(cover_path, table.concat(out, "\n") .. "\n")
    ami.log("coverage", #cover_order, "instructions")
end

local joys = {}   -- frame -> the joystick state from then on
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local state = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then state[d] = true end end
    joys[tonumber(f)] = state
end
local peer = recorder and ami.peer("recorder")
local clicks = {}
for _, f in ipairs(frames_of("click")) do clicks[f] = true; clicks[f + 3] = false end
local moves = {}
for f, dx, dy in (args:match("move=(%S+)") or ""):gmatch("(%d+):(-?%d+):(-?%d+)") do moves[tonumber(f)] = { tonumber(dx), tonumber(dy) } end
local function run_frames()
    local sent = 0
    while not until_frame or sent < until_frame do
        ami.wait(1)
        local f = ami.frame() - base
        if clicks[f] ~= nil then ami.mouse_button(0, clicks[f]) end
        if moves[f] then ami.mouse_move(moves[f][1], moves[f][2]) end
        if joys[f] then ami.joystick(joys[f]) end
        if cover_path and f > 0 and f % REARM == 0 then
            for _, pc in ipairs(cover_order) do ami.hook(pc, on_cover) end
        end
        if peer then
            local rgb = ""
            local w, h = ami.width(), ami.height()
            if screen_every > 0 and f % screen_every == 0 then rgb = ami.frame_rgb(0, 0, w, h) end
            local ram = ami.read(0, 0x80000)
            local msg = "F" .. string.pack("<I4I8I4I4I2I2", f, ami.frame_hash() & 0x7fffffffffffffff, #ram, #rgb, w, h) .. ram .. rgb
            if not pcall(peer.send, peer, msg) then
                ami.log("recorder ended at frame", f)
                ami.quit(1)
            end
        end
        sent = sent + 1
        if f % 5000 == 0 then ami.log("frame", f) end
    end
    if cover_path then write_cover() end
    if peer then peer:close() end
    ami.quit(0)
end
local hook_at = tonumber(args:match("hook=(%x+)") or "", 16)
if peer and hook_at then
    -- a snapshot at a point where the program's own progress is the same in every run: the interrupt has finished (it
    -- ran at the start of the frame) and the program is about to wait for the next one
    local n = 0
    local max_events = tonumber(args:match("maxevents=(%d+)")) or math.huge
    ami.hook(hook_at, function()
        n = n + 1
        local ram = ami.read(0, 0x80000)
        local msg = "F" .. string.pack("<I4I8I4I4I2I2", n, ami.frame(), #ram, 0, 0, 0) .. ram
        if not pcall(peer.send, peer, msg) then ami.log("recorder ended at event", n) ami.quit(1) end
    end)
    while n < max_events and (not until_frame or ami.frame() - base < until_frame) do ami.wait(1) end
    peer:close()
    ami.log("events", n)
    ami.quit(0)
end
if peer or cover_path then run_frames() end
local events = {}
for _, f in ipairs(frames_of("snap")) do events[#events + 1] = { f, "snap" } end
if shots_dir then
    local list = frames_of("frames")
    if #list == 0 then list = { 300, 600, 1200, 2400 } end
    for _, f in ipairs(list) do events[#events + 1] = { f, "shot" } end
end
for key, f in (args:match("press=(%S+)") or ""):gmatch("(%w+)@(%d+)") do
    events[#events + 1] = { tonumber(f), "press", key }
    events[#events + 1] = { tonumber(f) + 3, "release", key }
end
for f, v in pairs(moves) do events[#events + 1] = { f, "move", v } end
for f, v in pairs(joys) do events[#events + 1] = { f, "joy", v } end
for _, f in ipairs(frames_of("click")) do
    events[#events + 1] = { f, "click" }
    events[#events + 1] = { f + 3, "unclick" }
end
table.sort(events, function(a, b) return a[1] < b[1] end)
local snap_name = args:match("snapname=(%S+)")
for _, e in ipairs(events) do
    ami.wait_until(function() return ami.frame() - base >= e[1] end)
    if e[2] == "joy" then
        ami.joystick(e[3])
    elseif e[2] == "move" then
        ami.mouse_move(e[3][1], e[3][2])
    elseif e[2] == "click" then
        ami.mouse_button(0, true)
    elseif e[2] == "unclick" then
        ami.mouse_button(0, false)
    elseif e[2] == "press" then
        ami.key(e[3], true)
    elseif e[2] == "release" then
        ami.key(e[3], false)
    elseif e[2] == "snap" then
        local name = snap_name and (snap_name .. ".state") or ("gods-" .. e[1] .. ".state")
        ami.log("snapshot", name, ami.snapshot_save(name))
    else
        ami.screenshot(shots_dir .. "/gods-" .. e[1] .. ".ppm")
        ami.log("frame", e[1], "pc", string.format("%06x", ami.reg("pc")))
    end
end
local ram_path = args:match("ram=(%S+)")
if ram_path then
    ami.wait(1)
    ami.save_file(ram_path, ami.read(0, 0x80000))
    ami.log("ram", ram_path, "pc", string.format("%06x", ami.reg("pc")), "frame", ami.frame() - base)
end
if shots_dir or ram_path or snap_name then ami.quit(0) end
