/*
 * Copyright (C) 2012 The Android Open Source Project
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

/*!
 * \file      exynos_gscaler.c
 * \brief     header file for Gscaler HAL
 * \author    ShinWon Lee (shinwon.lee@samsung.com)
              Rama, Meka(v.meka@samsung.com)
 * \date      2012/01/09
 */

//#define LOG_NDEBUG 0
#include "exynos_gsc_utils.h"

static int exynos_m2m_node[NUM_OF_GSC_HW] = {NODE_NUM_GSC_0, NODE_NUM_GSC_1,
                                               NODE_NUM_GSC_2, NODE_NUM_GSC_3};

static unsigned int m_gsc_get_plane_count(
    int v4l_pixel_format)
{
    int plane_count = 0;

    switch (v4l_pixel_format) {
    case V4L2_PIX_FMT_RGB32:
    case V4L2_PIX_FMT_BGR32:
    case V4L2_PIX_FMT_RGB24:
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
        plane_count = 1;
        break;
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12MT_16X16:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
        plane_count = 2;
        break;
    case V4L2_PIX_FMT_YVU420M:
    case V4L2_PIX_FMT_YUV422P:
    case V4L2_PIX_FMT_YUV420M:
        plane_count = 3;
        break;
    default:
        ALOGE("%s::unmatched v4l_pixel_format color_space(0x%x)\n",
             __func__, v4l_pixel_format);
        plane_count = -1;
        break;
    }

    return plane_count;
}

static int m_gsc_get_plane_size(
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
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_RGB24:
        plane_size[0] = width * height * 3;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_RGB565:
    case V4L2_PIX_FMT_RGB555X:
    case V4L2_PIX_FMT_RGB444:
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_UYVY:
        plane_size[0] = width * height * 2;
        plane_size[1] = 0;
        plane_size[2] = 0;
        break;
    /* 2 planes */
    case V4L2_PIX_FMT_NV12M:
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV21:
        plane_size[0] = width * height;
        plane_size[1] = width * (height / 2);
        plane_size[2] = 0;
        break;
    case V4L2_PIX_FMT_NV16:
    case V4L2_PIX_FMT_NV61:
#if 1 //It will be updated after fixing the NV16 format issue in exynos5_format_v4l2.c file.
        plane_size[0] = width * height * 2;
        plane_size[1] = 0;
#else
        plane_size[0] = width * height;
        plane_size[1] = width * height;
#endif
        plane_size[2] = 0;
        break;
     case V4L2_PIX_FMT_NV12MT_16X16:
        plane_size[0] = ALIGN(width, 16) * ALIGN(height, 16);
        plane_size[1] = ALIGN(width, 16) * ALIGN(height / 2, 8);
        plane_size[2] = 0;
        break;
    /* 3 planes */
    case V4L2_PIX_FMT_YVU420M:
    case V4L2_PIX_FMT_YUV420M:
    case V4L2_PIX_FMT_YUV422P:
        plane_size[0] = width * height;
        plane_size[1] = (width / 2) * (height / 2);
        plane_size[2] = (width / 2) * (height / 2);
        break;
    default:
        ALOGE("%s::unmatched v4l_pixel_format color_space(0x%x)\n",
             __func__, v4l_pixel_format);
        return -1;
        break;
    }

    return 0;
}

static int m_exynos_gsc_multiple_of_n(
    int number,
    int N)
{
    int result = number;

    if (N & (N-1))
        result = number - (number % N);
    else
        result = (number - (number & (N-1)));

    return result;
}

static bool m_exynos_gsc_check_size(
    unsigned int w,
    unsigned int h,
    unsigned int crop_w,
    unsigned int crop_h,
    unsigned int rot)
{
    if ((rot == 0) || (rot == 180)) {
        if (w < GSC_MIN_W_SIZE || h < GSC_MIN_H_SIZE ||
            crop_w < GSC_MIN_W_SIZE || crop_h < GSC_MIN_H_SIZE)  {
            ALOGE("%s::too small size (crop_w : %d crop_h : %d  f_w : %d  f_h %d) (MIN_W %d MIN_H %d)",
                __func__, crop_w, crop_h, w, h, GSC_MIN_W_SIZE, GSC_MIN_H_SIZE);
            return false;
        }
    } else {
        if (h < GSC_MIN_W_SIZE || w < GSC_MIN_H_SIZE ||
            crop_h < GSC_MIN_W_SIZE || crop_w < GSC_MIN_H_SIZE)  {
            ALOGE("%s::too small size (rot %d crop_w : %d crop_h : %d  f_w : %d  f_h %d) (MIN_W %d MIN_H %d)",
                __func__, rot, crop_w, crop_h, w, h, GSC_MIN_W_SIZE, GSC_MIN_H_SIZE);
            return false;
        }
    }
    /* GSC driver will handle the other constraints */

    return true;
}

static int exynos_gsc_scale_ratio_check(struct GSC_HANDLE *gsc_handle)
{
    int ret = 0;

    if ((gsc_handle->src_img.w < gsc_handle->dst_img.w) ||
        (gsc_handle->src_img.h < gsc_handle->dst_img.h)){ //up_scaling
        switch (gsc_handle->gsc_mode) {
        case GSC_M2M_MODE:
        case GSC_OUTPUT_MODE:
        case GSC_CAPTURE_MODE:    //to do: check the proper limits
            if ((gsc_handle->src_img.w * 8 < gsc_handle->dst_img.w) ||
                (gsc_handle->src_img.h * 8 < gsc_handle->dst_img.h))
                ret = -1;
            break;
        default:
            break;
        }
    }

    if (ret < 0)
        return ret;

    if ((gsc_handle->dst_img.w < gsc_handle->src_img.w) ||
        (gsc_handle->dst_img.h < gsc_handle->src_img.h)) { //down_scaling
        switch (gsc_handle->gsc_mode) {
        case GSC_M2M_MODE:
             if ((gsc_handle->dst_img.w * 16 < gsc_handle->src_img.w) ||
                (gsc_handle->dst_img.h * 16 < gsc_handle->src_img.h))
                ret = -1;
             break;
        case GSC_OUTPUT_MODE:
            if (((gsc_handle->dst_img.fw == 720) || (gsc_handle->dst_img.fw == 640)) &&
                (gsc_handle->dst_img.fh == 480)) {
                if ((gsc_handle->dst_img.w * 4 < gsc_handle->src_img.w) ||
                    (gsc_handle->dst_img.h * 4 < gsc_handle->src_img.h))
                    ret = -1;
            } else if ((gsc_handle->dst_img.fw == 1280) && (gsc_handle->dst_img.fh == 720)) {
                if ((gsc_handle->dst_img.w * 4 < gsc_handle->src_img.w) ||
                    (gsc_handle->dst_img.h * 4 < gsc_handle->src_img.h))
                    ret = -1;
            } else if ((gsc_handle->dst_img.fw == 1280) && (gsc_handle->dst_img.fh == 800)) {
                if ((gsc_handle->dst_img.w * 3 < gsc_handle->src_img.w) ||
                    (gsc_handle->dst_img.h * 3 < gsc_handle->src_img.h) ||
                    (gsc_handle->out_mode != GSC_OUT_FIMD))
                    ret = -1;
             } else if ((gsc_handle->dst_img.fw == 800) && (gsc_handle->dst_img.fh == 1280)) {
                if ((gsc_handle->dst_img.w * 2 < gsc_handle->src_img.w) ||
                    (gsc_handle->dst_img.h * 2 < gsc_handle->src_img.h) ||
                    (gsc_handle->out_mode != GSC_OUT_FIMD))
                    ret = -1;
            } else if ((gsc_handle->dst_img.fw == 1920) && (gsc_handle->dst_img.fh == 1080)) {
                if ((gsc_handle->dst_img.w * 2 < gsc_handle->src_img.w) ||
                    (gsc_handle->dst_img.h * 2 < gsc_handle->src_img.h))
                    ret = -1;
            } else {
                ret = -1;
            }
            break;
        case GSC_CAPTURE_MODE:
            if ((gsc_handle->dst_img.w * 8 < gsc_handle->src_img.w) ||
                (gsc_handle->dst_img.h * 8 < gsc_handle->src_img.h))
                ret = -1;
            break;
        default:
            ret = -1;
            break;
        }
    }

    return ret;
}

