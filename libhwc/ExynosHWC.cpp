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
           Jamie Oh (jung-min.oh@samsung.com)
 * @date   2011-03-11
 *
 */

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <EGL/egl.h>

#include "ExynosHWCUtils.h"
#include <pthread.h>
#include <GLES/gl.h>

//#define CHECK_EGL_FPS
#ifdef CHECK_EGL_FPS
extern void check_fps();
#endif

#if defined(BOARD_USES_HDMI)
#include "ExynosHdmiClient.h"
#include "ExynosTVOutService.h"
#include "ExynosHdmi.h"

static int lcd_width, lcd_height;
static int prev_usage = 0;
static int  usage_3d = NOT_DEFINED;
static bool is_same_3d_usage = true;

#define CHECK_TIME_DEBUG 0
#endif

#ifdef  VSYNC_THREAD_ENABLE
#include <sys/resource.h>
#include <hardware_legacy/uevent.h>
#endif

#ifdef SKIP_DUMMY_UI_LAY_DRAWING
static void get_hwc_ui_lay_skipdraw_decision(struct hwc_context_t* ctx, hwc_layer_list_t* list)
{
    private_handle_t *prev_handle;
    hwc_layer_t* cur;
    int num_of_fb_lay_skip = 0;
    int fb_lay_tot = ctx->num_of_fb_layer + ctx->num_of_fb_lay_skip ;

    if (fb_lay_tot > NUM_OF_DUMMY_WIN)
        return;

    if(fb_lay_tot < 1) {
#ifdef GL_WA_OVLY_ALL
        ctx->ui_skip_frame_cnt++;
        if (ctx->ui_skip_frame_cnt >= THRES_FOR_SWAP) {
            ctx->ui_skip_frame_cnt = 0;
            ctx->num_of_fb_layer_prev = 1;
        }
#endif
        return;
    }
#if defined(SUPPORT_RGB_OVERLAY)
    if (ctx->is_rgb_ovly_ok)
        return;

#endif
    if (ctx->fb_lay_skip_initialized) {
        for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
            cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
            if (ctx->win_virt[cnt].layer_prev_buf == (uint32_t)cur->handle)
                num_of_fb_lay_skip++;
        }
#ifdef GL_WA_OVLY_ALL
        if (ctx->ui_skip_frame_cnt >= THRES_FOR_SWAP) {
            num_of_fb_lay_skip = 0;
            SEC_HWC_Log(HWC_LOG_DEBUG, "#####GL_WA_OVLY_ALL::::THRES_FOR_SWAP");
        }
#endif
        if (num_of_fb_lay_skip != fb_lay_tot) {
            ctx->num_of_fb_layer =  fb_lay_tot;
            ctx->num_of_fb_lay_skip = 0;
#ifdef GL_WA_OVLY_ALL
            ctx->ui_skip_frame_cnt = 0;
#endif
            for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
                cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
                ctx->win_virt[cnt].layer_prev_buf = (uint32_t)cur->handle;
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->win_virt[cnt].status = HWC_WIN_FREE;
            }
        } else {
            ctx->num_of_fb_layer = 0;
            ctx->num_of_fb_lay_skip = fb_lay_tot;
#ifdef GL_WA_OVLY_ALL
            ctx->ui_skip_frame_cnt++;
#endif
            for (int cnt = 0; cnt < fb_lay_tot; cnt++) {
                cur = &list->hwLayers[ctx->win_virt[cnt].layer_index];
                cur->compositionType = HWC_OVERLAY;
                ctx->win_virt[cnt].status = HWC_WIN_RESERVED;
            }
        }
    } else {
        ctx->num_of_fb_lay_skip = 0;
        for  (int i = 0; i < list->numHwLayers ; i++) {
            if(num_of_fb_lay_skip >= NUM_OF_DUMMY_WIN)
                break;

            cur = &list->hwLayers[i];
            if (cur->handle) {
                prev_handle = (private_handle_t *)(cur->handle);
                switch (prev_handle->format) {
                case HAL_PIXEL_FORMAT_RGBA_8888:
                case HAL_PIXEL_FORMAT_BGRA_8888:
                case HAL_PIXEL_FORMAT_RGBX_8888:
                case HAL_PIXEL_FORMAT_RGB_565:
#ifdef EXYNOS_SUPPORT_BGRX_8888
                case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
                    cur->compositionType = HWC_FRAMEBUFFER;
                    ctx->win_virt[num_of_fb_lay_skip].layer_prev_buf = (uint32_t)cur->handle;
                    ctx->win_virt[num_of_fb_lay_skip].layer_index = i;
                    ctx->win_virt[num_of_fb_lay_skip].status = HWC_WIN_FREE;
                    num_of_fb_lay_skip++;
                break;
                default:
                break;
                }
            } else {
                cur->compositionType = HWC_FRAMEBUFFER;
            }
        }
        if (num_of_fb_lay_skip == fb_lay_tot)
            ctx->fb_lay_skip_initialized = 1;
    }

    return;
}
#endif

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
#ifdef  VSYNC_THREAD_ENABLE
        module_api_version: HWC_MODULE_API_VERSION_0_1,
        hal_api_version: HARDWARE_HAL_API_VERSION,
#else
        version_major: 1,
        version_minor: 0,
