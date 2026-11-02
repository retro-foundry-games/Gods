-- Run the menu's C code in place of the original's main thread, and record the machine each frame.
--
-- Loads a savestate of the original menu (1 MB chip RAM), and records per frame the checksums of the screens, the copper
-- lists, the palette and the program's variables.  With mode=c, when the original reaches the top of its main menu loop (orig
-- $0063A2, after the top ten) the main thread continues in C (menu_main of native/build/menu.blob) while the original's interrupt
-- handlers go on; with mode=irq the interrupt vectors are C's too (vbl_irq, audio_irq).  mode=orig only records.
--
--   ami9000_host projects/gods/host/menu_takeover.lua --headless --script-args "mode=orig|c|irq state=NAME.state until=F out=FILE"
local args = ami.args or ""
local MODE = args:match("mode=(%w+)") or "orig"
local STATE = args:match("state=(%S+)") or "gods-menu-1m.state"
local UNTIL = tonumber(args:match("until=(%d+)")) or 3000
local OUT = args:match("out=(%S+)") or "/tmp/menu_takeover.txt"
ami.machine{
    name = "Gods menu takeover", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },
}
ami.boot()
local function read_file(name) return ami.file_read(name, 0, ami.file_size(name)) end
local syms = load(read_file("native/build/menu_syms.lua"))()
local blob = read_file("native/build/menu.blob")

-- the regions that are recorded: name, first address, length
local REGIONS = {
    { "screen1", 0x6f800, 0x8400 }, { "screen2", 0x77c00, 0x8400 }, { "copper", 0x4d00, 0x800 },
    { "vars", 0x100, 0xf00 }, { "system", 0x1d000, 0x600 }, { "sound", 0x20000, 0x600 },
}
local function checksum(data)
    local sum = 0
    local n = #data // 8
    for i = 0, n - 1, 512 do
        local count = math.min(512, n - i)
        for _, v in ipairs({ string.unpack(">" .. string.rep("I8", count), data, i * 8 + 1) }) do
            if type(v) == "number" then sum = (sum * 31 + v) & 0x7fffffffffffffff end
        end
    end
    return sum
end

local took = false
ami.wait(1)
assert(ami.snapshot_load(STATE), "cannot load " .. STATE)
-- kbd=1 and replay=FILE: the human run of the game (as in menu_oracle.lua)
local game_keys = {}
if args:match("kbd=1") then
    ami.write(0x1e608, read_file("native/build/keyboard_patch.bin"))
    ami.write(0x1e420, "\x4e\xb9\x00\x01\xe6\x08")
    ami.write(0x1e2f8, "\x4e\x71\x4e\x71")
end
do
    local rp = args:match("replay=(%S+)")
    if rp then
        for line in read_file(rp):gmatch("[^\n]+") do
            local f, kind, a, b = line:match("(%d+) (%a) (-?%d+) (-?%d+)")
            if f and kind == "k" then game_keys[tonumber(f)] = game_keys[tonumber(f)] or {}; table.insert(game_keys[tonumber(f)], { tonumber(a), tonumber(b) ~= 0 }) end
        end
    end
end
local start = ami.frame()
if MODE == "gameirq" then   -- the same, and the interrupts are C's too (the sound code starts with the original's state)
    ami.write(syms.base, blob)
    ami.write(syms.last_random, ami.read(0x1faca, 2))
    ami.write(syms.voice_pointers, ami.read(0x1fe2e, 16))
    ami.write(syms.voice_stacks, ami.read(0x1fefe, 16))
    ami.write(syms.tempo, ami.read(0x2034a, 2))
    ami.poke32(0x68, syms.keyboard_irq)
    ami.poke32(0x6c, syms.vbl_irq)
    ami.poke32(0x70, syms.audio_irq)
    MODE = "game"
end
if MODE == "game" then   -- the game loop's body in C each frame (its routines the original's)
    local glue = read_file("native/build/takeover_glue.bin")
    local glue_at = load(read_file("native/build/takeover_glue.lua"))()
    ami.hook(0x2c20, function()
        iter_top()
        ami.write(syms.base, blob)
        ami.write(glue_at.base, glue)
        ami.set_reg("pc", glue_at.game_frame)
    end)