static int exynos_gsc_out_streamoff(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_requestbuffers reqbuf;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->src.stream_on == false) {
        /* to handle special scenario.*/
        gsc_handle->src.buf_idx = 0;
        gsc_handle->src.qbuf_cnt = 0;
        ALOGD("%s::GSC is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }
    gsc_handle->src.buf_idx = 0;
    gsc_handle->src.qbuf_cnt = 0;
    gsc_handle->src.stream_on = false;

    if (exynos_v4l2_streamoff(gsc_handle->gsc_vd_entity->fd,
                                V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }
SKIP_STREAMOFF:
    return 0;
}

static int exynos_gsc_out_stop(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_requestbuffers reqbuf;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->src.stream_on == false) {
        /* to handle special scenario.*/
        gsc_handle->src.buf_idx = 0;
        gsc_handle->src.qbuf_cnt = 0;
        ALOGD("%s::GSC is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }
    gsc_handle->src.buf_idx = 0;
    gsc_handle->src.qbuf_cnt = 0;
    gsc_handle->src.stream_on = false;

    if (exynos_v4l2_streamoff(gsc_handle->gsc_vd_entity->fd,
                                V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd,
        V4L2_CID_USE_SYSMMU, 0) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_USE_SYSMMU) fail", __func__);
        return false;
    }

SKIP_STREAMOFF:
    /* Clear Buffer */
    /*todo: support for other buffer type & memory */
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = 0;

    if (exynos_v4l2_reqbufs(gsc_handle->gsc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }
    return 0;
}

static bool m_exnos_gsc_out_destroy(struct GSC_HANDLE *gsc_handle)
{
    struct media_link * links;
    int i;

    if (gsc_handle == NULL) {
        ALOGE("%s::gsc_handle is NULL", __func__);
        return false;
    }

    if (gsc_handle->src.stream_on == true) {
        if (exynos_gsc_out_stop((void *)gsc_handle) < 0)
            ALOGE("%s::exynos_gsc_out_stop() fail", __func__);

            gsc_handle->src.stream_on = false;
    }

    if (gsc_handle->media && gsc_handle->gsc_sd_entity &&
        gsc_handle->gsc_vd_entity && gsc_handle->sink_sd_entity) {

        /* unlink : gscaler-out --> fimd */
        for (i = 0; i < (int) gsc_handle->gsc_sd_entity->num_links; i++) {
            links = &gsc_handle->gsc_sd_entity->links[i];

            if (links == NULL || links->source->entity != gsc_handle->gsc_sd_entity ||
                                 links->sink->entity   != gsc_handle->sink_sd_entity) {
                continue;
            } else if (exynos_media_setup_link(gsc_handle->media,  links->source,
                                                                        links->sink, 0) < 0) {
                ALOGE("%s::exynos_media_setup_unlink [src.entity=%d->sink.entity=%d] failed",
                      __func__, links->source->entity->info.id, links->sink->entity->info.id);
            }
        }
    }

    if (gsc_handle->gsc_vd_entity && gsc_handle->gsc_vd_entity->fd > 0) {
        close(gsc_handle->gsc_vd_entity->fd);
        gsc_handle->gsc_vd_entity->fd = -1;
    }

    if (gsc_handle->gsc_sd_entity && gsc_handle->gsc_sd_entity->fd > 0) {
        close(gsc_handle->gsc_sd_entity->fd);
        gsc_handle->gsc_sd_entity->fd = -1;
    }

    if (gsc_handle->sink_sd_entity && gsc_handle->sink_sd_entity->fd > 0) {
        close(gsc_handle->sink_sd_entity->fd);
        gsc_handle->sink_sd_entity->fd = -1;
    }

    if (gsc_handle->media)
        exynos_media_close(gsc_handle->media);

    gsc_handle->media = NULL;
    gsc_handle->gsc_sd_entity = NULL;
    gsc_handle->gsc_vd_entity = NULL;
    gsc_handle->sink_sd_entity = NULL;

    return true;
}

static int m_exynos_gsc_out_create(
    struct GSC_HANDLE *gsc_handle,
    int dev_num,
    int out_mode)
{
    struct media_device *media;
    struct media_entity *gsc_sd_entity;
    struct media_entity *gsc_vd_entity;
    struct media_entity *sink_sd_entity;
    struct media_link *links;
    char node[32];
    char devname[32];
    unsigned int cap;
    int         i;
    int         fd = 0;

    if ((out_mode < GSC_OUT_FIMD) ||
        (out_mode >= GSC_OUT_RESERVED))
        return -1;

    gsc_handle->out_mode = out_mode;
    /* GSCX => FIMD_WINX : arbitrary linking is not allowed */
    if ((out_mode == GSC_OUT_FIMD) &&
        (dev_num > 1))
        return -1;

    /* media0 */
    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 0);
    media = exynos_media_open(node);
    if (media == NULL) {
        ALOGE("%s::exynos_media_open failed (node=%s)", __func__, node);
        return -1;
    }
    gsc_handle->media = media;

    /* Get the sink subdev entity by name and make the node of sink subdev*/
    if (out_mode == GSC_OUT_FIMD)
        sprintf(devname, PFX_FIMD_ENTITY, dev_num);
    else
        sprintf(devname, PFX_MXR_ENTITY, 0);

    sink_sd_entity = exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!sink_sd_entity) {
        ALOGE("%s:: failed to get the sink sd entity", __func__);
        goto gsc_output_err;
    }
    gsc_handle->sink_sd_entity = sink_sd_entity;

    sink_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if ( sink_sd_entity->fd < 0) {
        ALOGE("%s:: failed to open sink subdev node", __func__);
        goto gsc_output_err;
    }

    /* get GSC video dev & sub dev entity by name*/
    sprintf(devname, PFX_GSC_VIDEODEV_ENTITY, dev_num);
    gsc_vd_entity= exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!gsc_vd_entity) {
        ALOGE("%s:: failed to get the gsc vd entity", __func__);
        goto gsc_output_err;
    }
    gsc_handle->gsc_vd_entity = gsc_vd_entity;

    sprintf(devname, PFX_GSC_SUBDEV_ENTITY, dev_num);
    gsc_sd_entity= exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!gsc_sd_entity) {
        ALOGE("%s:: failed to get the gsc sd entity", __func__);
        goto gsc_output_err;
    }
    gsc_handle->gsc_sd_entity = gsc_sd_entity;

    /* gsc sub-dev open */
    sprintf(devname, PFX_GSC_SUBDEV_ENTITY, dev_num);
    gsc_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if (gsc_sd_entity->fd < 0) {
        ALOGE("%s: gsc sub-dev open fail", __func__);
        goto gsc_output_err;
    }

    /* setup link : GSC : video device --> sub device */
    for (i = 0; i < (int) gsc_vd_entity->num_links; i++) {
        links = &gsc_vd_entity->links[i];

        if (links == NULL ||
            links->source->entity != gsc_vd_entity ||
            links->sink->entity   != gsc_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media,  links->source,
                        links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* setup link : GSC: sub device --> sink device */
    for (i = 0; i < (int) gsc_sd_entity->num_links; i++) {
        links = &gsc_sd_entity->links[i];

        if (links == NULL || links->source->entity != gsc_sd_entity ||
                             links->sink->entity   != sink_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media,  links->source,
                        links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* gsc video-dev open */
    sprintf(devname, PFX_GSC_VIDEODEV_ENTITY, dev_num);
    gsc_vd_entity->fd = exynos_v4l2_open_devname(devname, O_RDWR);
    if (gsc_vd_entity->fd < 0) {
        ALOGE("%s: gsc video-dev open fail", __func__);
        goto gsc_output_err;
    }

    cap = V4L2_CAP_STREAMING |
          V4L2_CAP_VIDEO_OUTPUT_MPLANE;

    if (exynos_v4l2_querycap(gsc_vd_entity->fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        goto gsc_output_err;
    }

    return 0;

gsc_output_err:
    m_exnos_gsc_out_destroy(gsc_handle);

    return -1;
}

static int exynos_gsc_cap_streamoff(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_requestbuffers reqbuf;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        LOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->dst.stream_on == false) {
        /* to handle special scenario.*/
        gsc_handle->dst.buf_idx = 0;
        gsc_handle->dst.qbuf_cnt = 0;
        ALOGD("%s::GSC is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }
    gsc_handle->dst.buf_idx = 0;
    gsc_handle->dst.qbuf_cnt = 0;
    gsc_handle->dst.stream_on = false;

    if (exynos_v4l2_streamoff(gsc_handle->gsc_vd_entity->fd,
                                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }
SKIP_STREAMOFF:
    return 0;
}

static int exynos_gsc_cap_stop(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_requestbuffers reqbuf;
    int vsync = 0;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->dst.stream_on == false) {
        gsc_handle->dst.buf_idx = 0;
        ALOGD("%s::GSC is already stopped", __func__);
        goto SKIP_STREAMOFF;
    }
    gsc_handle->dst.buf_idx = 0;
    gsc_handle->dst.stream_on = false;

    if (ioctl(gsc_handle->fb_fd, S3CFB_SET_VSYNC_INT, &vsync) < 0) {
            ALOGE("%s::fb_ioctl(vsync=%d) failed", __func__, vsync);
            return -1;
    }

    if (exynos_v4l2_streamoff(gsc_handle->gsc_vd_entity->fd,
                                V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) < 0) {
        ALOGE("%s::stream off failed", __func__);
        return -1;
    }
SKIP_STREAMOFF:
    /* Clear Buffer */
    /*todo: support for other buffer type & memory */
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = 0;

    if (exynos_v4l2_reqbufs(gsc_handle->gsc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }
    return 0;
}

static bool m_exnos_gsc_cap_destroy(struct GSC_HANDLE *gsc_handle)
{
    struct media_link * links;
    int i;

    if (gsc_handle == NULL) {
        ALOGE("%s::gsc_handle is NULL", __func__);
        return false;
    }

    if (gsc_handle->dst.stream_on == true) {
        if (exynos_gsc_cap_stop((void *)gsc_handle) < 0)
            ALOGE("%s::exynos_gsc_cap_stop() fail", __func__);

            gsc_handle->dst.stream_on = false;
    }

    if (gsc_handle->media && gsc_handle->gsc_sd_entity &&
        gsc_handle->gsc_vd_entity && gsc_handle->sink_sd_entity) {

        /* unlink : fimd1 ----> gsc-cap */
        for (i = 0; i < (int) gsc_handle->sink_sd_entity->num_links; i++) {
            links = &gsc_handle->sink_sd_entity->links[i];

            if (links == NULL || links->source->entity != gsc_handle->sink_sd_entity ||
                                 links->sink->entity   != gsc_handle->gsc_sd_entity) {
                continue;
            } else if (exynos_media_setup_link(gsc_handle->media,  links->source,
                                                                        links->sink, 0) < 0) {
                ALOGE("%s::exynos_media_setup_unlink [src.entity=%d->sink.entity=%d] failed",
                      __func__, links->source->entity->info.id, links->sink->entity->info.id);
            }
        }
    }

    if (gsc_handle->gsc_vd_entity && gsc_handle->gsc_vd_entity->fd > 0) {
        close(gsc_handle->gsc_vd_entity->fd);
        gsc_handle->gsc_vd_entity->fd = -1;
    }

    if (gsc_handle->gsc_sd_entity && gsc_handle->gsc_sd_entity->fd > 0) {
        close(gsc_handle->gsc_sd_entity->fd);
        gsc_handle->gsc_sd_entity->fd = -1;
    }

    if (gsc_handle->sink_sd_entity && gsc_handle->sink_sd_entity->fd > 0) {
        close(gsc_handle->sink_sd_entity->fd);
        gsc_handle->sink_sd_entity->fd = -1;
    }

    if (gsc_handle->media)
        exynos_media_close(gsc_handle->media);

    gsc_handle->media = NULL;
    gsc_handle->gsc_sd_entity = NULL;
    gsc_handle->gsc_vd_entity = NULL;
    gsc_handle->sink_sd_entity = NULL;

    return true;
}


static int m_exynos_gsc_cap_create(
    struct GSC_HANDLE *gsc_handle,
    int dev_num)
{
    struct media_device *media;
    struct media_entity *gsc_sd_entity;
    struct media_entity *gsc_vd_entity;
    struct media_entity *sink_sd_entity;
    struct media_link *links;
    char node[32];
    char devname[32];
    unsigned int cap;
    int         i;
    int         fd = 0;

    /* media1 */
    sprintf(node, "%s%d", PFX_NODE_MEDIADEV, 1);
    media = exynos_media_open(node);
    if (media == NULL) {
        ALOGE("%s::exynos_media_open failed (node=%s)", __func__, node);
        return -1;
    }
    gsc_handle->media = media;

    /* get entity for local-in */
    sprintf(devname, PFX_FIMD1_ENTITY, 1);
    sink_sd_entity = exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!sink_sd_entity) {
        ALOGE("%s:: failed to get the sink sd entity", __func__);
        goto gsc_cap_err;
    }
    gsc_handle->sink_sd_entity = sink_sd_entity;

    sink_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if ( sink_sd_entity->fd < 0) {
        ALOGE("%s:: failed to open sink subdev node", __func__);
        goto gsc_cap_err;
    }

    /* get GSC video dev & sub dev entity by name*/
    sprintf(devname, PFX_GSC_CAP_VIDEODEV_ENTITY, dev_num);
    gsc_vd_entity= exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!gsc_vd_entity) {
        ALOGE("%s:: failed to get the gsc vd entity", __func__);
        goto gsc_cap_err;
    }
    gsc_handle->gsc_vd_entity = gsc_vd_entity;

    sprintf(devname, PFX_GSC_CAP_SUBDEV_ENTITY, dev_num);
    gsc_sd_entity= exynos_media_get_entity_by_name(media, devname, strlen(devname));
    if (!gsc_sd_entity) {
        ALOGE("%s:: failed to get the gsc sd entity", __func__);
        goto gsc_cap_err;
    }
    gsc_handle->gsc_sd_entity = gsc_sd_entity;

    /* gsc sub-dev open */
    sprintf(devname, PFX_GSC_CAP_SUBDEV_ENTITY, dev_num);
    gsc_sd_entity->fd = exynos_subdev_open_devname(devname, O_RDWR);
    if (gsc_sd_entity->fd < 0) {
        ALOGE("%s: gsc sub-dev open fail", __func__);
        goto gsc_cap_err;
    }

    /* setup link : GSC : cap sub device ---> video device */
    for (i = 0; i < (int) gsc_sd_entity->num_links; i++) {
        links = &gsc_sd_entity->links[i];

        if (links == NULL ||
            links->source->entity != gsc_sd_entity ||
            links->sink->entity   != gsc_vd_entity) {
            continue;
        } else if (exynos_media_setup_link(media,  links->source,
                        links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* setup link : FIMD: sub device --> GSC Cap device */
    for (i = 0; i < (int) sink_sd_entity->num_links; i++) {
        links = &sink_sd_entity->links[i];

        if (links == NULL || links->source->entity != sink_sd_entity ||
                             links->sink->entity   != gsc_sd_entity) {
            continue;
        } else if (exynos_media_setup_link(media,  links->source,
                        links->sink, MEDIA_LNK_FL_ENABLED) < 0) {
            ALOGE("%s::exynos_media_setup_link [src.entity=%d->sink.entity=%d] failed",
                  __func__, links->source->entity->info.id, links->sink->entity->info.id);
            return -1;
        }
    }

    /* gsc video-dev open */
    sprintf(devname, PFX_GSC_CAP_VIDEODEV_ENTITY, dev_num);
    gsc_vd_entity->fd = exynos_v4l2_open_devname(devname, O_RDWR);
    if (gsc_vd_entity->fd < 0) {
        ALOGE("%s: gsc video-dev open fail", __func__);
        goto gsc_cap_err;
    }

    cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE;

    if (exynos_v4l2_querycap(gsc_vd_entity->fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        goto gsc_cap_err;
    }

    /* open fb for controlling VSYNC INT */
    /* why is it fixed to win1 ??? */
    sprintf(devname, PFX_NODE_FB, 1);
    gsc_handle->fb_fd = open(devname, O_RDWR);
    if (gsc_handle->fb_fd <= 0) {
        ALOGE("%s::failed to open fb1", __func__);
        goto gsc_cap_err;
    }

    return 0;

gsc_cap_err:
    m_exnos_gsc_cap_destroy(gsc_handle);

    return -1;
}

static int m_exynos_gsc_create(
    int dev)
{
    int          fd = 0;
    int          video_node_num;
    unsigned int cap;
    char         node[32];

    switch(dev) {
    case 0:
        video_node_num = NODE_NUM_GSC_0;
        break;
    case 1:
        video_node_num = NODE_NUM_GSC_1;
        break;
    case 2:
        video_node_num = NODE_NUM_GSC_2;
        break;
    case 3:
        video_node_num = NODE_NUM_GSC_3;
        break;
    default:
        ALOGE("%s::unexpected dev(%d) fail", __func__, dev);
        return -1;
        break;
    }

    sprintf(node, "%s%d", PFX_NODE_GSC, video_node_num);
    fd = exynos_v4l2_open(node, O_RDWR);
    if (fd < 0) {
        ALOGE("%s::exynos_v4l2_open(%s) fail", __func__, node);
        return -1;
    }

    cap = V4L2_CAP_STREAMING |
          V4L2_CAP_VIDEO_OUTPUT_MPLANE |
          V4L2_CAP_VIDEO_CAPTURE_MPLANE;

    if (exynos_v4l2_querycap(fd, cap) == false) {
        ALOGE("%s::exynos_v4l2_querycap() fail", __func__);
        if (0 < fd)
            close(fd);
        fd = 0;
        return -1;
    }
    return fd;
}

static bool m_exynos_gsc_destroy(
    struct GSC_HANDLE *gsc_handle)
{
    if (gsc_handle->src.stream_on == true) {
        if (exynos_v4l2_streamoff(gsc_handle->gsc_fd, gsc_handle->src.buf_type) < 0)
            ALOGE("%s::exynos_v4l2_streamoff() fail", __func__);

        gsc_handle->src.stream_on = false;
    }

    if (gsc_handle->dst.stream_on == true) {
        if (exynos_v4l2_streamoff(gsc_handle->gsc_fd, gsc_handle->dst.buf_type) < 0)
            ALOGE("%s::exynos_v4l2_streamoff() fail", __func__);

        gsc_handle->dst.stream_on = false;
    }

    if (0 < gsc_handle->gsc_fd)
        close(gsc_handle->gsc_fd);
    gsc_handle->gsc_fd = 0;

    return true;
}

static int m_exnos_get_gsc_idx(void)
{
    int i  = 0;
    int j = 0;
    int idx = -1;
    int cnt[NUM_OF_GSC_HW];
    char         node[32];
    int fd;

    for (i = 0; i < NUM_OF_GSC_HW; i++) {
        cnt[i] = GSC_M2M_MAX_CTX_CNT;
        // HACK : HWComposer, HDMI uses gscaler in local-path mode.
        //So, those GSCs couldn't be used in time sharing M2M mode.
        if (i == 0 || i == 3)
            continue;

        sprintf(node, "%s%d", PFX_NODE_GSC, exynos_m2m_node[i]);
        fd = exynos_v4l2_open(node, O_RDWR);
        if (fd < 0) {
            continue;
        }

        if (exynos_v4l2_g_ctrl(fd, V4L2_CID_M2M_CTX_NUM, &cnt[i]) < 0) {
            ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_M2M_CTX_NUM) fail", __func__);
            cnt[i] = GSC_M2M_MAX_CTX_CNT;
            continue;
        }

        close(fd);
    }

    /* HWC is mostly active for all the multimedia scenarios. It uses GSC0.
    possibly..first try to allocate a GSC that is connected to different bus.*/
    idx = NUM_OF_GSC_HW - 1;
    for (i = NUM_OF_GSC_HW - 1; i >= 0; i--) {
        if (cnt[i] < cnt[idx])
            idx = i;
    }

    if (cnt[idx] == GSC_M2M_MAX_CTX_CNT)
        return -1;

    return idx;
}

bool m_exynos_gsc_find_and_create(
    struct GSC_HANDLE *gsc_handle)
{
    int     gsc_idx = 0;
    unsigned int total_sleep_time  = 0;

    /* to do: dynamic allocation */
    /* get the gsc_idx based on the usage stats */
    gsc_idx = m_exnos_get_gsc_idx();

    ALOGD("#################################\n\n\n\n\n");
    ALOGD("m_exynos_gsc_find_and_create (gsc_idx %d)", gsc_idx);
    ALOGD("\n\n\n\n\n#################################");

     // create new one.
    gsc_handle->gsc_fd = m_exynos_gsc_create(gsc_idx);
    if (gsc_handle->gsc_fd < 0) {
        gsc_handle->gsc_fd = -1;
        ALOGE("%s::we don't have free gsc.. fail", __func__);
        return false;
    }

    return true;
}

static bool m_exynos_gsc_set_format(
    int              fd,
    struct gsc_info *info)
{
    struct v4l2_requestbuffers req_buf;
    int                        plane_count;

    plane_count = m_gsc_get_plane_count(info->v4l2_colorformat);
    if (plane_count < 0) {
        ALOGE("%s::not supported v4l2_colorformat", __func__);
        return false;
    }

    if (info->stream_on == true) {
        if (exynos_v4l2_streamoff(fd, info->buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamoff() fail", __func__);
            return false;
        }
        info->stream_on = false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_ROTATE, info->rotation) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_ROTATE) fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_VFLIP, info->flip_horizontal) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_VFLIP) fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_HFLIP, info->flip_vertical) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_HFLIP) fail", __func__);
        return false;
    }
    info->format.type = info->buf_type;
    info->format.fmt.pix_mp.width       = info->width;
    info->format.fmt.pix_mp.height      = info->height;
    info->format.fmt.pix_mp.pixelformat = info->v4l2_colorformat;
    info->format.fmt.pix_mp.field       = V4L2_FIELD_ANY;
    info->format.fmt.pix_mp.num_planes  = plane_count;

    if (exynos_v4l2_s_fmt(fd, &info->format) < 0) {
        ALOGE("%s::exynos_v4l2_s_fmt() fail", __func__);
        return false;
    }

    info->crop.type     = info->buf_type;
    info->crop.c.left   = info->crop_left;
    info->crop.c.top    = info->crop_top;
    info->crop.c.width  = info->crop_width;
    info->crop.c.height = info->crop_height;

    if (exynos_v4l2_s_crop(fd, &info->crop) < 0) {
        ALOGE("%s::exynos_v4l2_s_crop() fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_CACHEABLE, info->cacheable) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl() fail", __func__);
        return false;
    }

    if (exynos_v4l2_s_ctrl(fd, V4L2_CID_USE_SYSMMU, !(info->mode_drm)) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl() fail", __func__);
        return false;
    }

    req_buf.count  = 1;
    req_buf.type   = info->buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs() fail", __func__);
        return false;
    }

    return true;
}

static bool m_exynos_gsc_set_addr(
    int              fd,
    struct gsc_info *info)
{
    unsigned int i;
    unsigned int plane_size[NUM_OF_GSC_PLANES];

    if (m_gsc_get_plane_size(plane_size, info->width,
                                info->height, info->v4l2_colorformat) < 0) {
        ALOGE("%s:m_gsc_get_plane_size:fail", __func__);
        return false;
    }

    info->buffer.index    = 0;
    info->buffer.type     = info->buf_type;
    info->buffer.memory   = V4L2_MEMORY_USERPTR;
    info->buffer.m.planes = info->planes;
    info->buffer.length   = info->format.fmt.pix_mp.num_planes;

    for (i = 0; i < info->format.fmt.pix_mp.num_planes; i++) {
        info->buffer.m.planes[i].m.userptr = (unsigned long)info->addr[i];
        info->buffer.m.planes[i].length    = plane_size[i];
        info->buffer.m.planes[i].bytesused = 0;
    }

    if (exynos_v4l2_qbuf(fd, &info->buffer) < 0) {
        ALOGE("%s::exynos_v4l2_qbuf() fail", __func__);
        return false;
    }

    return true;
}

void *exynos_gsc_create(
    void)
{
    int i     = 0;
    int op_id = 0;
    char mutex_name[32];

    struct GSC_HANDLE *gsc_handle = (struct GSC_HANDLE *)malloc(sizeof(struct GSC_HANDLE));
    if (gsc_handle == NULL) {
        ALOGE("%s::malloc(struct GSC_HANDLE) fail", __func__);
        goto err;
    }

    memset(gsc_handle, 0, sizeof(struct GSC_HANDLE));
    gsc_handle->gsc_fd = -1;

    gsc_handle->src.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    gsc_handle->dst.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    gsc_handle->op_mutex = NULL;

    srand(time(NULL));
    op_id = rand() % 1000000; // just make random id
    sprintf(mutex_name, "%sOp%d", LOG_TAG, op_id);
    gsc_handle->op_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_PRIVATE, mutex_name);
    if (gsc_handle->op_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);
    if (m_exynos_gsc_find_and_create(gsc_handle) == false) {
        ALOGE("%s::m_exynos_gsc_find_and_trylock_and_create() fail", __func__);
        goto err;
    }

    exynos_mutex_unlock(gsc_handle->op_mutex);

    return (void *)gsc_handle;

err:
    if (gsc_handle) {
        m_exynos_gsc_destroy(gsc_handle);

        if (gsc_handle->op_mutex)
            exynos_mutex_unlock(gsc_handle->op_mutex);

        if (exynos_mutex_destroy(gsc_handle->op_mutex) == false)
            ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

        free(gsc_handle);
    }

    return NULL;
}