#endif
        id: HWC_HARDWARE_MODULE_ID,
        name: "Samsung EXYNOS5XXX hwcomposer module",
        author: "SAMSUNG",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_t const* l) {
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, "
            "{%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

void calculate_rect(struct hwc_win_info_t *win, hwc_layer_t *cur,
        sec_rect *rect)
{
    rect->x = cur->displayFrame.left;
    rect->y = cur->displayFrame.top;
    rect->w = cur->displayFrame.right - cur->displayFrame.left;
    rect->h = cur->displayFrame.bottom - cur->displayFrame.top;

    if (rect->x < 0) {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres;
        else
            rect->w = rect->w + rect->x;
        rect->x = 0;
    } else {
        if (rect->w + rect->x > win->lcd_info.xres)
            rect->w = win->lcd_info.xres - rect->x;
    }
    if (rect->y < 0) {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres;
        else
            rect->h = rect->h + rect->y;
        rect->y = 0;
    } else {
        if (rect->h + rect->y > win->lcd_info.yres)
            rect->h = win->lcd_info.yres - rect->y;
    }
}

static int set_src_dst_img_rect(hwc_layer_t *cur,
        struct hwc_win_info_t *win,
        struct sec_img *src_img,
        struct sec_img *dst_img,
        struct sec_rect *src_rect,
        struct sec_rect *dst_rect,
        int win_idx)
{
    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    sec_rect rect;

    /* 1. Set src_img from prev_handle */
    src_img->f_w     = prev_handle->width;
    src_img->f_h     = prev_handle->height;
    src_img->w       = prev_handle->width;
    src_img->h       = prev_handle->height;
    src_img->format  = prev_handle->format;
    src_img->base    = EXYNOS_GET_SRC_ADDR(prev_handle);
    src_img->offset  = 0;
    src_img->mem_id  = 0;
    src_img->paddr  = 0;
    src_img->usage  = prev_handle->usage;
    src_img->uoffset  = prev_handle->uoffset;
    src_img->voffset  = prev_handle->voffset;

    switch (src_img->format) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        src_img->f_w = (src_img->f_w + 15) & ~15;
        src_img->f_h = (src_img->f_h + 15) & ~15;
        break;
    default:
        src_img->f_w = prev_handle->stride;
        src_img->f_h = src_img->h;
        break;
    }

    if (src_img->format == HAL_PIXEL_FORMAT_YV12)
        src_img->f_w = (prev_handle->stride + 15) & ~15;

    /* 2. Set dst_img from window(lcd) */
    calculate_rect(win, cur, &rect);
    dst_img->f_w = (win->lcd_info.xres + 15) & ~15;
    dst_img->f_h = (win->lcd_info.yres + 7) & ~7;
    dst_img->w = rect.w;
    dst_img->h = rect.h;

    switch (win->lcd_info.bits_per_pixel) {
    case 32:
        dst_img->format = HAL_PIXEL_FORMAT_RGBA_8888;
        break;
    default:
        dst_img->format = HAL_PIXEL_FORMAT_RGB_565;
        break;
    }

    dst_img->base     = win->virt_addr[win->buf_index];
    dst_img->offset   = 0;
    dst_img->mem_id   = 0;

    src_rect->x = SEC_MAX(cur->sourceCrop.left, 0);
    src_rect->y = SEC_MAX(cur->sourceCrop.top, 0);
    src_rect->w = SEC_MAX(cur->sourceCrop.right - cur->sourceCrop.left, 0);
    src_rect->w = SEC_MIN(src_rect->w, src_img->f_w - src_rect->x);
    src_rect->h = SEC_MAX(cur->sourceCrop.bottom - cur->sourceCrop.top, 0);
    src_rect->h = SEC_MIN(src_rect->h, src_img->f_h - src_rect->y);

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "crop information()::"
            "sourceCrop left(%d),top(%d),right(%d),bottom(%d),"
            "src_rect x(%d),y(%d),w(%d),h(%d),"
            "prev_handle w(%d),h(%d)",
            cur->sourceCrop.left,
            cur->sourceCrop.top,
            cur->sourceCrop.right,
            cur->sourceCrop.bottom,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            prev_handle->width, prev_handle->height);

    /* 4. Set dst_rect(fb or lcd)
     *    GSC dst image will be stored from left top corner
     */
    if ((win->gsc_mode != GSC_OUTPUT_MODE) ||
        (win->ovly_lay_type == HWC_RGB_OVLY)){
        dst_rect->x = 0;
        dst_rect->y = 0;
    } else {
        dst_rect->x = win->rect_info.x;
        dst_rect->y = win->rect_info.y;
    }

    if (cur->transform & HAL_TRANSFORM_ROT_90) {
        dst_rect->w = win->rect_info.w;
        dst_rect->h = (win->rect_info.h + 1) & (~1);
        dst_rect->h = SEC_MIN(dst_rect->h, dst_img->f_h - dst_rect->y);
    } else {
        dst_rect->w = (win->rect_info.w + 1) & (~1);
        dst_rect->w = SEC_MIN(dst_rect->w, dst_img->f_w - dst_rect->x);
        dst_rect->h = win->rect_info.h;
    }

    /* Summery */
    SEC_HWC_Log(HWC_LOG_DEBUG,
            "set_src_dst_img_rect()::"
            "SRC w(%d),h(%d),f_w(%d),f_h(%d),fmt(0x%x),"
            "base(0x%x),offset(%d),paddr(0x%X)=>\r\n"
            "   DST w(%d),h(%d),f(0x%x),base(0x%x),"
            "offset(%d),mem_id(%d),"
            "rot(%d),win_idx(%d)"
            "   SRC_RECT x(%d),y(%d),w(%d),h(%d)=>"
            "DST_RECT x(%d),y(%d),w(%d),h(%d)",
            src_img->w, src_img->h, src_img->f_w, src_img->f_h, src_img->format,
            src_img->base, src_img->offset, src_img->paddr,
            dst_img->w, dst_img->h,  dst_img->format, dst_img->base,
            dst_img->offset, dst_img->mem_id,
            cur->transform, win_idx,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);

    return 0;
}

static int check_yuv_format(unsigned int color_format) {
    switch (color_format) {
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_422_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCbCr_420_I:
    case HAL_PIXEL_FORMAT_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CbYCrY_420_I:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CbYCrY_422_I:
    case HAL_PIXEL_FORMAT_CUSTOM_CrYCbY_422_I:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        return 1;
    default:
        return 0;
    }
}

#if defined(SUPPORT_RGB_OVERLAY)
static int get_hwc_rgb_ovly_type(struct hwc_context_t *ctx, hwc_layer_t* cur)
{
    unsigned int bandwidth = ctx->ovly_bandwidth;
    int compositionType = HWC_FRAMEBUFFER;

#ifdef RGB_OVERLAY_BW_CHECK
    /* Check-1 : Bandwidth Limitation */
    bandwidth += (cur->displayFrame.right - cur->displayFrame.left) *
                (cur->displayFrame.bottom - cur->displayFrame.top);
    if (bandwidth <=  (ctx->win[0].lcd_info.xres * ctx->win[0].lcd_info.yres)) {
        ctx->ovly_bandwidth = bandwidth;
        compositionType = HWC_OVERLAY;
    }
    else
        return HWC_FRAMEBUFFER;

    return (compositionType);
#endif

    if (cur->transform != 0)
        return HWC_FRAMEBUFFER;

    /* Check-2 : Scaling Limitation */
    switch (cur->transform) {
    case 0:
    case HAL_TRANSFORM_ROT_180:
        if (((cur->sourceCrop.bottom - cur->sourceCrop.top) !=
              (cur->displayFrame.bottom - cur->displayFrame.top)) ||
             ((cur->sourceCrop.right - cur->sourceCrop.left) !=
              (cur->displayFrame.right - cur->displayFrame.left)))
            return HWC_FRAMEBUFFER;
        else
            compositionType = HWC_OVERLAY;
    break;
    case HAL_TRANSFORM_ROT_90:
    case HAL_TRANSFORM_ROT_270:
        if (((cur->sourceCrop.bottom - cur->sourceCrop.top) !=
              (cur->displayFrame.right - cur->displayFrame.left)) ||
             ((cur->sourceCrop.right - cur->sourceCrop.left) !=
              (cur->displayFrame.bottom - cur->displayFrame.top)))
            return HWC_FRAMEBUFFER;
        else
            compositionType = HWC_OVERLAY;
    break;
    }
        return compositionType;
}
#endif

static int gsc_check_src_align(int h_ratio, int v_ratio, int w, int h)
{
    int h_align = 0;
    int v_align = 0;

    if (h_ratio <= 4)
        h_align = ((w & 1) == 0);
    else if (h_ratio <= 8)
        h_align = ((w & 3) == 0);
    else if (h_ratio <= 16)
        h_align = ((w & 7) == 0);
    else
        h_align = 0;

    if (v_ratio <= 4)
        v_align = ((h & 1) == 0);
    else if (v_ratio <= 8)
        v_align = ((h & 3) == 0);
    else if (v_ratio <= 16)
        v_align = ((h & 7) == 0);
    else
        v_align = 0;

    if (h_align && v_align)
        return 1;

    return 0;
}

