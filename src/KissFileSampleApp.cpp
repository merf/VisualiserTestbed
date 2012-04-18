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


#include "cinder/Easing.h"
#include "cinder/Timeline.h"
#include <list>


// Imports
using namespace ci;
using namespace ci::app;
using namespace cinder::audio;
using namespace boost::filesystem;

namespace
{
	static COpenALSample* p_sample = NULL;
	static int samples_per_frame = 44100/30;
	static unsigned int curr_sample = 0;
}


struct DataItem
{
	float amp;
	float freq;
};


class Circle {
public:
	Circle( Color color, float radius, Vec2f initialPos )
		: m_Color( color ), m_Radius( radius ), m_Pos( initialPos )
	{}

	void draw() const 
	{
		gl::color( ColorA( m_Color, 0.75f ) );
		gl::drawSolidCircle( m_Pos, m_Radius );
	}

	Anim<Color>			m_Color;
	Anim<Vec2f>			m_Pos;
	Anim<float>			m_Radius;
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
	
	std::list<Circle>		m_Cirlces;
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
		case 'p':
			if(mTrack)
			{
				if(mTrack->isPlaying())
				{
					mTrack->stop();
				}
				else
				{
					mTrack->play();
				}
			}
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
	CSoundAnalyzer::StaticInit(num_bins, samples_per_frame);

	for(int i=0; i<5; i++)
	{
		float i_f = i/(float)5 - 0.4f;
		m_Cirlces.push_back(Circle(Color(1, 0, 0), 0, Vec2f(0, getWindowHeight() * i_f)));
	}
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



//*************************************************************************
void BeatDetectorApp::setup()
{	


	// Set up window
	setWindowSize(480, 480);
	
	// Set up OpenGL
	gl::enableAlphaBlending();
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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
	//gl::clear(ColorA(0.0f, 0.0f, 0.0f, 0.0f));

	// Clear the draw and depth buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Take the contents of the current accumulation buffer and copy it to the colour buffer with each pixel multiplied by a factor
	// i.e. we clear the screen, draw the last frame again (which we saved in the accumulation buffer), then draw our stuff at its new location on top of that
	glAccum(GL_RETURN, 0.999f);

	// Clear the accumulation buffer (don't worry, we re-grab the screen into the accumulation buffer after drawing our current frame!)
	glClear(GL_ACCUM_BUFFER_BIT);

	
	if(m_CurrentFile != "")
	{
		//gl::drawString(m_CurrentFile, Vec2f(10,10));
	}

	// Check init flag
	if (mFftInit)
	{
		//CSoundAnalyzer::Get().Draw();

		float* p_data = NULL;
		size_t num_items = 0;
		CSoundAnalyzer::Get().GetChunkedMoveData(&p_data, num_items);

		glPushMatrix();

		static float time = 0;
		time += 0.05f;

		float f = CSoundAnalyzer::Get().GetSOD();

		gl::color(hsvToRGB(Vec3f(f, 1, 1)));
		gl::drawSolidCircle(Vec2f(getWindowWidth()*0.5f, getWindowHeight()*0.5f), 200 * f);

		/*

		//gl::translate(getWindowWidth() * 0.5f + 0.25f * getWindowWidth() * sin(time), getWindowHeight() * 0.5f);
		gl::translate(0, getWindowHeight() * 0.5f);

		for(int i=0; i<num_items; ++i)
		{
			float f = i/(float)num_items;

			gl::drawLine(Vec2f(getWindowWidth()*f, 0), Vec2f(getWindowWidth()*f, getWindowHeight()*p_data[i]));
		}
		*/

		/*
		//gl::rotate(time * 30);

		const size_t num_bands = 5;
		float bands[num_bands];

		size_t items_per_band = num_items / num_bands;
		
		for(size_t band = 0; band < num_bands; band++)
		{
			float max_in_band = 0;
			for(size_t item = band * items_per_band; item < (band+1) * items_per_band; item++)
			{
				if(p_data[item] > max_in_band)
				{
					max_in_band = p_data[item];
				}
			}

			bands[band] = max_in_band;


		}
		*/

		/*
		int band = 0;
		for(std::list<Circle>::iterator it = m_Cirlces.begin(); it != m_Cirlces.end(); ++it)
		{
			if(bands[band] > 0.2f)
			{
				timeline().apply(&it->m_Radius, 50.0f, 0.05f, cinder::EaseInCubic());
				timeline().appendTo(&it->m_Radius, 0.0f, 0.7f, cinder::EaseOutCubic());
				//timeline().appendTo(&it->m_Radius, 1.0f, 0.5f);
				//timeline().appendTo(&it->m_Radius, 0.0f, 0.2f, cinder::EaseInCubic());

				timeline().apply(&it->m_Color, Color(randFloat(), randFloat(), randFloat()), 1.0f, cinder::EaseOutCubic());
			}
			//it->m_Radius = 20 * bands[band++];
			it->draw();

			band++;
		}
		*/

		glPopMatrix();
	}

	//glAccum(GL_ACCUM, 0.99f);

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
