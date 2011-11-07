#include "SoundEngine.h"

#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"
#include "cinder/CinderMath.h"

using namespace cinder;

float sampling_freq = 44100.0f;
//float sampling_freq = 22500.0f;
ENormalizationStrategy g_NormalizationStrategy = SMOOTH_LIMIT_BARK_BANDS;

static int short_history_size = 3;
static int long_history_size = 50;

CSoundEngine* CSoundEngine::mp_Impl = NULL;

//*************************************************************************
//*************************************************************************
void CSoundEngine::Init()
{
	// Load and play audio
	std::string dir = getHomeDirectory();
	std::string path = "C:/music.mp3";
	mAudioSource = audio::load(path);
	mTrack = audio::Output::addTrack(mAudioSource, false);
	mTrack->enablePcmBuffering(true);
	mTrack->play();

	// Set init flag
	mFftInit = false;
}

//*************************************************************************
void CSoundEngine::Shutdown()
{
	// Stop track
	mTrack->enablePcmBuffering(false);
	mTrack->stop();
}

//*************************************************************************
void CSoundEngine::InitBarkVals()
{
	m_NumFFTBands = mFft.getBinSize();

	//bins are 0-nyquist - ie 0Hz - 22,500Hz
	//we want to clump according bark scale - freq bands are:
	//20, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500.
	//http://en.wikipedia.org/wiki/Bark_scale

	m_BarkBandFrequencies[0] = 20;
	m_BarkBandFrequencies[1] = 100;
	m_BarkBandFrequencies[2] = 200;
	m_BarkBandFrequencies[3] = 300;
	m_BarkBandFrequencies[4] = 400;
	m_BarkBandFrequencies[5] = 510;
	m_BarkBandFrequencies[6] = 630;
	m_BarkBandFrequencies[7] = 770;
	m_BarkBandFrequencies[8] = 920;
	m_BarkBandFrequencies[9] = 1080;
	m_BarkBandFrequencies[10] = 1270;
	m_BarkBandFrequencies[11] = 1480;
	m_BarkBandFrequencies[12] = 1720;
	m_BarkBandFrequencies[13] = 2000;
	m_BarkBandFrequencies[14] = 2320;
	m_BarkBandFrequencies[15] = 2700;
	m_BarkBandFrequencies[16] = 3150;
	m_BarkBandFrequencies[17] = 3700;
	m_BarkBandFrequencies[18] = 4400;
	m_BarkBandFrequencies[19] = 5300;
	m_BarkBandFrequencies[20] = 6400;
	m_BarkBandFrequencies[21] = 7700;
	m_BarkBandFrequencies[22] = 9500;
	m_BarkBandFrequencies[23] = 12000;
	m_BarkBandFrequencies[24] = 15500;

	binToBark = new int[m_NumFFTBands];
	int bark = 0;
	int count = 0;
	for(int i=0; i<m_NumFFTBands; ++i)
	{
		float f = (float)i / (float)m_NumFFTBands;
		float freq = sampling_freq * 0.5f * f;
		if(freq > m_BarkBandFrequencies[bark] && bark <= NUM_BARK_SCALE_BANDS-1)
		{
			m_BarkBandMultipliers[bark] = 1.0f / count;
			count = 0;

			++bark;
		}
		++count;
		binToBark[i] = bark;
	}	

	//float div = 60.0f / ofGetFrameRate();
	float div = 1;

	long_history_size = ceil(long_history_size/div);
	short_history_size = ceil(short_history_size/div);

	for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
	{
		m_BarkLongAverages[i] = 0.1f;

		for(int j=0; j<long_history_size; ++j)
		{
			m_History[i].push_back(0.0f);
		}
	}

	//memset(m_BarkLongAverages, 1, sizeof(float) * NUM_BARK_SCALE_BANDS);
	memset(m_BarkShortAverages, 0, sizeof(float) * NUM_BARK_SCALE_BANDS);
	memset(m_BarkVariance, 0.0000000001f, sizeof(float) * NUM_BARK_SCALE_BANDS);
	//memset(mp_RawAudio, 0, sizeof(float) * m_BufferSize);
}

//*************************************************************************
void CSoundEngine::Update()
{
	// Check if track is playing and has a PCM buffer available
	if (mTrack->isPlaying() && mTrack->isPcmBuffering())
	{
		// Get buffer
		mBuffer = mTrack->getPcmBuffer();
		if (mBuffer && mBuffer->getInterleavedData())
		{
			// Get sample count
			uint32_t mSampleCount = mBuffer->getInterleavedData()->mSampleCount;
			if (mSampleCount > 0)
			{
				// Initialize analyzer, if needed
				if (!mFftInit)
				{
					mFftInit = true;
					mFft.setDataSize(mSampleCount);

					InitBarkVals();
				}

				// Analyze data
				if (mBuffer->getInterleavedData()->mData != 0) 
				{
					mFft.setData(mBuffer->getInterleavedData()->mData);
				}
			}
		}
	}

	if(mFftInit)
	{
		mp_FFTOutput = mFft.getAmplitude();
		ProcessAudio(1.0f / 60.0f);
	}
}