#if defined(BOARD_USES_HDMI)
static int check_gralloc_usage_flags(struct hwc_context_t *ctx, hwc_layer_list_t* list)
{
    /* initialization about external display*/
    ctx->num_of_ext_disp_layer = 0;
    ctx->num_of_ext_disp_video_layer = 0;

    ctx->num_of_s3d_layer = 0;
    ctx->num_of_protected_layer = 0;

    usage_3d = NOT_DEFINED;
    is_same_3d_usage = true;

    for (int i = 0; i < android::ExynosHdmiClient::HDMI_LAYER_MAX; i++)
        ctx->hdmi_layer_buf_index[i] = NOT_DEFINED;

    for (int i = 0; i < list->numHwLayers ; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        private_handle_t *prev_handle = NULL;
        if (cur->handle) {
            prev_handle = (private_handle_t *)(cur->handle);
            /* 1. check the number of layer for external(HDMI or WIFI common) */
            if (prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP) {
                ctx->num_of_ext_disp_layer++;
                ctx->num_of_ext_disp_video_layer++;
                if (prev_handle->usage & GRALLOC_USAGE_PROTECTED)
                    ctx->num_of_protected_layer++;
            }
            if ((prev_handle->usage & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
                (prev_handle->usage & GRALLOC_USAGE_PRIVATE_SBS_RL) ||
                (prev_handle->usage & GRALLOC_USAGE_PRIVATE_TB_LR) ||
                (prev_handle->usage & GRALLOC_USAGE_PRIVATE_TB_RL)) {
                if ((usage_3d != NOT_DEFINED) && (usage_3d != prev_handle->usage))
                    is_same_3d_usage = false;

                usage_3d = prev_handle->usage;
                ctx->num_of_s3d_layer++;
            }
        }
    }
    return 0;
}
#endif

static int get_hwc_compos_decision(struct hwc_context_t *ctx, hwc_layer_t* cur, int win_cnt)
{
    if (cur->flags & HWC_SKIP_LAYER  || !cur->handle) {
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::is_skip_layer  %d  cur->handle %x ",
                __func__, cur->flags & HWC_SKIP_LAYER, cur->handle);

        return HWC_FRAMEBUFFER;
    }

    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    int compositionType = HWC_FRAMEBUFFER;
    struct hwc_win_info_t *win = &ctx->win[win_cnt];

    if (ctx->gsc_handle == NULL)
        return compositionType;

#ifdef GSC_M2M_WA
    if ((win_cnt > 0) && (ctx->num_of_yuv_layers > 1)) {
        return compositionType;
    }
#endif

#ifdef GSC_DST_1PLANE_NOT_SUPPORTED
    if ((ctx->gsc_mode == GSC_M2M_MODE) && ctx->num_of_yuv_layers)
        return compositionType;
#endif

    if ((cur->displayFrame.left < 0) || (cur->displayFrame.right > win->lcd_info.xres) ||
          (cur->displayFrame.top < 0) || (cur->displayFrame.bottom > win->lcd_info.yres) ||
         ((cur->displayFrame.right - cur->displayFrame.left) > win->lcd_info.xres) ||
         ((cur->displayFrame.bottom - cur->displayFrame.top) > win->lcd_info.yres))
         return compositionType;

    /* check here....GSC resolution constraints */
    /* SRC_FW >= 16 & SRC_FH >= 8 */
    if (((cur->sourceCrop.right - cur->sourceCrop.left) < 16) ||
        ((cur->sourceCrop.bottom - cur->sourceCrop.top) < 8))
        return compositionType;

    /* DST_FW >= 16 & DST_FH >= 8 */
    if ((cur->transform == HAL_TRANSFORM_ROT_90) ||
        (cur->transform == HAL_TRANSFORM_ROT_270)) {
        if (((cur->displayFrame.right - cur->displayFrame.left) < 8) ||
            ((cur->displayFrame.bottom - cur->displayFrame.top) < 16))
            return compositionType;
    } else if (((cur->displayFrame.right - cur->displayFrame.left) < 16) ||
               ((cur->displayFrame.bottom - cur->displayFrame.top) < 8)) {
        return compositionType;
    }

    /* check upsacling limit, the scaling limit must not be more than MAX_RESIZING_RATIO_LIMIT */
    int width_scale_ratio;
    int height_scale_ratio;
    if ((cur->transform & HAL_TRANSFORM_ROT_90) ||
        (cur->transform & HAL_TRANSFORM_ROT_270)) {
        if ((((cur->sourceCrop.right - cur->sourceCrop.left) * MAX_UP_SCALING_RATIO_LIMIT) <
            (cur->displayFrame.bottom - cur->displayFrame.top)) ||
            (((cur->sourceCrop.bottom - cur->sourceCrop.top) * MAX_UP_SCALING_RATIO_LIMIT) <
            (cur->displayFrame.right - cur->displayFrame.left))) {
                return compositionType;
            }
    } else {
        if ((((cur->sourceCrop.right - cur->sourceCrop.left) * MAX_UP_SCALING_RATIO_LIMIT) <
            (cur->displayFrame.right - cur->displayFrame.left) ) ||
            (((cur->sourceCrop.bottom - cur->sourceCrop.top) * MAX_UP_SCALING_RATIO_LIMIT) <
            (cur->displayFrame.bottom - cur->displayFrame.top))) {
                return compositionType;
            }
    }

    /* Alignment Constraints */
    int src_real_w = cur->sourceCrop.right - cur->sourceCrop.left;
    int src_real_h = cur->sourceCrop.bottom - cur->sourceCrop.top;
    if ((cur->transform == HAL_TRANSFORM_ROT_90) ||
        (cur->transform == HAL_TRANSFORM_ROT_270)) {
        width_scale_ratio = src_real_w / (cur->displayFrame.bottom - cur->displayFrame.top);
        height_scale_ratio = src_real_h /(cur->displayFrame.right - cur->displayFrame.left);
    } else {
        width_scale_ratio = src_real_w / (cur->displayFrame.right - cur->displayFrame.left);
        height_scale_ratio = src_real_h /(cur->displayFrame.bottom - cur->displayFrame.top);
    }

    switch (prev_handle->format) {
    case HAL_PIXEL_FORMAT_YV12:                 /* YCrCb_420_P */
    case HAL_PIXEL_FORMAT_YCbCr_420_P:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        if ((prev_handle->usage & GRALLOC_USAGE_HWC_HWOVERLAY) &&
            (cur->blending == HWC_BLENDING_NONE)) {
            if (gsc_check_src_align(width_scale_ratio, height_scale_ratio, src_real_w, src_real_h))
                compositionType = HWC_OVERLAY;
            else
                compositionType = HWC_FRAMEBUFFER;
        } else
            compositionType = HWC_FRAMEBUFFER;
        break;
#if defined(SUPPORT_RGB_OVERLAY)
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBA_4444:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        if (ctx->is_rgb_ovly_ok) {
            compositionType = HWC_OVERLAY;
        } else
            compositionType = HWC_FRAMEBUFFER;
        break;
#endif
    default:
        compositionType = HWC_FRAMEBUFFER;
        break;
    }

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s::compositionType(%d)=>0:FB,1:OVERLAY \r\n"
            "format(0x%x)"
            "b_addr(0x%x),usage(%d),w(%d),h(%d),bpp(%d)",
            "get_hwc_compos_decision()", compositionType,
            prev_handle->format, prev_handle->base,
            prev_handle->usage, prev_handle->width, prev_handle->height,
            prev_handle->bpp);

    return  compositionType;
}

static void reset_win_rect_info(hwc_win_info_t *win)
{
    win->rect_info.x = 0;
    win->rect_info.y = 0;
    win->rect_info.w = 0;
    win->rect_info.h = 0;
    return;
}

static int assign_overlay_window(struct hwc_context_t *ctx, hwc_layer_t *cur,
        int win_idx, int layer_idx)
{
    struct hwc_win_info_t   *win;
    private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
    sec_rect   rect;
    int ret = 0;

    if (NUM_OF_WIN <= win_idx)
        return -1;

    win = &ctx->win[win_idx];

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: left(%d),top(%d),right(%d),bottom(%d),transform(%d)"
            "lcd_info.xres(%d),lcd_info.yres(%d)",
            "++assign_overlay_window()",
            cur->displayFrame.left, cur->displayFrame.top,
            cur->displayFrame.right, cur->displayFrame.bottom, cur->transform,
            win->lcd_info.xres, win->lcd_info.yres);

    win->ovly_lay_type = HWC_YUV_OVLY;
#ifdef SUPPORT_RGB_OVERLAY
    if (check_yuv_format(prev_handle->format) == 0) {
        win->ovly_lay_type = HWC_RGB_OVLY;
    }
