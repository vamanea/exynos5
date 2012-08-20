/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG_HDMI "libexynoshdmiutils"

#include <cutils/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <hardware/hardware.h>

#include "ExynosHdmiUtils.h"
#include "SecHdmiLog.h"

#if defined(BOARD_USES_HDMI_FIMGAPI)
#include "sec_g2d_4x.h"
#include "FimgApi.h"
#endif

#include "s3c_lcd.h"
#include "audio.h"
#include "video.h"
#include "libedid.h"
#include "libcec.h"
#include "SecHdmiCommon.h"

namespace android {

unsigned int output_type  = V4L2_OUTPUT_TYPE_HDMI_RGB;
unsigned int g_hdcp_en    = 0;

unsigned int ui_dst_memory[MAX_BUFFERS_MIXER];
unsigned int ui_src_memory[MAX_BUFFERS_GSCALER_CAP];
unsigned int ui_memory_size = 0;

int m_mxr_get_plane_size(
    unsigned int *plane_size,
    unsigned int  width,
    unsigned int  height,
    int           v4l_pixel_format)
{
    switch (v4l_pixel_format) {
    /* 1 plane */
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
        plane_size[0] = width * height * 4;
        break;
    case V4L2_PIX_FMT_RGB24:
        plane_size[0] = width * height * 3;
        break;
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
        plane_size[0] = width * height * 2;
        break;
    default:
        ALOGE("%s::unmatched v4l_pixel_format color_space(0x%x)\n",
             __func__, v4l_pixel_format);
        return -1;
        break;
    }

    return 0;
}

void *exynos_mxr_create(
    int dev_num,
    unsigned int layer_id)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    int i     = 0;
    int op_id = 0;
    char mutex_name[32];
    unsigned int total_sleep_time  = 0;

    struct media_device *media;
    struct media_entity *mxr_sd_entity;
    struct media_entity *mxr_vd_entity;
    struct media_link *links;
    char node[32];
    char devname[32];
    unsigned int cap = 0;

    struct MXR_HANDLE *mxr_handle = (struct MXR_HANDLE *)malloc(sizeof(struct MXR_HANDLE));

    if (mxr_handle == NULL) {
        ALOGE("%s::malloc(struct MXR_HANDLE) fail", __func__);
        goto mxr_output_err;
    }

    memset(mxr_handle, 0, sizeof(struct MXR_HANDLE));
    mxr_handle->mxr_fd    = -1;
    mxr_handle->layer_id  = layer_id;
    mxr_handle->buf_type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    mxr_handle->stream_on = false;
    mxr_handle->buf_idx   = 0;
    mxr_handle->qbuf_cnt  = 0;
    mxr_handle->op_mutex  = NULL;
    mxr_handle->out_mode  = V4L2_OUTPUT_TYPE_HDMI_RGB;

    srand(time(NULL));
    op_id = rand() % 1000000; // just make random id
    sprintf(mutex_name, "%sOp%d", LOG_TAG_HDMI, op_id);
    mxr_handle->op_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_PRIVATE, mutex_name);
    if (mxr_handle->op_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto mxr_output_err;
    }

    exynos_mutex_lock(mxr_handle->op_mutex);

    /* media0 */
    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    media = exynos_media_open(node);
    if (media == NULL) {
        ALOGE("%s::exynos_media_open failed (node=%s)", __func__, node);
        goto mxr_output_err;
    }
    mxr_handle->media = media;

    /* get Mixer video dev & sub dev entity by name*/
    sprintf(devname, PFX_MXR_VIDEODEV_ENTITY, dev_num, layer_id);
    mxr_vd_entity = exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!mxr_vd_entity) {
        ALOGE("%s:: failed to get the mixer vd entity", __func__);
        goto mxr_output_err;
    }
    mxr_handle->mxr_vd_entity = mxr_vd_entity;

    sprintf(devname, PFX_MXR_SUBDEV_ENTITY, dev_num);
    mxr_sd_entity = exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!mxr_sd_entity) {
        ALOGE("%s:: failed to get the mxr sd entity", __func__);
        goto mxr_output_err;
    }
    mxr_handle->mxr_sd_entity = mxr_sd_entity;

    /* mxr video-dev open */
    sprintf(devname, PFX_MXR_VIDEODEV_ENTITY, dev_num, layer_id);
    mxr_vd_entity->fd = exynos_v4l2_open_devname(devname, O_RDWR);
    if (mxr_vd_entity->fd < 0) {
        ALOGE("%s: mxr video-dev open fail", __func__);
        goto mxr_output_err;
    }

    /* mxr sub-dev open */
    sprintf(devname, PFX_MXR_SUBDEV_ENTITY, dev_num);
    mxr_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if (mxr_sd_entity->fd < 0) {
        ALOGE("%s: mxr sub-dev open fail", __func__);
        goto mxr_output_err;
    }

    /* setup link : Mixer : video device --> sub device */
    for (i = 0; i < (int) mxr_vd_entity->num_links; i++) {
        links = &mxr_vd_entity->links[i];

        if (links == NULL ||
            links->source->entity != mxr_vd_entity ||
            links->sink->entity   != mxr_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media, links->source, links->sink,
                    MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                __func__, links->source->entity->info.id, links->sink->entity->info.id);
            goto mxr_output_err;
        }
    }

    cap = V4L2_CAP_STREAMING |
          V4L2_CAP_VIDEO_OUTPUT_MPLANE;

    if (exynos_v4l2_querycap(mxr_vd_entity->fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        goto mxr_output_err;
    }

    return (void *)mxr_handle;

mxr_output_err:
    if (mxr_handle) {
        exynos_mxr_destroy(mxr_handle);

        if (mxr_handle->op_mutex)
            exynos_mutex_unlock(mxr_handle->op_mutex);

        if (exynos_mutex_destroy(mxr_handle->op_mutex) == false)
            ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

        free(mxr_handle);
    }

    return NULL;
}

