#ifndef SPDIF_ANALYZER_H
#define SPDIF_ANALYZER_H
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

#include <Analyzer.h>
#include "spdifAnalyzerResults.h"
#include "spdifSimulationDataGenerator.h"

extern "C" {
#include <stdint.h>
#include "spdif.h"
#include "wavhdr.h"
};

class spdifAnalyzerSettings;
class ANALYZER_EXPORT spdifAnalyzer : public Analyzer2
{
public:
	spdifAnalyzer();
	virtual ~spdifAnalyzer();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();
    virtual void SetupResults();

    /* callbacks from the "C" bitstream analyzer library */
    void sample_callback( uint64_t t, uint64_t tend, enum SpdifFrameType ft, uint32_t aud_sample );
    void status_callback( uint64_t t, uint64_t tend, struct SpdifChannelStatus *status );

protected: //vars
	std::auto_ptr< spdifAnalyzerSettings > mSettings;
	std::auto_ptr< spdifAnalyzerResults > mResults;
	AnalyzerChannelData* mSerial;

	spdifSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

	//Serial analysis vars:
	U32 mSampleRateHz;
	U32 mStartOfStopBitOffset;
	U32 mEndOfStopBitOffset;

	U16 m_PrevPCM;
	U16 m_Pad0;
	U32 m_AC3_Detected;	/* number of times AC3 frame headers have been noticed */

    /* "C" bitstream parser library */
    struct SpdifBitstreamAnalyzer *mSba;
    uint64_t                       mSamplesSinceLastBSync;
    uint64_t                       mPrevSample;
    uint64_t                       mPrevSampleEnd;
    uint64_t                       mPrevStatus;
    uint64_t                       mPrevStatusEnd;
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //SPDIF_ANALYZER_H
