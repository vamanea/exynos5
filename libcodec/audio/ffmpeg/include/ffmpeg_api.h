/*
 * @file      ffmpeg_api.h
 * @brief
 * @author    Sungyeon Kim (sy85.kim@samsung.com)
 * @version   1.0.0
 * @history
 *   2012.09.07 : Create
 */

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"

#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

typedef struct _FFmpeg {
    AVCodec *codec;
    AVCodecContext *pCodecCtx;
    int isCodecOpened;
} FFmpeg;

void FFmpeg_Init(FFmpeg *ffmpeg);
int FFmpeg_CodecOpen(FFmpeg *ffmpeg, int id, int bit_rate, void *extradata,
                     int extradata_size, int sample_rate, int channels, int block_size);
int FFmpeg_Decode(FFmpeg *ffmpeg, void *in_buffer, int *in_size, void *out_buffer, int *out_size);
void FFmpeg_DeInit(FFmpeg *ffmpeg);
