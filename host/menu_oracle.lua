-- Check the C functions of menu.c against the original program, on the machine as it runs.
--
-- Boots the original disks, loads the C code (native/build/menu.blob, from scripts/build_menu.py) into RAM above the original's
-- 512 KB, presses the fire button (the left mouse button) at frame CLICK so that the original loads the menu, and hooks the
-- original's entry of every function in host/menu_cases.lua.  At each call (the first SAMPLES of them) the state is saved,
-- the original routine is run on it with ami.call, the memory is put back, the C function is run on the same state with the
-- arguments in the registers the original has them in, and the two are compared: the result register and all of the original
-- chip RAM (except the C code and the stack of the test).  Then the state is put back, so the program goes on as if nothing had
-- happened.
--
--   ami9000_host projects/gods/host/menu_oracle.lua --headless --script-args "[only=NAME[,NAME]] [samples=N] [stride=N] [until=FRAMES] [click=FRAME] [state=NAME.state] [joy=F:DIRS,...]"
local args = ami.args or ""
local only = {}
for n in (args:match("only=([%w_,]+)") or ""):gmatch("[%w_]+") do only[n] = true end
local SAMPLES = tonumber(args:match("samples=(%d+)")) or 12
local UNTIL = tonumber(args:match("until=(%d+)")) or 9000
local CLICK = tonumber(args:match("click=(%d+)")) or 2700
local STRIDE = tonumber(args:match("stride=(%d+)")) or 1   -- compare every STRIDE-th call, to spread the samples over the run

local STATE = args:match("state=(%S+)")   -- start from a savestate (made with host/gods.lua snapname=, same options)
ami.machine{
    name = "Gods menu oracle", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    snapshots = "../local/save",
    content = "../local/gods.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } },
    opt = { puae_chipmem_size = 2, puae_joyport_order = "2143" },   -- 1 MB chip RAM for the C blob, the joystick in port 1
}
ami.boot()

local function read_file(name) return ami.file_read(name, 0, ami.file_size(name)) end
local syms = load(read_file("native/build/menu_syms.lua"))()
local blob = read_file("native/build/menu.blob")
local cases = load(read_file("host/menu_cases.lua"))()

