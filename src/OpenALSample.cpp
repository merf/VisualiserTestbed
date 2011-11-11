#include "OpenALSample.h"

//#include "samples/framework/win32/framework.h"
//#include "Framework.h"

#ifdef WIN32

#include "CWaves.h"
#include "al.h"

#else

#include "MyOpenALSupport.h"

#include "cinder/audio/SourceFile.h"
#include "cinder/cocoa/CinderCocoa.h"

#include <iostream>

#if defined( CINDER_COCOA )
#include <CoreFoundation/CoreFoundation.h>
#if defined( __MAC_OS_X_VERSION_MAX_ALLOWED ) && (__MAC_OS_X_VERSION_MAX_ALLOWED < 1060)
#define kAudioFileReadPermission fsRdPerm
#endif
#endif

#endif

#ifdef WIN32
static CWaves* gp_WaveLoader = NULL;
#endif

COpenALSampleManager* COpenALSampleManager::mp_Impl = new COpenALSampleManager();


using namespace ci;

//***********************************************************************
//***********************************************************************
void COpenALSampleManager::StaticInit()
{
    if(mp_Impl)
    {
        delete mp_Impl;
    }
    
    mp_Impl = new COpenALSampleManager();
}

//***********************************************************************
COpenALSampleManager::COpenALSampleManager()
{
#ifdef WIN32
	gp_WaveLoader = new CWaves();
#endif
}

//***********************************************************************
COpenALSampleManager::~COpenALSampleManager()
{
#ifdef WIN32
	if(gp_WaveLoader)
	{
		delete gp_WaveLoader;
		gp_WaveLoader = NULL;
	}
#endif
}

#ifndef WIN32
void* MyGetOpenALAudioData(CFURLRef inFileURL, ALsizei *outDataSize, ALenum *outDataFormat, ALsizei*	outSampleRate)
{
	OSStatus						err = noErr;	
	SInt64							theFileLengthInFrames = 0;
	AudioStreamBasicDescription		theFileFormat;
	UInt32							thePropertySize = sizeof(theFileFormat);
	ExtAudioFileRef					extRef = NULL;
	void*							theData = NULL;
	AudioStreamBasicDescription		theOutputFormat;
	UInt32							dataSize = 0;
	
	// Open a file with ExtAudioFileOpen()
	err = ExtAudioFileOpenURL(inFileURL, &extRef);
	if(err) 
	{ 
		printf("MyGetOpenALAudioData: ExtAudioFileOpenURL FAILED, Error = %ld\n", err); goto Exit; 
	}
	
	// Get the audio data format
	err = ExtAudioFileGetProperty(extRef, kExtAudioFileProperty_FileDataFormat, &thePropertySize, &theFileFormat);
	if(err) { printf("MyGetOpenALAudioData: ExtAudioFileGetProperty(kExtAudioFileProperty_FileDataFormat) FAILED, Error = %ld\n", err); goto Exit; }
	if (theFileFormat.mChannelsPerFrame > 2)  { printf("MyGetOpenALAudioData - Unsupported Format, channel count is greater than stereo\n"); goto Exit;}
	
	// Set the client format to 16 bit signed integer (native-endian) data
	// Maintain the channel count and sample rate of the original source format
	theOutputFormat.mSampleRate = theFileFormat.mSampleRate;
	theOutputFormat.mChannelsPerFrame = theFileFormat.mChannelsPerFrame;
	
	theOutputFormat.mFormatID = kAudioFormatLinearPCM;
	theOutputFormat.mBytesPerPacket = 2 * theOutputFormat.mChannelsPerFrame;
	theOutputFormat.mFramesPerPacket = 1;
	theOutputFormat.mBytesPerFrame = 2 * theOutputFormat.mChannelsPerFrame;
	theOutputFormat.mBitsPerChannel = 16;
	theOutputFormat.mFormatFlags = kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;
	
	// Set the desired client (output) data format
	err = ExtAudioFileSetProperty(extRef, kExtAudioFileProperty_ClientDataFormat, sizeof(theOutputFormat), &theOutputFormat);
	if(err) { printf("MyGetOpenALAudioData: ExtAudioFileSetProperty(kExtAudioFileProperty_ClientDataFormat) FAILED, Error = %ld\n", err); goto Exit; }
	
	// Get the total frame count
	thePropertySize = sizeof(theFileLengthInFrames);
	err = ExtAudioFileGetProperty(extRef, kExtAudioFileProperty_FileLengthFrames, &thePropertySize, &theFileLengthInFrames);
	if(err) { printf("MyGetOpenALAudioData: ExtAudioFileGetProperty(kExtAudioFileProperty_FileLengthFrames) FAILED, Error = %ld\n", err); goto Exit; }
	
	// Read all the data into memory
	dataSize = theFileLengthInFrames * theOutputFormat.mBytesPerFrame;;
	theData = malloc(dataSize);
	if (theData)
	{
		AudioBufferList		theDataBuffer;
		theDataBuffer.mNumberBuffers = 1;
		theDataBuffer.mBuffers[0].mDataByteSize = dataSize;
		theDataBuffer.mBuffers[0].mNumberChannels = theOutputFormat.mChannelsPerFrame;
		theDataBuffer.mBuffers[0].mData = theData;
		
		// Read the data into an AudioBufferList
		err = ExtAudioFileRead(extRef, (UInt32*)&theFileLengthInFrames, &theDataBuffer);
		if(err == noErr)
		{
			// success
			*outDataSize = (ALsizei)dataSize;
			*outDataFormat = (theOutputFormat.mChannelsPerFrame > 1) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
			*outSampleRate = (ALsizei)theOutputFormat.mSampleRate;
		}
		else 
		{ 
			// failure
			free (theData);
			theData = NULL; // make sure to return NULL
			printf("MyGetOpenALAudioData: ExtAudioFileRead FAILED, Error = %ld\n", err); goto Exit;
		}	
	}
	
Exit:
	// Dispose the ExtAudioFileRef, it is no longer needed
	if (extRef) ExtAudioFileDispose(extRef);
	return theData;
}
#endif

