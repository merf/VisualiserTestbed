#pragma once

class CSoundAnalyzer
{
public:
	CSoundAnalyzer(int num_bins);
	~CSoundAnalyzer() {}

	static void				StaticInit(int num_bins);

	static CSoundAnalyzer&	Get() { return *mp_Impl; }

	void					ProcessData(float* p_freq_data, float* p_amp_data);

	void					Draw();

private:
	void					AllocFrecDataArrays();

	int						m_NumBins;
	int						m_NumChunks;

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