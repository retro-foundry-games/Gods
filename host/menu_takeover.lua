-- Run the menu's C code in place of the original's main thread, and record the machine each frame.
--
-- Loads a savestate of the original menu (1 MB chip RAM), and records per frame the checksums of the screens, the copper
-- lists, the palette and the program's variables.  With mode=c, when the original reaches the top of its main menu loop (orig
-- $0063A2, after the top ten) the main thread continues in C (menu_main of build/menu.blob) while the original's interrupt
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
local syms = load(read_file("build/menu_syms.lua"))()
local blob = read_file("build/menu.blob")

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
local start = ami.frame()
local orig_irq3, orig_irq4 = ami.peek32(0x6c), ami.peek32(0x70)
if MODE ~= "orig" then
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
local joys = {}
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
    joys[tonumber(f)] = st
end
local DUMP = tonumber(args:match("dump=(%d+)"))
local EVENTS = {}
for name in ((" " .. args):match(" trace=(%S+)") or ""):gmatch("[%w_]+") do
    ami.hook(syms[name], function() EVENTS[#EVENTS + 1] = string.format("%d:%s", ami.frame() - start, name) end)
end
for addr, name in (args:match("otrace=(%S+)") or ""):gmatch("(%x+):([%w_]+)") do
    ami.hook(tonumber(addr, 16), function() EVENTS[#EVENTS + 1] = string.format("%d:%s", ami.frame() - start, name) end)
end
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
    if joys[f] then ami.joystick(joys[f]) end
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
ami.save_file(OUT, table.concat(log, "\n") .. "\n")
ami.quit(0)