void exynos_mxr_destroy(
    void *handle)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    int i = 0;
    struct media_link * links;
    struct MXR_HANDLE *mxr_handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return;
    }

    mxr_handle = (struct MXR_HANDLE *)handle;

    exynos_mutex_lock(mxr_handle->op_mutex);

    if (mxr_handle == NULL) {
        ALOGE("%s::mxr_handle is NULL", __func__);
        return;
    }

    if (mxr_handle->stream_on == true) {
        if (exynos_mxr_stop_n_clear(mxr_handle) < 0)
            ALOGE("%s::exynos_mxr_out_stop() fail", __func__);

        mxr_handle->stream_on = false;
    }

    if (mxr_handle->media &&
        mxr_handle->mxr_sd_entity &&
        mxr_handle->mxr_vd_entity) {

        /* unlink : Mixer : video device --> sub device */
        for (i = 0; i < (int) mxr_handle->mxr_vd_entity->num_links; i++) {
            links = &mxr_handle->mxr_vd_entity->links[i];

            if (links == NULL ||
                links->source->entity != mxr_handle->mxr_vd_entity ||
                links->sink->entity   != mxr_handle->mxr_sd_entity) {
                continue;
            } else if (exynos_media_setup_link(mxr_handle->media, links->source, links->sink, 0) < 0) {
                ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                    __func__, links->source->entity->info.id, links->sink->entity->info.id);
                return;
            }
        }
    }

    if (mxr_handle->mxr_vd_entity && mxr_handle->mxr_vd_entity->fd > 0) {
        close(mxr_handle->mxr_vd_entity->fd);
        mxr_handle->mxr_vd_entity->fd = -1;
    }

    if (mxr_handle->mxr_sd_entity && mxr_handle->mxr_sd_entity->fd > 0) {
        close(mxr_handle->mxr_sd_entity->fd);
        mxr_handle->mxr_sd_entity->fd = -1;
    }

    if (mxr_handle->media)
        exynos_media_close(mxr_handle->media);

    mxr_handle->media = NULL;
    mxr_handle->mxr_sd_entity = NULL;
    mxr_handle->mxr_vd_entity = NULL;

    exynos_mutex_unlock(mxr_handle->op_mutex);

    if (exynos_mutex_destroy(mxr_handle->op_mutex) == false)
        ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

    if (mxr_handle)
        free(mxr_handle);

    return;
}

