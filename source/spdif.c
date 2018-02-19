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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifdef GUI_DEBUGGER
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#endif

#include "spdif.h"
#include "wavhdr.h"

struct edge
{
    uint64_t    t;
    uint16_t    dt;
    uint16_t    bitval;
    uint16_t    flags;
};

struct SpdifBitstreamAnalyzer
{
    struct SpdifBitstreamCallbacks  cb;
    struct WAVHeader     wh;

    FILE                *fout;  /* RAW output */
    FILE                *wout;  /* WAV output */
    uint32_t             nsamples_written;

    /* local analyzer looks at most recent edges */
    #define SPDIF_ANALYZER_MAX_EDGES    (1<<8)
    #define SPDIF_ANALYZER_SAMPLE_EDGES (1<<6)
    #define SPDIF_ANALYZER_EDGE_MASK    (SPDIF_ANALYZER_MAX_EDGES-1)
    struct edge         edge[SPDIF_ANALYZER_MAX_EDGES];
    uint64_t            w_edgenum;
    uint64_t            r_edgenum;
    uint64_t            n_syncs;
    uint64_t            sync_start_time;
    uint64_t            sync_end_time;
    uint64_t            prev_b_dt;
    uint64_t            prev_b_nsyncs;
    uint64_t            n_b_syncs;
    uint64_t            last_b_sync;
    uint64_t            last_b_time;

    uint16_t            last_threshold_12;
    uint16_t            last_threshold_23;

    /* sample for current word */
    unsigned char       sample[4];

#define CHANNEL_STATUS_NBITS    (384>>1)
#define CHANNEL_STATUS_NBYTES   (CHANNEL_STATUS_NBITS>>3)
    unsigned int        channel_status_left_bits;
    unsigned int        channel_status_right_bits;

    struct SpdifChannelStatus   cur_cs;
    struct SpdifChannelStatus   prev_cs;
};

static struct SpdifBitstreamAnalyzer   s_sba;

/** WARNING: Assumes 48.0 kHz Stereo, 16 bit */

#define _WH_SAMPLE_RATE         48000
#define _WH_CHANNELS            2
#define _WH_BYTES_PER_CHANNEL   2

void wh_Init(
    struct WAVHeader    *wh,
    uint32_t             nsamples )
{
    wh->wh_RIFF[0] = 'R';   wh->wh_RIFF[1] = 'I';
    wh->wh_RIFF[2] = 'F';   wh->wh_RIFF[3] = 'F';

    wh->wh_len = 
        (nsamples * (_WH_BYTES_PER_CHANNEL * _WH_CHANNELS)) 
        + sizeof(struct WAVHeader) - 8;

    wh->wh_WAVE[0] = 'W';   wh->wh_WAVE[1] = 'A';
    wh->wh_WAVE[2] = 'V';   wh->wh_WAVE[3] = 'E';

    wh->wh_fmt_[0] = 'f';   wh->wh_fmt_[1] = 'm';
    wh->wh_fmt_[2] = 't';   wh->wh_fmt_[3] = ' ';

    wh->wh_fmtlen = 16;

    wh->wh_format = 1;      /* PCM */
    wh->wh_chans = 2;       /* stereo */

    wh->wh_samprate = _WH_SAMPLE_RATE;

    wh->wh_bytespersec = _WH_SAMPLE_RATE * _WH_CHANNELS * _WH_BYTES_PER_CHANNEL;

    wh->wh_bytespersmp = _WH_CHANNELS * _WH_BYTES_PER_CHANNEL;
    wh->wh_bitsperchan = _WH_BYTES_PER_CHANNEL * 8;

    wh->wh_data[0] = 'd';   wh->wh_data[1] = 'a';
    wh->wh_data[2] = 't';   wh->wh_data[3] = 'a';

    wh->wh_dlen = (nsamples * (_WH_BYTES_PER_CHANNEL * _WH_CHANNELS));
}

