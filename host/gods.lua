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
    opt = (function()
        local o = {}
        if script_args:match("rebuilt") or script_args:match("onemb") then o.puae_chipmem_size = 2 end   -- the C runs from chip RAM above 512 KB
        if script_args:match("joy1") then o.puae_joyport_order = "2143" end   -- the joystick in port 1, where the program polls it
        return next(o) and o or nil
    end)(),
    peers = recorder and { recorder = recorder } or nil,
}
ami.boot()

-- Script flags (--script-args "..."):
--   inputlog=FILE       play with the window and write the keys and mouse buttons used, with their frames, to FILE
--   replay=FILE         play such a file again (same snapshot, same frames); FILE is relative to projects/gods
--   onemb               1 MB chip RAM (the state files gods-*-1m.state)
--   joy1                the joystick in port 1 (puae_joyport_order=2143): needed for joy= to reach the program
--   keyboard            the original menu with the keyboard: cursor up/down, space = fire (native/src/amiga/keyboard_patch.asm; build it with scripts/build_keyboard_patch.py)
--   load=NAME            restore snapshot NAME (a file in ../local/save) before running; frames below count from it
--   snap=F[,F...]        save snapshot gods-F.state at frame F (relative to the start or the loaded snapshot)
--   snapname=NAME        save the snapshots as NAME.state (with one frame in snap=) and quit afterwards
--   click=F[,...]        press the left mouse button (the fire button the program polls) for 3 frames at frame F
--   move=F:DX:DY[,...]   move the mouse by DX, DY at frame F (the software pointer of the menu follows the counters)
--   joy=F:DIRS[,...]     set the joystick (port 1) at frame F: DIRS is up, down, left, right, fire joined by +, or none
--   press=KEY@F[xN][,...] press the key (a name from ami.keys, e.g. space) for N (default 3) frames at frame F (to reach the menu with
--                        space and take a snapshot; the lockstep recordings never press a key)
--   record=DIR [screen=N]  stream every frame (hash, chip RAM, every Nth screen) to tools/amiga/frame_recorder.py
--   record=DIR hook=HEX [maxevents=N]  record at every execution of the instruction at HEX (the entry of the program's
--                       wait for the vertical blank) instead of every frame: event number, frame and chip RAM
--   compare=DIR [survey] [mask=FILE]  compare each frame with a recording; stops at the first difference, or with
--                       survey reports every differing RAM range; mask lists RAM ranges to ignore
--   until=N             quit after N frames (with record/compare/cover)
--   coverstop=ADDR      with cover= (and ram=): stop at the first time ADDR (hex) is reached (e.g. 2c20: the game loop)
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
        ami.write(0x1e2f8, "\x4e\x71\x4e\x71")   -- the keyboard interrupt takes every key (it lost the second of two close events)
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
-- coverstop=ADDR (hex): write the coverage and quit at the first time the program reaches ADDR
local cover_stop = tonumber(args:match("coverstop=(%x+)") or "", 16)
if cover_path and cover_stop then
    ami.hook(cover_stop, function()
        write_cover()
        local ram = args:match("ram=(%S+)")   -- also the RAM at that moment
        if ram then ami.save_file(ram, ami.read(0, 0x80000)) end
        ami.quit(0)
    end)
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
-- count=ADDR[,ADDR...] (hex): how many times the program reaches each address (reported at the start and at the end of the shots/frames)
local counts = {}
for a in (args:match("count=([%x,]+)") or ""):gmatch("%x+") do
    local addr = tonumber(a, 16)
    counts[addr] = 0
    ami.hook(addr, function() counts[addr] = counts[addr] + 1 end)