int exynos_mxr_config(
    void *handle,
    exynos_mxr_img *src_img,
    exynos_mxr_img *dst_img,
    int out_mode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE  *mxr_handle;
    struct v4l2_format  fmt;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_subdev_format  sd_fmt;
    struct v4l2_subdev_crop    sd_crop;
    int i;

    struct v4l2_rect dst_rect;
    int32_t      src_color_space;
    int32_t      dst_color_space;
    int32_t      src_planes = 1;
    int          ctrl_id = 1;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    mxr_handle = (struct MXR_HANDLE *)handle;

    if (mxr_handle->stream_on != false) {
        ALOGE("Error: Src is already streamed on !!!!");
        return -1;
     }

    memcpy(&mxr_handle->src_img, src_img, sizeof(exynos_mxr_img));
    memcpy(&mxr_handle->dst_img, dst_img, sizeof(exynos_mxr_img));

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    dst_color_space = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
    mxr_handle->out_mode = out_mode;

    switch (mxr_handle->out_mode) {
    case V4L2_OUTPUT_TYPE_DIGITAL:
        dst_color_space = V4L2_MBUS_FMT_YUV8_1X24;
        break;
    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        dst_color_space = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
        break;
    case V4L2_OUTPUT_TYPE_DVI:
        dst_color_space = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced v4l2_output_type(%d)", __func__, mxr_handle->out_mode);
        break;
    }

    /* set src format  :Mixer video dev*/
    fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width       = mxr_handle->src_img.fw;
    fmt.fmt.pix_mp.height      = mxr_handle->src_img.fh;
    fmt.fmt.pix_mp.pixelformat = src_color_space;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.num_planes  = src_planes;

    if (exynos_v4l2_s_fmt(mxr_handle->mxr_vd_entity->fd, &fmt) < 0) {
            ALOGE("%s::videodev set format failed", __func__);
            return -1;
    }

    /* set src crop info :Mixer video dev*/
    crop.type     = fmt.type;
    crop.c.left   = mxr_handle->src_img.x;
    crop.c.top    = mxr_handle->src_img.y;
    crop.c.width  = mxr_handle->src_img.w;
    crop.c.height = mxr_handle->src_img.h;

    if (exynos_v4l2_s_crop(mxr_handle->mxr_vd_entity->fd, &crop) < 0) {
        ALOGE("%s::videodev set crop failed", __func__);
        return -1;
    }

    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::fmt.fmt.pix_mp.pixelformat=0x%08x, pix_mp.field=%d\r\n"
            "    pix_mp.width=%d, pix_mp.height=%d, pix_mp.num_planes=%d\r\n"
            "    crop.c.left=%d, c.top=%d, c.width=%d, c.height=%d",
            __func__, fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.field,
            fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, fmt.fmt.pix_mp.num_planes,
            crop.c.left, crop.c.top, crop.c.width, crop.c.height);

    /* set format: sink pad of Mixer sub-dev*/
    switch(mxr_handle->layer_id) {
    case 0:
        sd_fmt.pad = MIXER_G0_SUBDEV_PAD_SINK;
        break;
    case 1:
        sd_fmt.pad = MIXER_G1_SUBDEV_PAD_SINK;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced layer_id(%d)", __func__, mxr_handle->layer_id);
        break;
    }

    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.width  = mxr_handle->src_img.fw;
    sd_fmt.format.height = mxr_handle->src_img.fh;
    sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;

    if (exynos_subdev_s_fmt(mxr_handle->mxr_sd_entity->fd, &sd_fmt) < 0) {
        ALOGE("%s::Mixer subdev set format failed (PAD=%d)", __func__, sd_fmt.pad);
        return -1;
    }

    /* set crop: sink crop of Mixer sub-dev*/
    sd_crop.pad   = sd_fmt.pad;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_crop.rect.left   = mxr_handle->src_img.x;
    sd_crop.rect.top    = mxr_handle->src_img.y;
    sd_crop.rect.width  = mxr_handle->src_img.w;
    sd_crop.rect.height = mxr_handle->src_img.h;

    if (exynos_subdev_s_crop(mxr_handle->mxr_sd_entity->fd, &sd_crop) < 0) {
        ALOGE("%s::Mixer subdev set crop failed (PAD=%d)", __func__, sd_crop.pad);
        return -1;
    }

    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::sd_fmt.pad=%d\r\n"
            "    sd_fmt.format.width=%d, format.height=%d\r\n"
            "    sd_crop.rect.left=%d, rect.top=%d, rect.width=%d, rect.height=%d",
            __func__, sd_fmt.pad,
            sd_fmt.format.width, sd_fmt.format.height,
            sd_crop.rect.left, sd_crop.rect.top, sd_crop.rect.width, sd_crop.rect.height);

    /* set format: src pad of Mixer sub-dev*/
    switch(mxr_handle->layer_id) {
    case 0:
        sd_fmt.pad = MIXER_G0_SUBDEV_PAD_SOURCE;
        break;
    case 1:
        sd_fmt.pad = MIXER_G1_SUBDEV_PAD_SOURCE;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced layer_id(%d)", __func__, mxr_handle->layer_id);
        break;
    }
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.width  = mxr_handle->dst_img.fw;
    sd_fmt.format.height = mxr_handle->dst_img.fh;
    sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE; // FIX ME : get from common_output_type // carrotsm

    if (exynos_subdev_s_fmt(mxr_handle->mxr_sd_entity->fd, &sd_fmt) < 0) {
        ALOGE("%s::Mixer subdev set format failed (PAD=%d)", __func__, sd_fmt.pad);
        return -1;
    }

    /* set crop: src crop of Mixer sub-dev*/
    sd_crop.pad   = sd_fmt.pad;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_crop.rect.left   = mxr_handle->dst_img.x;
    sd_crop.rect.top    = mxr_handle->dst_img.y;
    sd_crop.rect.width  = mxr_handle->dst_img.w;
    sd_crop.rect.height = mxr_handle->dst_img.h;

    if (exynos_subdev_s_crop(mxr_handle->mxr_sd_entity->fd, &sd_crop) < 0) {
        ALOGE("%s::Mixer subdev set crop failed (PAD=%d)", __func__, sd_crop.pad);
        return -1;
    }

    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::sd_fmt.pad=%d\r\n"
            "    sd_fmt.format.width=%d, format.height=%d\r\n"
            "    sd_crop.rect.left=%d, rect.top=%d, rect.width=%d, rect.height=%d",
            __func__, sd_fmt.pad,
            sd_fmt.format.width, sd_fmt.format.height,
            sd_crop.rect.left, sd_crop.rect.top, sd_crop.rect.width, sd_crop.rect.height);

    /* set Mixer ctrls */
    if (exynos_v4l2_s_ctrl(mxr_handle->mxr_vd_entity->fd,
        V4L2_CID_TV_PIXEL_BLEND_ENABLE,
        mxr_handle->src_img.blending) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_TV_PIXEL_BLEND_ENABLE: %d) failed",
            __func__, mxr_handle->src_img.blending);
        return -1;
    }

    reqbuf.type   = fmt.type;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = MAX_BUFFERS_MIXER;

    if (exynos_v4l2_reqbufs(mxr_handle->mxr_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }

    return 0;
}