local BLITTER_WAIT = 0x01dfd8   -- the original's blitter_wait
local SCRATCH = 0xE8000   -- a stack for the test, above the memory that is compared
local CHIP = 0x100000   -- the whole of the machine's 1 MB chip RAM: the original program may use the half above 512 KB too
local DATA_REGS = { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6" }

local function reg_value(spec, regs)
    local name, width = spec:match("^(%w+)%.?(%w*)$")
    local v = regs[name]
    if width == "w" then v = v & 0xffff elseif width == "b" then v = v & 0xff end
    return v
end

-- the addresses at which two memory images differ (at most `limit`), each as {address = true}
local function differences(a, b, limit)
    local out, n = {}, 0
    for block = 1, #a, 4096 do
        if a:sub(block, block + 4095) ~= b:sub(block, block + 4095) then
            for i = block, math.min(block + 4095, #a) do
                if a:byte(i) ~= b:byte(i) then out[i - 1] = true; n = n + 1 end
            end
            if limit and n >= limit then break end
        end
    end
    return out
end

local log = {}
local function say(...) local s = string.format(...) log[#log + 1] = s end

local busy = false
local function check(case)
    if busy then return end   -- the hook fires inside the calls this function makes
    if case.refire then case.refire = false; return end   -- and once more as the program goes on after a comparison
    case.calls = (case.calls or 0) + 1
    if case.done >= SAMPLES or (case.calls - 1) % STRIDE ~= 0 then return end
    busy = true
    local regs = {}
    for _, r in ipairs(DATA_REGS) do regs[r] = ami.reg(r) end
    local before = ami.read(0, CHIP)
    local run = {}
    for k, v in pairs(regs) do run[k] = v end
    run.a7, run.sr = SCRATCH, case.sr or 0x2700
    -- functions that use the blitter: it goes on after they return, so wait for it (the original's own wait)
    local function settle()
        if case.blitter then ami.call(BLITTER_WAIT, { a7 = SCRATCH, sr = 0x2700 }) end
    end
    settle()
    local before_settled = ami.read(0, CHIP)
    if before_settled ~= before then before = before_settled end
    -- a routine that waits for the vertical blank cannot be run here (a nested call does not get across a frame): the wait
    -- routines of both versions are made to return at once for this comparison (the memory is put back afterwards)
    if case.no_wait then ami.write(case.no_wait.orig, "\x4e\x75") end
    local r1 = ami.call(case.orig, run)
    settle()
    local after_orig = ami.read(0, CHIP)
    ami.write(0, before)
    -- the original once more: what differs between the two runs is done by the chips (the disk DMA writes the track
    -- buffer while the test runs) and is left out of the comparison
    local run1b = {}
    for k, v in pairs(regs) do run1b[k] = v end
    run1b.a7, run1b.sr = SCRATCH, case.sr or 0x2700
    if case.no_wait then ami.write(case.no_wait.orig, "\x4e\x75") end
    ami.call(case.orig, run1b)
    settle()
    local noise = differences(after_orig, ami.read(0, CHIP))
    ami.write(0, before)
    -- and the buffer the disk DMA is filling, which the chips write at their own pace
    local dskpt = (ami.custom(0x20) << 16) | ami.custom(0x22)
    local dsklen = (ami.custom(0x24) & 0x3fff) * 2
    for i, spec in ipairs(case.args) do
        ami.write(SCRATCH + 4 * (i - 1), string.pack(">I4", reg_value(spec, regs)))
    end
    local run2 = {}
    for k, v in pairs(regs) do run2[k] = v end
    run2.a7, run2.sr = SCRATCH, case.sr or 0x2700
    ami.write(syms.base, blob)   -- the program may have used the memory the C code sits in
    if case.no_wait then ami.write(syms[case.no_wait.c], "\x4e\x75") end
    local r2 = ami.call(syms[case.name], run2)
    settle()
    local after_c = ami.read(0, CHIP)
    ami.write(0, before)
    busy = false
    case.refire = true
    case.done = case.done + 1
    local problems = {}
    if case.ret then
        local want = reg_value(case.ret, r1)
        local got = r2.d0
        local width = case.ret:match("%.(%w)")
        if width == "w" then got = got & 0xffff elseif width == "b" then got = got & 0xff end
        if want ~= got then problems[#problems + 1] = string.format("result %s: orig %x C %x", case.ret, want, got) end
    end
    if after_orig ~= after_c then
        local shown = 0
        local positions = {}
        for pos in pairs(differences(after_orig, after_c, 64)) do positions[#positions + 1] = pos end
        table.sort(positions)
        for _, pos in ipairs(positions) do
            if pos >= 8 and not (pos >= syms.base and pos < syms.base + #blob) and not (pos >= SCRATCH - 0x1000 and pos < SCRATCH + 0x100) and not noise[pos] and not (pos >= dskpt and pos < dskpt + dsklen) and shown < 12 then
                shown = shown + 1
                problems[#problems + 1] = string.format("memory $%05x orig %02x C %02x", pos, after_orig:byte(pos + 1), after_c:byte(pos + 1))
            end
        end
    end
    if #problems == 0 then
        case.ok = (case.ok or 0) + 1
    else
        case.bad = (case.bad or 0) + 1
        local inputs = {}
        for _, spec in ipairs(case.args) do inputs[#inputs + 1] = string.format("%s=%x", spec, reg_value(spec, regs)) end
        say("DIFF %s (call %d, frame %d) %s: %s", case.name, case.calls, ami.frame(), table.concat(inputs, " "), table.concat(problems, "; "))
    end
end

local active = {}
for _, case in ipairs(cases) do
    if (next(only) == nil or only[case.name]) and syms[case.name] then
        case.done = 0
        active[#active + 1] = case
        ami.hook(case.orig, function() check(case) end)
    elseif not syms[case.name] then
        say("missing in native/build/menu_syms.lua: %s", case.name)
    end
end

-- the joystick scenario of --script-args joy=F:DIRS,... (frames from the start, or from the savestate)
local joys = {}
for f, dirs in (args:match("joy=(%S+)") or ""):gmatch("(%d+):([%a+]+)") do
    local st = {}
    for d in dirs:gmatch("%a+") do if d ~= "none" then st[d] = true end end
    joys[tonumber(f)] = st
end
local start = 0
local loaded = false
if STATE then
    ami.wait(1)
    assert(ami.snapshot_load(STATE), "cannot load " .. STATE)
    ami.write(syms.base, blob); loaded = true
    start = ami.frame()
end
while ami.frame() - start < UNTIL do
    ami.wait(1)
    local f = ami.frame() - start
    if not loaded and ami.frame() >= CLICK - 100 then ami.write(syms.base, blob); loaded = true end
    if not STATE and f == CLICK then ami.mouse_button(0, true) end
    if not STATE and f == CLICK + 3 then ami.mouse_button(0, false) end
    if joys[f] then ami.joystick(joys[f]) end
end
say("--- %d frames; samples per function up to %d", UNTIL, SAMPLES)
for _, case in ipairs(active) do
    say("%-22s calls %5d  compared %3d  ok %3d  differ %3d", case.name, case.calls or 0, case.done, case.ok or 0, case.bad or 0)
end
ami.save_file(args:match("out=(%S+)") or "/tmp/menu_oracle.txt", table.concat(log, "\n") .. "\n")
ami.quit(0)
