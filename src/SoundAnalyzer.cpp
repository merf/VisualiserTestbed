#include "cinder/CinderMath.h"
#include "cinder/app/AppBasic.h"
#include "cinder/app/App.h"

#include "SoundAnalyzer.h"

#include <vector>
#include <stddef.h>

CSoundAnalyzer* CSoundAnalyzer::mp_Impl = NULL;

const float step_pow = 1.5f;

const int history_size = 100;
static int curr_history_item = 0;
static int item_index = 0;
static int master_count = 0;

using namespace ci;
using namespace ci::app;

//*********************************************************************************
//*********************************************************************************
CSoundAnalyzer::CSoundAnalyzer(int num_bins, int samples_per_frame)
	:
m_NumBins(num_bins),
m_SamplesPerFrame(samples_per_frame),
m_WindowsPerFrame(10)
{
	int num_bins_for_this_chunk = 0;
	std::vector<int> chunk_sizes;

	int bin_count = 0;
	for(int i=0; bin_count<num_bins; i++)
	{
		num_bins_for_this_chunk = (int)floor(pow(step_pow, i));
		bin_count += num_bins_for_this_chunk;

		chunk_sizes.push_back(num_bins_for_this_chunk);
	}

	m_NumChunks = chunk_sizes.size();

	if(m_NumChunks > 0)
	{
		mp_AmplitudeData = new float[m_SamplesPerFrame];
		mp_CarrierWave = new float[m_SamplesPerFrame];
		mp_RCC = new float[m_SamplesPerFrame];

		mp_StochasticComponent = new float[m_WindowsPerFrame];

		mp_ChunkedFreqData = new float[m_NumChunks];
		mp_ChunkedAvg = new float[m_NumChunks];
		mp_ChunkedMove = new float[m_NumChunks];
		mp_TotalMoveHistory = new float[history_size];
		mp_MoveAvgFreqHistory = new float[history_size];

		mp_ChunkSizes = new int[m_NumChunks];

		mp_History = new float[m_NumChunks * history_size];

		memset(mp_ChunkedAvg, 0, m_NumChunks * sizeof(float));
		memset(mp_ChunkedMove, 0, m_NumChunks * sizeof(float));
		memset(mp_TotalMoveHistory, 0, history_size * sizeof(float));
		memset(mp_MoveAvgFreqHistory, 0, history_size * sizeof(float));
		memset(mp_History, 0, m_NumChunks * history_size * sizeof(float));

		int i=0;
		for(std::vector<int>::iterator it=chunk_sizes.begin(); it != chunk_sizes.end(); ++it, ++i)
		{
			mp_ChunkSizes[i] = *it;
		}
	}
}

//*********************************************************************************
void CSoundAnalyzer::StaticInit(int num_bins, int samples_per_frame)
{
	if(mp_Impl)
	{
		delete mp_Impl;
	}
	mp_Impl = new CSoundAnalyzer(num_bins, samples_per_frame);
}

//*********************************************************************************
void CSoundAnalyzer::GetChunkedMoveData(float** p_data, size_t& num_items)
{
	*p_data = mp_ChunkedMove;
	num_items = m_NumChunks;
}

float autocorrelation(int n, float const * data)
{
	int lag = 1;
	float mean = 0;
	for(int i=0; i<n; ++i)
	{
		mean += data[i];
	}
	mean /= (float)n;

	float numer = 0;
	for(int i=0; i<n-lag; ++i)
	{
		numer += (data[i] - mean)*(data[i+lag] - mean);
	}

	float denom = 0;
	for(int i=0; i<n; ++i)
	{
		denom += (data[i] - mean)*(data[i] - mean);
	}

	return numer/denom;
}

