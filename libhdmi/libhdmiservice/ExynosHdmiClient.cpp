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

#define LOG_TAG "libhdmiclient"

#include "ExynosHdmiClient.h"

namespace android {

static sp<IExynosTVOut> g_ExynosTVOutService = 0;

ExynosHdmiClient::ExynosHdmiClient()
{
    g_ExynosTVOutService = m_getExynosTVOutService();
    mEnable = 0;
}

ExynosHdmiClient::~ExynosHdmiClient()
{
}

ExynosHdmiClient * ExynosHdmiClient::getInstance(void)
{
    static ExynosHdmiClient singleton;
    return &singleton;
}

void ExynosHdmiClient::setHdmiCableStatus(int status)
{
    ALOGD("%s HDMI status: %d\n", __func__, status);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiCableStatus(status);
}

uint32_t ExynosHdmiClient::getHdmiCableStatus()
{
    //ALOGD("%s get HDMI cable status: %d\n", __func__, cableStatus);

    uint32_t cableStatus = 0;
    if (g_ExynosTVOutService == 0)
        g_ExynosTVOutService = m_getExynosTVOutService();

    if (g_ExynosTVOutService != 0 )
        cableStatus = g_ExynosTVOutService->getHdmiCableStatus();

    return cableStatus;
}

void ExynosHdmiClient::setHdmiMode(int mode)
{
    //ALOGD("%s HDMI Mode: %d\n", __func__, mode);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiMode(mode);
}

void ExynosHdmiClient::setHdmiResolution(int resolution, HDMI_S3D_MODE s3dMode)
{
    //ALOGD("%s HDMI Resolution = %d, s3dMode = %d\n", __func__, resolution, s3dMode);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiResolution(resolution, s3dMode);
}

void ExynosHdmiClient::setHdmiHdcp(int enHdcp)
{
    //ALOGD("%s HDMI HDCP: %d\n", __func__, enHdcp);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiHdcp(enHdcp);
}

void ExynosHdmiClient::setHdmiRotate(int rotVal, uint32_t hwcLayer)
{
    //ALOGD("%s HDMI ROTATE: %d\n", __func__, rotVal);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiRotate(rotVal, hwcLayer);
}

void ExynosHdmiClient::setHdmiHwcLayer(uint32_t hwcLayer)
{
    //ALOGD("%s HDMI HWCLAYER: %d\n", __func__, hwcLayer);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiHwcLayer(hwcLayer);
}

void ExynosHdmiClient::setHdmiEnable(uint32_t enable)
{
    //ALOGD("%s HDMI ENABLE: %d\n", __func__, enable);

    if (g_ExynosTVOutService != 0) {
        mEnable = enable;

        //ALOGD("%s HDMI EARLYSUSPEND : %d\n", __func__, enable);
        g_ExynosTVOutService->setHdmiClearLayer(enable);
    }
}

void ExynosHdmiClient::setHdmiPath(int path)
{
    //ALOGD("%s HDMI PATH: %d", __func__, path);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiPath(path);
}

void ExynosHdmiClient::setHdmiDRM(int drmMode)
{
    //ALOGD("%s HDMI DRM: %d", __func__, drmMode);

    if (g_ExynosTVOutService != 0)
        g_ExynosTVOutService->setHdmiDRM(drmMode);
}

void ExynosHdmiClient::setHdmiLayerEnable(uint32_t hdmiLayer)
{
    //ALOGD("%s layer: %d\n", __func__, hdmiLayer);

    if (g_ExynosTVOutService == 0)
        g_ExynosTVOutService = m_getExynosTVOutService();

    if (g_ExynosTVOutService != 0 )
        g_ExynosTVOutService->setHdmiLayerEnable(hdmiLayer);
}

void ExynosHdmiClient::setHdmiLayerDisable(uint32_t hdmiLayer)
{
    //ALOGD("%s layer: %d\n", __func__, hdmiLayer);

    if (g_ExynosTVOutService == 0)
        g_ExynosTVOutService = m_getExynosTVOutService();

    if (g_ExynosTVOutService != 0 )
        g_ExynosTVOutService->setHdmiLayerDisable(hdmiLayer);
}

void ExynosHdmiClient::blit2Hdmi(uint32_t w, uint32_t h,
                                uint32_t colorFormat,
                                uint32_t physYAddr,
                                uint32_t physCbAddr,
                                uint32_t physCrAddr,
                                uint32_t dstX,
                                uint32_t dstY,
                                uint32_t hdmiLayer,
                                uint32_t num_of_hwc_layer)
{
    if (hdmiLayer == HDMI_MODE_UI)
        hdmiLayer = HDMI_MODE_MIRROR;

    if (g_ExynosTVOutService != 0 && mEnable == 1)
        g_ExynosTVOutService->blit2Hdmi(w, h, colorFormat, physYAddr, physCbAddr, physCrAddr, dstX, dstY, hdmiLayer, num_of_hwc_layer);
}

sp<IExynosTVOut> ExynosHdmiClient::m_getExynosTVOutService(void)
{
    int ret = 0;

    if (g_ExynosTVOutService == 0) {
        sp<IBinder> binder;
        sp<IExynosTVOut> sc;
        sp<IServiceManager> sm = defaultServiceManager();
        int getSvcTimes = 0;
        for(getSvcTimes = 0; getSvcTimes < GETSERVICETIMEOUT; getSvcTimes++) {
            binder = sm->getService(String16("ExynosTVOutService"));
            if (binder == 0) {
                ALOGW("ExynosTVOutService not published, waiting...");
                usleep(500000); // 0.5 s
            } else {
                break;
            }
        }
        // grab the lock again for updating g_surfaceFlinger
        if (getSvcTimes < GETSERVICETIMEOUT) {
            sc = interface_cast<IExynosTVOut>(binder);
            g_ExynosTVOutService = sc;
        } else {
            ALOGW("Failed to get ExynosTVOutService... ExynosHdmiClient will get it later..");
        }
    }
    return g_ExynosTVOutService;
}

}
