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

#include "ExynosHWCUtils.h"
#define HAL_PIXEL_FORMAT_G2D_BGRX_8888 8

#define V4L2_BUF_TYPE_OUTPUT V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
#define V4L2_BUF_TYPE_CAPTURE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
#define EXYNOS4_ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))

//#define CHECK_FPS
#ifdef CHECK_FPS
#include <sys/time.h>
#include <unistd.h>
#define CHK_FRAME_CNT 30

void check_fps()
{
    static struct timeval tick, tick_old;
    static int total = 0;
    static int cnt = 0;
    int FPS;
    cnt++;
    gettimeofday(&tick, NULL);
    if (cnt > 10) {
        if (tick.tv_sec > tick_old.tv_sec)
            total += ((tick.tv_usec/1000) + (tick.tv_sec - tick_old.tv_sec)*1000 - (tick_old.tv_usec/1000));
        else
            total += ((tick.tv_usec - tick_old.tv_usec)/1000);

        memcpy(&tick_old, &tick, sizeof(timeval));
        if (cnt == (10 + CHK_FRAME_CNT)) {
            FPS = 1000*CHK_FRAME_CNT/total;
            LOGE("[FPS]:%d\n", FPS);
            total = 0;
            cnt = 10;
        }
    } else {
        memcpy(&tick_old, &tick, sizeof(timeval));
        total = 0;
    }
}
#endif

int window_open(struct hwc_win_info_t *win, int id)
{
    int fd = 0;
    char name[64];
    int vsync = 1;
    int real_id = id;

    char const * const device_template = "/dev/graphics/fb%u";
    // window & FB maping
    // fb0 -> win-id : 2
    // fb1 -> win-id : 1
    // fb2 -> win-id : 0
    // fb3 -> no device node
    // fb4 -> no device node
    // it is pre assumed that ...win0 or win1 is used here..

    switch (id) {
    case 0:
        real_id = 2;
        break;
    case 1:
        real_id = 1;
        break;
    case 2:
        real_id = 0;
        break;
    default:
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::id(%d) is weird", __func__, id);
        goto error;
}

    snprintf(name, 64, device_template, real_id);

    win->fd = open(name, O_RDWR);
    if (win->fd <= 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::Failed to open window device (%s) : %s",
                __func__, strerror(errno), name);
        goto error;
    }

#ifdef ENABLE_FIMD_VSYNC
    vsync = 1;
    if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
        goto error;
    }
#endif

    return 0;

error:
    if (0 < win->fd)
        close(win->fd);
    win->fd = 0;

    return -1;
}

int window_close(struct hwc_win_info_t *win)
{
    int ret = 0;

    if (0 < win->fd) {
        ion_unmap((void *)win->virt_addr[0], ALIGN(win->size * NUM_OF_WIN_BUF, PAGE_SIZE));
        ion_free(win->ion_fd);

#ifdef ENABLE_FIMD_VSYNC
        /* Set using VSYNC Interrupt for FIMD_0   */
        int vsync = 0;
        if (ioctl(win->fd, S3CFB_SET_VSYNC_INT, &vsync) < 0)
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_SET_VSYNC_INT fail", __func__);
#endif
        ret = close(win->fd);
    }
    win->fd = 0;

    return ret;
}

int window_set_pos(struct hwc_win_info_t *win)
{
    struct s3cfb_user_window window;

#if defined(SUPPORT_RGB_OVERLAY)
    if ((win->gsc_mode == GSC_OUTPUT_MODE) && (win->ovly_lay_type == HWC_YUV_OVLY))
#else
    if (win->gsc_mode == GSC_OUTPUT_MODE)
#endif
        return 0;

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: x(%d), y(%d)",
            __func__, win->rect_info.x, win->rect_info.y);

    win->var_info.xres_virtual = (win->lcd_info.xres + 15) & ~ 15;
    win->var_info.yres_virtual = win->lcd_info.yres * NUM_OF_WIN_BUF;
    win->var_info.xres = win->rect_info.w;
    win->var_info.yres = win->rect_info.h;
    win->var_info.activate &= ~FB_ACTIVATE_MASK;
    win->var_info.activate |= FB_ACTIVATE_FORCE;
    if (ioctl(win->fd, FBIOPUT_VSCREENINFO, &(win->var_info)) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOPUT_VSCREENINFO(%d, %d) fail",
          __func__, win->rect_info.w, win->rect_info.h);
        return -1;
    }

    window.x = win->rect_info.x;
    window.y = win->rect_info.y;
    if (ioctl(win->fd, S3CFB_WIN_POSITION, &window) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::S3CFB_WIN_POSITION(%d, %d) fail",
            __func__, window.x, window.y);
      return -1;
    }

    return 0;
}

