/*
**
** Copyright 2012, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*!
 * \file      ExynosCameraHWInterface.h
 * \brief     source file for Android Camera HAL
 * \author    thun.hwang(thun.hwang@samsung.com)
 * \date      2010/06/03
 *
 * <b>Revision History: </b>
 * - 2011/12/31 : thun.hwang(thun.hwang@samsung.com) \n
 *   Initial version
 *
 * - 2012/02/01 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Adjust Android Standard features
 *
 * - 2012/03/14 : sangwoo.park(sw5771.park@samsung.com) \n
 *   Change file, class name to ExynosXXX.
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraHWInterface"
#include <cutils/log.h>

#include "ExynosCameraHWInterface.h"
#include "ExynosCameraHWImpl.h"

#include "exynos_format.h"
/*
#define CHECK_PREVIEW_TIME 0

#if ((CHECK_PREVIEW_TIME))
struct timeval oldtime, curtime;
#endif

#define VIDEO_COMMENT_MARKER_H          (0xFFBE)
#define VIDEO_COMMENT_MARKER_L          (0xFFBF)
#define VIDEO_COMMENT_MARKER_LENGTH     (4)
#define JPEG_EOI_MARKER                 (0xFFD9)
#define HIBYTE(x) (((x) >> 8) & 0xFF)
#define LOBYTE(x) ((x) & 0xFF)

//TODO: This values will be changed
#define BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR       "0.10,1.20,Infinity"
#define FRONT_CAMERA_FOCUS_DISTANCES_STR           "0.20,0.25,Infinity"

#define BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR      "0.10,0.20,Infinity"
#define BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR   "0.10,1.20,Infinity"

#define BACK_CAMERA_FOCUS_DISTANCE_INFINITY        "Infinity"
#define FRONT_CAMERA_FOCUS_DISTANCE_INFINITY       "Infinity"

#define PREVIEW_GSC_NODE_NUM (1)
#define PICTURE_GSC_NODE_NUM (2)
*/
// This hack does two things:
// -- it sets preview to NV21 (YUV420SP)
// -- it sets gralloc to YV12
//
// The reason being: the samsung encoder understands only yuv420sp, and gralloc
// does yv12 and rgb565.  So what we do is we break up the interleaved UV in
// separate V and U planes, which makes preview look good, and enabled the
// encoder as well.
//
// FIXME: Samsung needs to enable support for proper yv12 coming out of the
//        camera, and to fix their video encoder to work with yv12.
// FIXME: It also seems like either Samsung's YUV420SP (NV21) or img's YV12 has
//        the color planes switched.  We need to figure which side is doing it
//        wrong and have the respective party fix it.

namespace android {

static CameraInfo sCameraInfo[] = {
    {
        CAMERA_FACING_BACK,
        BACK_ROTATION,  /* orientation */
    },
    {
        CAMERA_FACING_FRONT,
        FRONT_ROTATION,  /* orientation */
    }
};

/** Close this device */

static camera_device_t *g_cam_device;

static int HAL_camera_device_close(struct hw_device_t* device)
{
    ALOGV("DEBUG(%s):", __func__);
    if (device) {
        camera_device_t *cam_device = (camera_device_t *)device;
        delete static_cast<ExynosCameraHWImpl *>(cam_device->priv);
        free(cam_device);
        g_cam_device = 0;
    }
    return 0;
}

static inline ExynosCameraHWImpl *obj(struct camera_device *dev)
{
    return reinterpret_cast<ExynosCameraHWImpl *>(dev->priv);
}

/** Set the preview_stream_ops to which preview frames are sent */
static int HAL_camera_device_set_preview_window(struct camera_device *dev,
                                                struct preview_stream_ops *buf)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->setPreviewWindow(buf);
}

/** Set the notification and data callbacks */
static void HAL_camera_device_set_callbacks(struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                           get_memory,
                           user);
}

/**
 * The following three functions all take a msg_type, which is a bitmask of
 * the messages defined in include/ui/Camera.h
 */

/**
 * Enable a message, or set of messages.
 */
static void HAL_camera_device_enable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->enableMsgType(msg_type);
}

/**
 * Disable a message, or a set of messages.
 *
 * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
 * HAL should not rely on its client to call releaseRecordingFrame() to
 * release video recording frames sent out by the cameral HAL before and
 * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
 * clients must not modify/access any video recording frame after calling
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
 */