void *exynos_gsc_create_exclusive(
    int dev_num,
    int mode,
    int out_mode)
{
    int i     = 0;
    int op_id = 0;
    char mutex_name[32];
    unsigned int total_sleep_time  = 0;
    bool    gsc_flag = false;
    int ret = 0;

    if ((dev_num < 0) || (dev_num >= NUM_OF_GSC_HW)) {
        ALOGE("%s::fail:: dev_num is not valid(%d) ", __func__, dev_num);
        return NULL;
    }

    if ((mode < 0) || (mode >= NUM_OF_GSC_HW)) {
        ALOGE("%s::fail:: mode is not valid(%d) ", __func__, mode);
        return NULL;
    }

    struct GSC_HANDLE *gsc_handle = (struct GSC_HANDLE *)malloc(sizeof(struct GSC_HANDLE));
    if (gsc_handle == NULL) {
        ALOGE("%s::malloc(struct GSC_HANDLE) fail", __func__);
        goto err;
    }
    memset(gsc_handle, 0, sizeof(struct GSC_HANDLE));
    gsc_handle->gsc_fd = -1;
    gsc_handle->gsc_mode = mode;

    gsc_handle->src.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    gsc_handle->dst.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    gsc_handle->op_mutex = NULL;

    srand(time(NULL));
    op_id = rand() % 1000000; // just make random id
    sprintf(mutex_name, "%sOp%d", LOG_TAG, op_id);
    gsc_handle->op_mutex = exynos_mutex_create(EXYNOS_MUTEX_TYPE_PRIVATE, mutex_name);
    if (gsc_handle->op_mutex == NULL) {
        ALOGE("%s::exynos_mutex_create(%s) fail", __func__, mutex_name);
        goto err;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    switch (mode) {
    case GSC_M2M_MODE:
        gsc_handle->gsc_fd = m_exynos_gsc_create(dev_num);
        if (gsc_handle->gsc_fd < 0) {
            ALOGE("%s::m_exynos_gsc_create(%i) fail", __func__, dev_num);
            goto err;
        }
        break;
    case GSC_OUTPUT_MODE:
        ret = m_exynos_gsc_out_create(gsc_handle, dev_num, out_mode);
        if (ret < 0) {
            ALOGE("%s::m_exynos_gsc_out_create(%i) fail", __func__, dev_num);
            goto err;
        }
        break;
    case GSC_CAPTURE_MODE:
        ret = m_exynos_gsc_cap_create(gsc_handle, dev_num);
        if (ret < 0) {
            ALOGE("%s::m_exynos_gsc_cap_create(%i) fail", __func__, dev_num);
            goto err;
        }
        break;
    default:
        break;
    }

    exynos_mutex_unlock(gsc_handle->op_mutex);
    return (void *)gsc_handle;

err:
    if (gsc_handle) {
        switch (mode) {
        case GSC_M2M_MODE:
            m_exynos_gsc_destroy(gsc_handle);
            break;
        case GSC_OUTPUT_MODE:
            m_exnos_gsc_out_destroy(gsc_handle);
            break;
        case GSC_CAPTURE_MODE:
            m_exnos_gsc_cap_destroy(gsc_handle);
            break;
        default:
            break;
        }

        if (gsc_handle->op_mutex)
            exynos_mutex_unlock(gsc_handle->op_mutex);

        if (exynos_mutex_destroy(gsc_handle->op_mutex) == false)
            ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

        free(gsc_handle);
    }

    return NULL;
}

void exynos_gsc_destroy(
    void *handle)
{
    int i = 0;
    struct GSC_HANDLE *gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
        m_exynos_gsc_destroy(gsc_handle);
        break;
    case GSC_OUTPUT_MODE:
        m_exnos_gsc_out_destroy(gsc_handle);
        break;
    case GSC_CAPTURE_MODE:
        m_exnos_gsc_cap_destroy(gsc_handle);
        break;
    default:
        break;
    }

    exynos_mutex_unlock(gsc_handle->op_mutex);

    if (exynos_mutex_destroy(gsc_handle->op_mutex) == false)
        ALOGE("%s::exynos_mutex_destroy(op_mutex) fail", __func__);

    if (gsc_handle)
        free(gsc_handle);
}

