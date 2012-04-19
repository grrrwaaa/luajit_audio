#include <iostream>
#include "al_AudioIO.hpp"
#include "al_Lua.hpp"

#define AUDIO_EXTERN __attribute__((visibility("default")))

extern "C" const char * exepath(const char * str) {
	std::string src = str;
	size_t pos = src.find_last_of("/");
	if(std::string::npos != pos){
		return src.substr(0, pos+1).c_str();
	}
	return "./";
}

bool active = 0;
char * script = 0;
void audioCB(al::AudioIOData& io) {
	al::Lua& L = io.user<al::Lua>();
	if (!active) return;
	
	io.zeroOut();
	
	if (script) {
		L.dofile(script);	
		script = 0;
	}
	
	// resume lua:
	L.getglobal("process");
	lua_pushlightuserdata(L, io.outBuffer(0));
	lua_pushlightuserdata(L, io.outBuffer(1));
	L.pcall(2, "process");
}

al::AudioIO * io;

int lua_audio_stop(lua_State * L) {
	active = 0;
	return 0;
}

int lua_audio_cpu(lua_State * L) {
	lua_pushnumber(L, io->cpu());
	return 1;
}

int luaopen_audio(lua_State * L) {
	struct luaL_reg lib[] = {
		{ "stop", lua_audio_stop },
		{ "cpu", lua_audio_cpu },
		{NULL, NULL}
	};
	lua_newtable(L);
	luaL_register(L, "audio", lib);
	return 1;
}

extern "C" {
	AUDIO_EXTERN float * audio_outbuffer(int c);
	AUDIO_EXTERN int audio_blocksize();
}

float * audio_outbuffer(int c) {
	return io->outBuffer(c);
}

int audio_blocksize() {
	return io->framesPerBuffer();
}

int main (int argc, char * const argv[]) {

	chdir(exepath(argv[0]));
	script = argv[1];
	
//	char path[1024];
//	getcwd(path, 1024);
//	//printf("cwd %s\n", path);
	
	al::Lua L;
	al::AudioIO audioIO(1024, 44100, audioCB, &L, 2,2);
	io = &audioIO;
	
	L.dostring("print(jit.version)");
	L.dostring("print(package.path)");
	L.preloadlib("audio", luaopen_audio);
	
	// run main script... 
	L.dofile(script);
	
	L.push(audioIO.framesPerBuffer());	
	L.setglobal("blocksize");
	L.push(audioIO.framesPerSecond());	
	L.setglobal("samplerate");
	
	audioIO.start();
	active = 1;
	
	// wait for input
	while (active and getchar()) {
		// sleep or wait on repl
		//al_sleep(0.01);
		script = argv[1];	
	}
	
	printf("done\n");
	audioIO.stop();
    return 0;
}
