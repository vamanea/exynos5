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


#ifndef __EXYNOS_TVOUTSERVICE_H__
#define __EXYNOS_TVOUTSERVICE_H__

#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <utils/KeyedVector.h>

#include "IExynosTVOut.h"
#include "ExynosHdmi.h"
#include "MessageQueue.h"

namespace android {
//#define CHECK_VIDEO_TIME
//#define CHECK_UI_TIME

    class ExynosHdmiEventMsg;

    class ExynosTVOutService : public BBinder
    {
        public :
            enum {
                HDMI_MODE_NONE = 0,
                HDMI_MODE_MIRROR,           /* for HDMI mirror mode */
                HDMI_MODE_UI_0,             /* for HDMI extension mode */
                HDMI_MODE_UI_1,             /* for HDMI extension mode */
                HDMI_MODE_VIDEO,
            };

            enum HDMI_S3D_MODE {
                HDMI_2D = 0,
                HDMI_S3D_TB,
                HDMI_S3D_SBS,
            };

            mutable Mutex mLock;

            class HDMIFlushThreadForUI : public Thread {
                ExynosTVOutService *mTVOutService;
            public:
                HDMIFlushThreadForUI(ExynosTVOutService *service):
                Thread(false),
                mTVOutService(service) { }
                virtual void onFirstRef() {
                    run("HDMIFlushThreadForUI", PRIORITY_URGENT_DISPLAY);
                }
                virtual bool threadLoop() {
                    mTVOutService->HdmiFlushThreadForUI();
                    return false;
                }
            };

            sp<HDMIFlushThreadForUI> mHdmiFlushThreadForUI;
            int                 HdmiFlushThreadForUI();

            bool                mExitHdmiFlushThreadForUI;

            class HDMIFlushThreadForVIDEO: public Thread {
                ExynosTVOutService *mTVOutService;
            public:
                HDMIFlushThreadForVIDEO(ExynosTVOutService *service):
                Thread(false),
                mTVOutService(service) { }
                virtual void onFirstRef() {
                    run("HDMIFlushThreadForVIDEO", PRIORITY_URGENT_DISPLAY);
                }
                virtual bool threadLoop() {
                    mTVOutService->HdmiFlushThreadForVIDEO();
                    return false;
                }
            };

            sp<HDMIFlushThreadForVIDEO> mHdmiFlushThreadForVIDEO;
            int                 HdmiFlushThreadForVIDEO();

            bool                mExitHdmiFlushThreadForVIDEO;

            ExynosTVOutService();
            static int instantiate ();
            virtual status_t onTransact(uint32_t, const Parcel &, Parcel *, uint32_t);
            virtual ~ExynosTVOutService ();

            virtual void                        setHdmiStatus(uint32_t status, bool isBooting);
            virtual void                        setHdmiMode(uint32_t mode);
            virtual void                        setHdmiResolution(uint32_t resolution, HDMI_S3D_MODE s3dMode);
            virtual void                        setHdmiHdcp(uint32_t enHdcp);
            virtual void                        setHdmiRotate(uint32_t rotVal, uint32_t hwcLayer);
            virtual void                        setHdmiHwcLayer(uint32_t hwcLayer);
            virtual void                        setHdmiClearLayer(uint32_t enable);
            virtual void                        setHdmiPath(uint32_t path);
            virtual void                        setHdmiDRM(uint32_t drmMode);
            virtual void                        blit2Hdmi(uint32_t w, uint32_t h,
                                                uint32_t colorFormat,
                                                uint32_t pPhyYAddr, uint32_t pPhyCbAddr, uint32_t pPhyCrAddr,
                                                uint32_t dstX, uint32_t dstY,
                                                uint32_t hdmiMode, uint32_t flag_full_display);
            virtual void                        setHdmiLayerEnable(uint32_t hdmiLayer);
            virtual void                        setHdmiLayerDisable(uint32_t hdmiLayer);
            virtual uint32_t                    getHdmiCableStatus();
            bool                                hdmiCableInserted(void);
            void                                setLCDsize(void);

            virtual void                        getHdmiResolution(uint32_t *width, uint32_t *height);