#endif

    calculate_rect(win, cur, &rect);

    if ((rect.x != win->rect_info.x) || (rect.y != win->rect_info.y) ||
        (rect.w != win->rect_info.w) || (rect.h != win->rect_info.h)) {
        if (prev_handle->usage & GRALLOC_USAGE_CAMERA) {
            if ((ctx->layer_prev_buf[win_idx] != (uint32_t)cur->handle) &&
                (win->gsc_mode == GSC_OUTPUT_MODE) && ctx->dis_rect_changed) {
                return 1;
            }
        } else if (win->ovly_lay_type == HWC_YUV_OVLY) {
            if ((win->rect_info.w == rect.w) && (win->rect_info.h == rect.h) &&
                (win->rect_info.x == (rect.x & (~1))) &&
                (win->rect_info.y == (rect.y & (~1)))) {
                rect.x = win->rect_info.x;
                rect.y = win->rect_info.y;
                win->layer_index = layer_idx;
                win->status = HWC_WIN_RESERVED;
                return 0;
            } else {
                rect.x = (rect.x + 1) & (~1);
                rect.y = (rect.y + 1) & (~1);
                rect.w = (rect.w + 1) & (~1);
            }
        }

        if (win->power_state) {
            ctx->need_to_try_overlay = 1;
            return 1;
        }
        ctx->need_to_try_overlay = 0;
        win->src_rect_info.x = cur->sourceCrop.left;
        win->src_rect_info.y = cur->sourceCrop.top;
        win->src_rect_info.w = cur->sourceCrop.right - cur->sourceCrop.left;
        win->src_rect_info.h = cur->sourceCrop.bottom - cur->sourceCrop.top;

        SEC_HWC_Log(HWC_LOG_DEBUG,"disp co-ordinates are changed [%d %d %d %d] to [%d %d %d %d]",
            win->rect_info.x,  win->rect_info.y, win->rect_info.w, win->rect_info.h,
            rect.x, rect.y, rect.w, rect.h);
        win->rect_info.x = rect.x;
        win->rect_info.y = rect.y;
        win->rect_info.w = rect.w;
        win->rect_info.h = rect.h;

        win->need_win_config = 1;
        win->need_gsc_config = 1;
        if (win->is_gsc_started) {
            win->is_gsc_started = 0;
            ret = exynos_gsc_stop_exclusive(ctx->gsc_handle);
            if (ret < 0) {
                SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : exynos_gsc_out_stop", __func__);
            }
        }

        if(!(prev_handle->usage & GRALLOC_USAGE_CAMERA))
            ctx->layer_prev_buf[win_idx] = 0;
    } else if ((win->src_rect_info.x != cur->sourceCrop.left) ||(win->src_rect_info.y!= cur->sourceCrop.top) ||
        (win->src_rect_info.w != (cur->sourceCrop.right - cur->sourceCrop.left)) ||
        (win->src_rect_info.h != (cur->sourceCrop.bottom - cur->sourceCrop.top))) {
        SEC_HWC_Log(HWC_LOG_DEBUG,"src co-ordinates are changed [%d %d %d %d] to [%d %d %d %d]",
            win->src_rect_info.x, win->src_rect_info.y, win->src_rect_info.w, win->src_rect_info.h,
            cur->sourceCrop.left, cur->sourceCrop.top,
            cur->sourceCrop.right - cur->sourceCrop.left, cur->sourceCrop.bottom - cur->sourceCrop.top);

        win->src_rect_info.x = cur->sourceCrop.left;
        win->src_rect_info.y = cur->sourceCrop.top;
        win->src_rect_info.w = cur->sourceCrop.right - cur->sourceCrop.left;
        win->src_rect_info.h = cur->sourceCrop.bottom - cur->sourceCrop.top;
        if (win->ovly_lay_type == HWC_YUV_OVLY) {
            win->need_gsc_config = 1;
            if (win->is_gsc_started) {
                win->is_gsc_started = 0;
                ret = exynos_gsc_stop_exclusive(ctx->gsc_handle);
                if (ret < 0) {
                    SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : exynos_gsc_out_stop", __func__);
                }
            }
        }
    }

    win->layer_index = layer_idx;
    win->status = HWC_WIN_RESERVED;

    SEC_HWC_Log(HWC_LOG_DEBUG,
            "%s:: win_x %d win_y %d win_w %d win_h %d  lay_idx %d win_idx %d\n",
            "--assign_overlay_window()",
            win->rect_info.x, win->rect_info.y, win->rect_info.w,
            win->rect_info.h, win->layer_index, win_idx );

    return 0;
}

static int get_hwc_num_of_yuv_layers(struct hwc_context_t* ctx,  hwc_layer_list_t* list)
{
    int num_yuv_layers = 0;

    for (int i = 0; i < (int) list->numHwLayers; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        if (cur->handle == NULL)
            continue;

        private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
        if (check_yuv_format(prev_handle->format))
            num_yuv_layers++;
    }

    return num_yuv_layers;
}

static int check_yuv_layers_for_hdmi(struct hwc_context_t* ctx,  hwc_layer_list_t* list)
{
    int num_YV12_layers = 0;

    for (int i = 0; i < (int) list->numHwLayers; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        if (cur->handle == NULL)
            continue;

        private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
        if ((prev_handle->format == HAL_PIXEL_FORMAT_YV12) &&
            (!(prev_handle->usage & GRALLOC_USAGE_CAMERA)))
            num_YV12_layers++;
    }

    return num_YV12_layers;
}

static int get_gsc_out_down_scale_ratio(int xres, int yres)
{
    if (((xres == 720) ||(xres == 640)) && (yres == 480))
        return 4;
    else if ((xres == 1280) && (yres == 720))
        return 4;
    else if ((xres == 1280) && (yres == 800))
        return 3;
    else if ((xres == 1920) && (yres == 1080))
        return 2;
    else if ((xres == 800) && (yres == 1280))
        return 2;
    else
        return 1;
}

static int gsc_mode_change_check( struct hwc_context_t* ctx,  hwc_layer_list_t* list)
{
    int num_yuv_layers = 0;
    int mode_change = GSC_DEFAULT_MODE;
    int width_scale_ratio;
    int height_scale_ratio;
    int max_down_scale_ratio;

    max_down_scale_ratio = SEC_MAX(ctx->gsc_out_max_down_scale_ratio, 1);
    max_down_scale_ratio = SEC_MIN(max_down_scale_ratio, 4);

#if defined(SUPPORT_RGB_OVERLAY)
    if (ctx->is_rgb_ovly_ok && (ctx->num_of_yuv_layers == 0)) {
        mode_change = GSC_M2M_MODE;
        num_yuv_layers = 1;
        goto GSC_MODE_CHANGE;
    }
#endif

    for (int i = 0; i < (int) list->numHwLayers; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        if (cur->handle == NULL)
            continue;
        private_handle_t *prev_handle = (private_handle_t *)(cur->handle);

        if (check_yuv_format(prev_handle->format)) {
            if (NUM_OF_WIN <= num_yuv_layers)
                break;

            num_yuv_layers++;

            /* Check1: If rotation is 90 or 270, gsc_mode should be M2M */
            switch (cur->transform) {
            case 0:
            case HAL_TRANSFORM_FLIP_H:
            case HAL_TRANSFORM_FLIP_V:
            case HAL_TRANSFORM_ROT_180:
                break;
            default:
                mode_change = GSC_M2M_MODE;
                goto GSC_MODE_CHANGE;
                break;
            }

            /* Check2: check the Resolution Limitations:
              down scaling Ratio */
            if ((((cur->displayFrame.right - cur->displayFrame.left) * max_down_scale_ratio) <
                (cur->sourceCrop.right - cur->sourceCrop.left)) ||
                (((cur->displayFrame.bottom - cur->displayFrame.top) * max_down_scale_ratio) <
                (cur->sourceCrop.bottom - cur->sourceCrop.top))) {
                mode_change = GSC_M2M_MODE;
                break;
            }

#ifdef USE_GSC_M2M_FOR_DRM
            /* Check3: DRM playback case :
             It will be removed after updating the driver for gsc output mode */
            if (prev_handle->usage & GRALLOC_USAGE_PROTECTED) {
                mode_change = GSC_M2M_MODE;
                break;
            }
#endif

            /* Check4: If num of YUV layers are more than 1, gsc_mode should be M2M */
#ifdef GSC_M2M_WA
            if (ctx->num_of_yuv_layers > 1)
                break;
#endif
            if (num_yuv_layers > 1) {
                mode_change = GSC_M2M_MODE;
                break;
            }
        }
    }

GSC_MODE_CHANGE:
    if ((ctx->gsc_mode != mode_change) && (num_yuv_layers)) {
        ALOGD("#################################\n\n\n\n\n");
        ALOGD("GSC_MODE_CHANGED (%d to %d)", ctx->gsc_mode, mode_change);
        ALOGD("\n\n\n\n\n#################################");
        /* destroy the old GSC instance */
        exynos_gsc_destroy(ctx->gsc_handle);

        /* Create a new GSC instance with new mode */
        ctx->gsc_mode = mode_change;
        ctx->gsc_handle = exynos_gsc_create_exclusive(GSC_OUT_DEV_ID,
                                                                    ctx->gsc_mode, GSC_OUT_FIMD);
        if (!ctx->gsc_handle) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::exynos_gsc_create_exclusive() fail", __func__);
            return -1;
        }

        for (int i = 0; i < NUM_OF_WIN; i++) {
            ctx->win[i].gsc_mode = ctx->gsc_mode;
            reset_win_rect_info(&ctx->win[i]);
        }
    }
    return 0;

}

