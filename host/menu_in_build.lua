-- Run the disk whose game is the C (local/gods-c.m3u), press space for the menu and report whether the C menu runs.
--   ami9000_host projects/gods/host/menu_in_build.lua --headless --script-args "until=FRAMES [shot=FILE.ppm] [keys=F:name:down|up,...]"
local args = ami.args or ""
local UNTIL = tonumber(args:match("until=(%d+)")) or 9000
ami.machine{ name = "Gods C menu", model = "A500", system = "../local/system", save = "../local/save", files = "..",
    content = "../local/gods-c.m3u", roms = { { "kick34005.A500", 0xc4f0f55f } }, opt = { puae_chipmem_size = 2 } }
ami.boot()
local function read_file(name) return ami.file_read(name, 0, ami.file_size(name)) end
local syms = load(read_file("native/build/menu_syms.lua"))()
local seen = {}
for _, name in ipairs({ "menu_entry", "menu_boot", "menu_start", "menu_main", "password_screen", "top_ten_screen" }) do
    ami.hook(syms[name], function() if not seen[name] then seen[name] = ami.frame(); ami.log(name .. " at frame " .. ami.frame()) end end)
end
for _, a in ipairs({ 0x80000, 0x150, 0x84, 0xb8, 0xf6, 0x39e50, 0x240c, 0x6388 }) do ami.hook(a, function() ami.log(string.format('pc %x at frame %d  a0=%x a7=%x stack=%s', a, ami.frame(), ami.reg('a0'), ami.reg('a7'), ami.read(ami.reg('a7'),16):gsub('.',function(c) return string.format('%02x',c:byte()) end))) end) end
for _, a in ipairs({ 0xae, 0xb0 }) do ami.hook(a, function() ami.log(string.format('exec %x frame %d bytes@ae=%s', a, ami.frame(), ami.read(0xae,8):gsub('.',function(c) return string.format('%02x',c:byte()) end))) end) end
ami.hook(syms.menu_start, function() if not DUMPED then DUMPED = true; ami.save_file('/tmp/claude-1000/hyb_start.ram', ami.read(0, 0x100000)) end end)
local keys = {}
for f, name, state in (args:match("keys=(%S+)") or ""):gmatch("(%d+):(%a+):(%a+)") do keys[tonumber(f)] = { name, state == "down" } end
while ami.frame() < UNTIL do
    ami.wait(1)
    local f = ami.frame()
    if f == 2700 then ami.key(ami.keys.space, true) end
    if f == 2703 then ami.key(ami.keys.space, false) end
    if keys[f] then ami.key(ami.keys[keys[f][1]], keys[f][2]) end
end
local dump = args:match('dump=(%S+)')
if dump then ami.save_file(dump, ami.read(0, 0x100000)) end
local shot = args:match("shot=(%S+)")
if shot then ami.screenshot(shot) end
ami.quit(0)
