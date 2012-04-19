
local ffi = require "ffi"
ffi.cdef[[
float * audio_outbuffer(int c);
int audio_blocksize();
]]

local clang = require "clang"
--for k, v in pairs(clang) do print("clang", k, v) end
local cc = clang.Compiler()

local format = string.format

function substitute(str, dict)
	-- letter or _ followed by alphanumeric or _
	return str:gsub("%$([%a_][%w_]*)[{}]*", dict)
end

local id = 0
local gensym = function(name)
	id = id + 1
	return format("%s_%d", name, id)
end

local OpMeta = {}
OpMeta.__index = OpMeta

function OpMeta.__add(a, b) return setmetatable({ kind="+", a, b }, OpMeta) end
function OpMeta.__sub(a, b) return setmetatable({ kind="-", a, b }, OpMeta) end
function OpMeta.__mul(a, b) return setmetatable({ kind="*", a, b }, OpMeta) end
function OpMeta.__div(a, b) return setmetatable({ kind="/", a, b }, OpMeta) end
function OpMeta.__mod(a, b) return setmetatable({ kind="%", a, b }, OpMeta) end
function OpMeta.__pow(a, b) return setmetatable({ kind="^", a, b }, OpMeta) end

function OpMeta:mod(name, modulator, kind)
	local kind = kind or "+"
	local m = setmetatable({ kind=kind, self[name], modulator }, ModMeta)
	self[name] = m
	return modulator
end

local
function Sine(freq, amp)
	return setmetatable({ kind="Sine", freq=freq or 440, amp=amp or 1 }, OpMeta)
end

local
function Smooth(x, factor)
	return setmetatable({ kind="Smooth", x or 0, factor or 0.9 }, OpMeta)
end

local
function Phasor(freq, amp)
	return setmetatable({ kind="Phasor", freq=freq or 1, amp=amp or 1 }, OpMeta)
end

local
function Pan2(x, pan)
	return setmetatable({ kind="Pan2", pan=pan or 0.5, x }, OpMeta)
end

local generators = {}

local
function codegen(op, dict)
	if type(op) == "table" then
		local res = dict.memo[op]
		if res then return res end
		local g = assert(generators[op.kind])
		local names = { g(op, dict) }
		dict.memo[op] = names
		return names
	else
		return {tostring(op)}
	end
end

local binop_generator = function(op, dict)
	local a, b = codegen(op[1], dict), codegen(op[2], dict)
	local name = gensym("binop")
	table.insert(dict.statements, 
		format("\tlocal %s = (%s %s %s)", 
		name, a[1], op.kind, b[1]
	))
	return name
end
generators["+"] = binop_generator
generators["*"] = binop_generator
generators["-"] = binop_generator
generators["/"] = binop_generator

generators.Pan2 = function(op, dict)
	local x = codegen(op[1], dict)
	local pan = codegen(op.pan, dict)
	local left = gensym("left")
	local right = gensym("right")
	local angle = gensym("angle")
	table.insert(dict.statements, 
		format("local %s = %s*pi", angle, pan[1]))
	table.insert(dict.statements, 
		format("local %s = %s*cos(%s)", left, x[1], angle))
	table.insert(dict.statements, 
		format("local %s = %s*sin(%s)", right, x[1], angle))
	return left, right
end

generators.Sine = function(op, dict)
	local freq = codegen(op.freq, dict)
	local amp = codegen(op.amp, dict)
	local name = gensym("sine")
	local closure = format("local %s = make_sine() ", name)
	table.insert(dict.objects, closure)
	local result = gensym("sine")	
	table.insert(dict.statements, 
		format("local %s = %s(%s) * %s", 
		result, name, freq[1], amp[1]
	))
	return result
end
generators.Phasor = function(op, dict)
	local freq = codegen(op.freq, dict)
	local amp = codegen(op.amp, dict)
	local name = gensym("phasor")
	local closure = format("local %s = make_phasor() ", name)
	table.insert(dict.objects, closure)
	local result = gensym("sine")	
	table.insert(dict.statements, 
		format("local %s = %s(%s) * %s", 
		result, name, freq[1], amp[1]
	))
	return result