//*************************************************************************
void CSoundEngine::Draw()
{
	gl::enableAlphaBlending();

	// Check init flag
	if (mFftInit)
	{
		float w = mp_App->getWindowWidth();
		float h = mp_App->getWindowHeight();

		float curr_x = 0;
		float bar_width = w / (float)NUM_BARK_SCALE_BANDS;
		float bar_height = h * 0.25f;

		for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
		{
			float f = i/(float)NUM_BARK_SCALE_BANDS;

			glColor4f(ColorA(CM_RGB, 0.5f, 0.5f, 0.5f, 0.5f));
			gl::drawSolidRect(Rectf(curr_x, h-bar_height, curr_x+bar_width, h));

			glColor3f(Color(CM_HSV, f, 1, 1));
			float val = GetMovement(f);
			gl::drawSolidRect(Rectf(curr_x, h-val*bar_height, curr_x+bar_width*0.5f, h));
			curr_x += bar_width * 0.5f;

			glColor3f(Color(CM_HSV, f, 1, 0.75f));
			val = GetShortAverage(f);
			gl::drawSolidRect(Rectf(curr_x, h-val*bar_height, curr_x+bar_width*0.25f, h));
			curr_x += bar_width * 0.25f;

			glColor3f(Color(CM_HSV, f, 1, 0.5f));
			val = GetAverage(f);
			gl::drawSolidRect(Rectf(curr_x, h-val*bar_height, curr_x+bar_width*0.25f, h));
			curr_x += bar_width * 0.25f;
		}

		//// Get data
		//float * mFreqData = mFft.getAmplitude();
		//float * mTimeData = mFft.getData();
		//int32_t mDataSize = mFft.getBinSize();

		//// Get dimensions
		//float mScale = ((float)w - 20.0f) / (float)mDataSize;
		//float mWindowHeight = (float)h;

		//// Use polylines to depict time and frequency domains
		//PolyLine<Vec2f> mFreqLine;
		//PolyLine<Vec2f> mTimeLine;

		//// Iterate through data
		//for (int32_t i = 0; i < mDataSize; i++) 
		//{
		//	// Do logarithmic plotting for frequency domain
		//	double mLogSize = log((double)mDataSize);
		//	float x = (float)(log((double)i) / mLogSize) * (double)mDataSize;
		//	float y = math<float>::clamp(mFreqData[i] * (x / mDataSize) * log((double)(mDataSize - i)), 0.0f, 2.0f);

		//	// Plot points on lines
		//	mFreqLine.push_back(Vec2f(x * mScale + 10.0f,           -y * (mWindowHeight - 20.0f) * 1.25f + (mWindowHeight - 10.0f)));
		//	mTimeLine.push_back(Vec2f(i * mScale + 10.0f, mTimeData[i] * (mWindowHeight - 20.0f) * 0.3f  + (mWindowHeight * 0.15f + 10.0f)));
		//}

		//// Draw signals
		//gl::draw(mFreqLine);
		//gl::draw(mTimeLine);

	}
}


//*******************************************************************************************************
float* CSoundEngine::GetRawAudio(int& num)
{
	num = mFft.getDataSize();
	return mFft.getData();
}

//*******************************************************************************************************
void CSoundEngine::ProcessAudio(float time_step)
{
	float max_vol = 0.0f;
	float avg_vol = 0.0f;
	for(int i = 0; i < m_NumFFTBands; i++)
	{
		if(mp_FFTOutput[i] > max_vol)
		{
			max_vol = mp_FFTOutput[i];
		}
		avg_vol += mp_FFTOutput[i];
	}
	avg_vol /= m_NumFFTBands;

	static float min_allowed = 0.01f;
	avg_vol = math<float>::max(min_allowed, avg_vol);

	m_SmoothVolume = lerp(m_SmoothVolume, avg_vol, 0.1f);
	m_SmoothVolume = lerp(m_SmoothVolume, max_vol, 0.1f);

	memset(m_BarkScaleValues, 0, sizeof(float) * NUM_BARK_SCALE_BANDS);
	
	//chunk up the FFT into the bark scale bins...
	for(int i = 0; i < m_NumFFTBands; i++)
	{
		float f = log((float)(i+2));
		int bark = binToBark[i];
		m_BarkScaleValues[bark] += mp_FFTOutput[i] * f;
	}
	
	NormalizeAudio();

	//TODO - try using proper averages as is seems this can sometimes get into a situation where the short
	//average is always greater than the long average :(
	for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
	{
		float popped_val = m_History[i].back();
		m_History[i].pop_back();
		m_History[i].push_front(m_BarkScaleValues[i]);

		float long_avg = 0.0f;
		float short_avg = 0.0f;
		float variance = 0.0f;

		int curr_el = 0;
		for(std::list<float>::iterator it = m_History[i].begin(); it != m_History[i].end(); ++it)
		{
			long_avg += *it;

			if(curr_el < short_history_size)
			{
				short_avg += *it;
			}

			variance += pow((m_BarkLongAverages[i] - *it), 2);

			curr_el++;
		}

		long_avg /= (float)long_history_size;
		variance /= (float)long_history_size;
		short_avg /= (float)short_history_size;

		m_BarkLongAverages[i] = long_avg;
		m_BarkShortAverages[i] = short_avg;
		m_BarkVariance[i] = variance;

		//static float long_num = 50;
		//m_BarkLongAverages[i] -= m_BarkLongAverages[i] / long_num;
		//m_BarkLongAverages[i] += m_BarkScaleValues[i] / long_num;
		//RMath::LimitUpper(m_BarkLongAverages[i], 1.0f);
		//
		//static float short_num = 5;
		//m_BarkShortAverages[i] -= m_BarkShortAverages[i] / short_num;
		//m_BarkShortAverages[i] += m_BarkScaleValues[i] / short_num;
		//RMath::LimitUpper(m_BarkShortAverages[i], 1.0f);
	}	
}

