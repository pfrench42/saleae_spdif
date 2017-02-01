#ifndef SPDIF_ANALYZER_RESULTS
#define SPDIF_ANALYZER_RESULTS
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

#include <AnalyzerResults.h>

class spdifAnalyzer;
class spdifAnalyzerSettings;

class spdifAnalyzerResults : public AnalyzerResults
{
public:
	spdifAnalyzerResults( spdifAnalyzer* analyzer, spdifAnalyzerSettings* settings );
	virtual ~spdifAnalyzerResults();

	virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
	virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

	virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base );
	virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
	virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

protected: //functions

protected:  //vars
	spdifAnalyzerSettings* mSettings;
	spdifAnalyzer* mAnalyzer;
};

#endif //SPDIF_ANALYZER_RESULTS