        private:
            sp<ExynosHdmiEventMsg>      mMsgForVideo;
            sp<ExynosHdmiEventMsg>      mMsgForUI;
            ExynosHdmi                  mExynosHdmi;
            bool                        mHdmiCableInserted;
            int                         mUILayerMode;
            uint32_t                    mLCD_width, mLCD_height;
            uint32_t                    mHwcLayer;
            uint32_t                    mHdmiResolution;
            HDMI_S3D_MODE               mHdmis3dMode;
            uint32_t                    mHdmiPath;
            uint32_t                    mEnable;
    };

    class ExynosHdmiEventMsg : public MessageBase {
        public:
            enum {
                HDMI_MODE_NONE = 0,
                HDMI_MODE_MIRROR,           /* for HDMI mirror mode */
                HDMI_MODE_UI_0,             /* for HDMI extension mode */
                HDMI_MODE_UI_1,             /* for HDMI extension mode */
                HDMI_MODE_VIDEO,
            };

            mutable     Mutex mBlitLock;

            ExynosHdmi     *pExynosHdmi;
            uint32_t                    mSrcWidth, mSrcHeight;
            uint32_t                    mSrcColorFormat;
            uint32_t                    mSrcYAddr, mSrcCbAddr, mSrcCrAddr;
            uint32_t                    mDstX, mDstY;
            uint32_t                    mHdmiMode;
            uint32_t                    mHdmiLayer, mflag_full_display;

            ExynosHdmiEventMsg(ExynosHdmi *ExynosHdmi, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcColorFormat,
                    uint32_t srcYAddr, uint32_t srcCbAddr, uint32_t srcCrAddr,
                    uint32_t dstX, uint32_t dstY, uint32_t hdmiLayer, uint32_t flag_full_display, uint32_t hdmiMode)
                : pExynosHdmi(ExynosHdmi), mSrcWidth(srcWidth), mSrcHeight(srcHeight), mSrcColorFormat(srcColorFormat),
                mSrcYAddr(srcYAddr), mSrcCbAddr(srcCbAddr), mSrcCrAddr(srcCrAddr),
                mDstX(dstX), mDstY(dstY), mHdmiLayer(hdmiLayer), mflag_full_display(flag_full_display), mHdmiMode(hdmiMode) {
            }

            virtual bool handler() {
                Mutex::Autolock _l(mBlitLock);
                bool ret = true;
#if defined(CHECK_UI_TIME) || defined(CHECK_VIDEO_TIME)
                nsecs_t start, end;
#endif

                switch (mHdmiMode) {
                case HDMI_MODE_MIRROR:
                case HDMI_MODE_UI_0:
                case HDMI_MODE_UI_1:
#ifdef CHECK_UI_TIME
                    start = systemTime();
#endif
                    if (pExynosHdmi->flush(mSrcWidth, mSrcHeight, mSrcColorFormat, mSrcYAddr, mSrcCbAddr, mSrcCrAddr,
                                mDstX, mDstY, mHdmiLayer, mHdmiMode, mflag_full_display) == false) {
                        ALOGE("%s::pExynosHdmi->flush() fail on HDMI_MODE_UI_X(or MIRROR)", __func__);
                        ret = false;
                    }

#ifdef CHECK_UI_TIME
                    end = systemTime();
                    ALOGD("[UI] pExynosHdmi->flush[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
                    break;
                case HDMI_MODE_VIDEO:
#ifdef CHECK_VIDEO_TIME
                    start = systemTime();
#endif
                    if (pExynosHdmi->flush(mSrcWidth, mSrcHeight, mSrcColorFormat, mSrcYAddr, mSrcCbAddr, mSrcCrAddr,
                                mDstX, mDstY, mHdmiLayer, mHdmiMode, mflag_full_display) == false) {
                        ALOGE("%s::pExynosHdmi->flush() fail on HDMI_MODE_VIDEO", __func__);
                        ret = false;
                    }
#ifdef CHECK_VIDEO_TIME
                    end = systemTime();
                    ALOGD("[VIDEO] pExynosHdmi->flush[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
                    break;
                default:
                    ALOGE("Undefined HDMI_MODE");
                    ret = false;
                    break;
                }
                return ret;
            }
    };

};
#endif
