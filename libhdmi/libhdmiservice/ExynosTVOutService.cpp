/*
**
** Copyright 2012, The Android Open Source Project
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

/*
**
** @author  Taikyung, Yu(taikyung.yu@samsung.com)
** @date    2011-07-06
*/

#define LOG_TAG "SecTVOutService"

#include <binder/IServiceManager.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/Log.h>
#include <linux/fb.h>
#include "exynos_format.h"
#include "ExynosTVOutService.h"

pthread_cond_t  sync_cond_video;
pthread_mutex_t sync_mutex_video;
pthread_cond_t  sync_cond_ui;
pthread_mutex_t sync_mutex_ui;

namespace android {
#define DEFAULT_LCD_WIDTH               2560
#define DEFAULT_LCD_HEIGHT              1600

    enum {
        SET_HDMI_STATUS = IBinder::FIRST_CALL_TRANSACTION,
        GET_HDMI_STATUS,
        SET_HDMI_MODE,
        SET_HDMI_RESOLUTION,
        SET_HDMI_HDCP,
        SET_HDMI_ROTATE,
        SET_HDMI_HWCLAYER,
        SET_HDMI_CLEARLAYER,
        SET_HDMI_PATH,
        SET_HDMI_DRM,
        BLIT_2_HDMI,
        SET_HDMI_LAYER_ENABLE,
        SET_HDMI_LAYER_DISABLE
    };

    int SecTVOutService::HdmiFlushThreadForUI()
    {
        while (!mExitHdmiFlushThreadForUI) {
            pthread_mutex_lock(&sync_mutex_ui);
            pthread_cond_wait(&sync_cond_ui, &sync_mutex_ui);
            pthread_mutex_unlock(&sync_mutex_ui);

#ifdef CHECK_UI_TIME
            struct timeval hdmi_start, hdmi_end;
            gettimeofday(&hdmi_start, NULL);
#endif
            if (mSecHdmi.flush(mMsgForUI->mSrcWidth,
                        mMsgForUI->mSrcHeight,
                        mMsgForUI->mSrcColorFormat,
                        mMsgForUI->mSrcYAddr,
                        mMsgForUI->mSrcCbAddr,
                        mMsgForUI->mSrcCrAddr,
                        mMsgForUI->mDstX,
                        mMsgForUI->mDstY,
                        mMsgForUI->mHdmiLayer,
                        mMsgForUI->mHdmiMode,
                        mMsgForUI->mflag_full_display) == false)
                ALOGE("%s::mSecHdmi.flush() on HDMI_MODE_UI fail", __func__);

#ifdef CHECK_UI_TIME
            gettimeofday(&hdmi_end, NULL);
            ALOGD("UI rendering : used time : %d",
                    (hdmi_end.tv_sec - hdmi_start.tv_sec)*1000+(hdmi_end.tv_usec - hdmi_start.tv_usec)/1000);
#endif
        }
        return 0;
    }

    int SecTVOutService::HdmiFlushThreadForVIDEO()
    {
        while (!mExitHdmiFlushThreadForVIDEO) {
            pthread_mutex_lock(&sync_mutex_video);
            pthread_cond_wait(&sync_cond_video, &sync_mutex_video);
            pthread_mutex_unlock(&sync_mutex_video);

#ifdef CHECK_VIDEO_TIME
            struct timeval hdmi_start, hdmi_end;
            gettimeofday(&hdmi_start, NULL);
#endif
            if (mSecHdmi.flush(mMsgForVideo->mSrcWidth,
                        mMsgForVideo->mSrcHeight,
                        mMsgForVideo->mSrcColorFormat,
                        mMsgForVideo->mSrcYAddr,
                        mMsgForVideo->mSrcCbAddr,
                        mMsgForVideo->mSrcCrAddr,
                        mMsgForVideo->mDstX,
                        mMsgForVideo->mDstY,
                        mMsgForVideo->mHdmiLayer,
                        mMsgForVideo->mHdmiMode,
                        mMsgForVideo->mflag_full_display) == false)
                ALOGE("%s::mSecHdmi.flush() on HDMI_MODE_VIDEO fail", __func__);

#ifdef CHECK_VIDEO_TIME
            gettimeofday(&hdmi_end, NULL);
            ALOGD("VIDEO rendering : used time : %d",
                    (hdmi_end.tv_sec - hdmi_start.tv_sec)*1000+(hdmi_end.tv_usec - hdmi_start.tv_usec)/1000);
#endif
        }
        return 0;
    }


    int SecTVOutService::instantiate()
    {
        ALOGD("SecTVOutService instantiate");
        int r = defaultServiceManager()->addService(String16( "SecTVOutService"), new SecTVOutService ());
        ALOGD("SecTVOutService r=%d", r);

        return r;
    }