end
generators.Smooth = function(op, dict)
	local x = codegen(op[1], dict)
	local factor = codegen(op[2], dict)
	local name = gensym("smooth")
	local result = gensym("smooth")	
	local closure = format("local %s = make_smooth() ", name)
	table.insert(dict.objects, closure)
	table.insert(dict.statements,
		format("local %s = %s(%s, %s)", 
		result, name, x[1], factor[1])
	)
	return result
end


local template = [[
local ffi = require "ffi"
local C = ffi.C
local samplerate = 44100
local pi = math.pi
local sin = math.sin
local cos = math.cos

function make_smooth()
	local last=0
	return function(x, factor)
		last = x + factor*(last-x)
		return last
	end
end

function make_phasor()
	local phase = 0
	return function (freq)
		phase = (phase + freq / samplerate) % 1
		return phase
	end
end

function make_sine()
	local phase = 0
	return function (freq)
		phase = (phase + freq / samplerate)
		return sin(phase * 2 * pi)
	end
end

-- generated:
$objects
return function()
	local blocksize = C.audio_blocksize()
	local lbuf = C.audio_outbuffer(0)
	local rbuf = C.audio_outbuffer(1)
	for i = 0, blocksize-1 do
		$statements
		lbuf[i] = lbuf[i] + $left
		rbuf[i] = rbuf[i] + $right
	end
end
]]

function OpMeta:generate()
	local dict = {
		memo = {},
		objects = {},
		statements = {},
	}
	
	local results = codegen(self, dict)
	dict.left = results[1] or "0"
	dict.right = results[2] or dict.left
	
	dict.objects = table.concat(dict.objects, "\n")
	dict.statements = substitute(table.concat(dict.statements, "\n\t"), dict)
	
	local code = substitute(template, dict)
	local create = assert(loadstring(code))
	local perform = create()
	self.perform = perform
	return self, code
end

-- template:
local template = [=[

typedef __SIZE_TYPE__		size_t;
typedef struct lua_State lua_State;
typedef int (*lua_CFunction) (lua_State *L);		

#define LUA_GLOBALSINDEX	(-10002)
#define lua_upvalueindex(i)	(LUA_GLOBALSINDEX-(i))

extern "C" {
	
	double sin(double);
	double cos(double);
	double floor ( double );
	
	#define luai_nummod(a,b)	((a) - floor((a)/(b))*(b))
	
	void *(lua_touserdata) (lua_State *L, int idx);
	void  (lua_pushnumber) (lua_State *L, double n);
	void *(lua_newuserdata) (lua_State *L, size_t sz);
	void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
	__PTRDIFF_TYPE__ (luaL_checkinteger) (lua_State *L, int numArg);
	
	int audio_blocksize();
	float * audio_outbuffer(int c);
}

struct Synth {
	$struct
	
	void init() {
		$init
	}	
	
	inline void perform() {
		int blocksize = audio_blocksize();
		float * lbuf = audio_outbuffer(0);
		float * rbuf = audio_outbuffer(1);
		for (int i = 0; i< blocksize; i++) {
			$perform
			lbuf[i] += $left;
			rbuf[i] += $right;
		}
	}
};

int perform_buffer(lua_State * L) {
	Synth * s = (Synth *)lua_touserdata(L, lua_upvalueindex(1));
	s->perform();
	return 0;
}

extern "C" int create(lua_State * L) {
	Synth * s = (Synth *)lua_newuserdata(L, sizeof(Synth));
	s->init();
	lua_pushcclosure(L, perform_buffer, 1);
	return 1;
}
]=]

local generatorsC = {}