end
local orig_irq3, orig_irq4 = ami.peek32(0x6c), ami.peek32(0x70)
if MODE == "c" or MODE == "irq" then
    ami.hook(0x63a2, function()
        if took then return end
        took = true
        ami.write(syms.base, blob)
        if MODE == "irq" then
            -- the sound code keeps some state in its own data: it starts as the original's is
            ami.write(syms.last_random, ami.read(0x1faca, 2))
            ami.write(syms.voice_pointers, ami.read(0x1fe2e, 16))
            ami.write(syms.voice_stacks, ami.read(0x1fefe, 16))
            ami.write(syms.tempo, ami.read(0x2034a, 2))
            ami.poke32(0x6c, syms.vbl_irq)
            ami.poke32(0x70, syms.audio_irq)
        end
        ami.set_reg("pc", syms.menu_main)
        TOOK_FRAME = ami.frame() - start
    end)
end

-- iterkeys=FILE: the keys by iteration of the game loop ("N k code state": at the top of iteration N+1); iterlog=FILE (with replay=, mode=orig): write that file from
-- the frame-timed replay (what iteration each key event fell into); itershot=N,N,...: a screenshot into shots=DIR at the top of the Nth iteration
iter_keys = {}
do
    local fp = args:match("iterkeys=(%S+)")
    if fp then
        for line in read_file(fp):gmatch("[^\n]+") do
            local n, a, b, off = line:match("^(%d+) k (-?%d+) (-?%d+) ?(%d*)")
            if n then iter_keys[tonumber(n)] = iter_keys[tonumber(n)] or {}; table.insert(iter_keys[tonumber(n)], { tonumber(a), tonumber(b) ~= 0, tonumber(off) or 0 }) end
        end
    end