    SecTVOutService::SecTVOutService () {
        ALOGV("SecTVOutService created");
        mHdmiCableInserted = false;
        mUILayerMode = SecHdmi::HDMI_LAYER_GRAPHIC_0;
        mHdmiPath = DEFAULT_UI_PATH;
        mHdmiResolution = DEFAULT_HDMI_RESOLUTION_VALUE;
        mHdmis3dMode = HDMI_2D;
        mHwcLayer = 0;
        mExitHdmiFlushThreadForUI = false;
        mExitHdmiFlushThreadForVIDEO = false;
        mEnable = 0;
        setLCDsize();
        mHdmiFlushThreadForUI = new HDMIFlushThreadForUI(this);
        mHdmiFlushThreadForVIDEO = new HDMIFlushThreadForVIDEO(this);

        pthread_cond_init(&sync_cond_video, NULL);
        pthread_mutex_init(&sync_mutex_video, NULL);
        pthread_cond_init(&sync_cond_ui, NULL);
        pthread_mutex_init(&sync_mutex_ui, NULL);

        if (mSecHdmi.create(mLCD_width, mLCD_height) == false)
            ALOGE("%s::mSecHdmi.create() fail", __func__);
        else
            setHdmiStatus(1, true);
    }

    void SecTVOutService::setLCDsize(void) {
            char const * const device_template[] = {
                "/dev/graphics/fb%u",
                "/dev/fb%u",
                0 };

            int fd = -1;
            int i = 0;
            char name[64];

            while ((fd==-1) && device_template[i]) {
                snprintf(name, 64, device_template[i], 0);
                fd = open(name, O_RDWR, 0);
                i++;
            }
            if (fd > 0) {
                struct fb_var_screeninfo info;
                if (ioctl(fd, FBIOGET_VSCREENINFO, &info) != -1) {
                    mLCD_width  = info.xres;
                    mLCD_height = info.yres;
                } else {
                    mLCD_width  = DEFAULT_LCD_WIDTH;
                    mLCD_height = DEFAULT_LCD_HEIGHT;
                }
                close(fd);
            }
            return;
    }

    SecTVOutService::~SecTVOutService () {
        ALOGV ("SecTVOutService destroyed");

        if (mHdmiFlushThreadForUI != NULL) {
            mHdmiFlushThreadForUI->requestExit();
            mExitHdmiFlushThreadForUI = true;
            mHdmiFlushThreadForUI->requestExitAndWait();
            mHdmiFlushThreadForUI.clear();
        }

        if (mHdmiFlushThreadForVIDEO != NULL) {
            mHdmiFlushThreadForVIDEO->requestExit();
            mExitHdmiFlushThreadForVIDEO = true;
            mHdmiFlushThreadForVIDEO->requestExitAndWait();
            mHdmiFlushThreadForVIDEO.clear();
        }
    }

    status_t SecTVOutService::onTransact(uint32_t code, const Parcel & data, Parcel * reply, uint32_t flags)
    {
        switch (code) {
        case SET_HDMI_STATUS: {
            int status = data.readInt32();
            setHdmiStatus(status, false);
        } break;

        case GET_HDMI_STATUS: {
             uint32_t status = getHdmiCableStatus();
             reply->writeInt32(status);
        } break;

        case SET_HDMI_MODE: {
            int mode = data.readInt32();
            setHdmiMode(mode);
        } break;

        case SET_HDMI_RESOLUTION: {
            int resolution = data.readInt32();
            int s3dMode = data.readInt32();
            setHdmiResolution(resolution, (HDMI_S3D_MODE)s3dMode);
        } break;

        case SET_HDMI_HDCP: {
            int enHdcp = data.readInt32();
            setHdmiHdcp(enHdcp);
        } break;

        case SET_HDMI_ROTATE: {
            int rotVal = data.readInt32();
            int hwcLayer = data.readInt32();
            setHdmiRotate(rotVal, hwcLayer);
        } break;

        case SET_HDMI_HWCLAYER: {
            int hwcLayer = data.readInt32();
            setHdmiHwcLayer((uint32_t)hwcLayer);
        } break;

        case SET_HDMI_CLEARLAYER: {
            int enable = data.readInt32();
            setHdmiClearLayer((uint32_t)enable);
        } break;

        case SET_HDMI_PATH: {
            int path = data.readInt32();
            setHdmiPath(path);
        } break;

        case SET_HDMI_DRM: {
            int drmMode = data.readInt32();
            setHdmiDRM(drmMode);
        } break;

        case BLIT_2_HDMI: {
            uint32_t w = data.readInt32();
            uint32_t h = data.readInt32();
            uint32_t colorFormat = data.readInt32();
            uint32_t physYAddr  = data.readInt32();
            uint32_t physCbAddr = data.readInt32();
            uint32_t physCrAddr = data.readInt32();
            uint32_t dstX   = data.readInt32();
            uint32_t dstY   = data.readInt32();
            uint32_t hdmiLayer   = data.readInt32();
            uint32_t flag_full_display = data.readInt32();

            blit2Hdmi(w, h, colorFormat, physYAddr, physCbAddr, physCrAddr, dstX, dstY, hdmiLayer, flag_full_display);
        } break;

        case SET_HDMI_LAYER_ENABLE: {
            int hdmiLayer = data.readInt32();
            setHdmiLayerEnable(hdmiLayer);
        } break;

        case SET_HDMI_LAYER_DISABLE: {
            int hdmiLayer = data.readInt32();
            setHdmiLayerDisable(hdmiLayer);
        } break;

        default :
            ALOGE ( "onTransact::default");
            return BBinder::onTransact (code, data, reply, flags);
        }

        return NO_ERROR;
    }