end
-- trace=ADDR[,ADDR...] (hex): log the registers the first 6 times the program reaches each address
for a in (args:match("trace=([%x,]+)") or ""):gmatch("%x+") do
    local addr, hits = tonumber(a, 16), 0
    ami.hook(addr, function()
        hits = hits + 1
        if hits > 6 then return end
        local t = {}
        for _, r in ipairs({ "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" }) do
            t[#t + 1] = r .. "=" .. ("%x"):format(ami.reg(r))
        end
        local top = ami.read(ami.reg("a7"), 4)
        ami.log(("trace $%x hit %d frame %d: %s ret=%x"):format(addr, hits, ami.frame(), table.concat(t, " "), string.unpack(">I4", top)))
    end)
end
-- track=ADDR (hex) [trackfrom=N trackto=M]: at the Nth to Mth time the program reaches ADDR (the game loop's top) write one line to the log with the
-- sums of the 8 blocks of 512 bytes of $0-$FFF (find the first iteration and the block in which two runs differ)
local track_at = tonumber(args:match("track=(%x+)") or "", 16)
if track_at then
    local from, to = tonumber(args:match("trackfrom=(%d+)")) or 1, tonumber(args:match("trackto=(%d+)")) or 400
    local n = 0
    ami.hook(track_at, function()
        n = n + 1
        if n < from or n > to then return end
        local len = 0x1000   -- (the sums are of $0-$FFF)   -- tracklen=HEX: the bytes from 0 to look at (default $1000)
        local ram = ami.read(0, len)
        local t = {}
        for b = 0, len // 512 - 1 do
            local sum = 0
            for i = b * 512 + 1, b * 512 + 512 do sum = (sum * 31 + ram:byte(i)) & 0xffffffff end
            t[#t + 1] = ("%08x"):format(sum)
        end
        ami.log(("track %d frame %d %s"):format(n, ami.frame() - base, table.concat(t, " ")))
        if n == tonumber(args:match("trackdump=(%d+)") or "") then
            ami.save_file(args:match("trackfile=(%S+)"), ami.read(tonumber(args:match("trackdumpfrom=(%x+)") or "0", 16), tonumber(args:match("trackdumplen=(%x+)") or "1000", 16)))   -- trackdump=N trackfile=PATH: the bytes $0-$FFF at that time
        end
    end)
end
-- iterhook=ADDR (hex) with iterjoy=N:DIRS,... and itershot=N,...: the joystick (as joy=) is set and a screenshot (to shots=) taken at the Nth time the program reaches
-- ADDR (the game loop's top): the input per iteration of the loop, the same for the original and for the C game whose iterations take another number of frames
local iter_at = tonumber(args:match("iterhook=(%x+)") or "", 16)
if iter_at then
    local ijoy, ishot, n = {}, {}, 0
    for k, dirs in (args:match("iterjoy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
        local st = {}
        for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
        ijoy[tonumber(k)] = st
    end
    for k in (args:match("itershot=([%d,]+)") or ""):gmatch("%d+") do ishot[tonumber(k)] = true end
    ami.hook(iter_at, function()
        n = n + 1
        if ijoy[n] then ami.joystick(ijoy[n]) end
        if ishot[n] then ami.screenshot(shots_dir .. "/iter-" .. n .. ".ppm") end
    end)
end
-- stopat=ADDR (hex) [ram=PATH]: at the first time the program reaches ADDR write the chip RAM (512 KiB) to ram= and quit
local stop_at = tonumber(args:match("stopat=(%x+)") or "", 16)
local stop_nth = tonumber(args:match("stopnth=(%d+)") or "") or 1   -- stopnth=N: at the Nth time (the loop's Nth iteration)
local stop_hits = 0
if stop_at then
    ami.hook(stop_at, function()
        stop_hits = stop_hits + 1
        if stop_hits < stop_nth then return end
        local ram = args:match("ram=(%S+)")
        if ram then ami.save_file(ram, ami.read(0, 0x80000)) end
        ami.log(("stopat $%x frame %d"):format(stop_at, ami.frame() - base))
        ami.quit(0)
    end)
end
-- watch=ADDR (hex): log each frame the word at ADDR has changed (its value and the pc)
local watch_at = tonumber(args:match("watch=(%x+)") or "", 16)
if watch_at then
    local last = nil
    local function poll()
        local v = string.unpack(">I2", ami.read(watch_at, 2))
        if v ~= last then
            ami.log(("watch $%x = %04x at frame %d pc %06x"):format(watch_at, v, ami.frame() - base, ami.reg("pc")))
            last = v
        end
    end
    watch_poll = poll
end
local function report_counts()
    for addr, n in pairs(counts) do ami.log(("count $%x: %d"):format(addr, n)) end
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
for key, f, hold in (args:match("press=(%S+)") or ""):gmatch("(%w+)@(%d+)x?(%d*)") do
    events[#events + 1] = { tonumber(f), "press", key }
    events[#events + 1] = { tonumber(f) + (tonumber(hold) or 3), "release", key }
end
-- replay=FILE: the keys and mouse buttons of a file written by inputlog= (frame kind a b), at the same frames
local replay_path = args:match("replay=(%S+)")
if replay_path then
    local text = ami.file_read(replay_path, 0, ami.file_size(replay_path))   -- a path inside the project directory
    for line in text:gmatch("[^\n]+") do
        local f, kind, a, b = line:match("(%d+) (%a) (-?%d+) (-?%d+)")
        if f and kind == "k" then events[#events + 1] = { tonumber(f), "key", tonumber(a), tonumber(b) ~= 0 } end
        if f and kind == "b" and tonumber(a) == 0 then events[#events + 1] = { tonumber(f), "button", tonumber(b) ~= 0 } end
    end
end
for f, v in pairs(moves) do events[#events + 1] = { f, "move", v } end
for f, v in pairs(joys) do events[#events + 1] = { f, "joy", v } end
-- poke=F:ADDR:VALUE[,...] (ADDR hex, VALUE hex word): at frame F the word is written to chip RAM (e.g. 3:1d4:2 makes the menu's Begin Quest start level 2)
for f, a, v in (args:match("poke=(%S+)") or ""):gmatch("(%d+):(%x+):(%x+)") do events[#events + 1] = { tonumber(f), "poke", { tonumber(a, 16), tonumber(v, 16) } } end
for _, f in ipairs(frames_of("click")) do
    events[#events + 1] = { f, "click" }
    events[#events + 1] = { f + 3, "unclick" }
end
table.sort(events, function(a, b) return a[1] < b[1] end)
local snap_name = args:match("snapname=(%S+)")
for _, e in ipairs(events) do
    ami.wait_until(function() if watch_poll then watch_poll() end return ami.frame() - base >= e[1] end)
    if e[2] == "poke" then
        ami.write(e[3][1], string.pack(">I2", e[3][2]))
    elseif e[2] == "joy" then
        ami.joystick(e[3])
    elseif e[2] == "move" then
        ami.mouse_move(e[3][1], e[3][2])
    elseif e[2] == "click" then
        ami.mouse_button(0, true)
    elseif e[2] == "unclick" then
        ami.mouse_button(0, false)
    elseif e[2] == "press" then
        ami.key(e[3], true)
    elseif e[2] == "key" then
        ami.key(e[3], e[4])
    elseif e[2] == "button" then
        ami.mouse_button(0, e[3])
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
if shots_dir or ram_path or snap_name then report_counts() ami.quit(0) end

-- inputlog=FILE: play (the window stays) and write every key and mouse event the host processes as "frame kind a b" (frame counted
-- from the start or the loaded snapshot); replay=FILE plays such a file again.  The file is rewritten every second.
local input_log_path = args:match("inputlog=(%S+)")
if input_log_path then
    local lines = {}
    local last_saved = 0
    while true do
        ami.wait(1)
        local f = ami.frame() - base
        for _, ev in ipairs(ami.input()) do lines[#lines + 1] = ("%d %s %d %d"):format(f, ev[1], ev[2], ev[3]) end
        if f - last_saved >= 50 then
            ami.save_file(input_log_path, table.concat(lines, "\n") .. "\n")
            last_saved = f
        end
    end
end