static enum SpdifFrameType sba_FindSync(
    struct SpdifBitstreamAnalyzer   *sba,
    uint16_t                         threshold_12,
    uint16_t                         threshold_23 )
{
    enum SpdifFrameType found_sync = sft_invalid;
    unsigned int    i,nbits;

    i = (unsigned int) sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK;

    for ( nbits = 0; nbits < SPDIF_ANALYZER_SAMPLE_EDGES; nbits++,i++ )
    {
        /* http://www.epanorama.net/documents/audio/spdif.html
         *  There are 3 different sync-patterns, but they can appear in
         *  different forms, depending on the last cell of the previous 32-bit word (parity):
         * 
         *    Preamble    cell-order         cell-order
         *        (last cell "0")    (last cell "1")
         *    ----------------------------------------------
         *    "B"         11101000           00010111
         *    "M"         11100010           00011101
         *    "W"         11100100           00011011
        */

        /* start of 111 or 000 code? */
        if ( sba->edge[i & SPDIF_ANALYZER_EDGE_MASK].dt > threshold_23 )
        {
            if ( sba->edge[(i+1) & SPDIF_ANALYZER_EDGE_MASK].dt > threshold_23 )
            {
                /* 111.000 */
                /* check for  10 */
                if ( (sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK ].dt < threshold_12) &&
                     (sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK ].dt < threshold_12) )
                {
                    //printf("m");
                    found_sync = sft_M; /* left  */
                }
                else
                {
                #ifdef SELF_TEST
                  printf("bad M sync @%d {%d,%d} [%d %d %d %d]\n",
                    sba->r_edgenum,
                    threshold_12,
                    threshold_23,
                    sba->edge[(i+0) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+1) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK].dt );
                #endif
                }
            }
            else if ( sba->edge[(i+1) & SPDIF_ANALYZER_EDGE_MASK].dt > threshold_12 )
            {
                /* 111.00 */
                /* check for  100 */
                if ( (sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK ].dt < threshold_12) &&
                     (sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK ].dt >= threshold_12) &&
                     (sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK ].dt <= threshold_23) )
                {
                    //printf("w");
                    found_sync = sft_W; /* right */
                }
                else
                {
                #ifdef SELF_TEST
                  printf("bad W sync @%d {%d,%d} [%d %d %d %d]\n",
                    sba->r_edgenum,
                    threshold_12,
                    threshold_23,
                    sba->edge[(i+0) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+1) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK].dt );
                #endif
                }
            }
            else
            {
                /* 111.0 */
                /* check for  1000 */
                if ( (sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK ].dt < threshold_12) &&
                     (sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK ].dt > threshold_23) )
                {
                    //printf("b");
                    found_sync = sft_B; /* left, B */
                }
                else
                {
                #ifdef SELF_TEST
                  printf("bad B sync @%d {%d,%d} [%d %d %d %d]\n",
                    sba->r_edgenum,
                    threshold_12,
                    threshold_23,
                    sba->edge[(i+0) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+1) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+2) & SPDIF_ANALYZER_EDGE_MASK].dt,
                    sba->edge[(i+3) & SPDIF_ANALYZER_EDGE_MASK].dt );
                #endif
                }
            }
        }

        if ( sft_invalid != found_sync )
        {
            break;
        }
    }

    /* move read pointer to either the beginning of sync or the end of the edges so far */
    sba->r_edgenum += nbits;

    return(found_sync);
}

