#ifndef WAVHDR_H
#define WAVHDR_H 1

#include <stdint.h>

struct WAVHeader
{
    char        wh_RIFF[4];     /* "RIFF" */

    uint32_t    wh_len;         /* total file size -8 */

    char        wh_WAVE[4];     /* "WAVE" */

    char        wh_fmt_[4];     /* "fmt " */

    uint32_t    wh_fmtlen;      /* format length (==16) */

    uint16_t    wh_format;      /* format specifier, 1==PCM */
    uint16_t    wh_chans;       /* number of channels, 2 */

    uint32_t    wh_samprate;    /* sample rate, 48000 */
    uint32_t    wh_bytespersec; /* bytes per second, */

    uint16_t    wh_bytespersmp; /* bytes per sample */
    uint16_t    wh_bitsperchan; /* bits per channel */

    char        wh_data[4];     /* "data" */

    uint32_t    wh_dlen;        /* data length */
};

#endif /* WAVHDR_H */
