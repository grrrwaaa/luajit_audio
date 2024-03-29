#ifndef INCLUDE_AL_AUDIO_IO_HPP
#define INCLUDE_AL_AUDIO_IO_HPP

/*	Allocore --
	Multimedia / virtual environment application class library
	
	Copyright (C) 2009. AlloSphere Research Group, Media Arts & Technology, UCSB.
	Copyright (C) 2006-2008. The Regents of the University of California (REGENTS). 
	All Rights Reserved.

	Permission to use, copy, modify, distribute, and distribute modified versions
	of this software and its documentation without fee and without a signed
	licensing agreement, is hereby granted, provided that the above copyright
	notice, the list of contributors, this paragraph and the following two paragraphs 
	appear in all copies, modifications, and distributions.

	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
	OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
	BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
	THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
	PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
	HEREUNDER IS PROVIDED "AS IS". REGENTS HAS  NO OBLIGATION TO PROVIDE
	MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.


	File description:
	An interface to low-level audio device streams

	File author(s):
	Lance Putnam, 2010, putnam.lance@gmail.com
*/

/*	This is a simple example demonstrating how to set up a callback
	and process input and output buffers.
	
	struct MyStuff{};

	// Simple: Using automatic indexing
	void audioCBSimple(AudioIOData& io){
		
		MyStuff& stuff = io.user<MyStuff>();

		while(io()){

			float inSample1 = io.in(0);
			float inSample2 = io.in(1);

			io.out(0) = -inSample1;
			io.out(1) = -inSample2;
		}
	}

	// Advanced: Using manual indexing
	void audioCB(AudioIOData& io){
		
		MyStuff& stuff = io.user<MyStuff>();

		for(unsigned i=0; i<io.framesPerBuffer(); ++i){

			float inSample1 = io.in(0,i);
			float inSample2 = io.in(1,i);

			io.out(0,i) = -inSample1;
			io.out(1,i) = -inSample2;
		}
	}
	
	int main(){
		MyStuff stuff;
		
		AudioIO audioIO(128, 44100, audioCB, &stuff, 2,2);
		audioIO.start();
	}
*/


#include <string>
#include <vector>

namespace al{

class AudioIOData;


/// Audio callback type
typedef void (* audioCallback)(AudioIOData& io);


/// Audio device abstraction
class AudioDevice{
public:

	/// @param[in] deviceNum	Device enumeration number
	AudioDevice(int deviceNum);
	
	/// @param[in] nameKeyword	Keyword to search for in device name
	/// @param[in] input		Whether to search input devices
	/// @param[in] output		Whether to search output devices
	AudioDevice(const std::string& nameKeyword, bool input=true, bool output=true);

	~AudioDevice();

	bool valid() const { return 0!=mImpl; }	///< Returns whether device is valid
	int id() const { return mID; }			///< Get device unique ID
	const char * name() const;				///< Get device name
	int channelsInMax() const;				///< Get maximum number of input channels supported
	int channelsOutMax() const;				///< Get maximum number of output channels supported
	double defaultSampleRate() const;		///< Get default sample rate
	
	bool hasInput() const;					///< Returns whether device has input
	bool hasOutput() const;					///< Returns whether device has output
	
	void print() const;						///< Prints info about specific i/o device to stdout

	static AudioDevice defaultInput();		///< Get system's default input device
	static AudioDevice defaultOutput();		///< Get system's default output device
	static int numDevices();				///< Returns number of audio i/o devices available
	static void printAll();					///< Prints info about all available i/o devices to stdout

private:
	void setImpl(int deviceNum);
	static void initDevices();
	int mID;
	const void * mImpl;
};



/// Audio data to be sent to callback

/// Audio buffers are guaranteed to be stored in a contiguous deinterleaved 
/// format, i.e., frames are tightly packed per channel.
class AudioIOData {
public:
	/// Constructor
	AudioIOData(void * user);

	virtual ~AudioIOData();