end
ITER, ITERLOG = 0, {}
iter_shots = {}
for n in (args:match("itershot=([%d,]+)") or ""):gmatch("%d+") do iter_shots[tonumber(n)] = true end
shots_dir = args:match("shots=(%S+)")
ITERVARS, ITERKEYS = {}, {}
KIRQ = 0
KLOG = {}
ami.hook(0x1e2b8, function() KIRQ = KIRQ + 1; KLOG[#KLOG + 1] = string.format("%d it=%d frame=%d sr=%04x pc_ret=%x", KIRQ, ITER, ami.frame() - start, ami.reg("sr") & 0xffff, ami.peek32(ami.reg("a7") + 2)) end)
-- hookmark=ADDR (hex): log the player and scroll words and the iteration each time the program reaches ADDR
do
    for hex in (args:match("hookmark=([%x,]+)") or ""):gmatch("%x+") do
        local at = tonumber(hex, 16)
        ami.hook(at, function() ami.log(string.format("HM %x it=%d px=%d sx=%d py=%d sy=%d sxa=%d", at, ITER, ami.peek16(0x1e0), ami.peek16(0x358), ami.peek16(0x1e2), ami.peek16(0x35a), ami.peek16(0x29a))) end)
    end
end
LIVE = args:match("live=(%S+)")
LIVE_KEYS = {}
-- the keys the game reads (the raw codes $4C-$4F and $40 of the table at $1E63A when the game reads them) are appended to PREFIX.keys as the SDL port's GODS_ITERKEYS has them ("N CODE V":
-- at the top of the SDL iteration N = ITER + 128), then the variables of the iteration go to the named pipe PREFIX.orig.fifo (nothing is kept: the reader compares them as they come), and
-- (liveshot=1) the frame on show, for the difference window of the SDL port (in memory: /dev/shm), and then the number of its iteration
function live_read()
    if not LIVE or not LIVE_PENDING then return end
    for _, code in ipairs({ 0x4c, 0x4d, 0x4e, 0x4f, 0x40 }) do
        local down = ami.read(0x1e63a + code, 1):byte() ~= 0
        if down ~= LIVE_KEYS[code] then
            LIVE_KEYS[code] = down
            ami.append_file(LIVE .. ".keys", string.format("%d %x %d\n", ITER + 128, code, down and 1 or 0))
        end
    end
    ami.append_file(LIVE .. ".orig.fifo", LIVE_PENDING)
    LIVE_PENDING = nil
    if args:match("liveshot=1") then
        ami.screenshot("/dev/shm/gods-live-orig.ppm")
        ami.save_file("/dev/shm/gods-live-orig.ppm.n", tostring(ITER))
    end
end
if LIVE then ami.hook(0x363a, live_read) end
PENDING_KEYS = {}
function iter_top()
    ITER = ITER + 1
    TOP_FRAME = ami.frame() - start
    if args:match("itervars=1") then ITERVARS[#ITERVARS + 1] = ami.read(0x100, 0xf00); ITERKEYS[#ITERKEYS + 1] = ami.read(0x1e63a + 0x4c, 4) .. ami.read(0x1e63a + 0x40, 1) .. string.pack(">I2I2", KIRQ, ami.peek16(0x1e630)) end
    for _, k in ipairs(iter_keys[ITER - 1] or {}) do   -- the key table of the keyboard interrupt is set at once ($1E63A: $FF for a key held down, by raw key code): the interrupt is
        local raw = ({ [273] = 0x4c, [274] = 0x4d, [275] = 0x4e, [276] = 0x4f, [32] = 0x40 })[k[1]]   -- asynchronous, two events close together lose one by the original's timing
        if raw then ami.write(0x1e63a + raw, k[2] and "\xff" or "\x00") end
    end
    -- iterregion=N:ADDR:LEN:PATH (hex ADDR and LEN): at the top of the Nth iteration that part of the memory is written to PATH
    for region_n, region_addr, region_len, region_path in (args:match("iterregion=(%S+)") or ""):gmatch("(%d+):(%x+):(%x+):([^,]+)") do
        if tonumber(region_n) == ITER then ami.save_file(region_path, ami.read(tonumber(region_addr, 16), tonumber(region_len, 16))) end
    end
    -- live=PREFIX (the window and the keyboard of the machine are used: scripts/lockstep_live.py): the variables $100-$FFF of the iteration are kept here until the game reads its keys
    -- (live_read, at $363A: the keyboard interrupt may come between the top of the iteration and that, and what the game reads is what counts)
    if LIVE then
        LIVE_PENDING = ami.read(0x100, 0xf00)
    end
    if iter_shots[ITER] and shots_dir then ami.screenshot(string.format("%s/iter-%d.ppm", shots_dir, ITER)) end
end
local joys = {}
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
    joys[tonumber(f)] = st
end
local keys = {}   -- keys=F:name:down|up,...
for f, name, state in (args:match("keys=(%S+)") or ""):gmatch("(%d+):(%a+):(%a+)") do keys[tonumber(f)] = { name, state == "down" } end
local STEPDUMP = tonumber(args:match("stepdump=(%d+)"))   -- at that frame: the pcs of the next instructions (the main thread)
local DUMP = tonumber(args:match("dump=(%d+)"))
local EVENTS = {}
for name in ((" " .. args):match(" trace=(%S+)") or ""):gmatch("[%w_]+") do
    ami.hook(syms[name], function() EVENTS[#EVENTS + 1] = string.format("%d:%s", ami.frame() - start, name) end)
end
for addr, name in (args:match("otrace=(%S+)") or ""):gmatch("(%x+):([%w_]+)") do
    ami.hook(tonumber(addr, 16), function() EVENTS[#EVENTS + 1] = string.format("%d:%s", ami.frame() - start, name) end)
end
if MODE ~= "game" then ami.hook(0x2c20, iter_top) end
local DUMPFLIP = tonumber(args:match("dumpflip=(%d+)"))
if args:match("gtrace=1") then
    local W = function() return string.format("%d d0=%d d1=%d d2=%d", ami.frame() - start, ami.reg("d0") & 0xffff, ami.reg("d1") & 0xffff, ami.reg("d2") & 0xffff) end
    local WC = function() local sp = ami.reg("a7"); return string.format("%d x=%d y=%d i=%d", ami.frame() - start, ami.peek32(sp + 4) & 0xffff, ami.peek32(sp + 8) & 0xffff, ami.peek32(sp + 12) & 0xffff) end
    ami.hook(0x17686, function() EVENTS[#EVENTS + 1] = "G:" .. W() end)
    if MODE ~= "orig" then ami.hook(syms.glyph_draw, function() EVENTS[#EVENTS + 1] = "C:" .. WC() end) end
end
local FLIPS = {}
local PENDING = {}
local function flip()   -- the screen is noted; its checksum is taken a frame later, when the blitter is done with it
    PENDING[#PENDING + 1] = { ami.peek32(0xe60), ami.peek16(0x38e), ami.peek16(0xb2a), ami.peek16(0x1d3a4), ami.read(0xe10, 32) }
end
local function settle_flips()
    for _, p in ipairs(PENDING) do
        if DUMPFLIP and #FLIPS == DUMPFLIP then ami.save_file(OUT .. ".flip", ami.read(p[1], 0x8400)) end
        FLIPS[#FLIPS + 1] = string.format("%x,%x,%x,%x,%x", checksum(ami.read(p[1], 0x8400)), checksum(p[5]), p[2], p[3], p[4])
    end
    PENDING = {}
end
ami.hook(0x1d302, flip)
if MODE ~= "orig" then ami.hook(syms.screens_flip, flip) end
local log = {}
while ami.frame() - start < UNTIL do
    ami.wait(1)
    settle_flips()
    local f = ami.frame() - start
    if STEPDUMP and f == STEPDUMP then
        local ring, n = {}, 0
        ami.step(function(pc) n = n + 1; ring[#ring + 1] = string.format("%x", pc); if #ring > 400 then table.remove(ring, 1) end; return n < 6000 end)
        ami.save_file(OUT .. ".steps", table.concat(ring, " "))
    end
    if joys[f] then ami.joystick(joys[f]) end
    if keys[f] then ami.key(ami.keys[keys[f][1]], keys[f][2]) end
    local i = 1
    while i <= #PENDING_KEYS do
        local p = PENDING_KEYS[i]
        if f >= p[1] then ami.key(p[2], p[3]); table.remove(PENDING_KEYS, i) else i = i + 1 end
    end
    for _, k in ipairs(game_keys[f] or {}) do ami.key(k[1], k[2]); ITERLOG[#ITERLOG + 1] = string.format("%d k %d %d %d", ITER, k[1], k[2] and 1 or 0, f - (TOP_FRAME or 0)) end
    if DUMP and f == DUMP then ami.save_file(OUT .. ".ram", ami.read(0, 0x80000)) end
    local line = { tostring(f) }
    PCS = PCS or {}
    PCS[#PCS + 1] = string.format("%d:%x", f, ami.reg("pc"))
    for _, r in ipairs(REGIONS) do line[#line + 1] = string.format("%x", checksum(ami.read(r[2], r[3]))) end
    line[#line + 1] = ami.read(0x100, 0xf00):gsub(".", function(c) return string.format("%02x", c:byte()) end)
    log[#log + 1] = table.concat(line, " ")
end
log[#log + 1] = string.format("final cursor=%d line=%s state=%d count=%d", ami.peek16(0x38e), ami.read(ami.peek32(0x3a8), 20):gsub("[%z\1-\31]", function(c) return string.format("<%d>", c:byte()) end), ami.peek16(0xb2a), ami.peek16(0x6384))
log[#log + 1] = "flips " .. table.concat(FLIPS, " ")
log[#log + 1] = "events " .. table.concat(EVENTS, " ")
log[#log + 1] = "pcs " .. table.concat(PCS, " ")
log[#log + 1] = string.format("took %s irq3 %x irq4 %x", tostring(TOOK_FRAME), orig_irq3, orig_irq4)
if args:match("itervars=1") then
    ami.save_file(OUT .. ".iter", table.concat(ITERVARS))
    ami.save_file(OUT .. ".kirq", table.concat(KLOG, "\n"))
    ami.save_file(OUT .. ".keys", table.concat(ITERKEYS))   -- key_down for the codes $4C to $4F and $40 (up, down, right, left, space) at each iteration   -- the variables ($100-$FFF) at the top of every iteration, one after the other
end
if args:match("iterlog=(%S+)") then ami.save_file(args:match("iterlog=(%S+)"), table.concat(ITERLOG, "\n") .. "\n") end
ami.save_file(OUT, table.concat(log, "\n") .. "\n")
ami.quit(0)
