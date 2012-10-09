/*
 * @file      ffmpeg_api.c
 * @brief
 * @author    Sungyeon Kim (sy85.kim@samsung.com)
 * @version   1.0.0
 * @history
 *   2012.09.07 : Create
 */

#include "ffmpeg_api.h"

#define LOG_NDEBUG 1
#define LOG_TAG "libffmpegapi"
#include <utils/Log.h>


void FFmpeg_Init(FFmpeg *ffmpeg)
{
    avcodec_init();
    avcodec_register_all();
    av_register_all();

    ffmpeg->isCodecOpened = 0;
}

int FFmpeg_CodecOpen(FFmpeg *ffmpeg, int id, int bit_rate, void *extradata,
                     int extradata_size, int sample_rate, int channels, int block_size)
{
    if (ffmpeg->isCodecOpened == 1) {
        ALOGE("codec already opened");
        return 0x90000004;
    }

    if (id == 0x160)
        ffmpeg->codec = avcodec_find_decoder(CODEC_ID_WMAV1);
    else if (id == 0x161)
        ffmpeg->codec = avcodec_find_decoder(CODEC_ID_WMAV2);
    if (!ffmpeg->codec) {
        ALOGE("cannot find decoder in ffmpeg");
        return 0x90000004;
    }

    ffmpeg->pCodecCtx = avcodec_alloc_context2(AVMEDIA_TYPE_UNKNOWN);

    ffmpeg->pCodecCtx->bit_rate = bit_rate;
    ffmpeg->pCodecCtx->extradata = extradata;
    ffmpeg->pCodecCtx->extradata_size = extradata_size;
    ffmpeg->pCodecCtx->sample_rate = sample_rate;
    ffmpeg->pCodecCtx->channels = channels;
    ffmpeg->pCodecCtx->block_align = block_size;

    if (avcodec_open(ffmpeg->pCodecCtx, ffmpeg->codec) < 0) {
        ALOGE("codec open failed");
        return 0x90000004;
    } else {
        ALOGE("codec opened!!!");
    }

    ffmpeg->isCodecOpened = 1;
    return 0;
}

int FFmpeg_Decode(FFmpeg *ffmpeg, void *in_buffer, int *in_size, void *out_buffer, int *out_size)
{
    AVPacket avpkt;
    av_init_packet(&avpkt);

    avpkt.data = in_buffer;
    avpkt.size = *in_size;

    /* Read frame and decode */
    *in_size = avcodec_decode_audio3(ffmpeg->pCodecCtx, (short *)out_buffer, out_size, &avpkt);

    if (*in_size < 0) {
        ALOGE("Error in Decoding. Nothing was decoded.");
        ALOGE("decoding in %d out %d...  %02X %02X %02X %02X\n", *in_size, *out_size,
              *avpkt.data, *(avpkt.data+1),*(avpkt.data+2),*(avpkt.data+3));
        return 0x90000002;
    }

    if (*out_size > 0) {
        /* A frame has been decoded. out_buffer filled.*/
        ALOGV("%d bytes out", *out_size);
    } else {
        ALOGE("Maybe has a problem in Decoding. Did you filled buffer by A PERFECT UNIT OF FRAME?");
        return 0x90000009;
    }
    return 0;
}

void FFmpeg_DeInit(FFmpeg *ffmpeg)
{
    avcodec_close(ffmpeg->pCodecCtx);
    ffmpeg->isCodecOpened = 0;
}
