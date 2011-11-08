// Includes
#include "cinder/app/AppBasic.h"
#include "cinder/Utilities.h"
#include "cinder/audio/Output.h"
#include "cinder/audio/Io.h"
#include "cinder/CinderMath.h"
#include "cinder/Rand.h"
#include "cinder/ImageIo.h"
#include "cinder/qtime/MovieWriter.h"


#include "Kiss.h"
//#include "Resources.h"

#include "boost/filesystem.hpp"

#include <list>
#include <ctime>

// Imports
using namespace ci;
using namespace ci::app;
using namespace ci::audio;
using namespace boost::filesystem;

//*********************************************************************************
//*********************************************************************************
class KissFileSampleApp : public AppBasic 
{

public:

	// Cinder callbacks
	void draw();
	void quit();
	void setup();
	void update();

	void Init( uint32_t mSampleCount );

	void NextFile();

private:

	// Audio file
	audio::SourceRef mAudioSource;
	audio::TrackRef mTrack;
	audio::PcmBuffer32fRef mBuffer;

	// Analyzer
	bool			mFftInit;
	Kiss			mFft;

	typedef std::vector<path> TFileList;
	TFileList		m_FileList;

	std::string		m_CurrentFile;


	void			AllocFrecDataArrays();
	void			ChunkFreqData();

	int				m_NumChunks;

	float*			mp_ChunkedFreqData;
	int*			mp_ChunkSizes;

	qtime::MovieWriter	mMovieWriter;
};

//*********************************************************************************
void KissFileSampleApp::setup()
{
	// Set up window
	setWindowSize(640, 640);

	qtime::MovieWriter::Format format;
	if( qtime::MovieWriter::getUserCompressionSettings( &format ) ) {
		mMovieWriter = qtime::MovieWriter( getHomeDirectory() + "vid.mov", getWindowWidth(), getWindowHeight(), format );
	}

	// Set up OpenGL
	gl::enableAlphaBlending();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

	// Set line color
	gl::color(Color(1, 1, 1));

	std::string dir = getHomeDirectory() + "Music/4vis/";

	if(is_directory(dir))
	{
		copy(directory_iterator(dir), directory_iterator(), back_inserter(m_FileList));
	}

	NextFile();

	// Set init flag
	mFftInit = false;
}

const float step_pow = 1.1f;

//*********************************************************************************
void KissFileSampleApp::Init( uint32_t mSampleCount )
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
		mp_ChunkSizes = new int[m_NumChunks];

		int i=0;
		for(std::vector<int>::iterator it=chunk_sizes.begin(); it != chunk_sizes.end(); ++it, ++i)
		{
			mp_ChunkSizes[i] = *it;
		}
	}
}

//*********************************************************************************
void KissFileSampleApp::ChunkFreqData()
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
				if(bin >= 2)
				{
					mp_ChunkedFreqData[i] += p_freq_data[bin];
				}
			}

			chunk_start = chunk_end;
		}
	}
}

//*********************************************************************************
void KissFileSampleApp::NextFile()
{
	if(mTrack && mTrack->isPlaying())
	{
		mTrack->enablePcmBuffering(false);
		mTrack->stop();
	}

	Rand r;
	r.seed(time(0));

	int rand_file = r.randInt(m_FileList.size());
	m_CurrentFile = m_FileList[rand_file].string();

	mAudioSource = audio::load(m_CurrentFile);
	mTrack = audio::Output::addTrack(mAudioSource, false);
	mTrack->enablePcmBuffering(true);
	mTrack->play();
}

//*********************************************************************************
void KissFileSampleApp::update() 
{
	// Check if track is playing and has a PCM buffer available
	if (mTrack->isPlaying() && mTrack->isPcmBuffering())
	{
		// Get buffer
		mBuffer = mTrack->getPcmBuffer();
		if (mBuffer && mBuffer->getChannelData(CHANNEL_FRONT_LEFT))
		{
			// Get sample count
			uint32_t mSampleCount = mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mSampleCount;
			if (mSampleCount > 0)
			{
				// Initialize analyzer, if needed
				if (!mFftInit)
				{
					Init(mSampleCount);
				}

				// Send data to fft.
				if (mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData != 0)
				{
					mFft.setData(mBuffer->getChannelData(CHANNEL_FRONT_LEFT)->mData);
				}

				ChunkFreqData();
			}
		}
	}
}