static void sba_ReadSample(
    struct SpdifBitstreamAnalyzer   *sba,
    enum SpdifFrameType              sample_type,
    uint16_t                         threshold_12 )
{
    unsigned int    bitpos;
    unsigned char   bitmask,submask,valmask;

    /* we've already read the first four edges of the preamble 
     *
     * The Coding Format
     * 
     * The digital signal is coded using the 'biphase-mark-code' (BMC), 
     * which is a kind of phase-modulation. In this system, two zero-crossings 
     * of the signal mean a logical 1 and one zero-crossing means a logical 0.
     * 
     *                 _   _   _   _   _   _   _   _   _   _   _   _
     *                | | | | | | | | | | | | | | | | | | | | | | | |
     * clock   0 ___ _| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_
     * 
     *                 ___         _______     ___         ___
     *                |   |       |       |   |   |       |   |
     * data    0 ___ _|   |_______|       |___|   |_______|   |___
     * signal           1   0   0   1   1   0   1   0   0   1   0
     * 
     *                 _   ___     _   _   ___   _     ___   _
     * Biphase        | | |   |   | | | | |   | | |   |   | | |
     * Mark    0 ___  | | |   |   | | | | |   | | |   |   | | |
     * signal         | | |   |   | | | | |   | | |   |   | | |
     *               _| |_|   |___| |_| |_|   |_| |___|   |_| |___
     * 
     * cells           1 0 1 1 0 0 1 0 1 0 1 1 0 1 0 0 1 1 0 1 0 0
     * 
    */
    sba->r_edgenum += 4;

    for ( bitpos = 4; bitpos < 32; bitpos++ )
    {
        bitmask = 1 << (bitpos & 0x7);

        if ( sba->edge[sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK].dt < threshold_12 )    /* short edge */
        {
            /* toggling indicates a 1 */
            sba->sample[ (bitpos >> 3) ] |= bitmask;
            sba->r_edgenum += 2;
        }
        else
        {
            /* non-toggling indicates a 0 */
            sba->sample[ (bitpos >> 3) ] &= ~bitmask;
            sba->r_edgenum += 1;
        }
    }

    /*
     * Word and Block Formats
     * 
     * Every sample is transmitted as a 32-bit word (subframe). These bits are used as follows:
     * 
     *    bits           meaning
     *    ----------------------------------------------------------
     *    0-3            Preamble (see above; special structure)
     * 
     *    4-7            Auxillary-audio-databits
     * 
     *    8-27           Sample
     *   (A 24-bit sample can be used (using bits 4-27).
     *    A CD-player uses only 16 bits, so only bits
     *    13 (LSB) to 27 (MSB) are used. Bits 4-12 are
     *    set to 0).
     * 
     *    28             Validity
     *   (When this bit is set, the sample should not
     *    be used by the receiver. A CD-player uses
     *    the 'error-flag' to set this bit).
     * 
     *    29             Subcode-data
     * 
     *    30             Channel-status-information
     * 
     *    31             Parity (bit 0-3 are not included)
     */

    /* get bit 30 */
    bitmask = (sba->sample[3] >> 6) & 0x1;  /* 6 = channel status */
    submask = (sba->sample[3] >> 5) & 0x1;  /* 7 = parity, 5 = subcode, 4 = validity */
    valmask = (sba->sample[3] >> 4) & 0x1;  /* 7 = parity, 5 = subcode, 4 = validity */

    if ( sft_W == sample_type ) /* right channel */
    {
        sba->cur_cs.channel_status_right[ sba->channel_status_right_bits>>3 ] |=
            bitmask << (sba->channel_status_right_bits & 0x7);

        sba->cur_cs.subframe_right[ sba->channel_status_right_bits>>3 ] |=
            submask << (sba->channel_status_right_bits & 0x7);

        sba->cur_cs.validity_right[ sba->channel_status_right_bits>>3 ] |=
            valmask << (sba->channel_status_right_bits & 0x7);

        if ( ++sba->channel_status_right_bits >= CHANNEL_STATUS_NBITS )
        {
            //printf("ERR: no B-sync???\n");
            sba->channel_status_right_bits = 0;
        }
    }
    else    /* 2 or 3 */    /* left channel */
    {
        sba->cur_cs.channel_status_left[ sba->channel_status_left_bits>>3 ] |=
            bitmask << (sba->channel_status_left_bits & 0x7);

        sba->cur_cs.subframe_left[ sba->channel_status_left_bits>>3 ] |=
            submask << (sba->channel_status_left_bits & 0x7);

        sba->cur_cs.validity_left[ sba->channel_status_left_bits>>3 ] |=
            valmask << (sba->channel_status_left_bits & 0x7);

        if ( ++sba->channel_status_left_bits >= CHANNEL_STATUS_NBITS )
        {
            //printf("ERR: no B-sync???\n");
            sba->channel_status_left_bits = 0;
        }
    }
}

