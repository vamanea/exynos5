/*
 *
 * Copyright 2012 Samsung Electronics S.LSI Co. LTD
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
 * \file      exynos_gscaler.h
 * \brief     header file for Gscaler HAL
 * \author    ShinWon Lee (shinwon.lee@samsung.com)
 * \date      2012/01/09
 *
 * <b>Revision History: </b>
 * - 2012/01/09 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Create
 *
 * - 2012/02/07 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Change file name to exynos_gscaler.h
 *
 * - 2012/02/09 : Sangwoo, Parkk(sw5771.park@samsung.com) \n
 *   Use Multiple Gscaler by Multiple Process
 *
 * - 2012/02/20 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Add exynos_gsc_set_rotation() API
 *
 * - 2012/02/20 : ShinWon Lee(shinwon.lee@samsung.com) \n
 *   Add size constrain
 *
 */

/*!
 * \defgroup exynos_gscaler
 * \brief API for gscaler
 * \addtogroup Exynos
 */

#ifndef EXYNOS_GSCALER_H_
#define EXYNOS_GSCALER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t fw;
    uint32_t fh;
    uint32_t format;
    uint32_t yaddr;
    uint32_t uaddr;
    uint32_t vaddr;
    uint32_t rot;
    uint32_t cacheable;
    uint32_t drmMode;
    uint32_t fieldOrder;
    uint32_t reqBufCnt;
    uint32_t rgb_csc_type;
    uint32_t rgb_csc_eq_type;
} exynos_gsc_img;

/*
 * Create libgscaler handle.
 * Gscaler dev_num is dynamically changed.
 *
 * \ingroup exynos_gscaler
 *
 * \return
 *   libgscaler handle
 */
void *exynos_gsc_create(
    void);

/*!
 * Create exclusive libgscaler handle.
 * Other module can't use dev_num of Gscaler.
 *
 * \ingroup exynos_gscaler
 *
 * \param dev_num
 *   gscaler dev_num[in]
 * \param gsc_mode
 *It should be set to GSC_M2M_MODE or GSC_OUTPUT_MODE.
 *
 *\param out_mode
 *It should be set to GSC_OUT_FIMD or GSC_OUT_TV.
 *
 * \return
 *   libgscaler handle
 */
void *exynos_gsc_create_exclusive(
    int dev_num,
    int gsc_mode,
    int out_mode);

/*!
 * Destroy libgscaler handle
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 */
void exynos_gsc_destroy(
    void *handle);

/*!
 * Set source format.
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \param width
 *   image width[in]
 *
 * \param height
 *   image height[in]
 *
 * \param crop_left
 *   image left crop size[in]
 *
 * \param crop_top
 *   image top crop size[in]
 *
 * \param crop_width
 *   cropped image width[in]
 *
 * \param crop_height
 *   cropped image height[in]
 *
 * \param v4l2_colorformat
 *   color format[in]
 *
 * \param cacheable
 *   ccacheable[in]
 *
 * \param mode_drm
 *   mode_drm[in]
 *
 * \return
 *   error code
 */
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
    unsigned int mode_drm);

/*!
 * Set destination format.
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \param width
 *   image width[in]
 *
 * \param height
 *   image height[in]
 *
 * \param crop_left
 *   image left crop size[in]
 *
 * \param crop_top
 *   image top crop size[in]
 *
 * \param crop_width
 *   cropped image width[in]
 *
 * \param crop_height
 *   cropped image height[in]
 *
 * \param v4l2_colorformat
 *   color format[in]
 *
 * \param cacheable
 *   ccacheable[in]
 *
 * \param mode_drm
 *   mode_drm[in]
 *
 * \return
 *   error code
 */
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
    unsigned int mode_drm);

/*!
 * Set rotation.
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \param rotation
 *   image rotation. It should be multiple of 90[in]
 *
 * \param flip_horizontal
 *   image flip_horizontal[in]
 *
 * \param flip_vertical
 *   image flip_vertical[in]
 *
 * \return
 *   error code
 */
int exynos_gsc_set_rotation(
    void *handle,
    int   rotation,
    int   flip_horizontal,
    int   flip_vertical);

/*!
 * Set source buffer
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \param addr
 *   buffer pointer array[in]
 *
 * \return
 *   error code
 */
int exynos_gsc_set_src_addr(
    void *handle,
    void *addr[3]);

/*!
 * Set destination buffer
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \param addr
 *   buffer pointer array[in]
 *
 * \return
 *   error code
 */
int exynos_gsc_set_dst_addr(
    void *handle,
    void *addr[3]);

/*!
 * Convert color space with presetup color format
 *
 * \ingroup exynos_gscaler
 *
 * \param handle
 *   libgscaler handle[in]
 *
 * \return
 *   error code
 */
int exynos_gsc_convert(
    void *handle);

int exynos_gsc_set_out_mode(
    void *handle,
    int out_mode);

/*
*api for setting the GSC config.
It configures the GSC for given config
*/
int exynos_gsc_config_exclusive(
    void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img);

/*
*api for GSC-OUT run.
It queues the srcBuf to GSC and deques a buf from driver.
It should be called after configuring the GSC.
*/
int exynos_gsc_run_exclusive(
    void *handle,
    exynos_gsc_img *src_img,
    exynos_gsc_img *dst_img);

/*
*api for GSC stop.
It stops the GSC OUT streaming.
*/
int exynos_gsc_stop_exclusive
(void *handle);

/*api for gsc streamoff
*/
int exynos_gsc_just_stop
(void *handle);

/*
*api for GSC wait for frame done.
It calls a dqbuf to wait for frame done.
*/
int exynos_gsc_wait_done
(void *handle);

/*
*api for setting the GSC flags & specific configuration.
*/
int exynos_gsc_set_ctrl
(void *handle,
unsigned int ctrl_id,
void *val);

enum {
    GSC_M2M_MODE = 0,
    GSC_OUTPUT_MODE,
    GSC_CAPTURE_MODE,
    GSC_RESERVED_MODE,
};

/*GSC out mode type */
enum {
    GSC_OUT_DUMMY = 0,
    GSC_OUT_FIMD,
    GSC_OUT_TV_HDMI_YCBCR,
    GSC_OUT_TV_HDMI_RGB,
    GSC_OUT_TV_DVI,
    GSC_OUT_RESERVED,
};

/*ctrl_id */
enum {
    GSC_CTRL_OUT_MODE = 0,
    GSC_CTRL_RESERVED,
};

enum {
    GSC_RGB_CSC_NARROW = 0,
    GSC_RGB_CSC_WIDE,
};

enum {
    GSC_RGB_601_EQ = 0,
    GSC_RGB_709_EQ,
};

#ifdef __cplusplus
}
#endif

#endif /*EXYNOS_GSCALER_H_*/