	/// Iterate frame counter, returning true while more frames
	bool operator()() const { return (++mFrame)<framesPerBuffer(); }
		
	/// Get current frame number
	int frame() const { return mFrame; }

	/// Get bus sample at current frame iteration on specified channel
	float& bus(int chan) const { return bus(chan, frame()); }

	/// Get bus sample at specified channel and frame
	float& bus(int chan, int frame) const;

	/// Get non-interleaved bus buffer on specified channel
	float * busBuffer(int chan=0) const { return &bus(chan,0); }

	/// Get input sample at current frame iteration on specified channel
	const float& in(int chan) const { return in (chan, frame()); }

	/// Get input sample at specified channel and frame
	const float& in (int chan, int frame) const;

	/// Get non-interleaved input buffer on specified channel
	const float * inBuffer(int chan=0) const { return &in(chan,0); }

	/// Get output sample at current frame iteration on specified channel
	float& out(int chan) const { return out(chan, frame()); }

	/// Get output sample at specified channel and frame
	float& out(int chan, int frame) const;

	/// Get non-interleaved output buffer on specified channel
	float * outBuffer(int chan=0) const { return &out(chan,0); }
	
	/// Add value to current output sample on specified channel
	void sum(float v, int chan) const { out(chan)+=v; }
	
	/// Add value to current output sample on specified channels
	void sum(float v, int ch1, int ch2) const { sum(v, ch1); sum(v,ch2); }
	
	/// Get sample from temporary buffer at specified frame
	float& temp(int frame) const;

	/// Get non-interleaved temporary buffer on specified channel
	float * tempBuffer() const { return &temp(0); }

	void * user() const{ return mUser; } ///< Get pointer to user data

	template<class UserDataType>
	UserDataType& user() const { return *(static_cast<UserDataType *>(mUser)); }

	int channelsIn () const;			///< Get effective number of input channels
	int channelsOut() const;			///< Get effective number of output channels
	int channelsBus() const;			///< Get number of allocated bus channels
	int channelsInDevice() const;		///< Get number of channels opened on input device
	int channelsOutDevice() const;		///< Get number of channels opened on output device
	int framesPerBuffer() const;		///< Get frames/buffer of audio I/O stream
	double framesPerSecond() const;		///< Get frames/second of audio I/O streams
	double fps() const { return framesPerSecond(); }
	double secondsPerBuffer() const;	///< Get seconds/buffer of audio I/O stream
	double time() const;				///< Get current stream time in seconds
	double time(int frame) const;		///< Get current stream time in seconds of frame

	void user(void * v){ mUser=v; }		///< Set user data
	void frame(int v){ mFrame=v-1; }	///< Set frame count for next iteration
	void zeroBus();						///< Zeros all the bus buffers
	void zeroOut();						///< Zeros all the internal output buffers

	AudioIOData& gain(float v){ mGain=v; return *this; }
	bool usingGain() const { return mGain != 1.f || mGainPrev != 1.f; }

protected:
	class Impl; Impl * mImpl;
	void * mUser;					// User specified data
	mutable int mFrame;
	int mFramesPerBuffer;
	double mFramesPerSecond;
	float *mBufI, *mBufO, *mBufB;	// input, output, and aux buffers
	float * mBufT;					// temporary one channel buffer
	int mNumI, mNumO, mNumB;		// input, output, and aux channels
public:
	float mGain, mGainPrev;
};



/// Interface for objects which can be registered with an audio IO stream
class AudioCallback {
public:
	virtual ~AudioCallback() {}
	virtual void onAudioCB(AudioIOData& io) = 0;	///< Callback
};


/// Audio input/output streaming.

///
/// 
class AudioIO : public AudioIOData {
public:

	/// Creates AudioIO using default I/O devices.

