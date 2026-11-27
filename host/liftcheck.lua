-- Oracle for scripts/test_lift_gods.py: call original functions on the real core from planned register states.
--
--   ../amiga_porting/ami9000/build/ami9000_host host/liftcheck.lua --headless --script-args "out=/abs/results.txt"
--
-- Reads ../native/build/lifttest/plan.txt (lines: entry d0..d7 a0..a7 flags, hex), restores gods-11000.state, then for each
-- line restores chip RAM, runs the routine with ami.call (interrupts masked) and writes one result line:
--   entry d0..d7 a0..a7 sr | addr:byte,addr:byte,...   (every chip RAM byte that changed)
ami.machine{
    name = "Gods liftcheck",
    model = "A500",
    system = "../local/system",
    save = "../local/save",
    content = "../local/gods.m3u",
    files = "../native/build/lifttest",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
}
ami.boot()

local out = (ami.args or ""):match("out=(%S+)")
ami.wait(1)
assert(ami.snapshot_load("gods-11000.state"), "cannot load snapshot")
ami.wait(1)
local base = ami.read(0, 0x80000)
ami.save_file(out .. ".base", base)

local function changed(after)
    local list = {}
    for off = 0, 0x7ffff, 4096 do
        local a, b = after:sub(off + 1, off + 4096), base:sub(off + 1, off + 4096)
        if a ~= b then
            for i = 1, 4096 do
                local x = a:byte(i)
                if x ~= b:byte(i) then
                    list[#list + 1] = ("%x:%02x"):format(off + i - 1, x)
                    if #list >= 400 then return list end
                end
            end
        end
    end
    return list
end

ami.hook(0x1dd6, function()
    ami.unhook(0x1dd6)
    local plan = ami.file_read("plan.txt")
    local lines = {}
    for line in plan:gmatch("[^\n]+") do
        local f = {}
        for w in line:gmatch("%S+") do f[#f + 1] = w end
        ami.write(0, base)
        local regs = { sr = 0x2700 | tonumber(f[18], 16) }
        for i = 0, 7 do
            regs["d" .. i] = tonumber(f[2 + i], 16)
            regs["a" .. i] = tonumber(f[10 + i], 16)
        end
        local r = ami.call(tonumber(f[1], 16), regs)
        local row = { f[1] }
        for i = 0, 7 do row[#row + 1] = ("%08x"):format(r["d" .. i]) end
        for i = 0, 7 do row[#row + 1] = ("%08x"):format(r["a" .. i]) end
        row[#row + 1] = ("%04x"):format(r.sr or 0)
        lines[#lines + 1] = table.concat(row, " ") .. " | " .. table.concat(changed(ami.read(0, 0x80000)), ",")
    end
    ami.save_file(out, table.concat(lines, "\n") .. "\n")
    ami.quit(0)
end)

while true do ami.wait(1) end