int exynos_mxr_run(void *handle,
    unsigned int yAddr)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;
    struct v4l2_plane  planes[NUM_OF_MXR_PLANES];
    struct v4l2_buffer buf;
    int32_t      src_color_space;
    int32_t      src_planes;
    unsigned int i = 0;
    unsigned int plane_size[NUM_OF_MXR_PLANES];

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    mxr_handle = (struct MXR_HANDLE *)handle;

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_MXR_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));
    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(mxr_handle->src_img.format);
    src_planes = 1;

    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.length   = src_planes;
    buf.index    = mxr_handle->buf_idx;
    buf.m.planes = planes;

    mxr_handle->src.addr[0] = (void *)yAddr;

    if (m_mxr_get_plane_size(plane_size, mxr_handle->src_img.fw,
                             mxr_handle->src_img.fh, src_color_space) < 0) {
        ALOGE("%s:m_gsc_get_plane_size:fail", __func__);
        return -1;
    }

    for (i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)mxr_handle->src.addr[i];
        buf.m.planes[i].length    = plane_size[i];
        buf.m.planes[i].bytesused = plane_size[i];
    }

    /* Queue the buf */
    if (exynos_v4l2_qbuf(mxr_handle->mxr_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::queue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            mxr_handle->buf_idx, MAX_BUFFERS_MIXER);
        return -1;
    }
    mxr_handle->buf_idx++;
    mxr_handle->qbuf_cnt++;

    HDMI_Log(HDMI_LOG_DEBUG, "mxr_handle->buf_idx=%d", mxr_handle->buf_idx);

    if (mxr_handle->stream_on == false) {
        /* stream on after queing the second buffer
           to do: below logic should be changed to handle the single frame videos */
#ifndef MXR_DELAYED_STREAMON
        if (mxr_handle->buf_idx == (MAX_BUFFERS_MIXER - 2))
#else
        if (mxr_handle->buf_idx == (MAX_BUFFERS_MIXER - 1))
#endif
        {
            if (exynos_v4l2_streamon(mxr_handle->mxr_vd_entity->fd, buf.type) < 0) {
                ALOGE("%s::stream on failed", __func__);
                return -1;
            }
            mxr_handle->stream_on = true;
        }
    }

    mxr_handle->buf_idx = mxr_handle->buf_idx % MAX_BUFFERS_MIXER;

    return 0;
}

int exynos_mxr_wait_done (
    void *handle)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;
    struct v4l2_plane  planes[NUM_OF_MXR_PLANES];
    struct v4l2_buffer buf;
    int32_t            src_planes;
    int                i;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }
    mxr_handle = (struct MXR_HANDLE *)handle;

    if (mxr_handle->qbuf_cnt < MAX_BUFFERS_MIXER)
        return 0;

    if (mxr_handle->stream_on == true) {
        src_planes = 1;

        memset(&buf, 0, sizeof(struct v4l2_buffer));
        for (i = 0; i < NUM_OF_MXR_PLANES; i++)
            memset(&planes[i], 0, sizeof(struct v4l2_plane));

        buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory   = V4L2_MEMORY_USERPTR;
        buf.length   = src_planes;
        buf.m.planes = planes;

        /* DeQueue a buf */
        if (exynos_v4l2_dqbuf(mxr_handle->mxr_vd_entity->fd, &buf) < 0) {
            ALOGE("%s::dequeue buffer failed (index=%d)(mSrcBufNum=%d)",
                    __func__, mxr_handle->buf_idx, MAX_BUFFERS_MIXER);
            return -1;
        }
    }

     return 0;
}