static void HAL_camera_device_disable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->disableMsgType(msg_type);
}

/**
 * Query whether a message, or a set of messages, is enabled.  Note that
 * this is operates as an AND, if any of the messages queried are off, this
 * will return false.
 */
static int HAL_camera_device_msg_type_enabled(struct camera_device *dev, int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->msgTypeEnabled(msg_type);
}

/**
 * Start preview mode.
 */
static int HAL_camera_device_start_preview(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->startPreview();
}

/**
 * Stop a previously started preview.
 */
static void HAL_camera_device_stop_preview(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->stopPreview();
}

/**
 * Returns true if preview is enabled.
 */
static int HAL_camera_device_preview_enabled(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->previewEnabled();
}

/**
 * Request the camera HAL to store meta data or real YUV data in the video
 * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
 * it is not called, the default camera HAL behavior is to store real YUV
 * data in the video buffers.
 *
 * This method should be called before startRecording() in order to be
 * effective.
 *
 * If meta data is stored in the video buffers, it is up to the receiver of
 * the video buffers to interpret the contents and to find the actual frame
 * data with the help of the meta data in the buffer. How this is done is
 * outside of the scope of this method.
 *
 * Some camera HALs may not support storing meta data in the video buffers,
 * but all camera HALs should support storing real YUV data in the video
 * buffers. If the camera HAL does not support storing the meta data in the
 * video buffers when it is requested to do do, INVALID_OPERATION must be
 * returned. It is very useful for the camera HAL to pass meta data rather
 * than the actual frame data directly to the video encoder, since the
 * amount of the uncompressed frame data can be very large if video size is
 * large.
 *
 * @param enable if true to instruct the camera HAL to store
 *      meta data in the video buffers; false to instruct
 *      the camera HAL to store real YUV data in the video
 *      buffers.
 *
 * @return OK on success.
 */
static int HAL_camera_device_store_meta_data_in_buffers(struct camera_device *dev, int enable)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->storeMetaDataInBuffers(enable);
}

/**
 * Start record mode. When a record image is available, a
 * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
 * frame. Every record frame must be released by a camera HAL client via
 * releaseRecordingFrame() before the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames,
 * and the client must not modify/access any video recording frames.
 */
static int HAL_camera_device_start_recording(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->startRecording();
}

/**
 * Stop a previously started recording.
 */
static void HAL_camera_device_stop_recording(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->stopRecording();
}

/**
 * Returns true if recording is enabled.
 */
static int HAL_camera_device_recording_enabled(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->recordingEnabled();
}

/**
 * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
 *
 * It is camera HAL client's responsibility to release video recording
 * frames sent out by the camera HAL before the camera HAL receives a call
 * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames.
 */
static void HAL_camera_device_release_recording_frame(struct camera_device *dev,
                                const void *opaque)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->releaseRecordingFrame(opaque);
}

/**
 * Start auto focus, the notification callback routine is called with
 * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
 * called again if another auto focus is needed.
 */
static int HAL_camera_device_auto_focus(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->autoFocus();
}

/**
 * Cancels auto-focus function. If the auto-focus is still in progress,
 * this function will cancel it. Whether the auto-focus is in progress or
 * not, this function will return the focus position to the default.  If
 * the camera does not support auto-focus, this is a no-op.
 */
static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->cancelAutoFocus();
}

/**
 * Take a picture.
 */
static int HAL_camera_device_take_picture(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->takePicture();
}

/**
 * Cancel a picture that was started with takePicture. Calling this method
 * when no picture is being taken is a no-op.
 */
static int HAL_camera_device_cancel_picture(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->cancelPicture();
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int HAL_camera_device_set_parameters(struct camera_device *dev,
                                            const char *parms)
{
    ALOGV("DEBUG(%s):", __func__);
    String8 str(parms);
    CameraParameters p(str);
    return obj(dev)->setParameters(p);
}

/** Return the camera parameters. */
char *HAL_camera_device_get_parameters(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    String8 str;
    CameraParameters parms = obj(dev)->getParameters();
    str = parms.flatten();
    return strdup(str.string());
}

static void HAL_camera_device_put_parameters(struct camera_device *dev, char *parms)
{
    ALOGV("DEBUG(%s):", __func__);
    free(parms);
}

/**
 * Send command to camera driver.
 */
static int HAL_camera_device_send_command(struct camera_device *dev,
                    int32_t cmd, int32_t arg1, int32_t arg2)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->sendCommand(cmd, arg1, arg2);
}