int window_get_info(struct hwc_win_info_t *win, int win_num)
{
    int temp_size = 0;

    if (ioctl(win->fd, FBIOGET_FSCREENINFO, &win->fix_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_FSCREENINFO failed : %s",
                strerror(errno));
        goto error;
    }

    win->size = win->fix_info.line_length * win->var_info.yres;

    struct s3c_fb_user_ion_client ion_handle;
    void *ion_start_addr;
    if (ioctl(win->fd, S3CFB_GET_ION_USER_HANDLE, &ion_handle) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "Get fb ion client is failed\n");
        return -1;
    }

    win->ion_fd = ion_handle.fd;
    ion_start_addr = ion_map(win->ion_fd, EXYNOS_WIN_SIZE(win->size) * NUM_OF_WIN_BUF, 0);

    for (int j = 0; j < NUM_OF_WIN_BUF; j++) {
        temp_size = EXYNOS_WIN_SIZE(win->size) * j;
        win->virt_addr[j] = (uint32_t)ion_start_addr + temp_size;
        win->phy_addr[j] = win->fix_info.smem_start + temp_size;
        SEC_HWC_Log(HWC_LOG_DEBUG, "%s::win-%d phy_add[%d]  %x virt_addr[%d] %x",
                    __func__, win_num, j,  win->phy_addr[j], j, win->virt_addr[j]);
    }
    return 0;

error:
    win->fix_info.smem_start = 0;

    return -1;
}

int window_pan_display(struct hwc_win_info_t *win)
{
    struct fb_var_screeninfo *lcd_info = &(win->lcd_info);
    unsigned int index;

#if defined(SUPPORT_RGB_OVERLAY)
    if ( (win->gsc_mode == GSC_OUTPUT_MODE) && (win->ovly_lay_type == HWC_YUV_OVLY))
#else
    if (win->gsc_mode == GSC_OUTPUT_MODE)
#endif
        return 0;

#ifdef ENABLE_FIMD_VSYNC
    int pan_num = 0;
    if (ioctl(win->fd, FBIO_WAITFORVSYNC, &pan_num) < 0)
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIO_WAITFORVSYNC fail(%s)",
                __func__, strerror(errno));
#endif

    lcd_info->yoffset = lcd_info->yres * win->buf_index;
    if (ExynosWinSet(win->fd, lcd_info, win->buf_index) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s::ExynosWinSet(%d / %d / %d) fail(%s)",
            __func__,
            lcd_info->yres,
            win->buf_index, lcd_info->yres_virtual,
            strerror(errno));
        return -1;
    }

    return 0;
}

int window_show(struct hwc_win_info_t *win)
{
#if defined(SUPPORT_RGB_OVERLAY)
    if ( (win->gsc_mode == GSC_OUTPUT_MODE) && (win->ovly_lay_type == HWC_YUV_OVLY))
#else
    if (win->gsc_mode == GSC_OUTPUT_MODE)
#endif
        return 0;

    if (win->power_state == 0) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
                __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 1;
    }
    return 0;
}

int window_hide(struct hwc_win_info_t *win)
{
#if defined(SUPPORT_RGB_OVERLAY)
    if ( (win->gsc_mode == GSC_OUTPUT_MODE) && (win->ovly_lay_type == HWC_YUV_OVLY))
#else
    if (win->gsc_mode == GSC_OUTPUT_MODE)
#endif
        return 0;

    if (win->power_state == 1) {
        if (ioctl(win->fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0) {
            SEC_HWC_Log(HWC_LOG_ERROR, "%s::FBIOBLANK failed : (%d:%s)",
             __func__, win->fd, strerror(errno));
            return -1;
        }
        win->power_state = 0;
    }
    return 0;
}

int window_get_global_lcd_info(int fd, struct fb_var_screeninfo *lcd_info)
{
    if (ioctl(fd, FBIOGET_VSCREENINFO, lcd_info) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "FBIOGET_VSCREENINFO failed : %s",
                strerror(errno));
        return -1;
    }

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s:: Default LCD x(%d),y(%d)",
            __func__, lcd_info->xres, lcd_info->yres);
    return 0;
}

