/*
 * Copyright (C) 2012 The Android Open Source Project
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

/*
 *
 * @author Rama, Meka(v.meka@samsung.com)
           Sangwoo, Park(sw5771.park@samsung.com)
           Jamie, Oh (jung-min.oh@samsung.com)
 * @date   2011-03-11
 *
 */

#ifndef ANDROID_EXYNOS_HWC_UTILS_H_
#define ANDROID_EXYNOS_HWC_UTILS_H_

#include <stdlib.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>

#include <ion.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <hardware/gralloc.h>

#include "linux/fb.h"

#include "s3c_lcd.h"
#include "exynos_format.h"
#include "exynos_gscaler.h"

#if defined(FIMG2D4X)
#include "FimgApi.h"
#include "sec_g2d_4x.h"
#endif

#include "ExynosHWCModule.h"

#if defined(BOARD_USES_HDMI)
#include "ExynosHdmiClient.h"
#endif

#define GSC_OUT_DEV_ID 0
#undef  USE_GSC_M2M_FOR_DRM

#if defined(BOARD_USES_HWC_FIMGAPI)
#define SUPPORT_RGB_OVERLAY
#define USE_VSYNC_FOR_RGB_OVERLAY
#endif
#define TURN_OFF_UI_WINDOW

#define GSC_VERSION            GSC_EVT1

//#define HWC_DEBUG
#define NUM_OF_WIN          (2)
#define NUM_OF_WIN_BUF      (2)
#define NUM_OF_MEM_OBJ      (1)
#define UI_WINDOW           (2)
#define GSC_M2M_WA

#if (NUM_OF_WIN_BUF < 2)
    #define ENABLE_FIMD_VSYNC
#endif

#define NOT_DEFINED         -1
#define NUM_OVLY_DELAY_FRAMES   (4)

#define PP_DEVICE_DEV_NAME  "/dev/video23"  /*GSC0*/

#define S3C_MEM_DEV_NAME "/dev/s3c-mem"

#define SKIP_DUMMY_UI_LAY_DRAWING

#define MAX_UP_SCALING_RATIO_LIMIT  (8)
#define LOCAL_MAX_DOWN_SCALING_RATIO  (4)

#ifdef SKIP_DUMMY_UI_LAY_DRAWING
#define GL_WA_OVLY_ALL
#define THRES_FOR_SWAP  (1800)    /* 60sec in Frames. 30fps * 60 = 1800 */
#endif
#define NUM_OF_DUMMY_WIN (2)

struct hwc_ui_lay_info{
    uint32_t    layer_prev_buf;
    int     layer_index;
    int     status;
};

#define CACHEABLE      1
#define NON_CACHEABLE  0

struct sec_rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

struct sec_img {
    uint32_t f_w;
    uint32_t f_h;
    uint32_t w;
    uint32_t h;
    uint32_t format;
    uint32_t base;
    uint32_t offset;
    uint32_t paddr;
    uint32_t uoffset;
    uint32_t voffset;
    int      usage;
    int      mem_id;
};

inline int SEC_MIN(int x, int y)
{
    return ((x < y) ? x : y);
}

inline int SEC_MAX(int x, int y)
{
    return ((x > y) ? x : y);
}

struct hwc_win_info_t {
    int        fd;
    int        size;
    sec_rect   rect_info;
    sec_rect   src_rect_info;
    uint32_t   phy_addr[NUM_OF_WIN_BUF];
    uint32_t   virt_addr[NUM_OF_WIN_BUF];
    int        buf_index;

    int        power_state;
    int        blending;
    int        layer_index;
    int        status;
    int        vsync;
    int        ion_fd;
    int        ovly_lay_type;
    struct fb_fix_screeninfo fix_info;
    struct fb_var_screeninfo var_info;
    struct fb_var_screeninfo lcd_info;
    int         gsc_mode;
    int         need_win_config;
    int         need_gsc_config;
    int         is_gsc_started;
};

enum {
    HWC_YUV_OVLY = 0,
    HWC_RGB_OVLY,
};

