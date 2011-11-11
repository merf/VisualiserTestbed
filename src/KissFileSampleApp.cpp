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


	void			AllocFrecDataArrays();
	void			ChunkFreqData();

	int				m_NumChunks;

	float*			mp_ChunkedFreqData;
	float*			mp_ChunkedAvg;
	float*			mp_ChunkedMove;
	int*			mp_ChunkSizes;

	float			m_AvgVolume;
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

const float step_pow = 1.1f;

//*********************************************************************************
void BeatDetectorApp::Init( uint32_t mSampleCount )
{
	mFftInit = true;
	mFft.setDataSize(mSampleCount);

	int num_bins = mFft.getBinSize();
	int num_bins_for_this_chunk = 0;

	std::vector<int> chunk_sizes;

	int bin_count = 0;
	for(int i=0; bin_count<num_bins; i++)
	{
		num_bins_for_this_chunk = floor(pow(step_pow, i));
		bin_count += num_bins_for_this_chunk;

		chunk_sizes.push_back(num_bins_for_this_chunk);
	}

	m_NumChunks = chunk_sizes.size();

	if(m_NumChunks > 0)
	{
		mp_ChunkedFreqData = new float[m_NumChunks];
		mp_ChunkedAvg = new float[m_NumChunks];
		mp_ChunkedMove = new float[m_NumChunks];
		mp_ChunkSizes = new int[m_NumChunks];

		memset(mp_ChunkedAvg, 0, m_NumChunks * sizeof(float));
		memset(mp_ChunkedMove, 0, m_NumChunks * sizeof(float));

		int i=0;
		for(std::vector<int>::iterator it=chunk_sizes.begin(); it != chunk_sizes.end(); ++it, ++i)
		{
			mp_ChunkSizes[i] = *it;
		}
	}

	m_AvgVolume = 0;
}

//*********************************************************************************
void BeatDetectorApp::ChunkFreqData()
{
	if(mFftInit)
	{
		float* p_freq_data = mFft.getAmplitude();
		int num_bins = mFft.getBinSize();

		int chunk_start = 0;
		for(int i=0; i<m_NumChunks; i++)
		{
			int chunk_end = chunk_start + mp_ChunkSizes[i];

			mp_ChunkedFreqData[i] = 0;
			for(int bin=chunk_start; bin<chunk_end; bin++)
			{
				//bottom few bins seem to be garbage
				//if(bin >= 2)
				{
					mp_ChunkedFreqData[i] += p_freq_data[bin];
				}
			}

			chunk_start = chunk_end;
		}

		float* p_amp = mFft.getData();
		for(int i=0; i<num_bins; ++i)
		{
			m_AvgVolume += p_amp[i] * (num_bins - i);
		}

		m_AvgVolume /= num_bins * num_bins;

		int memory_length = 30;
		float one_on_memory_length = 1.0f/(float)memory_length;
		for(int i=0; i<m_NumChunks; i++)
		{
			mp_ChunkedAvg[i] = mp_ChunkedAvg[i] - mp_ChunkedAvg[i]*one_on_memory_length + mp_ChunkedFreqData[i]*one_on_memory_length;
		}

		for(int i=0; i<m_NumChunks; i++)
		{
			mp_ChunkedMove[i] = math<float>::clamp(mp_ChunkedFreqData[i] - mp_ChunkedAvg[i]);
		}
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
	
	//timeval time;
	//gettimeofday(&time, NULL);
	
	Rand r;
	//r.seed(time.tv_sec);
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
	setWindowSize(480, 480);
	
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

			ChunkFreqData();
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

					ChunkFreqData();
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
		glEnableClientState( GL_VERTEX_ARRAY );
		GLfloat* p_verts = new GLfloat[m_NumChunks * 2 * 4];
		glVertexPointer( 2, GL_FLOAT, 0, p_verts );

		for(int i=0; i<m_NumChunks; ++i)
		{
			float i_f = i/(float)(m_NumChunks-1);
			float i_f2 = (i+1)/(float)(m_NumChunks-1);

			p_verts[i*4+0] = (i_f) * getWindowWidth();
			p_verts[i*4+1] = getWindowHeight() - mp_ChunkedFreqData[i] * getWindowHeight() * 0.5f;

			p_verts[i*4+2] = (i_f) * getWindowWidth();
			p_verts[i*4+3] = getWindowHeight();
		}

		gl::color(1,1,1,1);

		glDrawArrays( GL_TRIANGLE_STRIP, 0, m_NumChunks * 4 );

		for(int i=0; i<m_NumChunks; ++i)
		{
			float i_f = i/(float)m_NumChunks;
			//gl::color(hsvToRGB(Vec3f(i_f,1,1)));
			float w = getWindowWidth()*0.5f/(float)m_NumChunks;
			float y= getWindowHeight()*0.5f;
			float h = 1;

			//gl::drawSolidRect(Rectf(i_f*getWindowWidth()-w, y-mp_ChunkedAvg[i]*getWindowHeight()-h, i_f*getWindowWidth()+w, y-mp_ChunkedAvg[i]*getWindowHeight()+h));
			gl::drawSolidRect(Rectf(i_f*getWindowWidth()-w, getWindowHeight()*0.5f-h, i_f*getWindowWidth()+w, getWindowHeight()*0.5f+h));
		}

		for(int i=0; i<m_NumChunks; ++i)
		{
			float i_f = i/(float)m_NumChunks;
			gl::color(hsvToRGB(Vec3f(i_f,1,1)));
			float w = getWindowWidth()*0.5f/(float)m_NumChunks;
			float y= getWindowHeight()*0.5f;
			float h = 3;

			gl::drawSolidRect(Rectf(i_f*getWindowWidth()-w, y-mp_ChunkedMove[i]*getWindowHeight()-h, i_f*getWindowWidth()+w, y-mp_ChunkedMove[i]*getWindowHeight()+h));
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