int exynos_gsc_set_src_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm)
{
    struct GSC_HANDLE *gsc_handle;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    gsc_handle->src.width            = width;
    gsc_handle->src.height           = height;
    gsc_handle->src.crop_left        = crop_left;
    gsc_handle->src.crop_top         = crop_top;
    gsc_handle->src.crop_width       = crop_width;
    gsc_handle->src.crop_height      = crop_height;
    gsc_handle->src.v4l2_colorformat = v4l2_colorformat;
    gsc_handle->src.cacheable        = cacheable;
    gsc_handle->src.mode_drm         = mode_drm;

    exynos_mutex_unlock(gsc_handle->op_mutex);

    return 0;
}

int exynos_gsc_set_dst_format(
    void        *handle,
    unsigned int width,
    unsigned int height,
    unsigned int crop_left,
    unsigned int crop_top,
    unsigned int crop_width,
    unsigned int crop_height,
    unsigned int v4l2_colorformat,
    unsigned int cacheable,
    unsigned int mode_drm)
{
    struct GSC_HANDLE *gsc_handle;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    gsc_handle->dst.width            = width;
    gsc_handle->dst.height           = height;
    gsc_handle->dst.crop_left        = crop_left;
    gsc_handle->dst.crop_top         = crop_top;
    gsc_handle->dst.crop_width       = crop_width;
    gsc_handle->dst.crop_height      = crop_height;
    gsc_handle->dst.v4l2_colorformat = v4l2_colorformat;
    gsc_handle->dst.cacheable        = cacheable;
    gsc_handle->dst.mode_drm         = mode_drm;

    exynos_mutex_unlock(gsc_handle->op_mutex);

    return 0;
}

