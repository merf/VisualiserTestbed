#pragma once

class CSoundAnalyzer
{
public:
	CSoundAnalyzer(int num_bins, int samples_per_frame);
	~CSoundAnalyzer() {}

	static void				StaticInit(int num_bins, int samples_per_frame);

	static CSoundAnalyzer&	Get() { return *mp_Impl; }

	void					ProcessData(float* p_freq_data, float* p_amp_data);

	void					Draw();

	void					GetChunkedMoveData(float** p_data, size_t& num_items);

	float					GetSOD() const { return m_SODFrame; }
private:
	void					AllocFrecDataArrays();

	int						m_SamplesPerFrame;
	int						m_WindowsPerFrame;

	int						m_NumBins;
	int						m_NumChunks;

	float*					mp_AmplitudeData;
	float*					mp_CarrierWave;
	float*					mp_RCC;
	float*					mp_RCCDerivs;
	float*					mp_StochasticComponent;

	float					m_SODFrame;

	float*					mp_StochasticHistory;

	float*					mp_ChunkedFreqData;

	float*					mp_ChunkedAvg;
	float*					mp_ChunkedVariance;

	float*					mp_ChunkedMove;

	float*					mp_TotalMoveHistory;
	float*					mp_MoveAvgFreqHistory;

	float*					mp_History;

	int*					mp_ChunkSizes;

	static CSoundAnalyzer*	mp_Impl;
};