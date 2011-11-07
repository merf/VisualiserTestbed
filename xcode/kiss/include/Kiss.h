/*
* 
* Copyright (c) 2011, Ban the Rewind
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or 
* without modification, are permitted provided that the following 
* conditions are met:
* 
* Redistributions of source code must retain the above copyright 
* notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright 
* notice, this list of conditions and the following disclaimer in 
* the documentation and/or other materials provided with the 
* distribution.
* 
* Neither the name of the Ban the Rewind nor the names of its 
* contributors may be used to endorse or promote products 
* derived from this software without specific prior written 
* permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
* 
*/

#pragma once

// Includes
#include "cinder/Cinder.h"
#include "cinder/CinderMath.h"
#include "kiss/kiss_fftr.h"

// Imports
using namespace ci;
using namespace std;

// KissFFT wrapper
class Kiss
{

public:

// Filter types
enum Filter
{
	NONE, 
	LOW_PASS, 
	HIGH_PASS, 
	NOTCH
};

private:

	// The meat of this class is held here to allow
	// the separation of source and header
	class Obj
	{

	public:

		// Con/de-structors
		Obj(int32_t dataSize);
		~Obj();

		// Setters
		void setData(float * data);
		void setDataSize(int32_t dataSize);
		void setFilter(float frequency, Filter filter);
		void setFilter(float lowFrequency, float highFrequency);

		// Getters
		float * getAmplitude();
		int32_t getBinSize();
		float * getData();
		int32_t getDataSize();
		float * getImaginary();
		float * getPhase();
		float * getReal();

		// Stop
		void stop();

	private:

		// Clean up
		void dispose();

		// Arrays
		float * mAmplitude;
		float * mData;
		float * mImag;
		float * mInverseWindow;
		float * mPhase;
		float * mReal;
		float * mWindow;
		float * mWindowedData;

		// Dimensions
		int32_t mDataSize;
		int32_t mBinSize;
		float mWindowSum;

		// Flags
		bool mCartesianNormalized;
		bool mCartesianUpdated;
		bool mDataNormalized;
		bool mDataUpdated;
		bool mPolarNormalized;
		bool mPolarUpdated;

		// Performs FFT
		void transform();

		// Sets amplitude and phase arrays
		void cartesianToPolar();

		// KissFFT
		kiss_fftr_cfg mFftCfg;
		kiss_fftr_cfg mIfftCfg;
		kiss_fft_cpx * mCxOut;
		kiss_fft_cpx * mCxIn;

		// Filter frequencies
		float mFrequencyHigh;
		float mFrequencyLow;

		// Running flag
		bool mRunning;

	};

	// Pointer to object
	std::shared_ptr<Obj> mObj;

public:

	// Constructors
	Kiss() : mObj(std::shared_ptr<Obj>(new Obj(512))) {};
	~Kiss() { 
		mObj->stop(); 
		mObj.reset();
	}

	// Setters
	void removeFilter() { mObj->setFilter(0.0f, NONE); }
	void setData(float * data) { mObj->setData(data); }
	void setDataSize(int32_t dataSize) { mObj->setDataSize(dataSize); }
	void setFilter(float lowFrequency, float highFrequency) { mObj->setFilter(lowFrequency, highFrequency); }
	void setFilter(float frequency, Filter filter = LOW_PASS) { mObj->setFilter(frequency, filter); }

	// Getters
	float * getAmplitude() { return mObj->getAmplitude(); };
	int32_t getBinSize() { return mObj->getBinSize(); }
	float * getData() { return mObj->getData(); }
	int32_t getDataSize() { return mObj->getDataSize(); }
	float * getImaginary() { return mObj->getImaginary(); }
	float * getPhase() { return mObj->getPhase(); }
	float * getReal() { return mObj->getReal(); }

};
