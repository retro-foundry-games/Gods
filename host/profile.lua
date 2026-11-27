-- Profile an address range over a number of frames with a hook on every instruction: the cycles that pass between
-- one hit and the next (ami.cycles) are charged to the instruction that was running, and its executions are counted.
--
--   ../amiga_porting/ami9000/build/ami9000_host host/profile.lua --headless \
--       --script-args "[rebuilt] start=FRAME frames=N lo=HEX hi=HEX out=PATH"
--
-- Boots (the original disks, or with `rebuilt` the C game), runs to FRAME, hooks every even address in [lo, hi),
-- runs N frames and writes "address count cycles" lines.  Cycles charged to the last instruction before leaving the range include the
-- time spent outside it, so profile the whole program (the Kickstart idle loop is outside the range).
local args = ami.args or ""
ami.machine{
    name = "Gods profile",
    model = "A500",
    system = "../local/system",
    save = "../local/save",
    content = args:match("rebuilt") and "../local/gods-c.m3u" or "../local/gods.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
}
ami.boot()
local start, frames = tonumber(args:match("start=(%d+)")), tonumber(args:match("frames=(%d+)"))
local lo, hi = tonumber(args:match("lo=(%x+)"), 16), tonumber(args:match("hi=(%x+)"), 16)
local out = args:match("out=(%S+)")
while ami.frame() < start do ami.wait(1) end
local counts, spent = {}, {}
local last_pc, last_cycles
local function hit()
    local pc, c = ami.reg("pc"), ami.cycles()
    counts[pc] = (counts[pc] or 0) + 1
    if last_pc then spent[last_pc] = (spent[last_pc] or 0) + (c - last_cycles) end
    last_pc, last_cycles = pc, c
end
for a = lo, hi - 2, 2 do ami.hook(a, hit) end
local from = ami.frame()
while ami.frame() < from + frames do ami.wait(1) end
local lines = {}
for pc, n in pairs(counts) do lines[#lines + 1] = ("%x %d %d"):format(pc, n, spent[pc] or 0) end
ami.save_file(out, table.concat(lines, "\n") .. "\n")
ami.quit(0)