//*********************************************************************************
void CSoundAnalyzer::ProcessData(float* p_freq_data, float* p_amp_data)
{
	for(int i=0; i<m_SamplesPerFrame; ++i)
	{
		mp_AmplitudeData[i] = p_amp_data[i]-0.5f;

		mp_CarrierWave[i] = 0;
	}

	//we walk thru the amplitude data looking for turning points we then take the mid point of each pair of turning points
	//and linearly interpolate between these...
	int segment_start_i = 0;
	float segment_start_a = 0;
	int previous_midpoint_i = 0;
	float previous_midpoint_a = 0;
	float prev_a = 0;
	bool prev_increasing_a = mp_AmplitudeData[1] > mp_AmplitudeData[0];

	for(int i=0; i<m_SamplesPerFrame; ++i)
	{
		float a = mp_AmplitudeData[i];
		if(i!=0)
		{
			bool increasing_a = mp_AmplitudeData[i] > mp_AmplitudeData[i-1];

			//turning point?
			if(increasing_a != prev_increasing_a)
			{
				int midpoint_i = segment_start_i + (i-segment_start_i)/2;
				float midpoint_a = mp_AmplitudeData[midpoint_i];

				for(int j=previous_midpoint_i; j<midpoint_i; ++j)
				{
					float f = (j-previous_midpoint_i)/(float)(midpoint_i-previous_midpoint_i);
					mp_CarrierWave[j] = lerp(previous_midpoint_a, midpoint_a, f);

					mp_RCC[j] = mp_AmplitudeData[j] - mp_CarrierWave[j];
					//mp_CarrierWave[j] = midpoint_a;
				}

				previous_midpoint_i = midpoint_i;
				previous_midpoint_a = midpoint_a;

				segment_start_i = i;
			}

			prev_increasing_a = increasing_a;
		}
		prev_a = a;
	}

	for(int i=0; i<m_WindowsPerFrame; ++i)
	{
		int samples_for_window = m_SamplesPerFrame / m_WindowsPerFrame;

		mp_StochasticComponent[i] = 1 - autocorrelation(samples_for_window, &mp_RCC[i*samples_for_window]);
	}

	int midpoint_i = m_SamplesPerFrame;
	float midpoint_a = mp_AmplitudeData[midpoint_i];

	for(int j=previous_midpoint_i; j<midpoint_i; ++j)
	{
		float f = (j-previous_midpoint_i)/(float)(midpoint_i-previous_midpoint_i);
		mp_CarrierWave[j] = lerp(previous_midpoint_a, midpoint_a, f);

		mp_RCC[j] = mp_AmplitudeData[j] - mp_CarrierWave[j];
	}

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

	int memory_length = 30;
	float one_on_memory_length = 1.0f/(float)memory_length;
	for(int i=0; i<m_NumChunks; i++)
	{
		mp_ChunkedAvg[i] = mp_ChunkedAvg[i] - mp_ChunkedAvg[i]*one_on_memory_length + mp_ChunkedFreqData[i]*one_on_memory_length;
	}

	float total_move_for_frame = 0;
	float average_move_freq = 0;
	for(int i=0; i<m_NumChunks; i++)
	{
		mp_ChunkedMove[i] = math<float>::clamp(mp_ChunkedFreqData[i] - mp_ChunkedAvg[i]);
		total_move_for_frame += mp_ChunkedMove[i];
		average_move_freq += mp_ChunkedMove[i] * i;
	}

	total_move_for_frame /= (float)m_NumChunks;
	average_move_freq /= (float)m_NumChunks;

	for(int i=0; i<m_NumChunks; i++)
	{
		int index = i + curr_history_item * m_NumChunks;
		mp_History[index] = mp_ChunkedMove[i];
	}

	mp_TotalMoveHistory[curr_history_item] = total_move_for_frame;
	mp_MoveAvgFreqHistory[curr_history_item] = average_move_freq;

	curr_history_item++;
	curr_history_item = curr_history_item % history_size;

	master_count++;
}

