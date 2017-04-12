/* 

  GPL LICENSE SUMMARY

  Copyright(c) Pat Brouillette. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  Contact Information:
    Pat Brouillette  pfrench@acm.org

*/
#include "spdifAnalyzer.h"
#include "spdifAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

/* add the "C" bitstream parser library as code and set up some "C" callback stubs */
extern "C" {

#include "spdif.c"

static void c_sample_callback( void *userdata, uint64_t t, uint64_t tend, enum SpdifFrameType ft, uint32_t aud_sample )
{
    spdifAnalyzer   *sba = (spdifAnalyzer *)userdata;
    sba->sample_callback(t,tend,ft,aud_sample);
}

static void c_status_callback( void *userdata, uint64_t t, uint64_t tend, struct SpdifChannelStatus *status )
{
    spdifAnalyzer   *sba = (spdifAnalyzer *)userdata;
    sba->status_callback(t,tend,status);
}

};

spdifAnalyzer::spdifAnalyzer()
:	Analyzer2(),  
	mSettings( new spdifAnalyzerSettings() ),
	mSimulationInitilized( false )
{
    struct SpdifBitstreamCallbacks  cb;

    cb.userdata = this;
    cb.cb_sample = c_sample_callback;
    cb.cb_status = c_status_callback;

    mSba = SpdifBitstreamAnalyzer_Create(&cb);

	SetAnalyzerSettings( mSettings.get() );
}

spdifAnalyzer::~spdifAnalyzer()
{
	KillThread();
    SpdifBitstreamAnalyzer_Delete( mSba );
}

void spdifAnalyzer::SetupResults()
{
    mResults.reset( new spdifAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mInputChannel );
}

void spdifAnalyzer::WorkerThread()
{
	mSampleRateHz = GetSampleRate();

	mSerial = GetAnalyzerChannelData( mSettings->mInputChannel );

	U64 prev_edge = mSerial->GetSampleNumber();

    mPrevSample = mPrevStatus = prev_edge;
    mPrevSampleEnd = mPrevStatusEnd = prev_edge;
    mSamplesSinceLastBSync = 0;

    SpdifBitstreamAnalyzer_Reset(mSba);

	for( ; ; )
	{
		mSerial->AdvanceToNextEdge();

		U64 cur_edge = mSerial->GetSampleNumber();

		SpdifBitstreamAnalyzer_AddEdge( mSba, cur_edge - prev_edge, mSerial->GetBitState() == BIT_HIGH );

        prev_edge = cur_edge;
	}
}

bool spdifAnalyzer::NeedsRerun()
{
	return false;
}

U32 spdifAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 spdifAnalyzer::GetMinimumSampleRateHz()
{
	return /* mSettings->mBitRate */ 1000000 * 4;
}

const char* spdifAnalyzer::GetAnalyzerName() const
{
	return "SPDIF";
}

const char* GetAnalyzerName()
{
	return "SPDIF";
}

Analyzer* CreateAnalyzer()
{
	return new spdifAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}

void spdifAnalyzer::sample_callback( uint64_t t, uint64_t tend, enum SpdifFrameType ft, uint32_t aud_sample )
{
    //let's put a dot exactly where we sample this bit:
    if ( (mPrevSampleEnd != t) && (mPrevSampleEnd != 0) ) {
        Frame eframe;
        eframe.mData1 = t-mPrevSampleEnd;
        eframe.mData2 = 0;
        eframe.mFlags = DISPLAY_AS_ERROR_FLAG;    /* gap marker */
        eframe.mStartingSampleInclusive = mPrevSampleEnd+1;
        eframe.mEndingSampleInclusive = t-1;
        eframe.mType = sft_invalid;
        mResults->AddFrame( eframe );

        mResults->AddMarker( mPrevSampleEnd, AnalyzerResults::ErrorX, mSettings->mInputChannel );
    }

    Frame frame;

    mSamplesSinceLastBSync++;

    if ( sft_B == ft ) {
        if ( 384 == mSamplesSinceLastBSync ) {
            mResults->AddMarker( t, AnalyzerResults::Dot, mSettings->mInputChannel );
        } else {
            mResults->AddMarker( t, AnalyzerResults::ErrorDot, mSettings->mInputChannel );
        }
        mSamplesSinceLastBSync = 0;
    }

    frame.mData1 = ((int)aud_sample<<4) >> 16;   /* signed 16-bit audio sample */

	/* special sequence for embedded AC3 data */
	if ((0xf872 == m_PrevPCM) && (0x4e1f == frame.mData1)) {
		/* this looks like an AC3 stream */
		mResults->AddMarker(mPrevSample, AnalyzerResults::UpArrow, mSettings->mInputChannel);
		m_AC3_Detected++;
	}
	m_PrevPCM = (U16)frame.mData1;

	frame.mData2 = aud_sample;                   /* raw SPDIF */
    frame.mFlags = 0;
    frame.mStartingSampleInclusive = t+1;
    frame.mEndingSampleInclusive = tend;
    frame.mType = ft;
    mResults->AddFrame( frame );

    mPrevSample = t;
    mPrevSampleEnd = tend;
    mResults->CommitResults();
    ReportProgress( t );
}

void spdifAnalyzer::status_callback( uint64_t t, uint64_t tend, struct SpdifChannelStatus *status )
{
    // we data to save
    if ( mPrevStatus ) {
        mResults->CommitPacketAndStartNewPacket();
        mResults->CommitResults();
    }
    mPrevStatus = t;
    mPrevStatusEnd = tend;
}