#if defined(SUPPORT_RGB_OVERLAY)
static void hwc_rgb_ovly_flag_int(struct hwc_context_t* ctx, hwc_layer_list_t* list)
{
    ctx->num_of_yuv_layers = get_hwc_num_of_yuv_layers(ctx, list);
    ctx->ovly_bandwidth = 0;
    ctx->is_rgb_ovly_ok = 1;

#if defined(BOARD_USES_HDMI)
    if (ctx->mHdmiClient->getHdmiCableStatus() == 1) {
        ctx->is_rgb_ovly_ok = 0;
        ctx->mHdmiCableStatus = 1;
        return;
    } else
        ctx->mHdmiCableStatus = 0;
#endif

    for (int i = 0; i < (int) list->numHwLayers; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        if (list->numHwLayers > NUM_OF_WIN) {
            ctx->is_rgb_ovly_ok = 0;
            break;
        }
        if (cur->handle != NULL) {
            private_handle_t *prev_handle = (private_handle_t *)(cur->handle);
            if (check_yuv_format(prev_handle->format) != 1) {
                if (get_hwc_rgb_ovly_type(ctx, cur) == HWC_FRAMEBUFFER) {
                    ctx->is_rgb_ovly_ok = 0;
                    break;
                }
            }
        } else {
            ctx->is_rgb_ovly_ok = 0;
            break;
        }
    }

    return;
}
#endif
static int hwc_prepare(hwc_composer_device_t *dev, hwc_layer_list_t* list)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int overlay_win_cnt = 0;
    int compositionType = 0;
    int ret;

#ifdef SKIP_DUMMY_UI_LAY_DRAWING
    if((list && (!(list->flags & HWC_GEOMETRY_CHANGED))) &&
#if defined(BOARD_USES_HDMI) && defined(SUPPORT_RGB_OVERLAY)
    (ctx->mHdmiCableStatus == ctx->mHdmiClient->getHdmiCableStatus()) &&
#endif
    (!ctx->need_to_try_overlay) &&
    (ctx->num_of_hwc_layer > 0)) {
        get_hwc_ui_lay_skipdraw_decision(ctx, list);
        return 0;
    }
    ctx->fb_lay_skip_initialized = 0;
    ctx->num_of_fb_lay_skip = 0;
#ifdef GL_WA_OVLY_ALL
    ctx->ui_skip_frame_cnt = 0;
#endif

    for (int i = 0; i<NUM_OF_DUMMY_WIN; i++) {
        ctx->win_virt[i].layer_prev_buf = 0;
        ctx->win_virt[i].layer_index = -1;
        ctx->win_virt[i].status = HWC_WIN_FREE;
    }
#endif

    //if geometry is not changed, there is no need to do any work here
    if (!list || ((!(list->flags & HWC_GEOMETRY_CHANGED)) &&
#if defined(BOARD_USES_HDMI) && defined(SUPPORT_RGB_OVERLAY)
    (ctx->mHdmiCableStatus == ctx->mHdmiClient->getHdmiCableStatus()) &&
#endif
     (!ctx->need_to_try_overlay) &&
    (!ctx->dis_rect_changed)))
        return 0;

#if defined(SUPPORT_RGB_OVERLAY)
    hwc_rgb_ovly_flag_int(ctx, list);
#endif

    /* check and change the GSC mode if required */
    gsc_mode_change_check(ctx, list);
    if (list->flags & HWC_GEOMETRY_CHANGED)
        ctx->dis_rect_changed = 1;
    else
        ctx->dis_rect_changed = 0;

    //all the windows are free here....
    for (int i = 0 ; i < NUM_OF_WIN; i++) {
        ctx->win[i].status = HWC_WIN_FREE;
    }

    ctx->num_of_hwc_layer = 0;
    ctx->num_of_fb_layer = 0;

#if defined(BOARD_USES_HDMI)
    check_gralloc_usage_flags(ctx, list);
#endif

    for (int i = 0; i < list->numHwLayers ; i++) {
        hwc_layer_t* cur = &list->hwLayers[i];
        private_handle_t *prev_handle;
        prev_handle = (private_handle_t *)(cur->handle);

        if (overlay_win_cnt < NUM_OF_WIN) {
            compositionType = get_hwc_compos_decision(ctx, cur, overlay_win_cnt);

            if (compositionType == HWC_FRAMEBUFFER) {
                cur->compositionType = HWC_FRAMEBUFFER;
                ctx->num_of_fb_layer++;
            } else {
                ret = assign_overlay_window(ctx, cur, overlay_win_cnt, i);
                if (ret != 0) {
                    cur->compositionType = HWC_FRAMEBUFFER;
                    ctx->num_of_fb_layer++;
                    continue;
                }

                cur->compositionType = HWC_OVERLAY;
                cur->hints = HWC_HINT_CLEAR_FB;
                overlay_win_cnt++;
                ctx->num_of_hwc_layer++;
            }
        } else {
            cur->compositionType = HWC_FRAMEBUFFER;
            ctx->num_of_fb_layer++;
        }

#if defined(BOARD_USES_HDMI)
        if (ctx->num_of_ext_disp_video_layer >= 2) {
        } else if ((compositionType == HWC_OVERLAY) &&
                (prev_handle->usage & GRALLOC_USAGE_EXTERNAL_DISP)) {
            ctx->hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_VIDEO] = i;
        } else if (prev_handle != NULL) {
        }
#endif
    }

#if defined(BOARD_USES_HDMI)
    if (ctx->num_of_ext_disp_video_layer > 1) {
        ctx->mHdmiClient->setHdmiHwcLayer(0);
        ctx->hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_GRAPHIC_0] = NOT_DEFINED;
    } else {
        ctx->mHdmiClient->setHdmiHwcLayer(ctx->num_of_ext_disp_layer);
    }

    if ((ctx->num_of_hwc_layer == 1) && (get_hwc_num_of_yuv_layers(ctx, list) == 1)) {
        ctx->mHdmiClient->setHdmiPath(HDMI_PATH_OVERLAY);
        if (ctx->num_of_protected_layer)
            ctx->mHdmiClient->setHdmiDRM(android::ExynosHdmiClient::HDMI_DRM_MODE);
        else
            ctx->mHdmiClient->setHdmiDRM(android::ExynosHdmiClient::HDMI_NON_DRM_MODE);
    } else {
        if (!ctx->num_of_ext_disp_layer)
            ctx->mHdmiClient->setHdmiPath(DEFAULT_UI_PATH);
        else
            ctx->mHdmiClient->setHdmiPath(HDMI_PATH_OVERLAY);
        ctx->mHdmiClient->setHdmiDRM(android::ExynosHdmiClient::HDMI_NON_DRM_MODE);
    }

    if ((ctx->num_of_s3d_layer > 0) && is_same_3d_usage) {
        if ((usage_3d & GRALLOC_USAGE_PRIVATE_SBS_LR) ||
            (usage_3d & GRALLOC_USAGE_PRIVATE_SBS_RL)) {
            ctx->mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS,
                                                android::ExynosHdmiClient::HDMI_S3D_SBS);
        } else if((usage_3d & GRALLOC_USAGE_PRIVATE_TB_LR) ||
                  (usage_3d & GRALLOC_USAGE_PRIVATE_TB_RL)) {
            ctx->mHdmiClient->setHdmiResolution(DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB,
                                                android::ExynosHdmiClient::HDMI_S3D_TB);
        } else
            ctx->mHdmiClient->setHdmiResolution(0, android::ExynosHdmiClient::HDMI_2D);
    } else
        ctx->mHdmiClient->setHdmiResolution(0, android::ExynosHdmiClient::HDMI_2D);

    if ((ctx->num_of_ext_disp_layer == 0) ||
        (ctx->num_of_ext_disp_video_layer == 0 && ctx->num_of_ext_disp_layer >= 2) ||
        (ctx->num_of_ext_disp_video_layer > 1) ||
        (ctx->hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_GRAPHIC_0] != NOT_DEFINED)) {
    }