int exynos_gsc_set_rotation(
    void *handle,
    int   rotation,
    int   flip_horizontal,
    int   flip_vertical)
{
    int ret = -1;
    struct GSC_HANDLE *gsc_handle;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return ret;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    int new_rotation = rotation % 360;

    if (new_rotation % 90 != 0) {
        ALOGE("%s::rotation(%d) cannot be acceptable fail", __func__, rotation);
        goto done;
    }

    if(new_rotation < 0)
        new_rotation = -new_rotation;

    gsc_handle->dst.rotation        = new_rotation;
    gsc_handle->dst.flip_horizontal = flip_horizontal;
    gsc_handle->dst.flip_vertical   = flip_vertical;

    ret = 0;
done:
    exynos_mutex_unlock(gsc_handle->op_mutex);

    return ret;
}

int exynos_gsc_set_src_addr(
    void *handle,
    void *addr[3])
{
    struct GSC_HANDLE *gsc_handle;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    gsc_handle->src.addr[0] = addr[0];
    gsc_handle->src.addr[1] = addr[1];
    gsc_handle->src.addr[2] = addr[2];

    exynos_mutex_unlock(gsc_handle->op_mutex);

    return 0;
}

int exynos_gsc_set_dst_addr(
    void *handle,
    void *addr[3])
{
    struct GSC_HANDLE *gsc_handle;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    gsc_handle->dst.addr[0] = addr[0];
    gsc_handle->dst.addr[1] = addr[1];
    gsc_handle->dst.addr[2] = addr[2];

    exynos_mutex_unlock(gsc_handle->op_mutex);

    return 0;
}

static void rotateValueHAL2GSC(unsigned int transform,
    unsigned int *rotate,
    unsigned int *hflip,
    unsigned int *vflip)
{
    int rotate_flag = transform & 0x7;
    *rotate = 0;
    *hflip = 0;
    *vflip = 0;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        break;
    case HAL_TRANSFORM_ROT_180:
        *rotate = 180;
        break;
    case HAL_TRANSFORM_ROT_270:
        *rotate = 270;
        break;
    case HAL_TRANSFORM_FLIP_H | HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        *vflip = 1; /* set vflip to compensate the rot & flip order. */
        break;
    case HAL_TRANSFORM_FLIP_V | HAL_TRANSFORM_ROT_90:
        *rotate = 90;
        *hflip = 1; /* set hflip to compensate the rot & flip order. */
        break;
    case HAL_TRANSFORM_FLIP_H:
        *hflip = 1;
         break;
    case HAL_TRANSFORM_FLIP_V:
        *vflip = 1;
         break;
    default:
        break;
    }
}

int exynos_gsc_m2m_config(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    struct GSC_HANDLE *gsc_handle;
    int32_t      src_color_space;
    int32_t      dst_color_space;
    int ret;
    unsigned int rotate;
    unsigned int hflip;
    unsigned int vflip;

    gsc_handle = (struct GSC_HANDLE *)handle;
     if (gsc_handle == NULL) {
        ALOGE("%s::gsc_handle == NULL() fail", __func__);
        return -1;
    }

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    rotateValueHAL2GSC(dst_img->rot, &rotate, &hflip, &vflip);
    exynos_gsc_set_rotation(gsc_handle, rotate, hflip, vflip);

    ret = exynos_gsc_set_src_format(gsc_handle,  src_img->fw, src_img->fh,
                                  src_img->x, src_img->y, src_img->w, src_img->h,
                                  src_color_space, src_img->cacheable, src_img->drmMode);
    if (ret < 0) {
        ALOGE("%s: fail: exynos_gsc_set_src_format [fw %d fh %d x %d y %d w %d h %d f %x rot %d]",
            __func__, src_img->fw, src_img->fh, src_img->x, src_img->y, src_img->w, src_img->h,
            src_color_space, src_img->rot);
        return -1;
    }

    ret = exynos_gsc_set_dst_format(gsc_handle, dst_img->fw, dst_img->fh,
                                  dst_img->x, dst_img->y, dst_img->w, dst_img->h,
                                  dst_color_space, dst_img->cacheable, src_img->drmMode);
    if (ret < 0) {
        ALOGE("%s: fail: exynos_gsc_set_dst_format [fw %d fh %d x %d y %d w %d h %d f %x rot %d]",
            __func__, dst_img->fw, dst_img->fh, dst_img->x, dst_img->y, dst_img->w, dst_img->h,
            src_color_space, dst_img->rot);
        return -1;
    }
    return 0;
}