	/// @param[in] framesPerBuf		Number of sample frames to process per callback
	/// @param[in] framesPerSec		Frame rate.  Unsupported values will use default rate of device.
	/// @param[in] callback			Audio processing callback (optional)
	/// @param[in] userData			Pointer to user data accessible within callback (optional)
	/// @param[in] outChans			Number of output channels to open
	/// @param[in] inChans			Number of input channels to open
	/// If the number of input or output channels is greater than the device
	/// supports, virtual buffers will be created.
	AudioIO(
		int framesPerBuf=64, double framesPerSec=44100.0,
		void (* callback)(AudioIOData &) = 0, void * userData = 0,
		int outChans = 2, int inChans = 0 );

	virtual ~AudioIO();

	using AudioIOData::channelsIn;
	using AudioIOData::channelsOut;
	using AudioIOData::framesPerBuffer;
	using AudioIOData::framesPerSecond;

	audioCallback callback;						///< User specified callback function.
	
	/// Add an AudioCallback handler (internal callback is always called first)
	AudioIO& append(AudioCallback& v);		
	AudioIO& prepend(AudioCallback& v);

	/// Remove all input event handlers matching argument
	AudioIO& remove(AudioCallback& v);

	bool autoZeroOut() const { return mAutoZeroOut; }
	int channels(bool forOutput) const;
	bool clipOut() const { return mClipOut; }	///< Returns clipOut setting
	double cpu() const;							///< Returns current CPU usage of audio thread
	bool supportsFPS(double fps) const;			///< Return true if fps supported, otherwise false
	bool zeroNANs() const;						///< Returns whether to zero NANs in output buffer going to DAC
	
	void processAudio();						///< Call callback manually
	bool open();								///< Opens audio device.
	bool close();								///< Closes audio device. Will stop active IO.
	bool start();								///< Starts the audio IO.  Will open audio device if necessary.
	bool stop();								///< Stops the audio IO.

	void autoZeroOut(bool v){ mAutoZeroOut=v; }

	/// Sets number of effective channels on input or output device depending on 'forOutput' flag.
	
	/// An effective channel is either a real device channel or virtual channel 
	/// depending on how many channels the device supports. Passing in -1 for
	/// the number of channels opens all available channels.
	void channels(int num, bool forOutput);

	void channelsIn(int n){channels(n,false);}	///< Set number of input channels
	void channelsOut(int n){channels(n,true);}	///< Set number of output channels
	void channelsBus(int num);					///< Set number of bus channels
	void clipOut(bool v){ mClipOut=v; }			///< Set whether to clip output between -1 and 1
	void deviceIn(const AudioDevice& v);		///< Set input device
	void deviceOut(const AudioDevice& v);		///< Set output device
	void framesPerSecond(double v);				///< Set number of frames per second
	void framesPerBuffer(int n);				///< Set number of frames per processing buffer
	void zeroNANs(bool v){ mZeroNANs=v; }		///< Set whether to zero NANs in output buffer going to DAC

	void print();								///< Prints info about current i/o devices to stdout.

	static const char * errorText(int errNum);		// Returns error string.

private:
	AudioDevice mInDevice, mOutDevice;

	bool mInResizeDeferred, mOutResizeDeferred;
	bool mZeroNANs;			// whether to zero NANs
	bool mClipOut;			// whether to clip output between -1 and 1
	bool mAutoZeroOut;		// whether to automatically zero output buffers each block
	
	std::vector<AudioCallback *> mAudioCallbacks;

	void init();		// Initializes PortAudio and member variables.
	void deferBufferResize(bool forOutput);
	void reopen();		// reopen stream (restarts stream if needed)
	void resizeBuffer(bool forOutput);
};





//==============================================================================
inline float&       AudioIOData::bus(int c, int f) const { return mBufB[c*framesPerBuffer() + f]; }
inline const float& AudioIOData::in (int c, int f) const { return mBufI[c*framesPerBuffer() + f]; }
inline float&       AudioIOData::out(int c, int f) const { return mBufO[c*framesPerBuffer() + f]; }
inline float&       AudioIOData::temp(int f) const { return mBufT[f]; }

} // al::

#endif