static uint16_t sba_AnalyzeRecentEdges(
    struct SpdifBitstreamAnalyzer   *sba,
    uint16_t                        *pthreshold_12,
    uint16_t                        *pthreshold_23 )
{
    uint64_t        w,r;
    uint16_t        min_dt = 0;
    uint16_t        max_dt = 0;
    uint16_t        mid_dt = 0;

    w = sba->w_edgenum;
    r = sba->r_edgenum;

    min_dt = max_dt = sba->edge[ r & SPDIF_ANALYZER_EDGE_MASK ].dt;

    r++;

    while ( (int)(w - r) > 0 )
    {
        if ( min_dt > sba->edge[ r & SPDIF_ANALYZER_EDGE_MASK ].dt )
        {
            min_dt = sba->edge[ r & SPDIF_ANALYZER_EDGE_MASK ].dt;
        }
        else if ( max_dt < sba->edge[ r & SPDIF_ANALYZER_EDGE_MASK ].dt )
        {
            max_dt = sba->edge[ r & SPDIF_ANALYZER_EDGE_MASK ].dt;
        }

        r++;
    }

    /*
     *  The clocks pulses are 1, 2, and 3 spdif clocks wide
     *
     *  We've just captured the smallest clock pulse and the largest
     *  which should correspond to the shortest duration 1-clock
     *  pulse and the longest duration 3-clock pulse.
     *
     *  This means the 2 clock width should center between min/max
     *
     *  |-min                              max-|
     *  111                                  333
     *                    222 - calculated
     *                     |
     *          t12                 t23
     *           |-------------------|     -> solution 1, split the difference
     *
     *                  t12 t23
     *                    |-|              -> solution 2, surround the center by 1 clk
     */

    if ( (max_dt - min_dt) > 9 )
    {
      /* same as above but higher precision */
      mid_dt = (min_dt + max_dt);
      *pthreshold_12 = ((min_dt << 1) + mid_dt) >> 2;
      *pthreshold_23 = ((max_dt << 1) + mid_dt) >> 2;
    }
    else
    {
      mid_dt = (min_dt + max_dt);
      *pthreshold_12 = (mid_dt - 2) >> 1;
      *pthreshold_23 = (mid_dt + 2) >> 1;
    }

    /* is there any room for a two-tick width? */
    if ( (int)(*pthreshold_23 - *pthreshold_12) <= 1 )
    {
        /* no sync */
        min_dt = 0;
    }

    return(min_dt);
}

/* -------------------------------------------------------------------------------------------- */
/* Public API */
/* -------------------------------------------------------------------------------------------- */