local
function codegenC(op, dict)
	if type(op) == "table" then
		local res = dict.memo[op]
		if res then return res end
		local g = assert(generatorsC[op.kind], format("no generator for %s", op.kind))
		local names = { g(op, dict) }
		dict.memo[op] = names
		return names
	elseif type(op) == "number" then
		return { format("double(%s)", tostring(op)) }
	else
		return { tostring(op) }
	end
end

generatorsC.Pan2 = function(op, dict)
	local x = codegenC(op[1], dict)
	local pan = codegenC(op.pan, dict)
	local left = gensym("left")
	local right = gensym("right")
	local angle = gensym("angle")
	table.insert(dict.perform, 
		format("const double %s = %s * $pi;", angle, pan[1]))
	table.insert(dict.perform, 
		format("const double %s = %s * cos(%s);", left, x[1], angle))
	table.insert(dict.perform, 
		format("const double %s = %s * sin(%s);", right, x[1], angle))
	return left, right
end

generatorsC.Sine = function(op, dict)
	local freq = codegenC(op.freq, dict)
	local amp = codegenC(op.amp, dict)
	local phase = gensym("sinephase")
	local result = gensym("sine")
	table.insert(dict.struct, format("double %s;", phase))
	table.insert(dict.init, format("%s = 0;", phase))
	table.insert(dict.perform, format("%s += %s * $invsamplerate;", phase, freq[1]))
	table.insert(dict.perform, format("const double %s = sin(%s * $twopi) * %s;", result, phase, amp[1]))
	return result
end

generatorsC.Phasor = function(op, dict)
	local phase = gensym("phase")
	local result = gensym("phasor")
	local freq = codegenC(op.freq, dict)
	local amp = codegenC(op.amp, dict)
	table.insert(dict.struct, format("double %s;", phase))
	table.insert(dict.init, format("%s = 0;", phase))
	table.insert(dict.perform, format("%s = luai_nummod(%s + %s * $invsamplerate, 1.);", phase, phase, freq[1]))
	table.insert(dict.perform, format("const double %s = %s * %s;", result, phase, amp[1]))
	return result
end

generatorsC.Smooth = function(op, dict)
	local last = gensym("smooth")
	local x = codegenC(op[1], dict)
	local factor = codegenC(op[2], dict)
	table.insert(dict.struct, format("double %s;", last))
	table.insert(dict.init, format("%s = 0;", last))
	table.insert(dict.perform, format("%s = %s + %s*(%s-%s);", last, x[1], factor[1], last, x[1]))
	return last
end

local binop_generatorC = function(op, dict)
	local a, b = codegenC(op[1], dict), codegenC(op[2], dict)
	local result = gensym("binop")
	table.insert(dict.perform, format("const double %s = (%s %s %s);", result, a[1], op.kind, b[1]))
	return result
end
generatorsC["+"] = binop_generatorC
generatorsC["*"] = binop_generatorC
generatorsC["-"] = binop_generatorC
generatorsC["/"] = binop_generatorC

function OpMeta:generateC()
	local dict = {
		memo = {},
		struct = {},
		init = {},
		perform = {},
		
		samplerate = "44100.0",
		invsamplerate = tostring(1./44100),
		pi = tostring(math.pi),
		twopi = tostring(math.pi * 2),
	}
	local results = codegenC(self, dict)
	dict.left = results[1] or "0"
	dict.right = results[2] or dict.left
	dict.struct = table.concat(dict.struct, "\n\t")
	dict.init = substitute(table.concat(dict.init, "\n\t\t"), dict)
	dict.perform = substitute(table.concat(dict.perform, "\n\t\t\t"), dict)
	
	local code = substitute(template, dict)
	
	cc:compile(code)
	cc:optimize()
	--cc:dump()
	local jit = cc:jit()
	local create = assert(jit:pushcfunction("create"))
	--return create
	---[[
	local perform = create()
	self.perform = perform
	self.jit = jit
	return self, code
	--]]
end

return {
	Sine = Sine,
	Phasor = Phasor,
	Smooth = Smooth,
	Pan2 = Pan2,
}