#if defined(FIMG2D4X)
static inline rotation rotateValueHAL2G2D(unsigned char transform)
{
    int rotate_flag = transform & 0x7;

    switch (rotate_flag) {
    case HAL_TRANSFORM_ROT_90:  return ROT_90;
    case HAL_TRANSFORM_ROT_180: return ROT_180;
    case HAL_TRANSFORM_ROT_270: return ROT_270;
    }
    return ORIGIN;
}

enum color_format formatSkiaToDriver[] = {
    SRC_DST_FORMAT_END, //!< bitmap has not been configured
    CF_MSK_1BIT,
    CF_MSK_8BIT,
    SRC_DST_FORMAT_END, //!< kIndex8_Config is not supported by FIMG2D
    CF_RGB_565,
    CF_ARGB_4444,
    CF_ARGB_8888,
    SRC_DST_FORMAT_END, //!< kRLE_Index8_Config is not supported by FIMG2D
};

static unsigned int formatValueHAL2G2D(int hal_format,
    color_format *g2d_format,
    pixel_order *g2d_order,
    uint32_t *g2d_bytes)
{
    *g2d_format = MSK_FORMAT_END;
    *g2d_order = ARGB_ORDER_END;
    *g2d_bytes = 0;

    switch (hal_format) {
    /* 16bpp */
    case HAL_PIXEL_FORMAT_RGB_565:
        *g2d_format = CF_RGB_565;
        *g2d_order = AX_RGB;
        *g2d_bytes = 2;
        break;
    case HAL_PIXEL_FORMAT_RGBA_4444:
        *g2d_format = CF_ARGB_4444;
        *g2d_order = AX_BGR;
        *g2d_bytes = 2;
        break;
    /* 32bpp */
    case HAL_PIXEL_FORMAT_RGBX_8888:
        *g2d_format = CF_XRGB_8888;
        *g2d_order = AX_BGR;
        *g2d_bytes = 4;
        break;
    case HAL_PIXEL_FORMAT_G2D_BGRX_8888:
        *g2d_format = CF_XRGB_8888;
        *g2d_order = AX_RGB;
        *g2d_bytes = 4;
        break;
    case HAL_PIXEL_FORMAT_BGRA_8888:
#ifdef EXYNOS_SUPPORT_BGRX_8888
    case HAL_PIXEL_FORMAT_BGRX_8888:
#endif
        *g2d_format = CF_ARGB_8888;
        *g2d_order = AX_RGB;
        *g2d_bytes = 4;
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        *g2d_format = CF_ARGB_8888;
        *g2d_order = AX_BGR;
        *g2d_bytes = 4;
        break;
    /* 12bpp */
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCbCr_420_SP:
        *g2d_format = CF_YCBCR_420;
        *g2d_order = P2_CBCR;
        *g2d_bytes = 1;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
    case HAL_PIXEL_FORMAT_CUSTOM_YCrCb_420_SP:
        *g2d_format = CF_YCBCR_420;
        *g2d_order = P2_CRCB;
        *g2d_bytes = 1;
        break;
    default:
        ALOGE("%s::no matching colorformat(0x%x) fail\n",
        __func__, hal_format);
        return -1;
        break;
    }

    return 0;
}

