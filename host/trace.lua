-- Log the frame in which each of a set of addresses is reached (a timeline of one loop in both programs).
--
--   ami9000_host trace.lua --headless --script-args "[rebuilt] start=F frames=N hooks=name:HEX,name:HEX out=PATH"
local args = ami.args or ""
ami.machine{
    name = "Gods trace", model = "A500", system = "../local/system", save = "../local/save",
    content = args:match("rebuilt") and "../local/gods-c.m3u" or "../local/gods.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
}
ami.boot()
local start, frames = tonumber(args:match("start=(%d+)")), tonumber(args:match("frames=(%d+)"))
while ami.frame() < start do ami.wait(1) end
local log = {}
for name, addr in (args:match("hooks=(%S+)") or ""):gmatch("(%w+):(%x+)") do
    ami.hook(tonumber(addr, 16), function() log[#log + 1] = ("%d %s"):format(ami.frame(), name) end)
end
local from = ami.frame()
while ami.frame() < from + frames do ami.wait(1) end
ami.save_file(args:match("out=(%S+)"), table.concat(log, "\n") .. "\n")
ami.quit(0)