int exynos_mxr_stop_n_clear(
    void *handle)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;
    struct v4l2_requestbuffers reqbuf;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    mxr_handle = (struct MXR_HANDLE *)handle;

    if (mxr_handle->stream_on == false) {
        /* to handle special scenario.*/
        mxr_handle->buf_idx  = 0;
        mxr_handle->qbuf_cnt = 0;
        ALOGD("%s::Mixer is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }

    mxr_handle->stream_on = false;
    mxr_handle->buf_idx   = 0;
    mxr_handle->qbuf_cnt  = 0;

    if (exynos_v4l2_streamoff(mxr_handle->mxr_vd_entity->fd,
                              V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }

SKIP_STREAMOFF:
    /* Clear Buffer */
    /* TODO : support for other buffer type & memory */
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = 0;

    if (exynos_v4l2_reqbufs(mxr_handle->mxr_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }

    return 0;
}

int exynos_mxr_just_stop (
    void *handle)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    mxr_handle = (struct MXR_HANDLE *)handle;

    if (mxr_handle->stream_on == false) {
        /* to handle special scenario.*/
        mxr_handle->buf_idx = 0;
        mxr_handle->qbuf_cnt = 0;
        ALOGD("%s::GSC is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }

    mxr_handle->stream_on = false;
    mxr_handle->buf_idx   = 0;
    mxr_handle->qbuf_cnt  = 0;

    if (exynos_v4l2_streamoff(mxr_handle->mxr_vd_entity->fd,
                              V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }

SKIP_STREAMOFF:
    return 0;
}

int exynos_mxr_set_ctrl (
    void *handle,
    unsigned int id,
    int value)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;
    int ret = 0;

    mxr_handle = (struct MXR_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(mxr_handle->mxr_vd_entity->fd, id, value) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (id=%d: value=%d) failed", __func__, id, value);
        return -1;
    }

    return 0;
}

int exynos_mxr_get_ctrl (
    void *handle,
    unsigned int id,
    int *value)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle;
    int ret = 0;

    mxr_handle = (struct MXR_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (exynos_v4l2_g_ctrl(mxr_handle->mxr_vd_entity->fd, id, value) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (id=%d: value=%d) failed", __func__, id, *value);
        return -1;
    }

    return (*value);
}

void display_menu(void)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;

    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    ALOGI("=============== HDMI Audio  =============\n");

    if (EDIDAudioModeSupport(&audio))
        ALOGI("=  2CH_PCM 44100Hz audio supported      =\n");

    ALOGI("========= HDMI Mode & Color Space =======\n");

    video.mode = HDMI;
    if (EDIDHDMIModeSupport(&video)) {
        video.colorSpace = HDMI_CS_YCBCR444;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  1. HDMI(YCbCr)                       =\n");

        video.colorSpace = HDMI_CS_RGB;
        if (EDIDColorSpaceSupport(&video))
            ALOGI("=  2. HDMI(RGB)                         =\n");
    } else {
        video.mode = DVI;
        if (EDIDHDMIModeSupport(&video))
            ALOGI("=  3. DVI                               =\n");
    }

    ALOGI("=========  HDMI Rseolution [2D]  ========\n");

    /* 480P */
    video.resolution = v720x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  4. 480P_60_16_9                      =\n");

    video.resolution = v640x480p_60Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  5. 480P_60_4_3                       =\n");

    /* 576P */
    video.resolution = v720x576p_50Hz;
    video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  6. 576P_50_16_9                      =\n");

    video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  7. 576P_50_4_3                       =\n");

    /* 720P 60 */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  8. 720P_60                           =\n");

    /* 720P_50 */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  9. 720P_50                           =\n");

    /* 1080P_60 */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  a. 1080P_60                          =\n");

    /* 1080P_50 */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  b. 1080P_50                          =\n");

    /* 1080I_60 */
    video.resolution = v1920x1080i_60Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  c. 1080I_60                          =\n");

    /* 1080I_50 */
    video.resolution = v1920x1080i_50Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  d. 1080I_50                          =\n");

    /* 1080P_30 */
    video.resolution = v1920x1080p_30Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  e. 1080P_30                          =\n");

    /* 1080P_24 */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  f. 1080P_24                          =\n");

    ALOGI("=========  HDMI Rseolution [S3D]  =======\n");

    /* 720P_60_TB */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  10. 720P_60_TB                       =\n");

    /* 720P_50_TB */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  11. 720P_50_TB                       =\n");

    /* 1080P_60_TB */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  13. 1080P_60_TB                      =\n");

    /* 1080P_50_TB */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  14. 1080P_50_TB                      =\n");

    /* 1080P_24_TB */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  15. 1080P_24_TB                      =\n");

    /* 720P_60_SBS_HALF */
    video.resolution = v1280x720p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  16. 720P_60_SBS_HALF                 =\n");

    /* 720P_50_SBS_HALF */
    video.resolution = v1280x720p_50Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  17. 720P_50_SBS_HALF                 =\n");

    /* 1080P_60_SBS_HALF */
    video.resolution = v1920x1080p_60Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  18. 1080P_60_SBS_HALF                =\n");

    /* 1080P_50_SBS_HALF */
    video.resolution = v1920x1080p_50Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  19. 1080P_50_SBS_HALF                =\n");

    /* 1080P_24_SBS_HALF */
    video.resolution = v1920x1080p_24Hz;
    video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
    if (EDIDVideoResolutionSupport(&video))
        ALOGI("=  1a. 1080P_24_SBS_HALF                =\n");

    ALOGI("=========================================\n");
}

int tvout_init(int fd_tvout, __u32 preset_id)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::preset_id = 0x%x", __func__, preset_id);

    int ret;
    struct v4l2_output output;
    struct v4l2_dv_preset preset;
    unsigned int i = 0;

    do {
        output.index = i;
        ret = tvout_v4l2_enum_output(fd_tvout, &output);
        HDMI_Log(HDMI_LOG_DEBUG,
                "%s::output_type=%d output.index=%d .name=%s",
                __func__, output_type, output.index, output.name);
        if (output.type == output_type) {
            break;
        }
        i++;
    } while (ret >=0);


    HDMI_Log(HDMI_LOG_DEBUG, "%s::input preset_id=0x%08x", __func__, preset_id);

    if (output.capabilities & V4L2_OUT_CAP_PRESETS) {
        tvout_std_v4l2_enum_dv_presets(fd_tvout);
        preset.preset = preset_id;
        if (tvout_std_v4l2_s_dv_preset(fd_tvout, &preset) < 0 ) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::tvout_std_v4l2_s_dv_preset failed", __func__);
            return -1;
        }
    }

    return 0;
}

int tvout_v4l2_enum_output(int fd, struct v4l2_output *output)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    int ret = -1 ;
    ret = ioctl(fd, VIDIOC_ENUMOUTPUT, output);

    if (ret < 0) {
        if (errno == EINVAL)
            return -1;
        HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_ENUMOUTPUT", __func__);
        return -1;
    }
    HDMI_Log(HDMI_LOG_DEBUG, "%s::index=%d, type=0x%08x, name=%s",
          __func__, output->index, output->type, output->name);

    return ret;
}

int tvout_v4l2_s_output(int fd, int index)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (ioctl(fd, VIDIOC_S_OUTPUT, &index) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_S_OUTPUT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_v4l2_g_output(int fd, int *index)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (ioctl(fd, VIDIOC_G_OUTPUT, index) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_G_OUTPUT failed", __func__);
        return -1;
    }

    return 0;
}