    void SecTVOutService::setHdmiStatus(uint32_t status, bool isBooting)
    {
        //ALOGD("%s HDMI cable status = %d", __func__, status);
        {
            Mutex::Autolock _l(mLock);

            bool hdmiCableInserted = (bool)status;

            if (mHdmiCableInserted == hdmiCableInserted)
                return;

            if (hdmiCableInserted == true) {
                if (mSecHdmi.connect() == false) {
                    ALOGE("%s::mSecHdmi.connect() fail", __func__);
                    hdmiCableInserted = false;
                }
            } else {
                if (mSecHdmi.disconnect() == false)
                    ALOGE("%s::mSecHdmi.disconnect() fail", __func__);
            }

            mHdmiCableInserted = hdmiCableInserted;
        }

        if ((!isBooting) && (hdmiCableInserted() == true)) {
            this->setHdmiResolution(mHdmiResolution, mHdmis3dMode);
            if (mEnable == 1) {
                this->setHdmiPath(DEFAULT_UI_PATH);
                this->blit2Hdmi(mLCD_width, mLCD_height, HAL_PIXEL_FORMAT_BGRA_8888, 0, 0, 0, 0, 0, HDMI_MODE_MIRROR, 0);
            }
        }
    }

    uint32_t SecTVOutService::getHdmiCableStatus()
    {
        Mutex::Autolock _l(mLock);

        //ALOGD("%s TV Cable status = %d", __func__, hdmiCableInserted());
        return hdmiCableInserted();
    }

