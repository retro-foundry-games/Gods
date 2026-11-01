-- the points of the frame at which the interrupt handler starts, from the tick counter reset to the first vbl_wait
-- --script-args "[rebuilt] reset=HEX irq=HEX wait=HEX"
local rebuilt = ami.args:match("rebuilt")
ami.machine{ name="phase", model="A500", system="../local/system", save="../local/save",
  content = rebuilt and "../local/gods-c.m3u" or "../local/gods.m3u",
  roms={{"kick34005.A500",0xc4f0f55f}}, opt = rebuilt and {puae_chipmem_size=2} or nil }
ami.boot()
local boundary, state = 0, 0
local function hx(k) return tonumber(ami.args:match(k .. "=(%x+)"), 16) end
ami.hook(hx("reset"), function()
    if state == 0 then state = 1 print(string.format("RESET frame=%d since_boundary=%d", ami.frame(), ami.cycles() - boundary)) end
end)
ami.hook(hx("irq"), function()
    if state == 1 then print(string.format("IRQ   frame=%d since_boundary=%d", ami.frame(), ami.cycles() - boundary)) end
end)
ami.hook(hx("wait"), function()
    if state == 1 then state = 2 print(string.format("WAIT  frame=%d since_boundary=%d", ami.frame(), ami.cycles() - boundary)) end
end)
local n = 0
while state < 2 and n < 5000 do ami.wait(1) boundary = ami.cycles() n = n + 1 end
ami.quit(0)
