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

#ifndef __EXYNOS_HDMI_H__
#define __EXYNOS_HDMI_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "exynos_v4l2.h"
#include "v4l2-mediabus.h"

#if defined(BOARD_USES_HDMI_FIMGAPI)
#include "sec_g2d_4x.h"
#include "FimgApi.h"
#endif

#include "s3c_lcd.h"
#include "exynos_gscaler.h"

#include "../libhdmi/libsForhdmi/libedid/libedid.h"
#include "../libhdmi/libsForhdmi/libcec/libcec.h"
#include "../libhdmi/ExynosHdmi/ExynosHdmiCommon.h"

#include <linux/fb.h>
#include <utils/threads.h>

namespace android {

class ExynosHdmi: virtual public RefBase
{
public :
    enum HDMI_LAYER {
        HDMI_LAYER_BASE   = 0,
        HDMI_LAYER_VIDEO,
        HDMI_LAYER_GRAPHIC_0,
        HDMI_LAYER_GRAPHIC_1,
        HDMI_LAYER_MAX,
    };

    enum HDMI_MODE
    {
        HDMI_MODE_NONE = 0,
        HDMI_MODE_MIRROR,               /* for HDMI mirror mode */
        HDMI_MODE_UI_0,                 /* for HDMI extension mode */
        HDMI_MODE_UI_1,                 /* for HDMI extension mode */
        HDMI_MODE_VIDEO,
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

    enum HDMI_BLENDING_MODE {
        HDMI_BLENDING_DISABLE = 0,
        HDMI_BLENDING_ENABLE,
    };

private :
    class CECThread: public Thread
    {
        public:
            bool                mFlagRunning;

        private:
            sp<ExynosHdmi>         mExynosHdmi;
            Mutex               mThreadLoopLock;
            Mutex               mThreadControlLock;
            virtual bool        threadLoop();
            enum CECDeviceType  mDevtype;
            int                 mLaddr;
            int                 mPaddr;

        public:
            CECThread(sp<ExynosHdmi> secHdmi)
                :Thread(false),
                mFlagRunning(false),
                mExynosHdmi(secHdmi),
                mDevtype(CEC_DEVICE_PLAYER),
                mLaddr(0),
                mPaddr(0){
            };
            virtual ~CECThread();

            bool start();
            bool stop();

    };

    Mutex        mLock_UI;
    Mutex        mLock_VIDEO;

    sp<CECThread>               mCECThread;

    bool         mFlagCreate;
    bool         mFlagConnected;

    int          mSrcWidth[HDMI_LAYER_MAX];
    int          mSrcHeight[HDMI_LAYER_MAX];
    int          mSrcColorFormat[HDMI_LAYER_MAX];
    int          mHdmiResolutionWidth[HDMI_LAYER_MAX];
    int          mHdmiResolutionHeight[HDMI_LAYER_MAX];
    int          mHdmiDstWidth;
    int          mHdmiDstHeight;

    unsigned int mFBaddr;
    unsigned int mFBsize;
    int          mFBionfd;

    int          mHdmiOutputMode;
    unsigned int mHdmiResolutionValue;
    unsigned int mHdmiS3DMode;
    unsigned int mHdmiOutFieldOrder;

    unsigned int mHdmiPresetId;
    unsigned int mHdmiPath;
    int          mHdmiDrmMode;
    bool         mHdcpMode;
    unsigned int mUIRotVal;

    int          mCurrentHdmiOutputMode;
    unsigned int mCurrentHdmiResolutionValue;
    unsigned int mCurrentHdmiS3DMode;

    unsigned int mCurrentHdmiPresetId;
    unsigned int mCurrentHdmiPath;
    bool         mCurrentHdcpMode;
    int          mCurrentAudioMode;
    bool         mHdmiInfoChange;
    bool         mHdmiPathChange;

    unsigned int mHdmiResolutionValueList[NUM_SUPPORTED_RESOLUTION_2D];
    unsigned int mHdmiS3dTbResolutionValueList[NUM_SUPPORTED_RESOLUTION_S3D_TB];
    unsigned int mHdmiS3dSbsResolutionValueList[NUM_SUPPORTED_RESOLUTION_S3D_SBS];

    unsigned int mIonAddr_GSC;
    int          mIonClient;

    unsigned int mHdmiDstAddrIndex_UI;
    int          mDstWidth[HDMI_LAYER_MAX];
    int          mDstHeight[HDMI_LAYER_MAX];
    int          mPrevDstWidth[HDMI_LAYER_MAX];
    int          mPrevDstHeight[HDMI_LAYER_MAX];

    int          mDefaultFBFd;
    int          mDisplayWidth;
    int          mDisplayHeight;

    int         m_gsc_mode;
    void        *mExynosRotator;
    void        *m_gsc_cap_handle;
    void        *m_gsc_out_handle;
    void        *m_mxr_handle_grp0;
    void        *m_mxr_handle_grp1;
    struct v4l2_rect mDstRect;

#if defined(SCALABLE_FB)
    unsigned int mPreviousResolution;
#endif //SCALABLE_FB

public :

    ExynosHdmi();
    virtual ~ExynosHdmi();
    bool        create(int width, int height);
    bool        destroy(void);
    inline bool flagCreate(void) { return mFlagCreate; }

    bool        connect(void);
    bool        disconnect(void);

    bool        flagConnected(void);

    bool        flush(int srcW, int srcH, int srcColorFormat,
                        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
                        int dstX, int dstY,
                        int hdmiLayer,
                        int hdmiMode,
                        int flag_full_display);

    bool        clear(int hdmiLayer);
    bool        setHdmiLayerEnable(int hdmiLayer);
    bool        setHdmiLayerDisable(int hdmiLayer);
    bool        setHdmiPath(int hdmiPath);
    bool        setHdmiDrmMode(int drmMode);
    bool        setHdmiOutputMode(int hdmiOutputMode, bool forceRun = false);
    bool        setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode, bool forceRun = false);
    void        getHdmiResolution(uint32_t *width, uint32_t *height);
    bool        setHdcpMode(bool hdcpMode, bool forceRun = false);
    bool        setUIRotation(unsigned int rotVal, unsigned int hwcLayer);
    bool        setDisplaySize(int width, int height);

    inline unsigned int getHdmiResolution() { return mHdmiResolutionValue; }

private:
    bool        m_Doflush(int srcW, int srcH, int srcColorFormat,
                        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
                        int dstX, int dstY,
                        int hdmiLayer,
                        int hdmiMode,
                        int flag_full_display);

    bool        m_setupHdmi(void);
    bool        m_changeHdmiPath(void);
    bool        m_clearbuffer(int layer);

    bool        m_reset(int w, int h, int dstX, int dstY, int colorFormat, int hdmiLayer, int hdmiMode, int flag_full_display);

    bool        m_runHdmi(int layer, unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr);
    bool        m_stopHdmi(int layer);
    bool        m_clearHdmiWriteBack();

    bool        m_setHdmiOutputMode(int hdmiOutputMode);
    bool        m_setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode);
    bool        m_setHdcpMode(bool hdcpMode);
    bool        m_setAudioMode(int audioMode);

    int         m_resolutionValueIndex(unsigned int ResolutionValue, unsigned int s3dMode);
    bool        m_flagHWConnected(void);
    bool        m_enableLayerBlending();
    int         m_setMemory(int ionClient, int * fd, unsigned int map_size, unsigned int * ion_map_ptr, unsigned int flag);
    void        m_CheckFps(void);
};

}; // namespace android

#endif //__SEC_HDMI_H__
