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

// Include header
#include "Kiss.h"

// Constructor
Kiss::Obj::Obj(int32_t dataSize)
{

	// Set running flag
	mRunning = true;

	// Set data size
	mDataSize = dataSize;
	setDataSize(dataSize);

}

// Destructor
Kiss::Obj::~Obj()
{

    dispose();

}

// Polarize cartesian values
void Kiss::Obj::cartesianToPolar()
{

	// Check if polar data is current
    if (!mPolarUpdated)
    {

		// Perform FFT
        transform();

		// Find absolute maximums of and angles between values
		// and use them to set the amplitude and phase data, respectively
        for (int32_t i = 0; i < mBinSize; i++)
        {
            mAmplitude[i] = sqrtf(pow(mReal[i], 2) + pow(mImag[i], 2));
            mPhase[i] = atan2f(mImag[i], mReal[i]);
        }

		// Set flags
        mPolarUpdated = true;
        mPolarNormalized = mCartesianNormalized;

    }

	// Polar data is not yet normalized
    if (!mPolarNormalized)
    {

		// Normalize values
        float mNormalizer = 2.0f / mWindowSum;
        for (int32_t i = 0; i < mBinSize; i++)
            mAmplitude[i] *= mNormalizer;
        mPolarNormalized = true;

    }

}

// Free resources
void Kiss::Obj::dispose()
{

	// Free KISS resources
    kiss_fftr_free(mFftCfg);
    kiss_fftr_free(mIfftCfg);

    // Delete arrays
    delete [] mCxOut;
    delete [] mCxIn;
    delete [] mData;
    delete [] mReal;
    delete [] mImag;
    delete [] mAmplitude;
    delete [] mPhase;
    delete [] mWindow;
    delete [] mInverseWindow;
    delete [] mWindowedData;

}

// Returns array of amplitudes in frequency domain
float* Kiss::Obj::getAmplitude()
{

	// Prepare and return amplitude data
    cartesianToPolar();
    return mAmplitude;

}

// Returns sample size
int32_t Kiss::Obj::getBinSize()
{

    // DO IT!
    return mBinSize;

}

// Get input data
float * Kiss::Obj::getData()
{

	// Data has not been updated
    if (!mDataUpdated)
    {

		// Perform FFT
        transform();

		// Perform inverse FFT
        for (int32_t i = 0; i < mBinSize; i++)
        {
            mCxIn[i].r = mReal[i];
            mCxIn[i].i = mImag[i];
        }
        kiss_fftri(mIfftCfg, mCxIn, mData);

		// Populate data array
        for (int32_t i = 0; i < mDataSize; i++)
            mData[i] *= mInverseWindow[i];

		// Update flags
        mDataUpdated = true;
        mDataUpdated = true;
        mDataNormalized = false;

    }

	// Data has not been normalized
    if (!mDataNormalized)
    {

        // Normalize data
        float mNormalizer = (float)mWindowSum / (float)(2 * mDataSize);
        for (int32_t i = 0; i < mDataSize; i++)
            mData[i] *= mNormalizer;
        mDataNormalized = true;

    }

	// Return data
    return mData;

}

// Returns data size
int32_t Kiss::Obj::getDataSize()
{

    // DO IT!
    return mDataSize;

}

// Returns array of phase values in frequency domain
float * Kiss::Obj::getPhase()
{

	// Prepare and return phase data
    cartesianToPolar();
    return mPhase;

}

// Returns array of real part of complex values
float * Kiss::Obj::getReal()
{

	// Perform FFT and return real data
    transform();
    return mReal;

}

// Returns array of imaginary part of complex values
float * Kiss::Obj::getImaginary()
{

	// Perform FFT and return imaginary data
    transform();
    return mImag;

}

// Send signal to KISS
void Kiss::Obj::setData(float * data)
{

    // Set all flags to false
    mCartesianUpdated = false;
    mPolarUpdated = false;
    mCartesianNormalized = false;
    mPolarNormalized = false;
    mDataUpdated = false;
    mDataNormalized = false;

	// Copy incoming data
    memcpy(mData, data, sizeof(float) * mDataSize);

	// Set data flag
    mDataUpdated = true;
    mDataNormalized = true;

}

