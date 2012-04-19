local format = string.format
local audio = require "audio"
local ffi = require "ffi"
local g = require "generators"

local Sine, Phasor, Smooth, Pan2 = g.Sine, g.Phasor, g.Smooth, g.Pan2

jit.off()
jit.on()

local random = math.random

function noise()
	return random() * 2 - 1
end

function scene()
	local s = Sine(110 * math.random(16), 0.1)
	return s, s:generate()
end

function scene()
	local s = Sine(110 * math.random(16), 0.1 * Phasor(-6)) * Phasor(-2)
	return s, s:generate()
end

function scene()
	local s = Sine(110 * math.random() * 16, 0.05)
	--[[
	local p = Phasor(-math.random(10), 0.1)

	local lfo = Sine(math.random(8), 4 * math.random(10))

	s:mod("freq", lfo)
	s.amp = Smooth(s.amp * 4*p*p, 0.99)			-- s.freq = 440 + LFO
	p:mod("freq", Sine(math.random(20), math.random(6)))
	return Pan2(s, Sine(0.5/math.random(8))):generate()
	--]]
	return s:generate()
end

--local create = scene()

synths = {}
local code
for i = 1, 50 do
	synths[i], code = scene()
end
print(code)

local min, mean, max = 100, 0, 0

local count = 0
function process(l, r)
	-- cast l, r to proper pointer types:
	local lbuf = ffi.cast("float *", l)
	local rbuf = ffi.cast("float *", r)
	
	for _, v in ipairs(synths) do
		--v.perform(blocksize, lbuf, rbuf)
		v.perform()
	end
	
	
	local cpu = audio.cpu()*100
	mean = mean + 0.01*(cpu-mean)
	max = math.max(max, cpu)
	count = count + 1
	local often = 50
	if count % often == 0 then
		local id = 1+((count/often) % #synths)
		synths[id] = scene()
		
		print(format("cur %3.2f %% mean %3.2f %% max %3.2f %% cpu", cpu, mean, max))
		clang.sweep()
	end
	
	collectgarbage()
end