int exynos_gsc_out_config(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_format  fmt;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_subdev_format sd_fmt;
    struct v4l2_subdev_crop   sd_crop;
    int i;
    unsigned int rotate;
    unsigned int hflip;
    unsigned int vflip;
    unsigned int plane_size[NUM_OF_GSC_PLANES];

    struct v4l2_rect dst_rect;
    int32_t      src_color_space;
    int32_t      dst_color_space;
    int32_t      src_planes;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (gsc_handle == NULL) {
        ALOGE("%s::gsc_handle == NULL() fail", __func__);
        return -1;
    }

     if (gsc_handle->src.stream_on != false) {
        ALOGE("Error: Src is already streamed on !!!!");
        return -1;
     }

    memcpy(&gsc_handle->src_img, src_img, sizeof(exynos_gsc_img));
    memcpy(&gsc_handle->dst_img, dst_img, sizeof(exynos_gsc_img));

#ifdef GSC_FIMD_OUT_WIDTH_ALIGN_WA
    if (gsc_handle->out_mode == GSC_OUT_FIMD)
        gsc_handle->dst_img.w = gsc_handle->dst_img.w & (~1);
#endif

    if (exynos_gsc_scale_ratio_check(gsc_handle) < 0) {
        ALOGE("%s:::exynos_gsc_scale_ratio_check failed", __func__);
        return -1;
    }

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(src_img->format);
    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    src_planes = get_yuv_planes(src_color_space);
    src_planes = (src_planes == -1) ? 1 : src_planes;
    rotateValueHAL2GSC(dst_img->rot, &rotate, &hflip, &vflip);

    if (m_exynos_gsc_check_size(gsc_handle->src_img.fw, gsc_handle->src_img.fh,
                                        gsc_handle->src_img.w, gsc_handle->src_img.h, 0) == false) {
            ALOGE("%s::m_exynos_gsc_check_size():src: fail", __func__);
            return -1;
    }

    if (m_exynos_gsc_check_size(gsc_handle->dst_img.fw, gsc_handle->dst_img.fh,
                                        gsc_handle->dst_img.w, gsc_handle->dst_img.h,
                                        rotate) == false) {
            ALOGE("%s::m_exynos_gsc_check_size():dst: fail", __func__);
            return -1;
    }

    /*set: src v4l2_buffer*/
    gsc_handle->src.buf_idx = 0;
    gsc_handle->src.qbuf_cnt = 0;

    /* set src format  :GSC video dev*/
    fmt.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width       = gsc_handle->src_img.fw;
    fmt.fmt.pix_mp.height      = gsc_handle->src_img.fh;
    fmt.fmt.pix_mp.pixelformat = src_color_space;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes  = src_planes;

    if (exynos_v4l2_s_fmt(gsc_handle->gsc_vd_entity->fd, &fmt) < 0) {
        ALOGE("%s::videodev set format failed", __func__);
        return -1;
    }

    /* set src crop info :GSC video dev*/
    crop.type     = fmt.type;
    crop.c.left   = gsc_handle->src_img.x;
    crop.c.top    = gsc_handle->src_img.y;
    crop.c.width  = gsc_handle->src_img.w;
    crop.c.height = gsc_handle->src_img.h;

    if (exynos_v4l2_s_crop(gsc_handle->gsc_vd_entity->fd, &crop) < 0) {
        ALOGE("%s::videodev set crop failed", __func__);
        return -1;
    }

    /* set format: src pad of GSC sub-dev*/
    sd_fmt.pad   = GSCALER_SUBDEV_PAD_SOURCE;
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (gsc_handle->out_mode == GSC_OUT_FIMD) {
        sd_fmt.format.width  = gsc_handle->dst_img.fw;
        sd_fmt.format.height = gsc_handle->dst_img.fh;
    } else {
        sd_fmt.format.width  = gsc_handle->dst_img.w;
        sd_fmt.format.height = gsc_handle->dst_img.h;
    }

    sd_fmt.format.code   = V4L2_MBUS_FMT_YUV8_1X24;
    if (exynos_subdev_s_fmt(gsc_handle->gsc_sd_entity->fd, &sd_fmt) < 0) {
            ALOGE("%s::GSC subdev set format failed", __func__);
            return -1;
    }

    /* set crop: src crop of GSC sub-dev*/
    sd_crop.pad   = GSCALER_SUBDEV_PAD_SOURCE;
    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (gsc_handle->out_mode == GSC_OUT_FIMD) {
        sd_crop.rect.left   = gsc_handle->dst_img.x;
        sd_crop.rect.top    = gsc_handle->dst_img.y;
        sd_crop.rect.width  = gsc_handle->dst_img.w;
        sd_crop.rect.height = gsc_handle->dst_img.h;
    } else {
        sd_crop.rect.left   = 0;
        sd_crop.rect.top    = 0;
        sd_crop.rect.width  = gsc_handle->dst_img.w;
        sd_crop.rect.height = gsc_handle->dst_img.h;
    }

    if (exynos_subdev_s_crop(gsc_handle->gsc_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::GSC subdev set crop failed", __func__);
            return -1;
    }

    /* sink pad is connected to GSC out */
    /*  set format: sink sub-dev */
    if (gsc_handle->out_mode == GSC_OUT_FIMD) {
        sd_fmt.pad   = FIMD_SUBDEV_PAD_SINK;
        sd_fmt.format.width  = gsc_handle->dst_img.w;
        sd_fmt.format.height = gsc_handle->dst_img.h;
    } else {
        sd_fmt.pad   = MIXER_V_SUBDEV_PAD_SINK;
        sd_fmt.format.width  = gsc_handle->dst_img.w;
        sd_fmt.format.height = gsc_handle->dst_img.h;
    }

    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.code = V4L2_MBUS_FMT_YUV8_1X24;

    if (exynos_subdev_s_fmt(gsc_handle->sink_sd_entity->fd, &sd_fmt) < 0) {
        ALOGE("%s::sink:set format failed (PAD=%d)", __func__, sd_fmt.pad);
        return -1;
    }

    /*  set crop: sink sub-dev */
    if (gsc_handle->out_mode == GSC_OUT_FIMD)
        sd_crop.pad   = FIMD_SUBDEV_PAD_SINK;
    else
        sd_crop.pad   = MIXER_V_SUBDEV_PAD_SINK;

    sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (gsc_handle->out_mode == GSC_OUT_FIMD) {
        sd_crop.rect.left   = gsc_handle->dst_img.x;
        sd_crop.rect.top    = gsc_handle->dst_img.y;
        sd_crop.rect.width  = gsc_handle->dst_img.w;
        sd_crop.rect.height = gsc_handle->dst_img.h;
    } else {
        sd_crop.rect.left   = 0;
        sd_crop.rect.top    = 0;
        sd_crop.rect.width  = gsc_handle->dst_img.w;
        sd_crop.rect.height = gsc_handle->dst_img.h;
    }

    if (exynos_subdev_s_crop(gsc_handle->sink_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::sink: subdev set crop failed(PAD=%d)", __func__, sd_crop.pad);
            return -1;
    }

    if (gsc_handle->out_mode != GSC_OUT_FIMD) {
        sd_fmt.pad   = MIXER_V_SUBDEV_PAD_SOURCE;
        sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        sd_fmt.format.width  = gsc_handle->dst_img.fw;
        sd_fmt.format.height = gsc_handle->dst_img.fh;

        if (gsc_handle->out_mode == GSC_OUT_TV_DIGITAL)
            sd_fmt.format.code   = V4L2_MBUS_FMT_YUV8_1X24;
        else
            sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;

        if (exynos_subdev_s_fmt(gsc_handle->sink_sd_entity->fd, &sd_fmt) < 0) {
            ALOGE("%s::sink:set format failed (PAD=%d)", __func__, sd_fmt.pad);
            return -1;
        }

        sd_crop.pad   = MIXER_V_SUBDEV_PAD_SOURCE;
        sd_crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        sd_crop.rect.left   = gsc_handle->dst_img.x;
        sd_crop.rect.top    = gsc_handle->dst_img.y;
        sd_crop.rect.width  = gsc_handle->dst_img.w;
        sd_crop.rect.height = gsc_handle->dst_img.h;

        if (exynos_subdev_s_crop(gsc_handle->sink_sd_entity->fd, &sd_crop) < 0) {
            ALOGE("%s::sink: subdev set crop failed(PAD=%d)", __func__, sd_crop.pad);
            return -1;
        }
    }

    /*set GSC ctrls */
    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd, V4L2_CID_ROTATE, rotate) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_ROTATE: %d) failed", __func__,  rotate);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd, V4L2_CID_HFLIP, vflip) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_HFLIP: %d) failed", __func__,  vflip);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd, V4L2_CID_VFLIP, hflip) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_VFLIP: %d) failed", __func__,  hflip);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd,
        V4L2_CID_CACHEABLE, gsc_handle->src_img.cacheable) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_CACHEABLE: 1) failed", __func__);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd,
        V4L2_CID_USE_SYSMMU, !(gsc_handle->src_img.drmMode)) < 0) {
        ALOGE("%s::exynos_v4l2_s_ctrl(V4L2_CID_USE_SYSMMU) fail", __func__);
        return false;
    }

    reqbuf.type   = fmt.type;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = MAX_BUFFERS_GSCALER_OUT;

    if (exynos_v4l2_reqbufs(gsc_handle->gsc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }
    return 0;
}

static int exynos_gsc_out_run(void *handle,
    unsigned int yAddr,
    unsigned int uAddr,
    unsigned int vAddr)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_plane  planes[NUM_OF_GSC_PLANES];
    struct v4l2_buffer buf;
    int32_t      src_color_space;
    int32_t      src_planes;
    int             i;
    unsigned int plane_size[NUM_OF_GSC_PLANES];

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_GSC_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(gsc_handle->src_img.format);
    src_planes = get_yuv_planes(src_color_space);
    src_planes = (src_planes == -1) ? 1 : src_planes;

    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.length   = src_planes;
    buf.index    = gsc_handle->src.buf_idx;
    buf.m.planes = planes;

    gsc_handle->src.addr[0] = (void *)yAddr;
    gsc_handle->src.addr[1] = (void *)uAddr;
    gsc_handle->src.addr[2] = (void *)vAddr;

    if (m_gsc_get_plane_size(plane_size, gsc_handle->src_img.fw,
                                gsc_handle->src_img.fh, src_color_space) < 0) {
        ALOGE("%s:m_gsc_get_plane_size:fail", __func__);
        return -1;
    }

    for (i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)gsc_handle->src.addr[i];
        buf.m.planes[i].length    = plane_size[i];
        buf.m.planes[i].bytesused = plane_size[i];
    }

    /* Queue the buf */
    if (exynos_v4l2_qbuf(gsc_handle->gsc_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::queue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            gsc_handle->src.buf_idx, MAX_BUFFERS_GSCALER_OUT);
        return -1;
    }
    gsc_handle->src.buf_idx++;
    gsc_handle->src.qbuf_cnt++;

    if (gsc_handle->src.stream_on == false) {
        /* stream on after queing the second buffer
            to do: below logic should be changed to handle the single frame videos */
#ifndef GSC_OUT_DELAYED_STREAMON
        if (gsc_handle->src.buf_idx == (MAX_BUFFERS_GSCALER_OUT - 2))
#else
        if (gsc_handle->src.buf_idx == (MAX_BUFFERS_GSCALER_OUT - 1))
#endif
        {
            if (exynos_v4l2_streamon(gsc_handle->gsc_vd_entity->fd, buf.type) < 0) {
                ALOGE("%s::stream on failed", __func__);
                return -1;
            }
            gsc_handle->src.stream_on = true;
        }
        gsc_handle->src.buf_idx = gsc_handle->src.buf_idx % MAX_BUFFERS_GSCALER_OUT;
#ifndef GSC_OUT_DMA_BLOCKING
        return 0;
#endif
    }

    gsc_handle->src.buf_idx = gsc_handle->src.buf_idx % MAX_BUFFERS_GSCALER_OUT;

    return 0;
}

