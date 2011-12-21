#include "cinder/CinderMath.h"
#include "cinder/app/AppBasic.h"
#include "cinder/app/App.h"

#include "SoundAnalyzer.h"

#include <vector>
#include <stddef.h>

CSoundAnalyzer* CSoundAnalyzer::mp_Impl = NULL;

const float step_pow = 1.2f;

const int history_size = 100;
static int curr_history_item = 0;
static int item_index = 0;
static int master_count = 0;

using namespace ci;
using namespace ci::app;

//*********************************************************************************
//*********************************************************************************
CSoundAnalyzer::CSoundAnalyzer(int num_bins)
	:
m_NumBins(num_bins)
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
void CSoundAnalyzer::StaticInit(int num_bins)
{
	if(mp_Impl)
	{
		delete mp_Impl;
	}
	mp_Impl = new CSoundAnalyzer(num_bins);
}

//*********************************************************************************
void CSoundAnalyzer::ProcessData(float* p_freq_data, float* p_amp_data)
{
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

	gl::translate(0, getWindowHeight()*0.5f, 0);



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


	//for(int x=0; x<history_size; x++)
	//{
	//	int history_index = (x + master_count) % history_size;
	//	float w = getWindowWidth()/(float)history_size;
	//	float x_f = x*w;
	//	float t_val = mp_TotalMoveHistory[history_index] * 10;

	//	gl::color(t_val, t_val, t_val, 1);

	//	float y_f = getWindowHeight() * mp_MoveAvgFreqHistory[history_index];

	//	gl::drawSolidRect(Rectf(x_f, y_f, x_f+w, y_f+5));
	//}

	//for(int x=0; x<history_size; x++)
	//{
	//	int history_index = (x + master_count) % history_size;
	//	for(int y=0; y<m_NumChunks; y++)
	//	{
	//		float y_f = getWindowHeight() * y/(float)m_NumChunks;

	//		float x_f = getWindowWidth() * x/(float)history_size;
	//		float t_val = mp_History[history_index * m_NumChunks + y] * 1;

	//		gl::color(hsvToRGB(Vec3f(t_val, 1, t_val)));

	//		gl::drawSolidRect(Rectf(x_f-2, y_f, x_f+2, y_f+8));
	//	}
	//}

	//Movement Spectrum
	for(int i=0; i<m_NumChunks; ++i)
	{
		float i_f = i/(float)(m_NumChunks-1);
		gl::color(hsvToRGB(Vec3f(i_f,1,1)));
		float w = getWindowWidth()/(float)m_NumChunks;
		float y= getWindowHeight()*0.5f;
		float h = 3;

		gl::drawSolidRect(Rectf(i_f*getWindowWidth(), y-mp_ChunkedMove[i]*getWindowHeight()-h, i_f*getWindowWidth()+w, y-mp_ChunkedMove[i]*getWindowHeight()));
	}

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