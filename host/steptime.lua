-- Time, in CPU cycles (a PAL frame is 141,875), from one address being reached to another being reached (for example the start of a scroll
-- step's drawing and the page flip), averaged over the hits after a start frame.
--
--   ami9000_host projects/gods/host/steptime.lua --headless --script-args "[rebuilt] start=F from=HEX to=HEX n=30"
local args = ami.args or ""
ami.machine{
    name = "Gods steptime", model = "A500", system = "../local/system", save = "../local/save",
    content = args:match("rebuilt") and "../local/gods-c.m3u" or "../local/gods.m3u",
    roms = { { "kick34005.A500", 0xc4f0f55f } },
}
ami.boot()
local start = tonumber(args:match("start=(%d+)"))
local from, to = tonumber(args:match("from=(%x+)"), 16), tonumber(args:match("to=(%x+)"), 16)
local want = tonumber(args:match("n=(%d+)"))
local function stamp() return ami.cycles() end
local t0, samples = nil, {}
while ami.frame() < start do ami.wait(1) end
ami.hook(from, function() t0 = stamp() end)
ami.hook(to, function()
    if t0 then samples[#samples + 1] = stamp() - t0; t0 = nil end
end)
while #samples < want and ami.frame() < start + 3000 do ami.wait(1) end
local sum = 0
for _, d in ipairs(samples) do sum = sum + d end
ami.log(("%d samples, average %d cycles (%.2f frames); first: %s"):format(#samples, sum // math.max(#samples, 1),
    sum / math.max(#samples, 1) / 141875, table.concat(samples, ",", 1, math.min(#samples, 12))))
ami.quit(0)