static int exynos_gsc_m2m_run_core(void *handle)
{
    struct GSC_HANDLE *gsc_handle;

    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (m_exynos_gsc_check_size(gsc_handle->src.width, gsc_handle->src.height,
                                    gsc_handle->src.crop_width, gsc_handle->src.crop_height,
                                    gsc_handle->src.rotation) == false) {
        ALOGE("%s::m_exynos_gsc_check_size():src: fail", __func__);
        goto done;
    }

    if (m_exynos_gsc_check_size(gsc_handle->dst.width, gsc_handle->dst.height,
                                    gsc_handle->dst.crop_width, gsc_handle->dst.crop_height,
                                    gsc_handle->dst.rotation) == false) {
        ALOGE("%s::m_exynos_gsc_check_size():dst: fail", __func__);
        goto done;
    }

    if (m_exynos_gsc_set_format(gsc_handle->gsc_fd, &gsc_handle->src) == false) {
        ALOGE("%s::m_exynos_gsc_set_format(src) fail", __func__);
        goto done;
    }

    if (m_exynos_gsc_set_format(gsc_handle->gsc_fd, &gsc_handle->dst) == false) {
        ALOGE("%s::m_exynos_gsc_set_format(dst) fail", __func__);
        goto done;
    }

    if (m_exynos_gsc_set_addr(gsc_handle->gsc_fd, &gsc_handle->src) == false) {
        ALOGE("%s::m_exynos_gsc_set_addr(src) fail", __func__);
        goto done;
    }

    if (m_exynos_gsc_set_addr(gsc_handle->gsc_fd, &gsc_handle->dst) == false) {
        ALOGE("%s::m_exynos_gsc_set_addr(dst) fail", __func__);
        goto done;
    }

    if (gsc_handle->src.stream_on == false) {
        if (exynos_v4l2_streamon(gsc_handle->gsc_fd, gsc_handle->src.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamon(src) fail", __func__);
            goto done;
        }
        gsc_handle->src.stream_on = true;
    }

    if (gsc_handle->dst.stream_on == false) {
        if (exynos_v4l2_streamon(gsc_handle->gsc_fd, gsc_handle->dst.buf_type) < 0) {
            ALOGE("%s::exynos_v4l2_streamon(dst) fail", __func__);
            goto done;
        }
        gsc_handle->dst.stream_on = true;
    }

    return 0;

done:
    return -1;
}

static int exynos_gsc_m2m_wait_frame_done(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_requestbuffers req_buf;

    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if ((gsc_handle->src.stream_on == false) || (gsc_handle->dst.stream_on == false)) {
        ALOGE("%s:: src_strean_on or dst_stream_on are false", __func__);
        return -1;
    }

    if (exynos_v4l2_dqbuf(gsc_handle->gsc_fd, &gsc_handle->src.buffer) < 0) {
        ALOGE("%s::exynos_v4l2_dqbuf(src) fail", __func__);
        return -1;
    }

    if (exynos_v4l2_dqbuf(gsc_handle->gsc_fd, &gsc_handle->dst.buffer) < 0) {
        ALOGE("%s::exynos_v4l2_dqbuf(dst) fail", __func__);
        return -1;
    }

    if (exynos_v4l2_streamoff(gsc_handle->gsc_fd, gsc_handle->src.buf_type) < 0) {
        ALOGE("%s::exynos_v4l2_streamoff(src) fail", __func__);
        return -1;
    }
    gsc_handle->src.stream_on = false;

    if (exynos_v4l2_streamoff(gsc_handle->gsc_fd, gsc_handle->dst.buf_type) < 0) {
        ALOGE("%s::exynos_v4l2_streamoff(dst) fail", __func__);
        return -1;
    }
    gsc_handle->dst.stream_on = false;

    /* src: clear_buf */
    req_buf.count  = 0;
    req_buf.type   = gsc_handle->src.buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(gsc_handle->gsc_fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs():src: fail", __func__);
        return -1;
    }

    /* dst: clear_buf */
    req_buf.count  = 0;
    req_buf.type   = gsc_handle->dst.buf_type;
    req_buf.memory = V4L2_MEMORY_USERPTR;
    if (exynos_v4l2_reqbufs(gsc_handle->gsc_fd, &req_buf) < 0) {
        ALOGE("%s::exynos_v4l2_reqbufs():dst: fail", __func__);
        return -1;
    }

     return 0;
}

static int exynos_gsc_out_wait_frame_done(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_plane  planes[NUM_OF_GSC_PLANES];
    struct v4l2_buffer buf;
    int32_t      src_color_space;
    int32_t      src_planes;
    int             i;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->src.qbuf_cnt < MAX_BUFFERS_GSCALER_OUT)
        return 0;

    src_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(gsc_handle->src_img.format);
    src_planes = get_yuv_planes(src_color_space);
    src_planes = (src_planes == -1) ? 1 : src_planes;

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_GSC_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.length   = src_planes;
    buf.m.planes = planes;

    /* DeQueue a buf */
    if (exynos_v4l2_dqbuf(gsc_handle->gsc_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::dequeue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            gsc_handle->src.buf_idx, MAX_BUFFERS_GSCALER_OUT);
        return -1;
    }

     return 0;
}

int exynos_gsc_convert(
    void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    int ret    = -1;
    gsc_handle = (struct GSC_HANDLE *)handle;

    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    exynos_mutex_lock(gsc_handle->op_mutex);

    if (exynos_gsc_m2m_run_core(handle) < 0) {
        ALOGE("%s::exynos_gsc_run_core fail", __func__);
        goto done;
    }

    if (exynos_gsc_m2m_wait_frame_done(handle) < 0) {
        ALOGE("%s::exynos_gsc_m2m_wait_frame_done", __func__);
        goto done;
    }

    ret = 0;

done:
    exynos_mutex_unlock(gsc_handle->op_mutex);

    return ret;
}

int exynos_gsc_m2m_run(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    void *addr[3] = {NULL, NULL, NULL};
    int ret = 0;

    addr[0] = (void *)src_img->yaddr;
    addr[1] = (void *)src_img->uaddr;
    addr[2] = (void *)src_img->vaddr;

    ret = exynos_gsc_set_src_addr(handle, addr);
    if (ret < 0) {
        ALOGE("%s::fail: exynos_gsc_set_src_addr[%x %x %x]", __func__,
            (unsigned int)addr[0], (unsigned int)addr[1], (unsigned int)addr[2]);
        return -1;
    }

    addr[0] = (void *)dst_img->yaddr;
    addr[1] = (void *)dst_img->uaddr;
    addr[2] = (void *)dst_img->vaddr;
    ret = exynos_gsc_set_dst_addr(handle, addr);
    if (ret < 0) {
        ALOGE("%s::fail: exynos_gsc_set_dst_addr[%x %x %x]", __func__,
            (unsigned int)addr[0], (unsigned int)addr[1], (unsigned int)addr[2]);
        return -1;
    }

    ret = exynos_gsc_m2m_run_core(handle);
     if (ret < 0) {
        ALOGE("%s::fail: exynos_gsc_m2m_run_core", __func__);
        return -1;
    }
    return 0;
}