//*********************************************************************************
void CSoundAnalyzer::Draw()
{
	glPushMatrix();

	gl::color(1,1,1);

	gl::translate(0, getWindowHeight()*0.5f, 0);

	gl::drawLine(Vec2f(0, getWindowHeight()*0.5f), Vec2f(getWindowWidth(), getWindowHeight()*0.5f));

	gl::color(1,0,0);

	for(int i=0; i<m_SamplesPerFrame; ++i)
	{
		if(i<m_SamplesPerFrame-1)
		{
			int i0=i;
			int i1=i+1;

			float f0 = i0 / (float)m_SamplesPerFrame;
			float f1 = i1 / (float)m_SamplesPerFrame;

			gl::drawLine(Vec2f(f0*getWindowWidth(), getWindowHeight()*mp_AmplitudeData[i0]*0.5f), Vec2f(f1*getWindowWidth(), getWindowHeight()*mp_AmplitudeData[i1]*0.5f));
		}
	}

	gl::color(0,1,0);

	for(int i=0; i<m_SamplesPerFrame; ++i)
	{
		if(i<m_SamplesPerFrame-1)
		{
			int i0=i;
			int i1=i+1;

			float f0 = i0 / (float)m_SamplesPerFrame;
			float f1 = i1 / (float)m_SamplesPerFrame;

			gl::drawLine(Vec2f(f0*getWindowWidth(), getWindowHeight()*mp_CarrierWave[i0]*0.5f), Vec2f(f1*getWindowWidth(), getWindowHeight()*mp_CarrierWave[i1]*0.5f));
		}
	}

	gl::color(0,0,1);

	for(int i=0; i<m_SamplesPerFrame; ++i)
	{
		if(i<m_SamplesPerFrame-1)
		{
			int i0=i;
			int i1=i+1;

			float f0 = i0 / (float)m_SamplesPerFrame;
			float f1 = i1 / (float)m_SamplesPerFrame;

			gl::drawLine(Vec2f(f0*getWindowWidth(), getWindowHeight()*mp_RCC[i0]*0.5f), Vec2f(f1*getWindowWidth(), getWindowHeight()*mp_RCC[i1]*0.5f));
		}
	}

	gl::color(1,0,1);

	for(int i=0; i<m_WindowsPerFrame; ++i)
	{
		if(i<m_WindowsPerFrame-1)
		{
			int i0=i;
			int i1=i+1;

			float f0 = i0 / (float)m_WindowsPerFrame;
			float f1 = i1 / (float)m_WindowsPerFrame;

			gl::drawLine(Vec2f(f0*getWindowWidth(), getWindowHeight()*mp_StochasticComponent[i0]*0.5f), Vec2f(f1*getWindowWidth(), getWindowHeight()*mp_StochasticComponent[i1]*0.5f));
		}
	}


	//glEnableClientState( GL_VERTEX_ARRAY );
	//GLfloat* p_verts = new GLfloat[m_NumChunks * 2 * 4];
	//glVertexPointer( 2, GL_FLOAT, 0, p_verts );

	//for(int i=0; i<m_NumChunks; ++i)
	//{
	//	float i_f = i/(float)(m_NumChunks-1);

	//	p_verts[i*4+0] = (i_f) * getWindowWidth();
	//	p_verts[i*4+1] = getWindowHeight() * 0.5f - mp_ChunkedFreqData[i] * getWindowHeight() * 0.5f;

	//	p_verts[i*4+2] = (i_f) * getWindowWidth();
	//	p_verts[i*4+3] = getWindowHeight() * 0.5f;
	//}

	//gl::color(0.5f, 0.5f, 0.5f, 0.5f);

	//glDrawArrays( GL_TRIANGLE_STRIP, 0, (m_NumChunks) * 4 );

	/*
	for(int x=0; x<history_size; x++)
	{
		int history_index = (x + master_count) % history_size;
		float w = getWindowWidth()/(float)history_size;
		float x_f = x*w;
		float t_val = mp_TotalMoveHistory[history_index] * 10;

		gl::color(t_val, t_val, t_val, 1);

		float y_f = getWindowHeight() * mp_MoveAvgFreqHistory[history_index];

		gl::drawSolidRect(Rectf(x_f, y_f, x_f+w, y_f+5));
	}
	*/

	/*
	for(int x=0; x<history_size; x++)
	{
		int history_index = (x + master_count) % history_size;
		for(int y=0; y<m_NumChunks; y++)
		{
			float y_f = getWindowHeight() * y/(float)m_NumChunks;

			float x_f = getWindowWidth() * x/(float)history_size;
			float t_val = mp_History[history_index * m_NumChunks + y] * 10;

			gl::color(hsvToRGB(Vec3f(t_val, 1, t_val)));

			gl::drawSolidRect(Rectf(x_f-2, y_f, x_f+2, y_f+8));
		}
	}

	//Movement Spectrum
	for(int i=0; i<m_NumChunks; ++i)
	{
		float i_f = i/(float)(m_NumChunks-1);
		gl::color(hsvToRGB(Vec3f(i_f,1,1)));
		float w = getWindowWidth()/(float)m_NumChunks;
		float y= getWindowHeight()*0.5f;
		y=0;
		float h = 3;

		gl::drawSolidRect(Rectf(i_f*getWindowWidth(), y-mp_ChunkedMove[i]*getWindowHeight()-h, i_f*getWindowWidth()+w, y-mp_ChunkedMove[i]*getWindowHeight()));
	}
	*/

	glPopMatrix();

	//for(int i=0; i<m_NumChunks; ++i)
	//{
	//	float i_f = i/(float)m_NumChunks;
	//	//gl::color(hsvToRGB(Vec3f(i_f,1,1)));
	//	float w = getWindowWidth()*0.5f/(float)m_NumChunks;
	//	float y= getWindowHeight()*0.5f;
	//	float h = 10;

	//	if(mp_ChunkedFreqData[i] > mp_ChunkedAvg[i])
	//	{
	//		gl::color(0,1,0,0.5f);
	//	}
	//	else
	//	{
	//		gl::color(1,0,0,0.5f);
	//	}

	//	//gl::drawSolidRect(Rectf(i_f*getWindowWidth()-w, y-mp_ChunkedAvg[i]*getWindowHeight()-h, i_f*getWindowWidth()+w, y-mp_ChunkedAvg[i]*getWindowHeight()+h));
	//	gl::drawSolidRect(Rectf(i_f*getWindowWidth(), y-mp_ChunkedAvg[i]*getWindowHeight(), i_f*getWindowWidth()+w, y-mp_ChunkedFreqData[i]*getWindowHeight()));
	//	//gl::drawSolidRect(Rectf(i_f*getWindowWidth()-w, getWindowHeight()*0.5f-h, i_f*getWindowWidth()+w, getWindowHeight()*0.5f+h));
	//}


}