//*********************************************************************************
void KissFileSampleApp::draw()
{
	glPushMatrix();

	gl::clear(Color(0.0f, 0.0f, 0.0f));

	if (mFftInit)
	{
		// Get data
		float * mFreqData = mFft.getAmplitude();
		float * mTimeData = mFft.getData();
		int32_t mDataSize = mFft.getBinSize();

		// Get dimensions
		float mScale = ((float)getWindowWidth() - 20.0f) / (float)mDataSize;
		float mWindowHeight = (float)getWindowHeight();

		PolyLine<Vec2f> freq_line;

		for (int32_t i = 0; i < mDataSize; i++) 
		{
			// Do logarithmic plotting for frequency domain
			double mLogSize = log((double)mDataSize);
			float x = (float)(log((double)i) / mLogSize) * (double)mDataSize;
			float y = math<float>::clamp(mFreqData[i] * (x / mDataSize) * log((double)(mDataSize - i)), 0.0f, 2.0f);

			// Plot points on lines
			freq_line.push_back(Vec2f(x * mScale + 10.0f,           -y * (mWindowHeight - 20.0f) * 1.25f + (mWindowHeight - 10.0f)));
			//freq_line.push_back(Vec2f(i * mScale + 10.0f, mTimeData[i] * (mWindowHeight - 20.0f) * 0.3f  + (mWindowHeight * 0.15f + 10.0f)));
		}

		gl::color(1,1,1,1);
		gl::draw(freq_line);
	}

	if(mFftInit)
	{
		gl::scale(getWindowWidth()* 0.5f, getWindowHeight()*0.5f, 1);
		gl::translate(1.0, 1.0f);
		//gl::translate(0, 1.0f);


		PolyLine<Vec2f> chunked_freq_line;

		float w = 1;
		float h = 0.5f;

		for(int i=0; i<m_NumChunks; ++i)
		{
			float i_f = i/(float)(m_NumChunks-1);

			chunked_freq_line.push_back(Vec2f(1-i_f, -mp_ChunkedFreqData[i]));
		}

		glEnableClientState( GL_VERTEX_ARRAY );
		GLfloat* p_verts = new GLfloat[m_NumChunks * 2 * 2];
		glVertexPointer( 2, GL_FLOAT, 0, p_verts );

		std::vector<Vec2f> pts = chunked_freq_line.getPoints();

		for(int i=0; i<m_NumChunks; ++i)
		{
			p_verts[i*2+0] = pts[i].x;
			p_verts[i*2+1] = pts[i].y;
		}

		//gl::color(1,1,1,1);

		//glDrawArrays( GL_TRIANGLE_STRIP, 0, m_NumChunks * 2 );


		static float roto = 0;
		roto += 1.5f;
		static int num_spikes = 20;
		for(int i=0; i<num_spikes; ++i)
		{
			glPushMatrix();
			float i_f = i/(float)num_spikes;
			gl::color(hsvToRGB(Vec3f(i_f, 1, 1)));

			float inc = 1.0f/num_spikes;
			gl::rotate(360.f * i_f + roto);
			//gl::draw(chunked_freq_line);
			glDrawArrays( GL_TRIANGLE_STRIP, 0, m_NumChunks * 2 );
			glPopMatrix();
		}

		glDisableClientState( GL_VERTEX_ARRAY );

		delete[] p_verts;
	}

	glPopMatrix();

	static bool write_images = true;
	static int frame = 0;
	frame++;
	static int num_frames = 300;
	if(write_images && frame < num_frames)
	{
		mMovieWriter.addFrame(copyWindowSurface());
		//writeImage( getHomeDirectory() + "vis_frames" + getPathSeparator() + "saveImage_" + toString( frame ) + ".png", copyWindowSurface() );	
	}
	else
	{
		if(frame == num_frames)
		{
			mMovieWriter.finish();
			quit();
		}
	}
}

//*********************************************************************************
void KissFileSampleApp::quit() 
{
	// Stop track
	mTrack->enablePcmBuffering(false);
	mTrack->stop();
}



// Start application
CINDER_APP_BASIC(KissFileSampleApp, RendererGl)