int SpdifBitstreamAnalyzer_AddEdge(
    struct SpdifBitstreamAnalyzer   *sba,
    uint16_t                         dt,
    uint16_t                         bitval )
{
    sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].dt = dt;
    sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].bitval = bitval;
    sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t = sba->edge[ (sba->w_edgenum-1) & SPDIF_ANALYZER_EDGE_MASK ].t + dt;

    //printf("edge: %d, %d, %ld, %04lx, %04lx\n", sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].dt, sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].bitval, sba->edge[ sba->w_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t, sba->r_edgenum, sba->w_edgenum  );

    sba->w_edgenum++; /* no need to mask on increment */


    /* more than two samples worth of data present */
    while ( (int)(sba->w_edgenum - sba->r_edgenum) >= (SPDIF_ANALYZER_SAMPLE_EDGES<<1) )
    {
        uint16_t                        threshold_12;
        uint16_t                        threshold_23;

//        printf("%ld,%ld\n", sba->w_edgenum, sba->r_edgenum );

        if ( sba_AnalyzeRecentEdges(sba, &threshold_12, &threshold_23 ) )
        {
            enum SpdifFrameType         synctype;
            int16_t                     dt12,dt23;

            dt12 = (int16_t) (threshold_12 - sba->last_threshold_12);
            if ( dt12 < 0 )     
                dt12 = -dt12;

            dt23 = (int16_t) (threshold_23 - sba->last_threshold_23);
            if ( dt23 < 0 )     
                dt23 = -dt23;

            if ( (dt12 > 1) || (dt23 > 1) )
            {
                #ifdef SELF_TEST
                printf("thresholds [%d..%d] -> [%d..%d]\n",
                    sba->last_threshold_12,
                    sba->last_threshold_23,
                    threshold_12,
                    threshold_23 );
                #endif
                sba->last_threshold_12 = threshold_12;
                sba->last_threshold_23 = threshold_23;
            }
            else
            {
                //threshold_12 = sba->last_threshold_12;
                //threshold_23 = sba->last_threshold_23;
            }

            if ( 0 != (synctype = sba_FindSync( sba, threshold_12, threshold_23 )) )
            {
                sba->n_syncs++;

                //printf("synctype :%x @ %ld [%d,%d] %04x, %04x\n",
                //    synctype, 
                //    sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t,
                //    threshold_12, threshold_23,
                //    sba->r_edgenum, sba->w_edgenum );

                //if  ( 187565 == sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t )
                //    printf("debug...\n");


                if ( sft_B == synctype )
                {
                    sba->n_b_syncs++;

                    if ( sba->n_b_syncs <= 1 ) {
                        printf("B:{%ld, [%d,%d,%d,%d] [%d..%d] [%d,%d,%d,%d]}\n", (unsigned int) sba->r_edgenum,
                            sba->edge[(sba->r_edgenum+0) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+1) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+2) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+3) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            threshold_12,
                            threshold_23,
                            sba->edge[(sba->r_edgenum+4) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+5) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+6) & SPDIF_ANALYZER_EDGE_MASK].dt,
                            sba->edge[(sba->r_edgenum+7) & SPDIF_ANALYZER_EDGE_MASK].dt );
                    }

                    if ( sba->n_b_syncs > 1 ) {
                        /* do the callbacks */
                        (*sba->cb.cb_status)(  sba->cb.userdata,
                            sba->last_b_time,       /* last_b_time is when this started */
                            sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t,
                            &sba->cur_cs );

                        sba->prev_b_nsyncs = sba->n_syncs - sba->last_b_sync;
                        sba->prev_b_dt = sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t - sba->last_b_time;
                    }

                    sba->last_b_sync = sba->n_syncs;
                    sba->last_b_time = sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t;

                    /* B frame, start re-capturing channnel status stuff */
                    sba->channel_status_left_bits = 0;
                    sba->channel_status_right_bits = 0;

                    memcpy( &sba->prev_cs, &sba->cur_cs, sizeof(sba->cur_cs));

                    memset( &sba->cur_cs, 0, sizeof(sba->cur_cs));
                }

                /* read the rest of the bits */
                sba->sync_start_time = sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t;

                sba_ReadSample( sba, synctype, threshold_12 );

                sba->sync_end_time = sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t;

                /* do the callback */
                (*sba->cb.cb_sample)( sba->cb.userdata,
                    sba->sync_start_time,
                    sba->sync_end_time,
                    synctype,
                    ( ((unsigned int)sba->sample[0] <<  0) |
                      ((unsigned int)sba->sample[1] <<  8) |
                      ((unsigned int)sba->sample[2] << 16) |
                      ((unsigned int)sba->sample[3] << 24) ) );
            }
            else
            {
              #ifdef SELF_TEST
                static unsigned int skips = 0;
                if ( ++skips < 10 )
                {
                /* skip forward */
                printf("no sync [%2d..%2d], skipping @%ld...\n",threshold_12, threshold_23, (unsigned int) sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t);
                }
              #endif
                sba->r_edgenum += SPDIF_ANALYZER_SAMPLE_EDGES>>1;
            }
        }
        else
        {
            /* skip forward */
            printf("bad signal [%d,%d], skipping...\n",threshold_12, threshold_23 );
            sba->r_edgenum += SPDIF_ANALYZER_SAMPLE_EDGES>>1;
        }
    }

    return(0);
}

void SpdifBitstreamAnalyzer_Delete( struct SpdifBitstreamAnalyzer *sba )
{
    free ( sba );
}

void SpdifBitstreamAnalyzer_Reset( struct SpdifBitstreamAnalyzer *sba )
{
    struct SpdifBitstreamCallbacks cb;

    cb = sba->cb;
    memset(sba,0,sizeof(*sba));
    sba->cb = cb;
}

struct SpdifBitstreamAnalyzer *SpdifBitstreamAnalyzer_Create( 
    struct SpdifBitstreamCallbacks *callbacks )
{
    struct SpdifBitstreamAnalyzer   *sba;

    if ( NULL != (sba = (struct SpdifBitstreamAnalyzer *)calloc(1,sizeof(*sba))) )
    {
        /* copy the whole struct */
        sba->cb = *callbacks;
    }

    return(sba);
}

/* -------------------------------------------------------------------------------------------- */
/* Self-Test */
/* -------------------------------------------------------------------------------------------- */

#ifdef SELF_TEST

