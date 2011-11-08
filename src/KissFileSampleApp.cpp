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

#include <sys/time.h>

#include "OpenALSample.h"



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

const int history_size = 100;
static int item_index = 0;

static bool write_frames = true;


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

private:

	// Audio file
	audio::SourceRef mAudioSource;
	audio::TrackRef mTrack;
	audio::PcmBuffer32fRef mBuffer;

	// Analyzer
	bool mFftInit;
	Kiss mFft;

	typedef std::vector<boost::filesystem::directory_entry> TFileList;
	TFileList m_FileList;
	std::string m_CurrentFile;
	
	DataItem	m_HitsoryItems[history_size];
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

//*************************************************************************
void BeatDetectorApp::NextFile()
{
	roto = 0;
	if(mTrack && mTrack->isPlaying())
	{
		mTrack->enablePcmBuffering(false);
		mTrack->stop();
	}
	
	timeval time;
	gettimeofday(&time, NULL);
	
	Rand r;
	r.seed(time.tv_sec);
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
static int curr_sample = 0;
//*************************************************************************
void BeatDetectorApp::setup()
{	
	// Set up window
	setWindowSize(640, 480);
	
	// Set up OpenGL
	gl::enableAlphaBlending();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	//glEnable(GL_LINE_SMOOTH);
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	
	setFrameRate(30);
	
	// Set line color
	gl::color(Color(1, 1, 1));
	
	// Load and play audio
	//mAudioSource = audio::load(loadResource(RES_SAMPLE));
	//mTrack = audio::Output::addTrack(mAudioSource, false);
	///mTrack->enablePcmBuffering(true);
	//mTrack->play();
	
	// Set init flag
	mFftInit = false;
	
	std::string dir = getHomeDirectory() + "music/4vis";
	if(exists(dir))
	{
		if(is_directory(dir))
		{
			copy(directory_iterator(dir), directory_iterator(), back_inserter(m_FileList));
		}
	}
	
	for(TFileList::iterator it = m_FileList.begin(); it != m_FileList.end();)
	{
		std::string str = it->path().native();
		if(str.rfind(".mp3") != -1 || str.rfind(".m4a"))
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
				mFftInit = true;
				mFft.setDataSize(samples_per_frame);
			}
			
			mFft.setData(p_sample->mp_Buffer + (curr_sample*2));
			curr_sample += samples_per_frame;
		}
		else
		{
			quit();
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
				
				//mSampleCount = 700;
				
				if (mSampleCount > 0)
				{
					
					// Initialize analyzer, if needed
					if (!mFftInit)
					{
						mFftInit = true;
						mFft.setDataSize(mSampleCount);
					}
					
					// Analyze data
					if (mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData != 0) 
						mFft.setData(mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData);
					
					if(false)
					{
						if (mFftInit)
						{
							// Get data
							float * freq_data = mFft.getAmplitude();
							float * amp_data = mFft.getData();
							int32_t data_size = mFft.getBinSize();
							
							float avg_vol = 0;
							float avg_freq = 0;
							for (int32_t i = 0; i < data_size; i++) 
							{
								avg_vol += amp_data[i];
								
								// Do logarithmic plotting for frequency domain
								double mLogSize = log((double)data_size);
								float x = (float)(log((double)i) / mLogSize) * (double)data_size;
								float y = math<float>::clamp(freq_data[i] * (x / data_size) * log((double)(data_size - i)), 0.0f, 1.0f);
								
								avg_freq += y;
							}
							
							avg_vol /= data_size;
							avg_freq /= data_size;
							
							m_HitsoryItems[item_index].amp = avg_vol;
							m_HitsoryItems[item_index].freq = avg_freq;
							
							++item_index;
							
							item_index = item_index % history_size;
						}
					}
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
		gl::drawString(m_CurrentFile, Vec2f(10,10));
	}

	// Check init flag
	if (mFftInit)
	{
		// Get data
		float * mFreqData = mFft.getAmplitude();
		float * mTimeData = mFft.getData();
		int32_t mDataSize = mFft.getBinSize();

		// Get dimensions
		float mScale = ((float)getWindowWidth() - 20.0f) / (float)mDataSize;
		float mWindowHeight = (float)getWindowHeight();

		// Use polylines to depict time and frequency domains
		PolyLine<Vec2f> mFreqLine;
		PolyLine<Vec2f> mTimeLine;

		// Iterate through data
		for (int32_t i = 0; i < mDataSize; i++) 
		{

			// Do logarithmic plotting for frequency domain
			double mLogSize = log((double)mDataSize);
			float x = (float)(log((double)i) / mLogSize) * (double)mDataSize;
			float y = math<float>::clamp(mFreqData[i] * (x / mDataSize) * log((double)(mDataSize - i)), 0.0f, 2.0f);

			// Plot points on lines
			mFreqLine.push_back(Vec2f(x * mScale + 10.0f - getWindowWidth(),           -y * 1.25f * mWindowHeight));
			mTimeLine.push_back(Vec2f(i * mScale + 10.0f, mTimeData[i] * 0.3f * mWindowHeight));

		}

		glPushMatrix();
		
		gl::translate(Vec2f(getWindowWidth()*0.5f, getWindowHeight()*0.5f));
		
		roto += rot_inc;
		gl::rotate(roto * 0.01f);
		
		//gl::translate(Vec2f(-getWindowWidth()*0.5f, -mWindowHeight*0.5f));
		
		gl::scale(Vec3f::one() * 0.5f);
		
		float split = cos(roto * 0.0001f);
		
		int num_spikes = 20;
		for(int i=0; i<num_spikes; ++i)
		{
			glPushMatrix();
			float i_f = i/(float)num_spikes;
			
			gl::rotate(i_f * 360.0f * num_spikes * split);
			
			ColorA col = hsvToRGB(Vec3f(i_f, 1, 1));
			gl::color(col);
			gl::draw(mFreqLine);
			
			gl::scale(Vec3f(1,-1,1));
			gl::draw(mFreqLine);

			glPopMatrix();
		}
		
		
		glPopMatrix();
		
		if(false)
		{		
			float x=0;
			float x_inc = getWindowWidth() / history_size;
			float y = getWindowHeight() * 0.5f;
			float h = mWindowHeight * 0.2f;
			for(int i=0; i<history_size; i++)
			{
				ColorA col = hsvToRGB(Vec3f(m_HitsoryItems[i].freq, 1, 1));
				gl::color(col);			
				gl::drawSolidRect(Rectf(x, y - m_HitsoryItems[i].amp * h, x+x_inc*0.2f, y + m_HitsoryItems[i].amp * h));
				x+=x_inc;
			}
		}
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
