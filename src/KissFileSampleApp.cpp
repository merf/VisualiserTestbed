// Includes
#include "cinder/app/AppBasic.h"
#include "cinder/audio/Output.h"
#include "cinder/audio/Io.h"
#include "cinder/CinderMath.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"

#include "Kiss.h"
//#include "Resources.h"

#include <boost/filesystem.hpp>

//#include <sys/time.h>
#include <time.h>

#include "OpenALSample.h"

#include "SoundAnalyzer.h"



// Imports
using namespace ci;
using namespace ci::app;
using namespace cinder::audio;
using namespace boost::filesystem;

struct DataItem
{
	float amp;
	float freq;
};

static bool write_frames = false;


// Main application
class BeatDetectorApp : public AppBasic 
{

public:

	// Cinder callbacks
	void		draw();
	void		quit();
	void		setup();
	void		update();
	
	void		keyDown(KeyEvent event);
	
	void		NextFile();
	void		Init(uint32_t mSampleCount);


private:

	// Audio file
	audio::SourceRef		mAudioSource;
	audio::TrackRef			mTrack;
	audio::PcmBuffer32fRef	mBuffer;

	// Analyzer
	bool					mFftInit;
	Kiss					mFft;

	typedef std::vector<boost::filesystem::directory_entry> TFileList;
	TFileList				m_FileList;
	std::string				m_CurrentFile;
	
};

//*************************************************************************
void BeatDetectorApp::keyDown(KeyEvent event)
{
	switch(event.getChar())
	{
		case 'n':
			NextFile();
			break;
		case 'f':
			setFullScreen(!isFullScreen());
			break;
	}
}
static float roto = 0;
static float rot_inc = 6.0f;

//*********************************************************************************
void BeatDetectorApp::Init( uint32_t mSampleCount )
{
	mFftInit = true;
	mFft.setDataSize(mSampleCount);

	int num_bins = mFft.getBinSize();
	CSoundAnalyzer::StaticInit(num_bins);
}



//*************************************************************************
void BeatDetectorApp::NextFile()
{
	roto = 0;
	if(mTrack && mTrack->isPlaying())
	{
		mTrack->enablePcmBuffering(false);
		mTrack->stop();
	}

#ifdef WIN32
	time_t now;
	time(&now);
	int time_int = (int)now;
#else
	timeval now;
	gettimeofday(&now, NULL);
	int time_int = now.tv_sec;
#endif

	Rand r;
	r.seed(time_int);
	int rand_file = r.nextInt(m_FileList.size());
	path my_path = m_FileList[rand_file].path();
	m_CurrentFile = my_path.string();
	
	if(!write_frames)
	{
		mAudioSource = audio::load(m_CurrentFile);
		mTrack = audio::Output::addTrack(mAudioSource, false);
		mTrack->enablePcmBuffering(true);
		mTrack->play();		
	}
	//rot_inc = r.nextFloat(1.5f, 30.0f);
}

static COpenALSample* p_sample = NULL;
static int samples_per_frame = 44100/30;
static unsigned int curr_sample = 0;

//*************************************************************************
void BeatDetectorApp::setup()
{	
	// Set up window
	setWindowSize(480, 480);
	
	// Set up OpenGL
	gl::enableAlphaBlending();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	//glEnable(GL_LINE_SMOOTH);
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	
	if(write_frames)
	{
		setFrameRate(30);
	}
	else
	{
		setFrameRate(60);
	}
	
	// Set line color
	gl::color(Color(1, 1, 1));
	
	// Load and play audio
	//mAudioSource = audio::load(loadResource(RES_SAMPLE));
	//mTrack = audio::Output::addTrack(mAudioSource, false);
	///mTrack->enablePcmBuffering(true);
	//mTrack->play();
	
	// Set init flag
	mFftInit = false;
	
	std::string dir = getHomeDirectory() + "music\\4vis";
	if(exists(dir))
	{
		if(is_directory(dir))
		{
			copy(directory_iterator(dir), directory_iterator(), back_inserter(m_FileList));
		}
	}
	
	for(TFileList::iterator it = m_FileList.begin(); it != m_FileList.end();)
	{
		//std::string str = it->path().native();
		std::string str = it->path().generic_string();
//#ifdef WIN32
//		if(str.rfind(".wav") != -1)
//#else
		if(str.rfind(".mp3") != -1 || str.rfind(".m4a") != -1)
//#endif
		{
			++it;
		}
		else
		{
			it = m_FileList.erase(it);
		}
	}
	
	NextFile();
	
	if(write_frames)
	{
		COpenALSampleManager::StaticInit();
		p_sample = COpenALSampleManager::CreateSample(m_CurrentFile);
	}
}

//*************************************************************************
void BeatDetectorApp::update() 
{
	if(write_frames)
	{
		if(curr_sample < p_sample->m_SampleCount)
		{
			// Initialize analyzer, if needed
			if (!mFftInit)
			{
				Init(samples_per_frame);
			}
			
			mFft.setData(p_sample->mp_Buffer + (curr_sample));
			curr_sample += samples_per_frame;

			CSoundAnalyzer::Get().ProcessData(mFft.getAmplitude(), mFft.getData());
		}
		else
		{
			shutdown();
		}
	}
	else
	{
		// Check if track is playing and has a PCM buffer available
		if (mTrack->isPlaying() && mTrack->isPcmBuffering())
		{
			// Get buffer
			mBuffer = mTrack->getPcmBuffer();
			if (mBuffer && mBuffer->getInterleavedData())
			{
				// Get sample count
				uint32_t mSampleCount = mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mSampleCount;
				
				if (mSampleCount > 0)
				{
					// Initialize analyzer, if needed
					if (!mFftInit)
					{
						Init(samples_per_frame);
					}
					
					// Analyze data
					if (mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData != 0) 
						mFft.setData(mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData);

					CSoundAnalyzer::Get().ProcessData(mFft.getAmplitude(), mFft.getData());
				}
			}
		}
	}
}

//*************************************************************************
void BeatDetectorApp::draw()
{
	// Clear screen
	gl::clear(Color(0.0f, 0.0f, 0.0f));
	
	if(m_CurrentFile != "")
	{
		//gl::drawString(m_CurrentFile, Vec2f(10,10));
	}

	// Check init flag
	if (mFftInit)
	{
		CSoundAnalyzer::Get().Draw();
	}

	static int frame = 0;
	frame++;
	
	if(write_frames)
	{
		writeImage( getHomeDirectory() + "vis_frames2" + getPathSeparator() + "saveImage_" + toString( frame ) + ".png", copyWindowSurface() );
	}
}

//*************************************************************************
void BeatDetectorApp::quit() 
{
	// Stop track
	mTrack->enablePcmBuffering(false);
	mTrack->stop();
}

// Start application
CINDER_APP_BASIC(BeatDetectorApp, RendererGl)