enum {
    HWC_WIN_FREE = 0,
    HWC_WIN_RESERVED,
};

enum {
    HWC_UNKNOWN_MEM_TYPE = 0,
    HWC_PHYS_MEM_TYPE,
    HWC_VIRT_MEM_TYPE,
};

struct hwc_context_t {
    hwc_composer_device_t device;

    /* our private state goes below here */
    struct hwc_win_info_t     win[NUM_OF_WIN];
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
    struct hwc_ui_lay_info  win_virt[NUM_OF_DUMMY_WIN];
    int         fb_lay_skip_initialized;
    int         num_of_fb_lay_skip;
#ifdef GL_WA_OVLY_ALL
    int         ui_skip_frame_cnt;
#endif
#endif
    struct hwc_win_info_t     ui_win;
    struct fb_var_screeninfo  lcd_info;
    int            num_of_fb_layer;
    int            num_of_hwc_layer;
    int            num_of_fb_layer_prev;
    int            gsc_mode;
    void           *gsc_handle;
    int            num_2d_blit_layer;
    uint32_t    layer_prev_buf[NUM_OF_WIN];
    int           dis_rect_changed;
    int         gsc_out_max_down_scale_ratio;
#if defined(BOARD_USES_HDMI)
    android::ExynosHdmiClient    *mHdmiClient;
    int                       mHdmiCableStatus;
    int                       num_of_s3d_layer;
    int                       num_of_protected_layer;
    int                       num_of_ext_disp_layer;
    int                       num_of_ext_disp_video_layer;
    int                       hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_MAX];
#endif

    int                       num_of_yuv_layers;
#ifdef SUPPORT_RGB_OVERLAY
    int                       is_rgb_ovly_ok;
    int                       ovly_bandwidth;
#endif
#ifdef GRALLOC_MOD_ACCESS
    gralloc_module_public_t *psGrallocModule;
#endif
#ifdef  VSYNC_THREAD_ENABLE
    hwc_procs_t               *procs;
    pthread_t                 vsync_thread;
#endif
    int     need_to_try_overlay;
    int     overaly_delay_frames;
};

typedef enum _LOG_LEVEL {
    HWC_LOG_DEBUG,
    HWC_LOG_WARNING,
    HWC_LOG_ERROR,
} HWC_LOG_LEVEL;

#define SEC_HWC_LOG_TAG     "SECHWC_LOG"

#ifdef HWC_DEBUG
#define SEC_HWC_Log(a, ...)    ((void)_SEC_HWC_Log(a, SEC_HWC_LOG_TAG, __VA_ARGS__))
#else
#define SEC_HWC_Log(a, ...)                                         \
    do {                                                            \
        if (a == HWC_LOG_ERROR)                                     \
            ((void)_SEC_HWC_Log(a, SEC_HWC_LOG_TAG, __VA_ARGS__)); \
    } while (0)
#endif

extern void _SEC_HWC_Log(HWC_LOG_LEVEL logLevel, const char *tag, const char *msg, ...);

int window_open       (struct hwc_win_info_t *win, int id);
int window_close      (struct hwc_win_info_t *win);
int window_set_pos    (struct hwc_win_info_t *win);
int window_get_info   (struct hwc_win_info_t *win, int win_num);
int window_pan_display(struct hwc_win_info_t *win);
int window_show       (struct hwc_win_info_t *win);
int window_hide       (struct hwc_win_info_t *win);
int window_get_global_lcd_info(int fd, struct fb_var_screeninfo *lcd_info);

#if defined(FIMG2D4X)
int runCompositor(struct hwc_context_t *ctx,
        struct sec_img *src_img, struct sec_rect *src_rect,
        struct sec_img *dst_img, struct sec_rect *dst_rect,
        uint32_t transform, uint32_t global_alpha,
        unsigned long solid, blit_op mode, addr_space addr_type);
#endif
#endif /* ANDROID_SEC_HWC_UTILS_H_*/