int tvout_std_v4l2_enum_dv_presets(int fd)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct v4l2_dv_enum_preset enum_preset;
    int ret = -1;

    for (int index = 0; ; index++) {
        enum_preset.index = index;
        ret = ioctl(fd, VIDIOC_ENUM_DV_PRESETS, &enum_preset);

        if (ret < 0) {
            if (errno == EINVAL)
                break;
            HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_ENUM_DV_PRESETS", __func__);
            return -1;
        }
        HDMI_Log(HDMI_LOG_DEBUG,
                "%s::enum_preset.index=%d, enum_preset.preset=0x%08x, "
                "enum_preset.name=%s, enum_preset.width=%d, enum_preset.height=%d",
                __func__, enum_preset.index, enum_preset.preset,
                enum_preset.name, enum_preset.width, enum_preset.height);
    }

    return 0;
}

int tvout_std_v4l2_s_dv_preset(int fd, struct v4l2_dv_preset *preset)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (ioctl(fd, VIDIOC_S_DV_PRESET, preset) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_S_DV_PRESET failed preset_id=%d", __func__, preset->preset);
        return -1;
    }
    HDMI_Log(HDMI_LOG_DEBUG, "%s::preset_id=0x%08x", __func__, preset->preset);

    return 0;
}

void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect)
{
    if (dst_w * src_h <= dst_h * src_w) {
        dst_rect->left   = 0;
        dst_rect->top    = (dst_h - ((dst_w * src_h) / src_w)) >> 1;
        dst_rect->width  = dst_w;
        dst_rect->height = ((dst_w * src_h) / src_w);
    } else {
        dst_rect->left   = (dst_w - ((dst_h * src_w) / src_h)) >> 1;
        dst_rect->top    = 0;
        dst_rect->width  = ((dst_h * src_w) / src_h);
        dst_rect->height = dst_h;
    }
}

int hdmi_captureRun_byGSC(
        unsigned int dst_address,
        void *gsc_cap_handle)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    exynos_gsc_img src_info;
    exynos_gsc_img dst_info;
    memset(&src_info, 0, sizeof(src_info));
    memset(&dst_info, 0, sizeof(dst_info));

    dst_info.yaddr = dst_address;
    dst_info.uaddr = 0;
    dst_info.vaddr = 0;

    if (exynos_gsc_run_exclusive(gsc_cap_handle, &src_info, &dst_info) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_run_exclusive", __func__);
        return -1;
    }

    if (exynos_gsc_wait_done(gsc_cap_handle) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_wait_done", __func__);
        return -1;
    }

    return 0;
}

int hdmi_Blit_byG2D(
        int srcColorFormat,
        int src_w,
        int src_h,
        unsigned int src_address,
        int dst_w,
        int dst_h,
        unsigned int dst_address,
        int rotVal)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

#if defined(BOARD_USES_HDMI_FIMGAPI)
    int             src_color_format, dst_color_format;
    int             src_bpp, dst_bpp;
    fimg2d_blit     BlitParam;
    fimg2d_param    g2d_param;
    rotation        g2d_rotation;

    fimg2d_addr  srcAddr;
    fimg2d_image srcImage;
    fimg2d_rect  srcRect;

    fimg2d_addr  dstAddr;
    fimg2d_image dstImage;
    fimg2d_rect  dstRect;

    fimg2d_scale Scaling;
    fimg2d_repeat Repeat;
    fimg2d_bluscr Bluscr;
    fimg2d_clip Clipping;

    if (srcColorFormat == HAL_PIXEL_FORMAT_RGB_565) {
        src_color_format = CF_RGB_565;
        src_bpp = 2;
        dst_color_format = CF_RGB_565;
        dst_bpp = 2;
    } else {
        src_color_format = CF_ARGB_8888;
        src_bpp = 4;
        dst_color_format = CF_ARGB_8888;
        dst_bpp = 4;
    }

    srcAddr  = {(addr_space)ADDR_USER, (unsigned long)src_address};
    srcRect = {0, 0, src_w, src_h};
    srcImage = {src_w, src_h, src_w*src_bpp, AX_RGB, (color_format)src_color_format, srcAddr, srcAddr, srcRect, false};

    dstAddr  = {(addr_space)ADDR_USER, (unsigned long)dst_address};
    dstRect = {0, 0, dst_w, dst_h};
    dstImage = {dst_w, dst_h, dst_w*dst_bpp, AX_RGB, (color_format)dst_color_format, dstAddr, dstAddr, dstRect, false};

    if (rotVal == 0 || rotVal == 180)
        Scaling = {SCALING_BILINEAR, src_w, src_h, dst_w, dst_h};
    else
        Scaling = {SCALING_BILINEAR, src_w, src_h, dst_h, dst_w};

    switch (rotVal) {
    case 0:
        g2d_rotation = ORIGIN;
        break;
    case 90:
        g2d_rotation = ROT_90;
        break;
    case 180:
        g2d_rotation = ROT_180;
        break;
    case 270:
        g2d_rotation = ROT_270;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::invalid rotVal(%d) : failed", __func__, rotVal);
        return -1;
        break;
    }
    Repeat = {NO_REPEAT, 0};
    Bluscr = {OPAQUE, 0, 0};

    Clipping = {false, 0, 0, 0, 0};

    g2d_param = {0, 0xff, 0, g2d_rotation, NON_PREMULTIPLIED, Scaling, Repeat, Bluscr, Clipping};
    BlitParam = {BLIT_OP_SRC, g2d_param, &srcImage, NULL, NULL, &dstImage, BLIT_SYNC, 0};

    if (stretchFimgApi(&BlitParam) < 0) {
        HDMI_Log(HDMI_LOG_ERROR,
                "%s::stretchFimgApi(src_w=%d, src_h=%d, dst_w=%d, dst_h=%d) failed",
                __func__, src_w, src_h, dst_w, dst_h);
        return -1;
    }