    void SecTVOutService::setHdmiMode(uint32_t mode)
    {
        //ALOGD("%s TV mode = %d", __func__, mode);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdmiOutputMode(mode)) == false) {
            ALOGE("%s::mSecHdmi.setHdmiOutputMode() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiResolution(uint32_t resolution, HDMI_S3D_MODE s3dMode)
    {
        //ALOGD("%s TV resolution = %d s3dMode = %d", __func__, resolution, s3dMode);
        Mutex::Autolock _l(mLock);

        if (resolution == 0) {
            resolution = mHdmiResolution;
        }

        mHdmiResolution = resolution;
        mHdmis3dMode    = s3dMode;

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdmiResolution(resolution, s3dMode)) == false) {
            ALOGE("%s::mSecHdmi.setHdmiResolution() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiHdcp(uint32_t hdcp_en)
    {
        //ALOGD("%s TV HDCP = %d", __func__, hdcp_en);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setHdcpMode(hdcp_en)) == false) {
            ALOGE("%s::mSecHdmi.setHdcpMode() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer)
    {
        //ALOGD("%s TV ROTATE = %d", __func__, rotVal);
        Mutex::Autolock _l(mLock);

        if ((hdmiCableInserted() == true) && (mSecHdmi.setUIRotation(rotVal, hwcLayer)) == false) {
            ALOGE("%s::mSecHdmi.setUIRotation() fail", __func__);
            return;
        }
    }

    void SecTVOutService::setHdmiHwcLayer(uint32_t hwcLayer)
    {
        //ALOGD("%s TV HWCLAYER = %d", __func__, hwcLayer);
        Mutex::Autolock _l(mLock);

        mHwcLayer = hwcLayer;
        return;
    }

    void SecTVOutService::setHdmiClearLayer(uint32_t enable)
    {
        Mutex::Autolock _l(mLock);

        if (enable == 0) {
            if (mSecHdmi.clearHdmiWriteBack() == false)
                ALOGE("%s::mSecHdmi.clearHdmiWriteBack() fail", __func__);

            for (int layer = SecHdmi::HDMI_LAYER_BASE + 1; layer < SecHdmi::HDMI_LAYER_MAX; layer++)
                if (mSecHdmi.clear(layer) == false)
                    ALOGE("%s::mSecHdmi.clear(%d) fail", __func__, layer);
        }
        mEnable = enable;
        return;
    }

    void SecTVOutService::setHdmiPath(uint32_t path)
    {
        //ALOGD("%s HdmiPath = %d", __func__, path);
        Mutex::Autolock _l(mLock);

        if (mHdmiPath == path)
            return;

        if (mSecHdmi.setHdmiPath(path) == false) {
            ALOGE("%s::mSecHdmi.setHdmiPath(%d) fail", __func__, path);
            return;
        }
        mHdmiPath = path;

        return;
    }

    void SecTVOutService::setHdmiDRM(uint32_t drmMode)
    {
        //ALOGD("%s HdmiDrmMode = %d", __func__, drmMode);
        Mutex::Autolock _l(mLock);

        if (mSecHdmi.setHdmiDrmMode(drmMode) == false) {
            ALOGE("%s::mSecHdmi.setHdmiDrmMode(%d) fail", __func__, drmMode);
            return;
        }

        return;
    }

    void SecTVOutService::blit2Hdmi(uint32_t w, uint32_t h, uint32_t colorFormat,
            uint32_t pPhyYAddr, uint32_t pPhyCbAddr, uint32_t pPhyCrAddr,
            uint32_t dstX, uint32_t dstY,
            uint32_t hdmiMode,
            uint32_t flag_full_display)
    {
        Mutex::Autolock _l(mLock);

        if (hdmiCableInserted() == false)
            return;

        int hdmiLayer = SecHdmi::HDMI_LAYER_VIDEO;

        switch (hdmiMode) {
            case HDMI_MODE_MIRROR:
#if !defined(BOARD_USES_HDMI_SUBTITLES)
                if ((mHwcLayer == 0) && (mHdmiPath == HDMI_PATH_WRITEBACK))
                    hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_0;
                else
                    return;
#else
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_0;
#endif

                pthread_mutex_lock(&sync_mutex_ui);
                mMsgForUI = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                        dstX, dstY, hdmiLayer, flag_full_display, hdmiMode);
                pthread_cond_signal(&sync_cond_ui);
                pthread_mutex_unlock(&sync_mutex_ui);
                break;

            case HDMI_MODE_UI_0:
                if (mHdmiPath == HDMI_PATH_WRITEBACK) {
                    ALOGE("HDMI_MODE_UI_0 should not use HDMI_PATH_WRITEBACK\n");
                    break;
                }
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_0;

                pthread_mutex_lock(&sync_mutex_ui);
                mMsgForUI = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                        dstX, dstY, hdmiLayer, flag_full_display, hdmiMode);
                pthread_cond_signal(&sync_cond_ui);
                pthread_mutex_unlock(&sync_mutex_ui);
                break;

            case HDMI_MODE_UI_1:
                if (mHdmiPath == HDMI_PATH_WRITEBACK) {
                    ALOGE("HDMI_MODE_UI_1 should not use HDMI_PATH_WRITEBACK\n");
                    break;
                }
                hdmiLayer = SecHdmi::HDMI_LAYER_GRAPHIC_1;
                pthread_mutex_lock(&sync_mutex_ui);
                mMsgForUI = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                        dstX, dstY, hdmiLayer, flag_full_display, hdmiMode);
                pthread_cond_signal(&sync_cond_ui);
                pthread_mutex_unlock(&sync_mutex_ui);
                break;

            case HDMI_MODE_VIDEO :
                if (mHdmiPath == HDMI_PATH_WRITEBACK) {
                    ALOGE("HDMI_MODE_VIDEO should not use HDMI_PATH_WRITEBACK\n");
                    break;
                }

                pthread_mutex_lock(&sync_mutex_video);
                mMsgForVideo = new SecHdmiEventMsg(&mSecHdmi, w, h, colorFormat, pPhyYAddr, pPhyCbAddr, pPhyCrAddr,
                        dstX, dstY, SecHdmi::HDMI_LAYER_VIDEO, flag_full_display, HDMI_MODE_VIDEO);

                pthread_cond_signal(&sync_cond_video);
                pthread_mutex_unlock(&sync_mutex_video);
                break;

            default:
                ALOGE("unmatched HDMI_MODE : %d", hdmiMode);
                break;
        }

        return;
    }

    void SecTVOutService::setHdmiLayerEnable(uint32_t hdmiLayer)
    {
        mSecHdmi.setHdmiLayerEnable(hdmiLayer);
    }

    void SecTVOutService::setHdmiLayerDisable(uint32_t hdmiLayer)
    {
        mSecHdmi.setHdmiLayerDisable(hdmiLayer);
    }

    bool SecTVOutService::hdmiCableInserted(void)
    {
        return mHdmiCableInserted;
    }
}