//***********************************************************************
bool LoadWavIntoFloatBuffer(const char *file_name, float** p_buff, ci::uint32_t* num_samples, ci::uint32_t* num_channels)
{
	bool ret = false;
	
	ALchar*			p_data = NULL;
	
	ALsizei size;
	ALsizei freq;


#ifndef WIN32
	ALenum  format;
	
	::CFStringRef pathString = cocoa::createCfString( file_name );
	::CFURLRef urlRef = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false );
	::CFRelease(pathString);
	
	if (urlRef)
	{
		p_data = reinterpret_cast<ALchar*>(MyGetOpenALAudioData(urlRef, &size, &format, &freq));
		
		int16_t* srcBuffer = reinterpret_cast<int16_t *>(p_data);		
	}
	::CFRelease(urlRef);
	

#else	
	WAVEID			wave_id;
	bool			delete_wav = false;

	if(gp_WaveLoader)
	{
		if (SUCCEEDED(gp_WaveLoader->LoadWaveFile(file_name, &wave_id)))
		{
			//if ((SUCCEEDED(gp_WaveLoader->GetWaveSize(wave_id, (unsigned long*)&data_size))) &&
			//	(SUCCEEDED(gp_WaveLoader->GetWaveData(wave_id, (void**)&p_data))) &&
			//	(SUCCEEDED(gp_WaveLoader->GetWaveFrequency(wave_id, (unsigned long*)&frequency))) &&
			//	(SUCCEEDED(gp_WaveLoader->GetWaveALBufferFormat(wave_id, &alGetEnumValue, (unsigned long*)&buffer_format))))
			if ((SUCCEEDED(gp_WaveLoader->GetWaveSize(wave_id, (unsigned long*)&size))) &&
				(SUCCEEDED(gp_WaveLoader->GetWaveData(wave_id, (void**)&p_data))) &&
				(SUCCEEDED(gp_WaveLoader->GetWaveFrequency(wave_id, (unsigned long*)&freq))))
			{
				delete_wav = true;
			}
		}
	}
#endif
	
	if(p_data)
	{
		//We want to read the source data 16 bits at a time so we reinterpret cast the pointer
		int16_t* srcBuffer = reinterpret_cast<int16_t *>(p_data);
		
		uint32_t bytes_per_sample = 2;
		*num_samples = size/bytes_per_sample;

		static bool mono = true;

		if(mono)
		{ 
			*num_samples /= 2;
		}
				
		*p_buff = new float[*num_samples];
		
		for(uint32_t i=0; i<*num_samples; i++)
		{
			//static float mul = 1.0f / (float)0x7fff;
			static float mul = 0.5f / (float)0x7fff;
			if(mono)
			{
				(*p_buff)[i] = srcBuffer[i*2] * mul;
			}
			else
			{
				(*p_buff)[i] = srcBuffer[i] * mul;
			}
		}
		
		//free(p_data);
	}

#ifdef WIN32
	if(gp_WaveLoader && delete_wav)
	{
		gp_WaveLoader->DeleteWaveFile(wave_id);
	}
#endif

	return ret;
}

//***********************************************************************
COpenALSample* COpenALSampleManager::CreateSample(const std::string& path)
{
	return new COpenALSample(path);
}





//***********************************************************************
//***********************************************************************
COpenALSample::COpenALSample(const std::string& path)
:
mp_Buffer(NULL),
m_SampleCount(0),
m_NumChannels(2)
{
	LoadWavIntoFloatBuffer(path.c_str(), &mp_Buffer, &m_SampleCount, &m_NumChannels);

	m_SampleCount /= m_NumChannels;
}