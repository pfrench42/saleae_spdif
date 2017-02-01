#ifndef SPDIF_SIMULATION_DATA_GENERATOR
#define SPDIF_SIMULATION_DATA_GENERATOR
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

#include <SimulationChannelDescriptor.h>
#include <string>
class spdifAnalyzerSettings;

class spdifSimulationDataGenerator
{
public:
	spdifSimulationDataGenerator();
	~spdifSimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, spdifAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

protected:
	spdifAnalyzerSettings* mSettings;
	U32 mSimulationSampleRateHz;

protected:
	void CreateSerialByte();
	std::string mSerialText;
	U32 mStringIndex;

	SimulationChannelDescriptor mSerialSimulationData;

};
#endif //SPDIF_SIMULATION_DATA_GENERATOR