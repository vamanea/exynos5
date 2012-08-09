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
**
** @author Sangwoo, Park(sw5771.park@samsung.com)
** @date   2010-09-10
**
*/
#ifndef __SEC_HDMI_COMMON_H__
#define __SEC_HDMI_COMMON_H__

#include <cutils/log.h>

#define BOARD_USES_EDID
//#define BOARD_USES_CEC
#define SUPPORT_G2D_UI_MODE
//#define USE_MEMCPY_USERPTR_GSC
#define DEFAULT_UI_PATH          HDMI_PATH_WRITEBACK

#define DEFAULT_FB_INDEX            (0)
#define DEFAULT_GSC_OUT_INDEX       (3)
#define DEFAULT_GSC_CAP_INDEX       (3)
#define MAX_MIXER_NUM               (1)
#define MAX_BUFFERS_MIXER           (3)

#define NUM_SUPPORTED_RESOLUTION_2D      (10)
#define NUM_SUPPORTED_RESOLUTION_S3D_TB  (4)
#define NUM_SUPPORTED_RESOLUTION_S3D_SBS (3)

#define MAX_BUFFERS_GSCALER_OUT     (3)
#define MAX_BUFFERS_GSCALER_CAP     (1)
#define HDMI_VIDEO_BUFFER_BPP_SIZE  (1.5)
#define HDMI_UI_BUFFER_BPP_SIZE     (4)
#define HDMI_MAX_WIDTH              (1920)
#define HDMI_MAX_HEIGHT             (1080)

#define ALIGN(x, a)    (((x) + (a) - 1) & ~((a) - 1))
#define ROUND_DOWN(value, boundary)     (((uint32_t)(value)) & \
                                        (~(((uint32_t) boundary)-1)))
#define ROUND_UP(value, boundary)       ((((uint32_t)(value)) + \
                                        (((uint32_t) boundary)-1)) & \
                                        (~(((uint32_t) boundary)-1)))

#if defined(STD_1080P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_RGB)
    #define DEFAULT_HDMI_RESOLUTION_VALUE         (1080960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB  (1080960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS (1080960)
    #define DEFAULT_HDMI_DV_ID            (V4L2_DV_1080P60)
    #define DEFALULT_DISPLAY_WIDTH        (1920)
    #define DEFALULT_DISPLAY_HEIGHT       (1080)
#elif defined(STD_720P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE         (720960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB  (720960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS (720960)
    #define DEFAULT_HDMI_DV_ID            (V4L2_DV_720P60)
    #define DEFALULT_DISPLAY_WIDTH        (1280)
    #define DEFALULT_DISPLAY_HEIGHT       (720)
#elif defined(STD_480P)
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE         (4809601)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB  (720960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS (720960)
    #define DEFAULT_HDMI_DV_ID            (V4L2_DV_480P60)
    #define DEFALULT_DISPLAY_WIDTH        (720)
    #define DEFALULT_DISPLAY_HEIGHT       (480)
#else
    #define DEFAULT_OUPUT_MODE            (HDMI_OUTPUT_MODE_YCBCR)
    #define DEFAULT_HDMI_RESOLUTION_VALUE         (4809602)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB  (720960)
    #define DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS (720960)
    #define DEFAULT_HDMI_DV_ID            (V4L2_DV_480P60)
    #define DEFALULT_DISPLAY_WIDTH        (720)
    #define DEFALULT_DISPLAY_HEIGHT       (480)
#endif

enum hdp_cable_status {
    HPD_CABLE_OUT = 0, // HPD_CABLE_OUT indicates HDMI cable out.
    HPD_CABLE_IN       // HPD_CABLE_IN indicates HDMI cable in.
};

enum state {
    OFF = 0,
    ON = 1,
    NOT_SUPPORT = 2,
};

enum tv_mode {
    HDMI_OUTPUT_MODE_YCBCR = 0,
    HDMI_OUTPUT_MODE_RGB = 1,
    HDMI_OUTPUT_MODE_DVI = 2,
};

enum hdmi_layer {
    HDMI_LAYER_BASE   = 0,
    HDMI_LAYER_VIDEO,
    HDMI_LAYER_GRAPHIC_0,
    HDMI_LAYER_GRAPHIC_1,
    HDMI_LAYER_MAX,
};

enum hdmi_path {
    HDMI_PATH_OVERLAY = 0,
    HDMI_PATH_WRITEBACK,
    HDMI_PATH_MAX,
};

enum hdmi_s3d_mode {
    HDMI_2D = 0,
    HDMI_S3D_TB,
    HDMI_S3D_SBS,
};

enum hdmi_drm {
    HDMI_NON_DRM_MODE = 0,
    HDMI_DRM_MODE,
};

#endif