// Set data size
void Kiss::Obj::setDataSize(int32_t dataSize)
{

	// Dispose is size has changed
	if (dataSize != mDataSize)
		dispose();

	// Set dimensions
    mDataSize = dataSize;
    mBinSize = (mDataSize / 2) + 1;
	mWindowSum = 0.0f;
    
	// Set flags
    mCartesianNormalized = true;
	mCartesianUpdated = true;
	mDataNormalized = true;
    mPolarNormalized = true;
	mPolarUpdated = true;

    // Allocate arrays
    mAmplitude = new float[mBinSize];
	mData = new float[mDataSize];
	mImag = new float[mBinSize];
	mInverseWindow = new float[mDataSize];
	mReal = new float[mBinSize];
    mPhase = new float[mBinSize];
	mWindow = new float[mDataSize];
	mWindowedData = new float[mDataSize];

	// Set frequencies
	mFrequencyHigh = 1.0f;
	mFrequencyLow = 0.0f;

    // Initialize array values
    memset(mData, 0, sizeof(float) * mDataSize);
    memset(mReal, 0, sizeof(float) * mBinSize);
    memset(mImag, 0, sizeof(float) * mBinSize);
    memset(mAmplitude, 0, sizeof(float) * mBinSize);
    memset(mPhase, 0, sizeof(float) * mBinSize);
    for (int32_t i = 0; i < mDataSize; i++)
        mWindow[i] = sin((M_PI * i) / (mDataSize - 1));
    for (int32_t i = 0; i < mDataSize; i++)
        mWindowSum += mWindow[i];
    for(int32_t i = 0; i < mDataSize; i++)
        mInverseWindow[i] = 1.0 / mWindow[i];

	// Set up KISS
    mFftCfg = kiss_fftr_alloc(mDataSize, 0, 0, 0);
    mIfftCfg = kiss_fftr_alloc(mDataSize, 1, 0, 0);
    mCxIn = new kiss_fft_cpx[mBinSize];
    mCxOut = new kiss_fft_cpx[mBinSize];

}

// Set filter
void Kiss::Obj::setFilter(float frequency, Filter filter)
{

	// Set low and high frequencies based on filter type
	switch (filter)
	{
	case HIGH_PASS:
		setFilter(frequency, 1.0f);
		break;
	case LOW_PASS:
		setFilter(0.0f, frequency);
		break;
	case NONE:
		setFilter(0.0f, 1.0f);
		break;
	case NOTCH:
		setFilter(frequency, frequency);
		break;
	}

}

// Set band pass filter
void Kiss::Obj::setFilter(float lowFrequency, float highFrequency)
{

	// Set frequencies
	mFrequencyLow = lowFrequency;
	mFrequencyHigh = highFrequency;

}

// Stop running
void Kiss::Obj::stop()
{

	// Turn off flag (prevents crash on exit)
	mRunning = false;

}

// Performs FFT
void Kiss::Obj::transform()
{

	// Check flag
	if (!mRunning)
		return;

	// Cartesian values need updating
    if (!mCartesianUpdated)
    {

		// Polar values need updating
        if (!mPolarUpdated)
        {

			// Copy data to windowed array
            memcpy(mWindowedData, mData, sizeof(float) * mDataSize);
            for (int32_t i = 0; i < mDataSize; i++)
                mWindowedData[i] *= mWindow[i];

			// Perform FFT
            kiss_fftr(mFftCfg, mWindowedData, mCxOut);

			// Iterate through complex values
            for (int32_t i = 0; i < mBinSize; i++)
            {

				// Bail if running flag turns off
				if (!mRunning)
					return;

				// Apply complex value if index is within filter range
				bool mApplyComlex = i >= mFrequencyLow * mBinSize && i <= mFrequencyHigh * mBinSize;

				// Extract complex values
				mReal[i] = mApplyComlex ? mCxOut[i].r : 0.0f;
				mImag[i] = mApplyComlex ? mCxOut[i].i : 0.0f;

            }

			// Update flag
            mCartesianUpdated = true;

        }
        else
        {

			// Apply phase and amplitude to values
            for (int32_t i = 0; i < mBinSize; i++)
            {
                mReal[i] = cosf(mPhase[i]) * mAmplitude[i];
                mImag[i] = sinf(mPhase[i]) * mAmplitude[i];
            }
            mCartesianUpdated = true;
            mCartesianNormalized = mPolarNormalized;

        }

    }

	// Complex values are not yet normalized
    if (!mCartesianNormalized)
    {

		// Normalize values
        float mNormalizer = 2.0f / mWindowSum;
        for(int32_t i = 0; i < mBinSize; i++)
        {
            mReal[i] *= mNormalizer;
            mImag[i] *= mNormalizer;
        }
        mCartesianNormalized = true;

    }

}