#endif
    return 0;
}

int hdmi_outputmode_2_v4l2_output_type(int output_mode)
{
    int v4l2_output_type = -1;

    switch (output_mode) {
    case HDMI_OUTPUT_MODE_YCBCR:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DIGITAL;
        break;
    case HDMI_OUTPUT_MODE_RGB:
        v4l2_output_type = V4L2_OUTPUT_TYPE_HDMI_RGB;
        break;
    case HDMI_OUTPUT_MODE_DVI:
        v4l2_output_type = V4L2_OUTPUT_TYPE_DVI;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced HDMI_mode(%d)", __func__, output_mode);
        v4l2_output_type = -1;
        break;
    }

    return v4l2_output_type;
}

int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type)
{
    int outputMode = -1;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL:
        outputMode = HDMI_OUTPUT_MODE_YCBCR;
        break;
    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        outputMode = HDMI_OUTPUT_MODE_RGB;
        break;
    case V4L2_OUTPUT_TYPE_DVI:
        outputMode = HDMI_OUTPUT_MODE_DVI;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced v4l2_output_type(%d)", __func__, v4l2_output_type);
        outputMode = -1;
        break;
    }

    return outputMode;
}

int hdmi_check_output_mode(int v4l2_output_type)
{
    struct HDMIVideoParameter video;
    struct HDMIAudioParameter audio;
    int    calbirate_v4l2_mode = v4l2_output_type;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

    switch (v4l2_output_type) {
    case V4L2_OUTPUT_TYPE_DIGITAL :
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_YCBCR444;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
            ALOGI("Change mode into HDMI_RGB\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_HDMI_RGB:
        video.mode = HDMI;
        if (!EDIDHDMIModeSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DVI;
            ALOGI("Change mode into DVI\n");
            break;
        }

        video.colorSpace = HDMI_CS_RGB;
        if (!EDIDColorSpaceSupport(&video)) {
            calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
            ALOGI("Change mode into HDMI_YCBCR\n");
        }
        break;

    case V4L2_OUTPUT_TYPE_DVI:
        video.mode = DVI;
        if (!EDIDHDMIModeSupport(&video)) {
            video.colorSpace = HDMI_CS_YCBCR444;
            if (!EDIDColorSpaceSupport(&video)) {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_HDMI_RGB;
                ALOGI("Change mode into HDMI_RGB\n");
            } else {
                calbirate_v4l2_mode = V4L2_OUTPUT_TYPE_DIGITAL;
                ALOGI("Change mode into HDMI_YCBCR\n");
            }
            break;
        }

        break;

    default:
        break;
    }
    return calbirate_v4l2_mode;
}

int hdmi_check_resolution(unsigned int dv_id)
{
    struct HDMIVideoParameter video;

    switch (dv_id) {
    case V4L2_DV_480P60:
        video.resolution = v720x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    /*
    case V4L2_DV_480P60: // Needs to be modified after kernel driver updated.
        video.resolution = v640x480p_60Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    */
    case V4L2_DV_576P50:
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_16_9;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    /*
    case V4L2_DV_576P50: // Needs to be modified after kernel driver updated.
        video.resolution = v720x576p_50Hz;
        video.pixelAspectRatio = HDMI_PIXEL_RATIO_4_3;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    */
    case V4L2_DV_720P60:
    case V4L2_DV_720P59_94:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_720P50:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P60:
    case V4L2_DV_1080P59_94:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P50:
        video.resolution = v1920x1080p_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P24:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080I60:
    case V4L2_DV_1080I59_94:
        video.resolution = v1920x1080i_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080I50:
        video.resolution = v1920x1080i_50Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_480P59_94:
        video.resolution = v720x480p_60Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    case V4L2_DV_1080P30:
        video.resolution = v1920x1080p_30Hz;
        video.hdmi_3d_format = HDMI_2D_VIDEO_FORMAT;
        break;
    /* S3D Top and Bottom format */
    case V4L2_DV_720P60_TB:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_DV_720P50_TB:
        video.resolution = v1280x720p_50Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_DV_1080P24_TB:
    case V4L2_DV_1080P23_98_TB:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    case V4L2_DV_1080P60_TB:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_3D_TB_FORMAT;
        break;
    /* S3D Side by Side format */
    case V4L2_DV_720P60_SB_HALF:
    case V4L2_DV_720P59_94_SB_HALF:
        video.resolution = v1280x720p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_DV_1080P24_SB_HALF:
        video.resolution = v1920x1080p_24Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    case V4L2_DV_1080P60_SB_HALF:
        video.resolution = v1920x1080p_60Hz;
        video.hdmi_3d_format = HDMI_3D_SSH_FORMAT;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced dv_id(%lld)", __func__, dv_id);
        return -1;
        break;
    }

    if (!EDIDVideoResolutionSupport(&video)) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::EDIDVideoResolutionSupport(%llx) fail (not suppoted dv_id) \n", __func__, dv_id);
        return -1;
    }

    return 0;
}