static void print_sample   ( void *userdata, uint64_t t, uint64_t tend, enum SpdifFrameType ft, uint32_t aud_sample )
{
    struct SpdifBitstreamAnalyzer   *sba = (struct SpdifBitstreamAnalyzer *)userdata;

    if ( NULL != sba->fout )
    {
        fwrite( sba->sample, sizeof(sba->sample), 1, sba->fout );
    }

    if ( NULL != sba->wout )
    {
        uint16_t        pcmval;

        /*
         *    8-27           Sample
         *   (A 24-bit sample can be used (using bits 4-27).
         *    A CD-player uses only 16 bits, so only bits
         *    13 (LSB) to 27 (MSB) are used. Bits 4-12 are
         *    set to 0).
         */
#if 1
        pcmval = (uint16_t)((aud_sample & 0x0ffff000) >> 12);
#else
        pcmval = (((uint16_t)sba->sample[3] & 0x0f) << 12) | 
                 (((uint16_t)sba->sample[2] & 0xff) << 4) | 
                 (((uint16_t)sba->sample[1] & 0xf0) >> 4);
#endif

        if ( sft_W == ft ) { /* right channel */
            if ( 0 == (1 & sba->nsamples_written) ) { /* missing sample ? */
                fwrite( &pcmval, sizeof(pcmval), 1, sba->wout );
                sba->nsamples_written++;
            }
        } else {
            if ( 1 == (1 & sba->nsamples_written) ) { /* missing sample ? */
                fwrite( &pcmval, sizeof(pcmval), 1, sba->wout );
                sba->nsamples_written++;
            }
        }

        fwrite( &pcmval, sizeof(pcmval), 1, sba->wout );
        sba->nsamples_written++;
    }
}

static void print_chstatus ( void *userdata, uint64_t t, uint64_t tend, const unsigned char *chstatus, const unsigned char *chstatus_r, int error_present )
{
    unsigned int    cs_byte;

    struct SpdifBitstreamAnalyzer   *sba = (struct SpdifBitstreamAnalyzer *)userdata;
    if ( sba->n_b_syncs > 1 ) {
        printf("@%12lu->%12lu B[%2ld,%2ld,", t, tend,
            sba->n_syncs - sba->last_b_sync - sba->prev_b_nsyncs,
            (sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t - sba->last_b_time - sba->prev_b_dt) );
    } else {
        printf("@%12lu->%12lu B[%2ld,%2ld,", t, tend,
            sba->n_syncs - sba->last_b_sync,
            sba->edge[ sba->r_edgenum & SPDIF_ANALYZER_EDGE_MASK ].t - sba->last_b_time );
    }

    printf("%02x%02x%02x%02x] [",
        sba->sample[0],
        sba->sample[1],
        sba->sample[2],
        sba->sample[3] );

    for ( cs_byte = 0; cs_byte < CHANNEL_STATUS_NBYTES; cs_byte++ )
    {
        printf("%02x", chstatus[cs_byte] );
    }
}

static void print_subframe ( void *userdata, uint64_t t, const unsigned char *subframe, const unsigned char *subframe_r, int error_present )
{
    #define PRINT_SUBFRAME
    #ifdef PRINT_SUBFRAME
//    struct SpdifBitstreamAnalyzer   *sba = (struct SpdifBitstreamAnalyzer *)userdata;
    unsigned int    cs_byte;
    printf("] [");
    for ( cs_byte = 0; cs_byte < CHANNEL_STATUS_NBYTES; cs_byte++ )
    {
        unsigned char        ch;

        ch = subframe[cs_byte];

        printf("%02x", ch );
    }
    #endif
}

static void print_validity ( void *userdata, uint64_t t, const unsigned char *validity, const unsigned char *validity_r, int error_present )
{
    #define PRINT_VALIDITY
    #ifdef PRINT_VALIDITY
//    struct SpdifBitstreamAnalyzer   *sba = (struct SpdifBitstreamAnalyzer *)userdata;
    unsigned int    cs_byte;
    printf("] [");
    for ( cs_byte = 0; cs_byte < CHANNEL_STATUS_NBYTES; cs_byte++ )
    {
        unsigned char        ch;

        ch = validity[cs_byte];
      #if 1
        if ( 0 == ch )          ch = '-';
        else if ( 0xff == ch )  ch = '!';
        else                    ch = '#';
        printf("%c", ch );
      #else
        printf("%02x", ch );
      #endif
    }
    #endif
}