int runCompositor(struct hwc_context_t *ctx,
        struct sec_img *src_img, struct sec_rect *src_rect,
        struct sec_img *dst_img, struct sec_rect *dst_rect,
        uint32_t transform, uint32_t global_alpha,
        unsigned long solid, blit_op mode, addr_space addr_type)
{
    struct fimg2d_blit cmd;
    struct fimg2d_scale scale;
    struct fimg2d_image srcImage, dstImage, mskImage;
    struct fimg2d_rect srcRect, dstRect;
    enum color_format g2d_format;
    enum pixel_order g2d_order;
    uint32_t g2d_bytes;

    cmd.op = mode;
    cmd.param.g_alpha = global_alpha;
    cmd.param.premult = PREMULTIPLIED;
    cmd.param.dither = false;
    cmd.param.rotate = rotateValueHAL2G2D(transform);
    cmd.param.solid_color = solid;
    cmd.param.repeat.mode = NO_REPEAT;
    cmd.param.repeat.pad_color = NULL;
    cmd.param.bluscr.mode = OPAQUE;
    cmd.param.bluscr.bs_color = NULL;
    cmd.param.bluscr.bg_color = NULL;

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s::\n"
            "s_fw %d s_fh %d s_w %d s_h %d s_f %x address %x s_offset %d "
            "s_mem_id %d\n"
            "d_fw %d d_fh %d d_w %d d_h %d d_f %x address %x d_offset %d "
            "d_mem_id %d rot %d ",
            __func__,
            src_img->f_w, src_img->f_h, src_img->w, src_img->h, src_img->format,
            src_img->paddr, src_img->offset, src_img->mem_id,
            dst_img->f_w, dst_img->f_h, dst_img->w, dst_img->h, dst_img->format,
            dst_img->paddr, dst_img->offset, dst_img->mem_id,
            transform);

    SEC_HWC_Log(HWC_LOG_DEBUG, "%s::\nsr_x %d sr_y %d sr_w %d sr_h %d "
            "\ndr_x %d dr_y %d dr_w %d dr_h %d ", __func__,
            src_rect->x, src_rect->y, src_rect->w, src_rect->h,
            dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);

    if ((cmd.param.rotate == ROT_90) || (cmd.param.rotate == ROT_270)) {
        if ((src_img->paddr != NULL) &&
                ((src_img->w != dst_img->h) || (src_img->h != dst_img->w))) {
            scale.mode = SCALING_BILINEAR;
            scale.src_w = src_img->w;
            scale.src_h = src_img->h;
            scale.dst_w = dst_img->h;
            scale.dst_h = dst_img->w;
            cmd.param.scaling = scale;
        }
        else {
            cmd.param.scaling.mode = NO_SCALING;
        }
    } else {
        if ((src_img->paddr != NULL) &&
                ((src_img->w != dst_img->w) || (src_img->h != dst_img->h))) {
            scale.mode = SCALING_BILINEAR;
            scale.src_w = src_img->w;
            scale.src_h = src_img->h;
            scale.dst_w = dst_img->w;
            scale.dst_h = dst_img->h;
            cmd.param.scaling = scale;
        }
        else {
           cmd.param.scaling.mode = NO_SCALING;
        }
    }

     if (src_img->paddr != 0) {
        formatValueHAL2G2D(src_img->format, &g2d_format, &g2d_order, &g2d_bytes);
        srcImage.addr.type = addr_type;
        srcImage.addr.start = (long unsigned)src_img->paddr;

        /* In case of YUV Surface g2d_bytes value will be 1 */
        if (g2d_bytes == 1) {
            srcImage.plane2.type = addr_type;
            srcImage.plane2.start =
                (long unsigned)(src_img->paddr + src_img->uoffset);
        }

        srcImage.width = src_img->f_w;
        srcImage.height = src_img->f_h;
        srcImage.stride = src_img->f_w * g2d_bytes;
        srcImage.order = g2d_order;
        srcImage.fmt = g2d_format;
        srcRect = {src_rect->x, src_rect->y,
            src_rect->x + src_rect->w, src_rect->y + src_rect->h};
        srcImage.rect = srcRect;
        srcImage.need_cacheopr = false;

        cmd.src = &srcImage;
    } else {
        cmd.src = NULL;
        cmd.param.scaling.mode = NO_SCALING;
    }

    if (dst_img->paddr != 0) {
        if (dst_img->format == HAL_PIXEL_FORMAT_RGBA_8888)
            dst_img->format = HAL_PIXEL_FORMAT_BGRA_8888;

        formatValueHAL2G2D(dst_img->format, &g2d_format, &g2d_order, &g2d_bytes);
        dstImage.addr.type = addr_type;
        dstImage.addr.start = (long unsigned)dst_img->paddr;

        /* In case of YUV Surface g2d_bytes value will be 1 */
        if (g2d_bytes == 1) {
            dstImage.plane2.type = addr_type;
            dstImage.plane2.start =
                (long unsigned)(dst_img->paddr + dst_img->uoffset);
        }

        dstImage.width = dst_img->f_w;
        dstImage.height = dst_img->f_h;
        dstImage.stride = dst_img->f_w * g2d_bytes;
        dstImage.order = g2d_order;
        dstImage.fmt = g2d_format;
        dstRect = {dst_rect->x, dst_rect->y,
            dst_rect->x + dst_rect->w, dst_rect->y + dst_rect->h};
        dstImage.rect = dstRect;
        dstImage.need_cacheopr = false;

        cmd.dst = &dstImage;
        cmd.param.clipping.enable = false;
    } else {
        cmd.dst = NULL;
    }

    cmd.msk = NULL;
    cmd.tmp = NULL;

    if (stretchFimgApi(&cmd) < 0) {
        SEC_HWC_Log(HWC_LOG_ERROR, "%s:stretch failed\n", __FUNCTION__);
        return -1;
    }

    return 0;
}
#endif
