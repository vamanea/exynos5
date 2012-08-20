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

#ifndef __EXYNOS_HDMI_UTILS_H__
#define __EXYNOS_HDMI_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

namespace android {
#include "videodev2.h"
#include "videodev2_exynos_media.h"
#include "ExynosMutex.h"
#include "exynos_v4l2.h"
#include "exynos_format.h"
#include "exynos_gscaler.h"

//#define MXR_DELAYED_STREAMON
#define PFX_NODE_MEDIADEV         "/dev/media"
#define PFX_NODE_SUBDEV           "/dev/v4l-subdev"
#define PFX_NODE_VIDEODEV         "/dev/video"

#define PFX_MXR_SUBDEV_ENTITY     "s5p-mixer%d"
#define PFX_MXR_VIDEODEV_ENTITY   "mxr%d_graph%d"
#define PFX_NODE_FB               "/dev/graphics/fb%d"

#define MIXER_G0_SUBDEV_PAD_SINK    (1)
#define MIXER_G0_SUBDEV_PAD_SOURCE  (4)
#define MIXER_G1_SUBDEV_PAD_SINK    (2)
#define MIXER_G1_SUBDEV_PAD_SOURCE  (5)

#define NUM_OF_MXR_PLANES           (1)

#define V4L2_OUTPUT_TYPE_DIGITAL        10
#define V4L2_OUTPUT_TYPE_HDMI           V4L2_OUTPUT_TYPE_DIGITAL
#define V4L2_OUTPUT_TYPE_HDMI_RGB       11
#define V4L2_OUTPUT_TYPE_DVI            12

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t fw;
    uint32_t fh;
    uint32_t format;
    uint32_t addr;
    uint32_t blending;
} exynos_mxr_img;

struct mxr_info {
    unsigned int       width;
    unsigned int       height;
    unsigned int       crop_left;
    unsigned int       crop_top;
    unsigned int       crop_width;
    unsigned int       crop_height;
    unsigned int       v4l2_colorformat;

    void              *addr[NUM_OF_MXR_PLANES];

    struct v4l2_format format;
    struct v4l2_buffer buffer;
    struct v4l2_plane  planes[NUM_OF_MXR_PLANES];
    struct v4l2_crop   crop;
};

struct MXR_HANDLE {
    int              mxr_fd;
    unsigned int     layer_id;
    struct mxr_info  src;
    struct mxr_info  dst;

    enum v4l2_buf_type buf_type;
    bool               stream_on;
    int                buf_idx;
    int                qbuf_cnt;

    exynos_mxr_img   src_img;
    exynos_mxr_img   dst_img;
    void            *op_mutex;
    struct media_device *media;
    struct media_entity *mxr_sd_entity;
    struct media_entity *mxr_vd_entity;
    int    out_mode;
};

void *exynos_mxr_create(
    int dev_num,
    unsigned int layer_id);

void exynos_mxr_destroy(
    void *handle);

int exynos_mxr_config(
    void *handle,
    exynos_mxr_img *src_img,
    exynos_mxr_img *dst_img,
    int out_mode);

int exynos_mxr_run(void *handle,
    unsigned int yAddr);

int exynos_mxr_wait_done (
    void *handle);

int exynos_mxr_stop_n_clear(
    void *handle);

int exynos_mxr_just_stop (
    void *handle);

int exynos_mxr_set_ctrl (
    void *handle,
    unsigned int id,
    int value);

int exynos_mxr_get_ctrl (
    void *handle,
    unsigned int id,
    int *value);

void display_menu(void);

int tvout_init(int fd_tvout, __u32 preset_id);
int tvout_v4l2_enum_output(int fp, struct v4l2_output *output);
int tvout_v4l2_s_output(int fp, int index);
int tvout_v4l2_g_output(int fp, int *index);
int tvout_std_v4l2_enum_dv_presets(int fd);
int tvout_std_v4l2_s_dv_preset(int fd, struct v4l2_dv_preset *preset);

void hdmi_cal_rect(int src_w, int src_h, int dst_w, int dst_h, struct v4l2_rect *dst_rect);

int hdmi_captureRun_byGSC(
        unsigned int dst_address,
        void *gsc_cap_handle);

int hdmi_Blit_byG2D(
        int srcColorFormat,
        int src_w,
        int src_h,
        unsigned int src_address,
        int dst_w,
        int dst_h,
        unsigned int dst_address,
        int rotVal);

int hdmi_outputmode_2_v4l2_output_type(int output_mode);
int hdmi_v4l2_output_type_2_outputmode(int v4l2_output_type);

int hdmi_check_output_mode(int v4l2_output_type);
int hdmi_check_resolution(unsigned int dv_id);

int hdmi_resolution_2_preset_id(unsigned int resolution, unsigned int s3dMode, int *w, int *h, __u32 *preset_id);

#if 0 // Before activate this code, check the driver support, first.
int hdmi_enable_hdcp(int fd, unsigned int hdcp_en);
int hdmi_check_audio(int fd);
#endif

bool hdmi_check_interlaced_resolution(unsigned int dv_id);
bool getFrameSize(int V4L2_PIX, unsigned int * size, unsigned int frame_size);
#ifdef __cplusplus
}
#endif

}  //namespace android

#endif