static void print_status ( void *userdata, uint64_t t, uint64_t tend, struct SpdifChannelStatus *status )
{
    struct SpdifBitstreamAnalyzer   *sba = (struct SpdifBitstreamAnalyzer *)userdata;
    unsigned int    cs_byte;

    print_chstatus(userdata,t,tend,status->channel_status_left,status->channel_status_right,0);
    print_subframe(userdata,t,status->subframe_left,status->subframe_right,0);
    print_validity(userdata,t,status->validity_left,status->validity_right,0);

    /* example Channelstatus: 000c00020000000.... */

    /*
     * Channelstatus and subcode information
     * 
     * In each block, 384 bits of channelstatus and subcode info are transmitted. The 
     * Channel-status bits are equal for both subframes, so actually only 192 useful
     * bits are transmitted:
     * 
     *    bit            meaning
     *    -------------------------------------------------------------
     *    0-3            controlbits:
     * 
     *   bit 0: 0 (is set to 1 during 4 channel transmission)
     *   bit 1: 0=Digital audio, 1=Non-audio   (reserved to be 0 on old S/PDIF specs)
     *   bit 2: copy-protection. Copying is allowed
     *  when this bit is set.
     *                   bit 3: is set when pre-emphasis is used.
     * 
     *    4-7            0 (reserved)
     * 
     *    9-15           catagory-code:
     * 
     *   0 = common 2-channel format
     *   1 = 2-channel CD-format
     *       (set by a CD-player when a subcode is
     *        transmitted)
     *                   2 = 2-channel PCM-encoder-decoder format
     * 
     *                   others are not used
     * 
     *    19-191         0 (reserved)
     * 
     * The subcode-bits can be used by the manufacturer at will. They are used
     * in blocks of 1176 bits before which a sync-word of 16 "0"-bits is transmitted
     */

    if ( sba->n_b_syncs > 1 ) {
        /* compare left/right channel status bits */
        if ( memcmp(sba->cur_cs.channel_status_left,
                    sba->cur_cs.channel_status_right,
                    sizeof(sba->cur_cs.channel_status_left)))
        {
            printf("] [%d..%d] [", sba->last_threshold_12, sba->last_threshold_23);
            for ( cs_byte = 0; cs_byte < CHANNEL_STATUS_NBYTES; cs_byte++ )
            {
                printf("%02x", sba->cur_cs.channel_status_right[cs_byte] );
            }
            printf("] err?\n");
        }
        else
        {
            printf("]\n");
        }
    }
}

int main ( int argc, char *argv[] )
{
    int         err = 0;
    uint64_t    last_t = 0;
    int         last_bitval=0;
    uint64_t    sample_num = 0;;
    struct SpdifBitstreamAnalyzer   *sba = &s_sba;

    /* set callbacks */
    sba->cb.userdata = sba;
    sba->cb.cb_sample = print_sample;
    sba->cb.cb_status = print_status;

    if ( argc > 1 )
    {
        if ( NULL != (sba->fout = fopen(argv[1],"w")) )
        {
            printf("opened \"%s\" for output\n", argv[1] );
        }
    }

    if ( argc > 2 )
    {
        wh_Init( &sba->wh, 0 );
        if ( NULL != (sba->wout = fopen(argv[2],"w")) )
        {
            fwrite( &sba->wh, sizeof(sba->wh), 1, sba->wout );

            printf("opened \"%s\" for output\n", argv[2] );
        }
    }


#ifdef GUI_DEBUGGER
    {
 // programmatically redirect stdio
    assert( dup2(open("test.csv" ,O_RDONLY),0) != -1 );
    }
#endif

    while ( ! feof(stdin) )
    {
        uint64_t    now;
        int         bitval;

        scanf("%lu, %d",&now,&bitval);
        sample_num++;

        /* only record changes */
        if ( bitval != last_bitval )
        {
            /* push into analysis */
            SpdifBitstreamAnalyzer_AddEdge(sba,now-last_t,bitval);
            last_t = now;
            last_bitval = bitval;
        }
    }

    printf("DONE: Read %ld samples\n", sample_num );

    if ( NULL != sba->wout )
    {
        wh_Init(&sba->wh,sba->nsamples_written>>1);
        rewind( sba->wout );
        fwrite( &sba->wh, sizeof(sba->wh), 1, sba->wout );

        fclose( sba->wout );
        sba->wout = NULL;
    }

    if ( NULL != sba->fout )
    {
        fclose( sba->fout );
        sba->fout = NULL;
    }

    return(err);
}

#endif /* SELF_TEST */