int hdmi_resolution_2_preset_id(unsigned int resolution, unsigned int s3dMode, int * w, int * h, __u32 *preset_id)
{
    int ret = 0;

    if (s3dMode == HDMI_2D) {
        switch (resolution) {
        case 1080960:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P60;
            break;
        case 1080950:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P50;
            break;
        case 1080930:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P30;
            break;
        case 1080924:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P24;
            break;
        case 1080160:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080I60;
            break;
        case 1080150:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080I50;
            break;
        case 720960:
            *w      = 1280;
            *h      = 720;
            *preset_id = V4L2_DV_720P60;
            break;
        case 720950:
            *w      = 1280;
            *h      = 720;
            *preset_id = V4L2_DV_720P50;
            break;
        case 5769501: // 16:9
            *w      = 720;
            *h      = 576;
            *preset_id = V4L2_DV_576P50;
            break;
        case 5769502: // 4:3
            *w      = 720;
            *h      = 576;
            *preset_id = V4L2_DV_576P50;
            break;
        case 4809601: // 16:9
            *w      = 720;
            *h      = 480;
            *preset_id = V4L2_DV_480P60;
            break;
        case 4809602: // 4:3
            *w      = 720;
            *h      = 480;
            *preset_id = V4L2_DV_480P60;
            break;
        default:
            HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    } else if (s3dMode == HDMI_S3D_TB) {
        switch (resolution) {
        case 1080960:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P60_TB;
            break;
        case 1080924:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P24_TB;
            break;
        case 720960:
            *w      = 1280;
            *h      = 720;
            *preset_id = V4L2_DV_720P60_TB;
            break;
        case 720950:
            *w      = 1280;
            *h      = 720;
            *preset_id = V4L2_DV_720P50_TB;
            break;
        default:
            HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    } else if (s3dMode == HDMI_S3D_SBS) {
        switch (resolution) {
        case 1080960:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P60_SB_HALF;
            break;
        case 1080924:
            *w      = 1920;
            *h      = 1080;
            *preset_id = V4L2_DV_1080P24_SB_HALF;
            break;
        case 720960:
            *w      = 1280;
            *h      = 720;
            *preset_id = V4L2_DV_720P60_SB_HALF;
            break;
        default:
            HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced resolution(%d)", __func__, resolution);
            ret = -1;
            break;
        }
    } else {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Unsupported S3D mode(%d)\n", __func__, s3dMode);
        ret = -1;
    }

    return ret;
}

#if 0 // Before activate this code, check the driver support, first.
int hdmi_enable_hdcp(int fd, unsigned int hdcp_en)
{
    if (ioctl(fd, VIDIOC_HDCP_ENABLE, hdcp_en) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_HDCP_ENABLE(%d) fail", __func__, hdcp_en);
        return -1;
    }

    return 0;
}

int hdmi_check_audio(int fd)
{
    struct HDMIAudioParameter audio;
    enum state audio_state = ON;
    int ret = 0;

    audio.formatCode = LPCM_FORMAT;
    audio.outPacket  = HDMI_ASP;
    audio.channelNum = CH_2;
    audio.sampleFreq = SF_44KHZ;

#if defined(BOARD_USES_EDID)
    if (!EDIDAudioModeSupport(&audio))
        audio_state = NOT_SUPPORT;
    else
        audio_state = ON;
#endif
    if (audio_state == ON) {
        if (ioctl(fd, VIDIOC_INIT_AUDIO, 1) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_INIT_AUDIO(1) failed", __func__);
            ret = -1;
        }
    } else {
        if (ioctl(fd, VIDIOC_INIT_AUDIO, 0) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::VIDIOC_INIT_AUDIO(0) failed", __func__);
            ret = -1;
        }
    }

    return ret;
}
#endif

bool hdmi_check_interlaced_resolution(unsigned int resolution)
{
    bool ret = false;

    switch (resolution) {
    case 1080160:
    case 1080150:
        ret = true;
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}

bool getFrameSize(int V4L2_PIX, unsigned int * size, unsigned int frame_size)
{
    unsigned int frame_ratio = 8;
    int src_planes = get_yuv_planes(V4L2_PIX);
    int src_bpp    = get_yuv_bpp(V4L2_PIX);

    src_planes = (src_planes == -1) ? 1 : src_planes;
    frame_ratio = frame_ratio * (src_planes -1) / (src_bpp - 8);

    switch (src_planes) {
    case 1:
        switch (V4L2_PIX) {
        case V4L2_PIX_FMT_BGR32:
        case V4L2_PIX_FMT_RGB32:
            size[0] = frame_size << 2;
            break;
        case V4L2_PIX_FMT_RGB565X:
        case V4L2_PIX_FMT_NV16:
        case V4L2_PIX_FMT_NV61:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
        case V4L2_PIX_FMT_YVYU:
            size[0] = frame_size << 1;
            break;
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
            size[0] = (frame_size * 3) >> 1;
            break;
        default:
            HDMI_Log(HDMI_LOG_ERROR, "%s::invalid color type", __func__);
            return false;
            break;
        }
        size[1] = 0;
        size[2] = 0;
        break;
    case 2:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = 0;
        break;
    case 3:
        size[0] = frame_size;
        size[1] = frame_size / frame_ratio;
        size[2] = frame_size / frame_ratio;
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::invalid planes (src_planes=%d)", __func__, src_planes);
        return false;
        break;
    }

    return true;
}

}