static int exynos_gsc_cap_config(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_format  fmt;
    struct v4l2_crop    crop;
    struct v4l2_requestbuffers reqbuf;
    struct v4l2_subdev_format sd_fmt;
    struct v4l2_subdev_crop   sd_crop;

    int32_t      dst_color_space;
    int32_t      dst_planes;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (gsc_handle == NULL) {
        ALOGE("%s::gsc_handle == NULL() fail", __func__);
        return -1;
    }

    if (gsc_handle->dst.stream_on != false) {
        ALOGE("Error: dst is already streamed on !!!!");
        return -1;
    }

    memcpy(&gsc_handle->src_img, src_img, sizeof(exynos_gsc_img));
    memcpy(&gsc_handle->dst_img, dst_img, sizeof(exynos_gsc_img));
    if (exynos_gsc_scale_ratio_check(gsc_handle) < 0) {
        ALOGE("%s:::exynos_gsc_scale_ratio_check failed", __func__);
        return -1;
    }

    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(dst_img->format);
    dst_planes = get_yuv_planes(dst_color_space);
    dst_planes = (dst_planes == -1) ? 1 : dst_planes;

    /*set: src v4l2_buffer*/
    gsc_handle->dst.buf_idx = 0;
    gsc_handle->src.qbuf_cnt = 0;

    /* set src format: src pad of GSC sub-dev*/
    sd_fmt.pad   = GSCALER_SUBDEV_PAD_SINK;
    sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    sd_fmt.format.width  = gsc_handle->src_img.fw;
    sd_fmt.format.height = gsc_handle->src_img.fh;
    sd_fmt.format.code   = V4L2_MBUS_FMT_XRGB8888_4X8_LE;
    if (exynos_subdev_s_fmt(gsc_handle->gsc_sd_entity->fd, &sd_fmt) < 0) {
        ALOGE("%s::GSC subdev set format failed", __func__);
        return -1;
    }

    /* set dst format  :GSC video dev*/
    fmt.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width       = gsc_handle->dst_img.fw;
    fmt.fmt.pix_mp.height      = gsc_handle->dst_img.fh;
    fmt.fmt.pix_mp.pixelformat = dst_color_space;
    fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes  = dst_planes;

    if (exynos_v4l2_s_fmt(gsc_handle->gsc_vd_entity->fd, &fmt) < 0) {
        ALOGE("%s::videodev set format failed", __func__);
        return -1;
    }

    /* set src crop info :GSC video dev*/
    crop.type     = fmt.type;
    crop.c.left   = gsc_handle->dst_img.x;
    crop.c.top    = gsc_handle->dst_img.y;
    crop.c.width  = gsc_handle->dst_img.w;
    crop.c.height = gsc_handle->dst_img.h;

    if (exynos_v4l2_s_crop(gsc_handle->gsc_vd_entity->fd, &crop) < 0) {
        ALOGE("%s::videodev set crop failed", __func__);
        return -1;
    }

    if (exynos_v4l2_s_ctrl(gsc_handle->gsc_vd_entity->fd,
        V4L2_CID_CACHEABLE, gsc_handle->src_img.cacheable) < 0) {
        ALOGE("%s:: exynos_v4l2_s_ctrl (V4L2_CID_CACHEABLE: 1) failed", __func__);
        return -1;
    }

    reqbuf.type   = fmt.type;
    reqbuf.memory = V4L2_MEMORY_USERPTR;
    reqbuf.count  = 1;

    if (exynos_v4l2_reqbufs(gsc_handle->gsc_vd_entity->fd, &reqbuf) < 0) {
        ALOGE("%s::request buffers failed", __func__);
        return -1;
    }

    return 0;
}

static int exynos_gsc_cap_run(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_plane  planes[NUM_OF_GSC_PLANES];
    struct v4l2_buffer buf;
    int32_t      dst_color_space;
    int32_t      dst_planes;
    int             i;
    int             vsync;
    unsigned int plane_size[NUM_OF_GSC_PLANES];

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_GSC_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(gsc_handle->dst_img.format);
    dst_planes = get_yuv_planes(dst_color_space);
    dst_planes = (dst_planes == -1) ? 1 : dst_planes;

    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.length   = dst_planes;
    buf.index    = gsc_handle->dst.buf_idx;
    buf.m.planes = planes;

    gsc_handle->dst.addr[0] = (void *)(dst_img->yaddr);
    gsc_handle->dst.addr[1] = (void *)(dst_img->uaddr);
    gsc_handle->dst.addr[2] = (void *)(dst_img->vaddr);

    if (m_gsc_get_plane_size(plane_size, gsc_handle->dst_img.fw,
                                gsc_handle->dst_img.fh, dst_color_space) < 0) {
        ALOGE("%s:m_gsc_get_plane_size:fail", __func__);
        return -1;
    }

    for (i = 0; i < buf.length; i++) {
        buf.m.planes[i].m.userptr = (unsigned long)gsc_handle->dst.addr[i];
        buf.m.planes[i].length    = plane_size[i];
        buf.m.planes[i].bytesused = plane_size[i];
    }

    /* Queue the buf */
    if (exynos_v4l2_qbuf(gsc_handle->gsc_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::queue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            gsc_handle->dst.buf_idx, MAX_BUFFERS_GSCALER_CAP);
        return -1;
    }
    gsc_handle->dst.buf_idx++;

    if (gsc_handle->dst.stream_on == false) {
        if (exynos_v4l2_streamon(gsc_handle->gsc_vd_entity->fd, buf.type) < 0) {
            ALOGE("%s::stream on failed", __func__);
            return -1;
        }
        gsc_handle->dst.stream_on = true;

        vsync = 1;
        if (ioctl(gsc_handle->fb_fd, S3CFB_SET_VSYNC_INT, &vsync) < 0) {
            ALOGE("%s::fb_ioctl(vsync=%d) failed", __func__, vsync);
            return -1;
        }
    }
    gsc_handle->dst.buf_idx = gsc_handle->dst.buf_idx % MAX_BUFFERS_GSCALER_CAP;

    return 0;
}

static int exynos_gsc_cap_wait_frame_done(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    struct v4l2_plane  planes[NUM_OF_GSC_PLANES];
    struct v4l2_buffer buf;
    struct pollfd events_c;
    int32_t      dst_color_space;
    int32_t      dst_planes;
    int             i;

    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    dst_color_space = HAL_PIXEL_FORMAT_2_V4L2_PIX(gsc_handle->dst_img.format);
    dst_planes = get_yuv_planes(dst_color_space);
    dst_planes = (dst_planes == -1) ? 1 : dst_planes;

    memset(&buf, 0, sizeof(struct v4l2_buffer));
    for (i = 0; i < NUM_OF_GSC_PLANES; i++)
        memset(&planes[i], 0, sizeof(struct v4l2_plane));

    buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory   = V4L2_MEMORY_USERPTR;
    buf.length   = dst_planes;
    buf.m.planes = planes;

    memset(&events_c, 0, sizeof(events_c));
    events_c.fd = gsc_handle->gsc_vd_entity->fd;
    events_c.events = POLLIN | POLLERR;

    if (poll(&events_c, 1, GSC_CAP_TIMEOUT) <= 0) {
        ALOGE("%s::poll() failed", __func__);
        return -1;
    }

    /* DeQueue a buf */
    if (exynos_v4l2_dqbuf(gsc_handle->gsc_vd_entity->fd, &buf) < 0) {
        ALOGE("%s::dequeue buffer failed (index=%d)(mSrcBufNum=%d)", __func__,
            gsc_handle->dst.buf_idx, MAX_BUFFERS_GSCALER_CAP);
        return -1;
    }

     return 0;
}

int exynos_gsc_config_exclusive(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
     struct GSC_HANDLE *gsc_handle;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
        ret = exynos_gsc_m2m_config(handle, src_img, dst_img);
        break;
    case GSC_OUTPUT_MODE:
        ret = exynos_gsc_out_config(handle, src_img, dst_img);
        break;
    case  GSC_CAPTURE_MODE:
        ret = exynos_gsc_cap_config(handle, src_img, dst_img);
        break;
    default:
        break;
    }
    return ret;

}

int exynos_gsc_run_exclusive(void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img)
{
    struct GSC_HANDLE *gsc_handle;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
       ret = exynos_gsc_m2m_run(handle, src_img, dst_img);
        break;
    case GSC_OUTPUT_MODE:
        ret = exynos_gsc_out_run(handle, src_img->yaddr,
                                                src_img->uaddr, src_img->vaddr);
        break;
    case  GSC_CAPTURE_MODE:
        ret =  exynos_gsc_cap_run(handle, src_img, dst_img);
        break;
    default:
        break;
    }
    return ret;
}

int exynos_gsc_wait_done(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
        ret = exynos_gsc_m2m_wait_frame_done(handle);
        break;
    case GSC_OUTPUT_MODE:
        ret = exynos_gsc_out_wait_frame_done(handle);
        break;
    case  GSC_CAPTURE_MODE:
        ret = exynos_gsc_cap_wait_frame_done(handle);
        break;
    default:
        break;
    }
    return ret;

}

int exynos_gsc_stop_exclusive(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
        //nothing to do;
        break;
    case GSC_OUTPUT_MODE:
        ret = exynos_gsc_out_stop(handle);
        break;
    case  GSC_CAPTURE_MODE:
        ret = exynos_gsc_cap_stop(handle);
        break;
    default:
        break;
    }
    return ret;
}

int exynos_gsc_just_stop(void *handle)
{
    struct GSC_HANDLE *gsc_handle;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (gsc_handle->gsc_mode) {
    case GSC_M2M_MODE:
        //nothing to do;
        break;
    case GSC_OUTPUT_MODE:
        ret = exynos_gsc_out_streamoff(handle);
        break;
    case  GSC_CAPTURE_MODE:
        ret = exynos_gsc_cap_streamoff(handle);
        break;
    default:
        break;
    }
    return ret;
}

int exynos_gsc_set_ctrl(
    void *handle,
    unsigned int ctrl_id,
    void *val)
{
    struct GSC_HANDLE *gsc_handle;
    int value;
    int ret = 0;
    gsc_handle = (struct GSC_HANDLE *)handle;
    if (handle == NULL) {
        ALOGE("%s::handle == NULL() fail", __func__);
        return -1;
    }

    switch (ctrl_id) {
    case GSC_CTRL_OUT_MODE:
        value = *((int *) val);
        if ((value <= GSC_OUT_DUMMY) || (GSC_OUT_RESERVED <= value))
            return -1;
        gsc_handle->out_mode =  value;
        break;
    default:
        break;
    }

    return ret;
}