#ifndef PERSISTENT_UI
    else
        ctx->mHdmiClient->setHdmiLayerDisable(android::ExynosHdmiClient::HDMI_LAYER_GRAPHIC_0);
#endif

    if (ctx->hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_VIDEO] == NOT_DEFINED)
        ctx->mHdmiClient->setHdmiLayerDisable(android::ExynosHdmiClient::HDMI_LAYER_VIDEO);

#endif

    if (list->numHwLayers != (ctx->num_of_fb_layer + ctx->num_of_hwc_layer))
        SEC_HWC_Log(HWC_LOG_DEBUG,
                "%s:: numHwLayers %d num_of_fb_layer %d num_of_hwc_layer %d ",
                __func__, list->numHwLayers, ctx->num_of_fb_layer,
                ctx->num_of_hwc_layer);

    return 0;
}

static void src_dst_img_info_gsc_out(struct sec_img *src_img,
        struct sec_rect *src_rect,
        struct sec_img *dst_img,
        struct sec_rect *dst_rect,
        exynos_gsc_img *src_info,
        exynos_gsc_img *dst_info,
        unsigned int rot)
{
    src_info->x = src_rect->x;
    src_info->y = src_rect->y;
    src_info->w = src_rect->w;
    src_info->h = src_rect->h;
    src_info->fw = src_img->f_w;
    src_info->fh = src_img->f_h;
    src_info->format = src_img->format;
    src_info->yaddr = src_img->base;
    src_info->uaddr = src_img->base + src_img->uoffset;
    src_info->vaddr = src_img->base + src_img->uoffset + src_img->voffset;
    src_info->rot = 0;
    src_info->cacheable = 1;
    src_info->drmMode = 0;
    if (src_img->usage & GRALLOC_USAGE_PROTECTED)
        src_info->drmMode = 1;

    dst_info->x = dst_rect->x;
    dst_info->y = dst_rect->y;
    dst_info->w = dst_rect->w;
    dst_info->h = dst_rect->h;
    dst_info->fw = dst_img->f_w;
    dst_info->fh = dst_img->f_h;
    dst_info->format = dst_img->format;
    dst_info->yaddr = dst_img->base;
    dst_info->uaddr = 0;
    dst_info->vaddr = 0;
    dst_info->rot = rot;
    dst_info->cacheable = 0;
    dst_info->drmMode = 0;

    SEC_HWC_Log(HWC_LOG_DEBUG, "HWC:SRC[x %d y %d w %d h %d fw %d fh %d f %x yaddr %x uaddr %x vaddr %x rot %d]",
            src_info->x, src_info->y, src_info->w, src_info->h, src_info->fw, src_info->fh, src_info->format,
            src_info->yaddr, src_info->uaddr, src_info->vaddr, src_info->rot);
    SEC_HWC_Log(HWC_LOG_DEBUG, "HWC:DST[x %d y %d w %d h %d fw %d fh %d f %x yaddr %x uaddr %x vaddr %x rot %d]",
            dst_info->x, dst_info->y, dst_info->w, dst_info->h, dst_info->fw, dst_info->fh, dst_info->format,
            dst_info->yaddr, dst_info->uaddr, dst_info->vaddr, dst_info->rot);
    return;
}

static int hwc_set(hwc_composer_device_t *dev,
                   hwc_display_t dpy,
                   hwc_surface_t sur,
                   hwc_layer_list_t* list)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    int skipped_window_mask = 0;
    hwc_layer_t* cur;
    struct hwc_win_info_t   *win;
    int ret = 0;
    int crtc = 0;
    struct sec_img src_img;
    struct sec_img dst_img;
    struct sec_rect src_work_rect;
    struct sec_rect dst_work_rect;

    int skip_lay_rendering[NUM_OF_WIN];
#if defined(USE_HWC_CSC_THREAD)
    int counter;
