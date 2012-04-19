local g = require "generators"
local Sine, Phasor, Smooth = g.Sine, g.Phasor, g.Smooth


function scene()
	local s = Sine(110 * math.random(16), 0.1)
	return s, s:generateC()
end

function scene()
	local s = Sine(110 * math.random(16), 0.5)
	local p = Phasor(-math.random(10), 0.1)

	local lfo = Sine(math.random(8), 4 * math.random(10))

	s:mod("freq", lfo)
	s.amp = Smooth(s.amp * 4*p*p, 0.99)			-- s.freq = 440 + LFO
	p:mod("freq", Sine(math.random(20), math.random(6)))
	return s:generate()
end

local s = scene()

for i = 1, 10 do
	local perform
	for j = 1,10 do 
		s = scene()
		collectgarbage()
	end
	s.perform(0)
	collectgarbage()
end