/**
 * Release the hardware resources owned by this object.  Note that this is
 * *not* done in the destructor.
 */
static void HAL_camera_device_release(struct camera_device *dev)
{
    ALOGV("DEBUG(%s):", __func__);
    obj(dev)->release();
}

/**
 * Dump state of the camera hardware
 */
static int HAL_camera_device_dump(struct camera_device *dev, int fd)
{
    ALOGV("DEBUG(%s):", __func__);
    return obj(dev)->dump(fd);
}

static int HAL_getNumberOfCameras()
{
    ALOGV("DEBUG(%s):", __func__);
    return sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
}

static int HAL_getCameraInfo(int cameraId, struct camera_info *cameraInfo)
{
    ALOGV("DEBUG(%s):", __func__);
    memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));
    return 0;
}

#define SET_METHOD(m) m : HAL_camera_device_##m

static camera_device_ops_t camera_device_ops = {
        SET_METHOD(set_preview_window),
        SET_METHOD(set_callbacks),
        SET_METHOD(enable_msg_type),
        SET_METHOD(disable_msg_type),
        SET_METHOD(msg_type_enabled),
        SET_METHOD(start_preview),
        SET_METHOD(stop_preview),
        SET_METHOD(preview_enabled),
        SET_METHOD(store_meta_data_in_buffers),
        SET_METHOD(start_recording),
        SET_METHOD(stop_recording),
        SET_METHOD(recording_enabled),
        SET_METHOD(release_recording_frame),
        SET_METHOD(auto_focus),
        SET_METHOD(cancel_auto_focus),
        SET_METHOD(take_picture),
        SET_METHOD(cancel_picture),
        SET_METHOD(set_parameters),
        SET_METHOD(get_parameters),
        SET_METHOD(put_parameters),
        SET_METHOD(send_command),
        SET_METHOD(release),
        SET_METHOD(dump),
};

#undef SET_METHOD

static int HAL_camera_device_open(const struct hw_module_t* module,
                                  const char *id,
                                  struct hw_device_t** device)
{
    ALOGD("DEBUG(%s):", __func__);

    int cameraId = atoi(id);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        ALOGE("ERR(%s):Invalid camera ID %s", __func__, id);
        return -EINVAL;
    }

    if (g_cam_device) {
        if (obj(g_cam_device)->getCameraId() == cameraId) {
            ALOGV("DEBUG(%s):returning existing camera ID %s", __func__, id);
            goto done;
        } else {
            ALOGE("ERR(%s):Cannot open camera %d. camera %d is already running!",
                    __func__, cameraId, obj(g_cam_device)->getCameraId());
            return -ENOSYS;
        }
    }

    g_cam_device = (camera_device_t *)malloc(sizeof(camera_device_t));
    if (!g_cam_device)
        return -ENOMEM;

    g_cam_device->common.tag     = HARDWARE_DEVICE_TAG;
    g_cam_device->common.version = 1;
    g_cam_device->common.module  = const_cast<hw_module_t *>(module);
    g_cam_device->common.close   = HAL_camera_device_close;

    g_cam_device->ops = &camera_device_ops;

    ALOGD("DEBUG(%s):open camera %s", __func__, id);

    g_cam_device->priv = new ExynosCameraHWImpl(cameraId, g_cam_device);

done:
    *device = (hw_device_t *)g_cam_device;
    ALOGV("DEBUG(%s):opened camera %s (%p)", __func__, id, *device);
    return 0;
}

static hw_module_methods_t camera_module_methods = {
            open : HAL_camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
      common : {
          tag           : HARDWARE_MODULE_TAG,
          version_major : 1,
          version_minor : 0,
          id            : CAMERA_HARDWARE_MODULE_ID,
          name          : "orion camera HAL",
          author        : "Samsung Corporation",
          methods       : &camera_module_methods,
      },
      get_number_of_cameras : HAL_getNumberOfCameras,
      get_camera_info       : HAL_getCameraInfo
    };
}

}; // namespace android