#endif

    exynos_gsc_img src_info;
    exynos_gsc_img dst_info;
    bool need_swap_buffers = ctx->num_of_fb_layer > 0;

    if (sur == NULL || dpy == NULL) {
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
        ctx->fb_lay_skip_initialized = 0;
        ctx->num_of_fb_layer += ctx->num_of_fb_lay_skip;
        ctx->num_of_fb_lay_skip = 0;
#endif
#if defined(TURN_OFF_UI_WINDOW)
        window_show(&ctx->ui_win);
#endif
        for (int i = 0; i < NUM_OF_WIN; i++) {
            ctx->layer_prev_buf[i] = 0;
            window_hide(&ctx->win[i]);
        }
        return ret;
    }

    if (!list) {
        //turn off the all windows
        for (int i = 0; i < NUM_OF_WIN; i++) {
            window_hide(&ctx->win[i]);
        }
        ctx->num_of_hwc_layer = 0;
        need_swap_buffers = true;
    }

    if(ctx->num_of_hwc_layer > NUM_OF_WIN)
        ctx->num_of_hwc_layer = NUM_OF_WIN;

    /*
     * H/W composer documentation states:
     * There is an implicit layer containing opaque black
     * pixels behind all the layers in the list.
     * It is the responsibility of the hwcomposer module to make
     * sure black pixels are output (or blended from).
     *
     * Since we're using a blitter, we need to erase the frame-buffer when
     * switching to all-overlay mode.
     *
     */
    if (ctx->num_of_hwc_layer
        && ctx->num_of_fb_layer==0
        && ctx->num_of_fb_layer_prev
#ifdef SUPPORT_RGB_OVERLAY
        && (!ctx->is_rgb_ovly_ok)
#endif
        ) {
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
        if (ctx->num_of_fb_lay_skip == 0)
#endif
        {
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);
            need_swap_buffers = true;
        }
    }
    ctx->num_of_fb_layer_prev = ctx->num_of_fb_layer;

    for (int i = 0; i < NUM_OF_WIN; i++)
        skip_lay_rendering[i] = 0;

    if ((ctx->num_of_hwc_layer <= 0) && (ctx->gsc_mode == GSC_OUTPUT_MODE)) {
        for (int i = 0; i < 1; i++) {  /* to do : for multiple windows */
            if (ctx->win[i].is_gsc_started) {
                ctx->win[i].is_gsc_started = 0;
                ret = exynos_gsc_stop_exclusive(ctx->gsc_handle);
                if (ret < 0) {
                    SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : exynos_gsc_out_stop", __func__);
                }
                ctx->win[i].power_state = 0;
            }
        }
    }

    //compose hardware layers here
    for (int i = 0; i < ctx->num_of_hwc_layer; i++) {
        win = &ctx->win[i];
        if (win->status == HWC_WIN_RESERVED) {
            cur = &list->hwLayers[win->layer_index];

            if (cur->compositionType == HWC_OVERLAY) {
                if (ctx->layer_prev_buf[i] == (uint32_t)cur->handle) {
                    /*
                     * In android platform, all the graphic buffer are at least
                     * double buffered (2 or more) this buffer is already rendered.
                     * It is the redundant src buffer for GSC rendering.
                     */
                    SEC_HWC_Log(HWC_LOG_DEBUG, "HWC:SKIP GSC rendering for Layer%d", win->layer_index);

                    skip_lay_rendering[i] = 1;
                    continue;
                }
                skip_lay_rendering[i] = 0;

                ctx->layer_prev_buf[i] = (uint32_t)cur->handle;
                // initialize the src & dist context for gsc & g2d
                set_src_dst_img_rect(cur, win, &src_img, &dst_img,
                                &src_work_rect, &dst_work_rect, i);

#if defined(SUPPORT_RGB_OVERLAY)
                if (win->ovly_lay_type == HWC_RGB_OVLY) {
                    addr_space addr_type;
                    uint32_t winPhysAddr;

                    addr_type = ADDR_USER;
                    src_img.paddr = src_img.base;
                    dst_img.paddr = dst_img.base;
#ifdef USE_VSYNC_FOR_RGB_OVERLAY
                    winPhysAddr = 0;
                    if (ioctl(win->fd, S3CFB_GET_CUR_WIN_BUF_ADDR,
                                &(winPhysAddr)) < 0) {
                        SEC_HWC_Log(HWC_LOG_ERROR,
                                "%s::S3CFB_GET_CUR_WIN_BUF_ADDR(%d, %d) fail",
                                __func__, win->rect_info.w, win->rect_info.h);
                    }
                    /* Wait for vsync */
                    if (winPhysAddr == win->phy_addr[win->buf_index]) {
                        if (ioctl(win->fd, FBIO_WAITFORVSYNC, &crtc) < 0) {
                            SEC_HWC_Log(HWC_LOG_ERROR,
                                    "%s::FBIO_WAITFORVSYNC fail(%s)",
                                    __func__, strerror(errno));
                        }
                    }
#ifdef  REPORT_VSYNC
                    ExynosReportVsync();
#endif
#endif

#ifdef GRALLOC_MOD_ACCESS
                    ExynosWaitForRenderFinish(ctx->psGrallocModule, &cur->handle, 1);
#endif
                    ret = runCompositor(ctx, &src_img, &src_work_rect,
                            &dst_img, &dst_work_rect, cur->transform,
                            0xff, NULL, BLIT_OP_SRC, addr_type);

                    if (ret < 0) {
                        SEC_HWC_Log(HWC_LOG_ERROR, "%s::runCompositor fail : ret=%d\n",
                                    __func__, ret);
                        skipped_window_mask |= (1 << i);
                        continue;
                    }
                } else {
#endif
                if (i > 0) { //dual video scenario
                    if ((ctx->win[i - 1].gsc_mode == GSC_M2M_MODE) &&
                        ctx->win[i - 1].is_gsc_started) {
                        ctx->win[i - 1].is_gsc_started = 0;
                        if (exynos_gsc_wait_done(ctx->gsc_handle) < 0) {
                            SEC_HWC_Log(HWC_LOG_ERROR,
                                "%s:: First-video: error : exynos_gsc_wait_done", __func__);
                        }
                    }
                }
                src_dst_img_info_gsc_out(&src_img, &src_work_rect,
                            &dst_img, &dst_work_rect, &src_info, &dst_info, cur->transform);
                if ((win->need_gsc_config) || (win->gsc_mode != GSC_OUTPUT_MODE)) {
                    ret = exynos_gsc_config_exclusive(ctx->gsc_handle, &src_info, &dst_info);
                    if (ret < 0) {
                        SEC_HWC_Log(HWC_LOG_ERROR, "%s::exynos_gsc_out_config fail : ret=%d\n",
                                    __func__, ret);
                        continue;
                    }
                    win->need_gsc_config = 0;
                    win->is_gsc_started = 1;
                }
                ret = exynos_gsc_run_exclusive(ctx->gsc_handle, &src_info, &dst_info);
                if (ret < 0) {
                        SEC_HWC_Log(HWC_LOG_ERROR, "%s::exynos_gsc_out_run fail : ret=%d\n",
                                    __func__, ret);
                        continue;
                }

#if defined(BOARD_USES_HDMI)
                if (ctx->hdmi_layer_buf_index[android::ExynosHdmiClient::HDMI_LAYER_VIDEO] == win->layer_index) {

                    /* DRM contents should not use ARM memcpy.
                     * If hdmi resolution is 1080930, it use ARM memcpy to avoid tearing
                     */
                    /*
                    if ((ctx->num_of_protected_layer) || (ctx->hdmi_resolution != 1080930))
                        flagHdmiMemCpy = 0;
                    else
                        flagHdmiMemCpy = 1;
                        */

                    if ((src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP_TILED)||
                        (src_img.format == HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP) ||
                        (src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED) ||
                        (src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) ||
                        (src_img.format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                        (src_img.format == HAL_PIXEL_FORMAT_YCbCr_420_P) ||
                        (src_img.format == HAL_PIXEL_FORMAT_YV12)) {
                        ctx->mHdmiClient->blit2Hdmi(src_work_rect.w, src_work_rect.h,
                                src_img.format,
                                src_img.base,
                                src_img.base + src_img.uoffset,
                                src_img.base + src_img.uoffset + src_img.voffset,
                                0, 0,
                                android::ExynosHdmiClient::HDMI_MODE_VIDEO,
                                1);
                    }
                }
#endif

#if defined(SUPPORT_RGB_OVERLAY)
                    }
#endif

            } else {
                SEC_HWC_Log(HWC_LOG_ERROR,
                        "%s:: error : layer %d compositionType should have been"
                        " HWC_OVERLAY ", __func__, win->layer_index);
                skipped_window_mask |= (1 << i);
                continue;
            }
        } else {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : window status should have "
                    "been HWC_WIN_RESERVED by now... ", __func__);
             skipped_window_mask |= (1 << i);
             continue;
        }
    }

    if (skipped_window_mask) {
#if defined(TURN_OFF_UI_WINDOW)
        window_show(&ctx->ui_win);
#else
        /* turn off the free windows */
        for (int i = 0; i < NUM_OF_WIN; i++) {
            if (skipped_window_mask & (1 << i)) {
                window_hide(&ctx->win[i]);
            }
        }
#endif
    }

    for (int i = 0; i < ctx->num_of_hwc_layer; i++) {
        win = &ctx->win[i];
        if ((win->status == HWC_WIN_RESERVED) &&
            (skip_lay_rendering[i] == 0) && (win->ovly_lay_type == HWC_RGB_OVLY)) {
            if (win->need_win_config) {
                window_set_pos(win);
                win->need_win_config = 0;
            }
            window_pan_display(win);
            win->buf_index = (win->buf_index + 1) % NUM_OF_WIN_BUF;
            if (win->power_state == 0)
                window_show(win);
        }
    }

    if (ctx->num_of_hwc_layer > 0 ) {
        for (int i = 0; i < ctx->num_of_hwc_layer; i++) {
            if (ctx->win[i].ovly_lay_type == HWC_YUV_OVLY) {
                if (skip_lay_rendering[i] == 0) {
                    if (ctx->win[i].is_gsc_started) {
                        if (exynos_gsc_wait_done(ctx->gsc_handle) < 0) {
                            SEC_HWC_Log(HWC_LOG_ERROR, "%s:: error : exynos_gsc_wait_done", __func__);
                        }
                    }
                    if (ctx->win[i].gsc_mode == GSC_M2M_MODE) {
                        ctx->win[i].is_gsc_started = 0;
                        if (ctx->win[i].need_win_config) {
                            window_set_pos(&ctx->win[i]);
                            ctx->win[i].need_win_config = 0;
                        }
                        window_pan_display(&ctx->win[i]);
                        ctx->win[i].buf_index =
                                (ctx->win[i].buf_index + 1) % NUM_OF_WIN_BUF;
                        if (ctx->win[i].power_state == 0)
                           window_show(&ctx->win[i]);
                    }
                }
            }
        }
    }

    if (need_swap_buffers) {
        if (sur == NULL || dpy == NULL){
            return HWC_EGL_ERROR;
        }
#ifdef CHECK_EGL_FPS
            check_fps();
#endif
#ifdef HWC_HWOVERLAY
            glFinish();
#endif
            EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
            if (!sucess)
                return HWC_EGL_ERROR;

#if defined(TURN_OFF_UI_WINDOW)
        window_show(&ctx->ui_win);
#endif

    }
