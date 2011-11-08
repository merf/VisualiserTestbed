#pragma once
#include "cinder/Cinder.h"
#include <string>

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
class COpenALSample
{
public:
	COpenALSample(const std::string& path);

	float*			mp_Buffer;
	ci::uint32_t	m_SampleCount;
	ci::uint32_t	m_NumChannels;
};
typedef boost::shared_ptr<COpenALSample> TOpenALSampleRef;

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
class COpenALSampleManager
{
public:
	COpenALSampleManager();
	~COpenALSampleManager();
    
    static void             StaticInit();

	static COpenALSample*	CreateSample(const std::string& path);

private:
	static COpenALSampleManager* mp_Impl;
};