//*******************************************************************************************************
void CSoundEngine::NormalizeAudio()
{
	switch(g_NormalizationStrategy)
	{
	case NORMALIZE_CLAMP_BARK_BANDS:
		{
			//find the max
			float max_bark = 0.0f;
			for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
			{
				if(m_BarkScaleValues[i] > max_bark)
				{
					max_bark = m_BarkScaleValues[i];
				}
			}

			//multiply through by 1/max;
			float normalization_mul = 1.0f / max_bark;
			if(m_SmoothVolume > 0.0f)
			{
				for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
				{
					m_BarkScaleValues[i] *= normalization_mul;
				}
			}
		}

		break;
	case SMOOTH_LIMIT_BARK_BANDS:
		{
			//find the max
			float max_bark = 0.0f;
			for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
			{
				if(m_BarkScaleValues[i] > max_bark)
				{
					max_bark = m_BarkScaleValues[i];
				}
			}

			max_bark += 0.1f;

			static float lerp_speed_inc = 0.1f;
			static float lerp_speed_dec = 0.01f;

			if(max_bark > m_SmoothBarkBandMax)
			{
				m_SmoothBarkBandMax = lerp(m_SmoothBarkBandMax, max_bark, lerp_speed_inc);
			}
			else
			{
				m_SmoothBarkBandMax = lerp(m_SmoothBarkBandMax, max_bark, lerp_speed_dec);
			}

			//multiply through by 1/max;
			float normalization_mul = 1.0f / m_SmoothBarkBandMax;
			if(m_SmoothVolume > 0.0f)
			{
				for(int i=0; i<NUM_BARK_SCALE_BANDS; ++i)
				{
					m_BarkScaleValues[i] *= normalization_mul;
					m_BarkScaleValues[i] = math<float>::min(m_BarkScaleValues[i], 1.0f);
				}
			}
		}

		break;
	}
}


//*******************************************************************************************************
float CSoundEngine::GetAmplitude(float f)
{
	return GetValue(m_BarkScaleValues, f);
}

//*******************************************************************************************************
float CSoundEngine::GetAverage(float f)
{
	return GetValue(m_BarkLongAverages, f);
}

//*******************************************************************************************************
float CSoundEngine::GetShortAverage(float f)
{
	return GetValue(m_BarkShortAverages, f);
}

//*******************************************************************************************************
float CSoundEngine::GetVariance(float f)
{
	return GetValue(m_BarkVariance, f);
}
//*******************************************************************************************************
float CSoundEngine::GetValue(float* vals, float f)
{
	float f_bin = f * NUM_BARK_SCALE_BANDS;
	int bark_bin = floor(f_bin);
	if(bark_bin == NUM_BARK_SCALE_BANDS - 1)
	{
		return vals[bark_bin];
	}
	else
	{
		f_bin -= bark_bin;
		return lerp(vals[bark_bin], vals[bark_bin+1], f_bin);
	}
}

//*******************************************************************************************************
float CSoundEngine::GetMovement(float f)
{
	//TODO perhaps movement etc. should be calculated once per frame and cached...
	static bool use_variance = true;
	if(use_variance)
	{
		float long_average = GetAverage(f);
		float short_average = GetShortAverage(f);

		float diff = short_average - long_average;
		
		float diff_sq = pow(diff, 2);
		float variance = GetVariance(f);
		variance = math<float>::max(variance, 0.0000000001f);
		float movement_mag = diff_sq / variance;

		static float one_on_num_standard_dev_that_counts_as_max = 1.0f / 3.0f;
		movement_mag *= one_on_num_standard_dev_that_counts_as_max;

		if(diff < 0)
		{
			movement_mag *= -1.0f;
		}

		movement_mag = math<float>::clamp(movement_mag, -1.0f, 1.0f);
		return movement_mag;
	}
	else
	{
		float long_average = GetAverage(f);
		float short_average = GetShortAverage(f);

		float diff = short_average - long_average;

		static float amount_for_max = 0.5f;

		float val = diff/(amount_for_max * long_average);

		val = math<float>::clamp(val, -1.0f, 1.0f);

		return val;
	}
}