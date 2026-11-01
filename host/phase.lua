-- Where in the frame an instruction runs: cycles (ami.cycles, chipset units, about 70,937 a frame) since the last frame
-- boundary, at the first execution of each address given.   --script-args "[rebuilt] at=HEX[,HEX...]"
local rebuilt = ami.args:match("rebuilt")
ami.machine{ name="phase", model="A500", system="../local/system", save="../local/save",
  content = rebuilt and "../local/gods-c.m3u" or "../local/gods.m3u",
  roms={{"kick34005.A500",0xc4f0f55f}}, opt = rebuilt and {puae_chipmem_size=2} or nil }
ami.boot()
local boundary = 0
local seen, want = {}, 0
for h in (ami.args:match("at=([%x,]+)") or ""):gmatch("%x+") do
    local addr = tonumber(h, 16)
    want = want + 1
    ami.hook(addr, function()
        if seen[addr] then return end
        seen[addr] = true
        local c = ami.cycles()
        print(string.format("PHASE %x frame=%d cycles=%d since_boundary=%d", addr, ami.frame(), c, c - boundary))
    end)
end
local n = 0
while next(seen) == nil or #seen < 0 or n < 4000 do
    ami.wait(1); boundary = ami.cycles(); n = n + 1
    local k = 0 for _ in pairs(seen) do k = k + 1 end
    if k >= want then break end
end
ami.quit(0)