#if defined(TURN_OFF_UI_WINDOW)
#ifdef SKIP_DUMMY_UI_LAY_DRAWING
    else if ((ctx->num_of_fb_layer + ctx->num_of_fb_lay_skip  == 0) &&
            ctx->num_of_hwc_layer)
#else
    else if ((ctx->num_of_fb_layer == 0) && ctx->num_of_hwc_layer)
#endif
    {
        window_hide(&ctx->ui_win);
    }
#endif

#if defined(TURN_OFF_UI_WINDOW)
    if (ctx->num_of_hwc_layer <= 0)
        window_show(&ctx->ui_win);
#endif

    for (int i = ctx->num_of_hwc_layer; i < NUM_OF_WIN; i++) {
        window_hide(&ctx->win[i]);
        reset_win_rect_info(&ctx->win[i]);
    }

    return 0;
}

#ifdef    VSYNC_THREAD_ENABLE
static void hwc_registerProcs(struct hwc_composer_device* dev,
        hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ctx->procs = const_cast<hwc_procs_t *>(procs);
}

static int hwc_query(struct hwc_composer_device* dev,
        int what, int* value)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
 SEC_HWC_Log(HWC_LOG_ERROR, "HWC: %s", __func__);
    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
        // we don't support the background layer yet
        value[0] = 0;
        break;
    case HWC_VSYNC_PERIOD:
        // vsync period in nanosecond
        value[0] = 1000000000.0 / 60;
        //value[0] = 1000000000.0 / fps;
        break;
    default:
        // unsupported query
        return -EINVAL;
    }
    return 0;
}

static int hwc_eventControl(struct hwc_composer_device* dev,
        int event, int enabled)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        int val = !!enabled;
        int err = ioctl(ctx->ui_win.fd, S3CFB_SET_VSYNC_INT, &val);
        if (err < 0)
            return -errno;

        return 0;
    }

    return -EINVAL;
}

void handle_vsync_uevent(hwc_context_t *ctx, const char *buff, int len)
{
    uint64_t timestamp = 0;
    const char *s = buff;

    if(!ctx->procs || !ctx->procs->vsync)
       return;

    s += strlen(s) + 1;
    while(*s) {
        if (!strncmp(s, "VSYNC=", strlen("VSYNC=")))
            timestamp = strtoull(s + strlen("VSYNC="), NULL, 0);

        s += strlen(s) + 1;
        if (s - buff >= len)
            break;
    }

    ctx->procs->vsync(ctx->procs, 0, timestamp);
}

static void *hwc_vsync_thread(void *data)
{
    hwc_context_t *ctx = (hwc_context_t *)(data);
    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);
    uevent_init();
    while(true) {
        int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
        bool vsync = !strcmp(uevent_desc, VSYNC_DEV_NAME);
        if(vsync)
            handle_vsync_uevent(ctx, uevent_desc, len);
    }

    return NULL;
}

static const struct hwc_methods hwc_methods = {
    eventControl: hwc_eventControl
};
#endif

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    int ret = 0;
    int i;

    if (ctx) {
        exynos_gsc_destroy(ctx->gsc_handle);

        for (i = 0; i < NUM_OF_WIN; i++) {
            if (window_close(&ctx->win[i]) < 0)
                SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
        }
        if (window_close(&ctx->ui_win) < 0)
                SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);

        free(ctx);
    }

    return ret;
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;
    struct hwc_win_info_t   *win;

    if (strcmp(name, HWC_HARDWARE_COMPOSER))
        return  -EINVAL;

    struct hwc_context_t *dev;
    dev = (hwc_context_t*)malloc(sizeof(*dev));

    /* initialize our state here */
    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.module = const_cast<hw_module_t*>(module);
    dev->device.common.close = hwc_device_close;

    dev->device.prepare = hwc_prepare;
    dev->device.set = hwc_set;
#ifndef    VSYNC_THREAD_ENABLE
    dev->device.common.version = 0;
#else
    dev->device.common.version = HWC_DEVICE_API_VERSION_0_3;
    dev->device.registerProcs = hwc_registerProcs;
    dev->device.query = hwc_query;
    dev->device.methods = &hwc_methods;
#endif
    *device = &dev->device.common;

    //initializing
     /* open WIN0 & WIN1 here */
     for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_open(&(dev->win[i]), i)  < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR,
                    "%s:: Failed to open window %d device ", __func__, i);
             status = -EINVAL;
             goto err;
        }
     }

     /* open UI window */
    if (window_open(&(dev->ui_win), UI_WINDOW)  < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s:: Failed to open window %d device ", __func__, UI_WINDOW);
        status = -EINVAL;
        goto err;
    }

    if (window_get_global_lcd_info(dev->ui_win.fd, &dev->lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR,
                "%s::window_get_global_lcd_info is failed : %s",
                __func__, strerror(errno));
        status = -EINVAL;
        goto err;
    }
    dev->ui_win.ovly_lay_type = HWC_RGB_OVLY;
    dev->ui_win.power_state = 1;

#if defined(BOARD_USES_HDMI)
    lcd_width   = dev->lcd_info.xres;
    lcd_height  = dev->lcd_info.yres;
#endif

    /* initialize the window context */
    for (int i = 0; i < NUM_OF_WIN; i++) {
        win = &dev->win[i];
        memcpy(&win->lcd_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));
        memcpy(&win->var_info, &dev->lcd_info, sizeof(struct fb_var_screeninfo));

        win->rect_info.x = 0;
        win->rect_info.y = 0;
        win->rect_info.w = win->var_info.xres;
        win->rect_info.h = win->var_info.yres;

       if (window_set_pos(win) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_set_pos is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }

        if (window_get_info(win, i) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::window_get_info is failed : %s",
                    __func__, strerror(errno));
            status = -EINVAL;
            goto err;
        }
    }

#ifdef GRALLOC_MOD_ACCESS
    ExynosOpenGraphicsHAL(&dev->psGrallocModule);
#endif

    dev->gsc_out_max_down_scale_ratio =
                get_gsc_out_down_scale_ratio(dev->lcd_info.xres, dev->lcd_info.yres);
    dev->gsc_mode = GSC_DEFAULT_MODE;
    dev->gsc_handle = exynos_gsc_create_exclusive(GSC_OUT_DEV_ID,
                                                dev->gsc_mode, GSC_OUT_FIMD);
    if (!dev->gsc_handle) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::exynos_gsc_create_exclusive() fail", __func__);
        status = -EINVAL;
        goto err;
    }

    for (int i = 0; i < NUM_OF_WIN; i++) {
        dev->win[i].gsc_mode = dev->gsc_mode;
        if (dev->gsc_mode == GSC_OUTPUT_MODE)
            reset_win_rect_info(&dev->win[i]);
    }

#if defined(BOARD_USES_HDMI)
    dev->mHdmiClient = android::ExynosHdmiClient::getInstance();
#endif

#ifdef    VSYNC_THREAD_ENABLE
    status = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
    if (status) {
        ALOGE("%s::pthread_create() failed : %s", __func__, strerror(status));
        status = -EINVAL;
        goto err;
    }
#endif

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: hwc_device_open: SUCCESS", __func__);
    return 0;

err:
    exynos_gsc_destroy(dev->gsc_handle);

    for (int i = 0; i < NUM_OF_WIN; i++) {
        if (window_close(&dev->win[i]) < 0)
            SEC_HWC_Log(HWC_LOG_DEBUG, "%s::window_close() fail", __func__);
    }

    return status;
}

