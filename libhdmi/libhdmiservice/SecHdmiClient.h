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

#ifndef __SEC_HDMI_CLIENT_H__
#define __SEC_HDMI_CLIENT_H__

#include "utils/Log.h"

#include <linux/errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <utils/RefBase.h>
#include <cutils/log.h>
#include <binder/IBinder.h>
#include <binder/IServiceManager.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include "ISecTVOut.h"

#define GETSERVICETIMEOUT (5)

namespace android {

class SecHdmiClient
{
public:
    enum HDMI_MODE
    {
        HDMI_MODE_NONE = 0,
        HDMI_MODE_MIRROR,
        HDMI_MODE_UI_0,
        HDMI_MODE_UI_1,
        HDMI_MODE_VIDEO,
        HDMI_MODE_UI,
    };

    enum HDMI_LAYER {
        HDMI_LAYER_BASE   = 0,
        HDMI_LAYER_VIDEO,
        HDMI_LAYER_GRAPHIC_0,
        HDMI_LAYER_GRAPHIC_1,
        HDMI_LAYER_MAX,
    };

    enum HDMI_PATH {
        HDMI_PATH_OVERLAY = 0,
        HDMI_PATH_WRITEBACK,
        HDMI_PATH_MAX,
    };

    enum HDMI_S3D_MODE {
        HDMI_2D = 0,
        HDMI_S3D_TB,
        HDMI_S3D_SBS,
    };

    enum HDMI_DRM {
        HDMI_NON_DRM_MODE = 0,
        HDMI_DRM_MODE,
    };

private:
    SecHdmiClient();
    virtual ~SecHdmiClient();
    uint32_t    mEnable;

public:
        static SecHdmiClient * getInstance(void);
        void setHdmiCableStatus(int status);
        void setHdmiMode(int mode);
        void setHdmiResolution(int resolution, HDMI_S3D_MODE s3dMode);
        void setHdmiHdcp(int enHdcp);
        void setHdmiRotate(int rotVal, uint32_t hwcLayer);
        void setHdmiHwcLayer(uint32_t hwcLayer);
        void setHdmiEnable(uint32_t enable);
        void setHdmiPath(int path);
        void setHdmiDRM(int drmMode);
        virtual void blit2Hdmi(uint32_t w, uint32_t h,
                                        uint32_t colorFormat,
                                        uint32_t physYAddr,
                                        uint32_t physCbAddr,
                                        uint32_t physCrAddr,
                                        uint32_t dstX,
                                        uint32_t dstY,
                                        uint32_t hdmiLayer,
                                        uint32_t num_of_hwc_layer);

        void setHdmiLayerEnable(uint32_t hdmiLayer);
        void setHdmiLayerDisable(uint32_t hdmiLayer);

        uint32_t getHdmiCableStatus();
private:
        sp<ISecTVOut> m_getSecTVOutService(void);

};

};

#endif
