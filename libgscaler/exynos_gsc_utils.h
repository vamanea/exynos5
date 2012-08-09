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

#ifndef EXYNOS_GSC_H_
#define EXYNOS_GSC_H_

#ifdef __cplusplus
extern "C" {
#endif

//#define LOG_NDEBUG 0
#define LOG_TAG "libexynosgscaler"
#include <cutils/log.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <videodev2.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <system/graphics.h>
#include "exynos_gscaler.h"
#include <sys/poll.h>
#include "s3c_lcd.h"
#include "exynos_format.h"
#include "ExynosMutex.h"
#include "exynos_v4l2.h"

#define GSC_FIMD_OUT_WIDTH_ALIGN_WA
#define NUM_OF_GSC_PLANES           (3)
#define MAX_BUFFERS_GSCALER_OUT (3)
#define MAX_BUFFERS_GSCALER_CAP (1)
#define GSCALER_SUBDEV_PAD_SINK     (0)
#define GSCALER_SUBDEV_PAD_SOURCE   (1)
#define MIXER_V_SUBDEV_PAD_SINK     (0)
#define MIXER_V_SUBDEV_PAD_SOURCE   (3)
#define FIMD_SUBDEV_PAD_SINK     (0)
#define MAX_BUFFERS                 (6)

#define NUM_OF_GSC_HW               (4)
#define NODE_NUM_GSC_0              (23)
#define NODE_NUM_GSC_1              (26)
#define NODE_NUM_GSC_2              (29)
#define NODE_NUM_GSC_3              (32)

#define PFX_NODE_GSC                "/dev/video"
#define PFX_NODE_MEDIADEV         "/dev/media"
#define PFX_MXR_ENTITY              "s5p-mixer%d"
#define PFX_FIMD_ENTITY             "s3c-fb-window%d"
#define PFX_GSC_VIDEODEV_ENTITY   "exynos-gsc.%d.output"
#define PFX_GSC_SUBDEV_ENTITY     "exynos-gsc-sd.%d"
#define PFX_SUB_DEV		"/dev/v4l-subdev%d"
#define GSC_VD_PAD_SOURCE	0
#define GSC_SD_PAD_SINK	0
#define GSC_SD_PAD_SOURCE	1
#define GSC_OUT_PAD_SINK	0
//#define GSC_OUT_DMA_BLOCKING
//#define GSC_OUT_DELAYED_STREAMON


#define PFX_FIMD1_ENTITY    "s5p-fimd%d"
#define PFX_GSC_CAP_SUBDEV_ENTITY   "gsc-cap-subdev.%d"
#define PFX_GSC_CAP_VIDEODEV_ENTITY "exynos-gsc.%d.capture"
#define PFX_NODE_FB               "/dev/graphics/fb%d"
#define GSC_M2M_MAX_CTX_CNT     (100)   //arbitrary big number

#define GSC_VERSION GSC_EVT1

#if (GSC_VERSION == GSC_EVT0)
#define GSC_MIN_W_SIZE (64)
#define GSC_MIN_H_SIZE (32)
#else
#define GSC_MIN_W_SIZE (16)
#define GSC_MIN_H_SIZE (8)
#endif

#define GSC_CAP_TIMEOUT     (5000) //5sec
#define MAX_GSC_WAITING_TIME_FOR_TRYLOCK (16000) // 16msec
#define GSC_WAITING_TIME_FOR_TRYLOCK      (8000) //  8msec
#define MAX_UP_SCALING_RATIO_LIMIT  (8)
#define MAX_DOWN_SCALING_RATIO_LIMIT  (16)

struct gsc_info {
    unsigned int       width;
    unsigned int       height;
    unsigned int       crop_left;
    unsigned int       crop_top;
    unsigned int       crop_width;
    unsigned int       crop_height;
    unsigned int       v4l2_colorformat;
    unsigned int       cacheable;
    unsigned int       mode_drm;

    int                rotation;
    int                flip_horizontal;
    int                flip_vertical;

    void              *addr[NUM_OF_GSC_PLANES];
    bool               stream_on;

    enum v4l2_buf_type buf_type;
    struct v4l2_format format;
    struct v4l2_buffer buffer;
    struct v4l2_plane  planes[NUM_OF_GSC_PLANES];
    struct v4l2_crop   crop;
    int             buf_idx;
    int             qbuf_cnt;
};

struct GSC_HANDLE {
    int              gsc_fd;
    struct gsc_info  src;
    struct gsc_info  dst;
    exynos_gsc_img   src_img;
    exynos_gsc_img   dst_img;
    void            *op_mutex;
    struct media_device *media;
    struct media_entity *gsc_sd_entity;
    struct media_entity *gsc_vd_entity;
    struct media_entity *sink_sd_entity;
    int     gsc_mode;
    int     out_mode;
    int     fb_fd;
};

enum {
    GSC_TV_OUT_PROGRESSIVE = 0,
    GSC_TV_OUT_INTERLACED,
};

#ifdef __cplusplus
}
#endif

#endif //__EXYNOS_MUTEX_H__
