#ifndef SPDIF_BITSTREAM_ANALYZER_H
#define SPDIF_BITSTREAM_ANALYZER_H 1
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

enum SpdifFrameType {
    sft_invalid,
    sft_B,
    sft_M,
    sft_W
};

#define CHANNEL_STATUS_NBITS    (384>>1)
#define CHANNEL_STATUS_NBYTES   (CHANNEL_STATUS_NBITS>>3)

struct SpdifChannelStatus {
    unsigned char       validity_left[CHANNEL_STATUS_NBYTES];
    unsigned char       validity_right[CHANNEL_STATUS_NBYTES];
    unsigned char       subframe_left[CHANNEL_STATUS_NBYTES];
    unsigned char       subframe_right[CHANNEL_STATUS_NBYTES];
    unsigned char       channel_status_left[CHANNEL_STATUS_NBYTES];
    unsigned char       channel_status_right[CHANNEL_STATUS_NBYTES];
};

struct SpdifBitstreamCallbacks
{
    void *userdata;
    void (*cb_sample)   ( void *userdata, uint64_t t, uint64_t tend, enum SpdifFrameType ft, uint32_t aud_sample );
    void (*cb_status)   ( void *userdata, uint64_t t, uint64_t tend, struct SpdifChannelStatus *status );
};

/* pre-declaration for the API */
struct SpdifBitstreamAnalyzer;
struct WAVHeader;

void wh_Init(
    struct WAVHeader    *wh,
    uint32_t             nsamples );

struct SpdifBitstreamAnalyzer *SpdifBitstreamAnalyzer_Create( 
    struct SpdifBitstreamCallbacks *callbacks );

void SpdifBitstreamAnalyzer_Reset( struct SpdifBitstreamAnalyzer *sba );

void SpdifBitstreamAnalyzer_Delete( struct SpdifBitstreamAnalyzer *sba );

int SpdifBitstreamAnalyzer_AddEdge(
    struct SpdifBitstreamAnalyzer   *sba,
    uint16_t                         dt,
    uint16_t                         bitval );

#endif /* SPDIF_BITSTREAM_ANALYZER_H */
