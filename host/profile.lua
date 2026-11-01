-- Count the instructions executed in an address range over a number of frames (a statistical profile by hooks).
--
--   tools/engine9000/ami9000_host projects/gods/host/profile.lua --headless \
--       --script-args "[rebuilt] start=FRAME frames=N lo=HEX hi=HEX out=PATH"
--
-- Boots (the original disks, or with `rebuilt` the C game), runs to FRAME, hooks every even address in [lo, hi),
-- runs N frames and writes "address count" lines.  A hook removes itself never, so each execution is counted.
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
local counts = {}
local function hit() local pc = ami.reg("pc") counts[pc] = (counts[pc] or 0) + 1 end
for a = lo, hi - 2, 2 do ami.hook(a, hit) end
local from = ami.frame()
while ami.frame() < from + frames do ami.wait(1) end
local lines = {}
for pc, n in pairs(counts) do lines[#lines + 1] = ("%x %d"):format(pc, n) end
ami.save_file(out, table.concat(lines, "\n") .. "\n")
ami.quit(0)
