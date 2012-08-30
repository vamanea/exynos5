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
 */

//#define LOG_NDEBUG 0
//#define LOG_TAG "libhdmi"
#include <cutils/log.h>
#include "ion.h"
#include "ExynosHdmi.h"
#include "exynos_format.h"
#include "exynos_gsc_utils.h"
#include "ExynosHdmiLog.h"
#include "ExynosHdmiUtils.h"
#include "ExynosHdmiModule.h"

#define CHECK_GRAPHIC_LAYER_TIME (0)

namespace android {

extern unsigned int output_type;
extern unsigned int g_hdcp_en;

extern unsigned int ui_dst_memory[MAX_BUFFERS_MIXER];
extern unsigned int ui_src_memory[MAX_BUFFERS_GSCALER_CAP];
extern unsigned int ui_memory_size;

#if defined(BOARD_USES_CEC)
ExynosHdmi::CECThread::~CECThread()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    mFlagRunning = false;
}

bool ExynosHdmi::CECThread::threadLoop()
{
    unsigned char buffer[CEC_MAX_FRAME_SIZE];
    int size;
    unsigned char lsrc, ldst, opcode;

    {
        Mutex::Autolock lock(mThreadLoopLock);
        mFlagRunning = true;

        size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 100000);

        if (!size) // no data available or ctrl-c
            return true;

        if (size == 1)
            return true; // "Polling Message"

        lsrc = buffer[0] >> 4;

        /* ignore messages with src address == mLaddr*/
        if (lsrc == mLaddr)
            return true;

        opcode = buffer[1];

        if (CECIgnoreMessage(opcode, lsrc)) {
            HDMI_Log(HDMI_LOG_ERROR, "### ignore message coming from address 15 (unregistered)");
            return true;
        }

        if (!CECCheckMessageSize(opcode, size)) {
            HDMI_Log(HDMI_LOG_ERROR, "### invalid message size: %d(opcode: 0x%x) ###", size, opcode);
            return true;
        }

        /* check if message broadcasted/directly addressed */
        if (!CECCheckMessageMode(opcode, (buffer[0] & 0x0F) == CEC_MSG_BROADCAST ? 1 : 0)) {
            HDMI_Log(HDMI_LOG_ERROR, "### invalid message mode (directly addressed/broadcast) ###");
            return true;
        }

        ldst = lsrc;

        //TODO: macroses to extract src and dst logical addresses
        //TODO: macros to extract opcode

        switch (opcode) {
        case CEC_OPCODE_GIVE_PHYSICAL_ADDRESS:
            /* responce with "Report Physical Address" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_REPORT_PHYSICAL_ADDRESS;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            buffer[4] = mDevtype;
            size = 5;
            break;

        case CEC_OPCODE_REQUEST_ACTIVE_SOURCE:
            ALOGD("[CEC_OPCODE_REQUEST_ACTIVE_SOURCE]");
            /* responce with "Active Source" */
            buffer[0] = (mLaddr << 4) | CEC_MSG_BROADCAST;
            buffer[1] = CEC_OPCODE_ACTIVE_SOURCE;
            buffer[2] = (mPaddr >> 8) & 0xFF;
            buffer[3] = mPaddr & 0xFF;
            size = 4;
            ALOGD("Tx : [CEC_OPCODE_ACTIVE_SOURCE]");
            break;

        case CEC_OPCODE_ABORT:
        case CEC_OPCODE_FEATURE_ABORT:
        default:
            /* send "Feature Abort" */
            buffer[0] = (mLaddr << 4) | ldst;
            buffer[1] = CEC_OPCODE_FEATURE_ABORT;
            buffer[2] = CEC_OPCODE_ABORT;
            buffer[3] = 0x04; // "refused"
            size = 4;
            break;
        }

        if (CECSendMessage(buffer, size) != size)
            HDMI_Log(HDMI_LOG_ERROR, "CECSendMessage() failed!!!");

    }
    return true;
}

bool ExynosHdmi::CECThread::start()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    Mutex::Autolock lock(mThreadControlLock);
    if (exitPending()) {
        if (requestExitAndWait() == WOULD_BLOCK) {
            HDMI_Log(HDMI_LOG_ERROR, "mCECThread.requestExitAndWait() == WOULD_BLOCK");
            return false;
        }
    }

    HDMI_Log(HDMI_LOG_DEBUG, "EDIDGetCECPhysicalAddress");

    /* set to not valid physical address */
    mPaddr = CEC_NOT_VALID_PHYSICAL_ADDRESS;

    if (!EDIDGetCECPhysicalAddress(&mPaddr)) {
        HDMI_Log(HDMI_LOG_ERROR, "Error: EDIDGetCECPhysicalAddress() failed.");
        return false;
    }

    HDMI_Log(HDMI_LOG_DEBUG, "CECOpen");

    if (!CECOpen()) {
        HDMI_Log(HDMI_LOG_ERROR, "CECOpen() failed!!!");
        return false;
    }

    /* a logical address should only be allocated when a device
       has a valid physical address, at all other times a device
       should take the 'Unregistered' logical address (15)
    */

    /* if physical address is not valid device should take
       the 'Unregistered' logical address (15)
    */

    HDMI_Log(HDMI_LOG_DEBUG, "CECAllocLogicalAddress");

    mLaddr = CECAllocLogicalAddress(mPaddr, mDevtype);

    if (!mLaddr) {
        HDMI_Log(HDMI_LOG_ERROR, "CECAllocLogicalAddress() failed!!!");
        if (!CECClose())
            HDMI_Log(HDMI_LOG_ERROR, "CECClose() failed!");
        return false;
    }

    HDMI_Log(HDMI_LOG_DEBUG, "request to run CECThread");

    status_t ret = run("ExynosHdmi::CECThread", PRIORITY_DISPLAY);
    if (ret != NO_ERROR) {
        HDMI_Log(HDMI_LOG_ERROR, "%s fail to run thread", __func__);
        return false;
    }
    return true;
}

bool ExynosHdmi::CECThread::stop()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s request Exit", __func__);

    Mutex::Autolock lock(mThreadControlLock);
    if (requestExitAndWait() == WOULD_BLOCK) {
        HDMI_Log(HDMI_LOG_ERROR, "mCECThread.requestExitAndWait() == WOULD_BLOCK");
        return false;
    }

    if (!CECClose())
        HDMI_Log(HDMI_LOG_ERROR, "CECClose() failed!\n");

    mFlagRunning = false;
    return true;
}
#endif

ExynosHdmi::ExynosHdmi():
#if defined(BOARD_USES_CEC)
    mCECThread(NULL),
#endif
    mFlagCreate(false),
    mFlagConnected(false),
    mHdmiDstWidth(0),
    mHdmiDstHeight(0),
    mFBaddr(0),
    mFBsize(0),
    mFBionfd(-1),
    mHdmiOutputMode(DEFAULT_OUPUT_MODE),
    mHdmiResolutionValue(DEFAULT_HDMI_RESOLUTION_VALUE),
    mHdmiS3DMode(HDMI_2D),
    mHdmiOutFieldOrder(GSC_TV_OUT_PROGRESSIVE),
    mHdmiPresetId(DEFAULT_HDMI_DV_ID),
    mHdmiPath(DEFAULT_UI_PATH),
    mHdcpMode(false),
    mUIRotVal(0),
    mCurrentHdmiOutputMode(-1),
    mCurrentHdmiResolutionValue(0),
    mCurrentHdmiS3DMode(0), // 0 = 2D mode
    mCurrentHdmiPresetId(0),
    mCurrentHdmiPath(-1),
    mCurrentHdcpMode(false),
    mCurrentAudioMode(-1),
    mHdmiInfoChange(true),
    mHdmiPathChange(false),
    mHdmiDrmMode(HDMI_NON_DRM_MODE),
    mIonAddr_GSC(0),
    mHdmiDstAddrIndex_UI(0),
    mDefaultFBFd(-1),
    mDisplayWidth(DEFALULT_DISPLAY_WIDTH),
    mDisplayHeight(DEFALULT_DISPLAY_HEIGHT),
    m_gsc_mode(0),
    mExynosRotator(NULL),
    m_gsc_cap_handle(NULL),
    m_gsc_out_handle(NULL),
    m_mxr_handle_grp0(NULL),
    m_mxr_handle_grp1(NULL),
#if defined(SCALABLE_FB)
    mPreviousResolution(-1),
#endif //SCALABLE_FB
    mIonClient(NULL)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    for (int i = 0; i < HDMI_LAYER_MAX; i++) {
        mSrcWidth      [i] = 0;
        mSrcHeight     [i] = 0;
        mSrcColorFormat[i] = 0;
        mHdmiResolutionWidth  [i] = 0;
        mHdmiResolutionHeight [i] = 0;
        mDstWidth  [i] = 0;
        mDstHeight [i] = 0;
        mPrevDstWidth  [i] = 0;
        mPrevDstHeight [i] = 0;
    }

    mHdmiResolutionValueList[0] = 1080960;
    mHdmiResolutionValueList[1] = 1080950;
    mHdmiResolutionValueList[2] = 1080930;
    mHdmiResolutionValueList[3] = 1080924;
    mHdmiResolutionValueList[4] = 1080160;
    mHdmiResolutionValueList[5] = 1080150;
    mHdmiResolutionValueList[6] = 720960;
    mHdmiResolutionValueList[7] = 720950;
    mHdmiResolutionValueList[8] = 576950;
    mHdmiResolutionValueList[9] = 480960;

    mHdmiS3dTbResolutionValueList[0] = 1080960;
    mHdmiS3dTbResolutionValueList[1] = 1080924;
    mHdmiS3dTbResolutionValueList[2] = 720960;
    mHdmiS3dTbResolutionValueList[3] = 720950;

    mHdmiS3dSbsResolutionValueList[0] = 1080960;
    mHdmiS3dSbsResolutionValueList[1] = 1080924;
    mHdmiS3dSbsResolutionValueList[2] = 720960;

#if defined(BOARD_USES_CEC)
    mCECThread = new CECThread(this);
#endif

    memset(&mDstRect, 0 , sizeof(struct v4l2_rect));

}

ExynosHdmi::~ExynosHdmi()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (mFlagCreate == true)
        HDMI_Log(HDMI_LOG_ERROR, "%s::this is not Destroyed fail", __func__);
    else
        disconnect();
}

bool ExynosHdmi::create(int width, int height)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    unsigned int mIonBufSize_GSC = 0;
    int stride;
    int vstride;

    int ionfd_UI = 0;
    int ionfd_GSC = 0;
    unsigned int ion_base_addr;

    char node[32];
    char devname[32];

    struct s3c_fb_user_ion_client ion_handle;

    setDisplaySize(width, height);
    stride  = ALIGN(HDMI_MAX_WIDTH,  16);
    vstride = ALIGN(HDMI_MAX_HEIGHT, 16);

#if defined(USE_MEMCPY_USERPTR_GSC)
    mIonBufSize_GSC = stride * vstride * MAX_BUFFERS_GSCALER_OUT * HDMI_VIDEO_BUFFER_BPP_SIZE;
#endif
    ui_memory_size = stride * vstride * HDMI_UI_BUFFER_BPP_SIZE;

    if (mFlagCreate == true) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Already Created", __func__);
        return true;
    }

    if (mDefaultFBFd <= 0) {
        sprintf(node, "%s%d", "/dev/graphics/fb", DEFAULT_FB_INDEX);
        if ((mDefaultFBFd = open(node, O_RDWR)) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:Failed to open default FB", __func__);
            return false;
        }
    }

    mIonClient = ion_client_create();
    if (mIonClient < 0) {
        mIonClient = -1;
        HDMI_Log(HDMI_LOG_ERROR, "%s::ion_client_create() failed", __func__);
        goto CREATE_FAIL;
    }

#if defined(USE_MEMCPY_USERPTR_GSC)
    if (m_setMemory(mIonClient, &ionfd_GSC, mIonBufSize_GSC, &mIonAddr_GSC, ION_HEAP_EXYNOS_MASK) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::ion_allocation failed", __func__);
        goto CREATE_FAIL;
    }
#endif

    for (int i = 0; i < MAX_BUFFERS_GSCALER_CAP; i++) {
        if (m_setMemory(mIonClient, &ionfd_UI, ui_memory_size, &ion_base_addr, ION_HEAP_EXYNOS_CONTIG_MASK) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::ion_allocation failed", __func__);
            goto CREATE_FAIL;
        }
        ui_src_memory[i] = (unsigned int)ion_base_addr;
    }

    for (int i = 0; i < MAX_BUFFERS_MIXER; i++) {
        if (m_setMemory(mIonClient, &ionfd_UI, ui_memory_size, &ion_base_addr, ION_HEAP_EXYNOS_MASK) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::ion_allocation failed", __func__);
            goto CREATE_FAIL;
        }
        ui_dst_memory[i] = (unsigned int)ion_base_addr;
    }
    __u32 preset_id;

    HDMI_Log(HDMI_LOG_DEBUG, "%s::mHdmiOutputMode(%d)", __func__, mHdmiOutputMode);

    if (hdmi_resolution_2_preset_id(mHdmiResolutionValue, mHdmiS3DMode, &mHdmiDstWidth, &mHdmiDstHeight, &preset_id) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_resolution_2_preset_id(%d) fail", __func__, mHdmiResolutionValue);
        goto CREATE_FAIL;
    }

    m_mxr_handle_grp0 = exynos_mxr_create(0, 0);
    if (m_mxr_handle_grp0 == NULL) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::create mixer graphic layer 0 failed", __func__);
        return false;
    }

    m_mxr_handle_grp1 = exynos_mxr_create(0, 1);
    if (m_mxr_handle_grp1 == NULL) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::create mixer graphic layer 1 failed", __func__);
        return false;
    }

    mFlagCreate = true;

    return true;

CREATE_FAIL :

    mHdmiResolutionValue = 0;
    mFlagCreate = false;

    ALOGE("%s::Gscaler create failed", __func__);

    return false;
}

bool ExynosHdmi::destroy(void)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    char node[32];
    struct media_link   *links;
    unsigned int mIonBufSize_GSC = 0;
    int stride;
    int vstride;
    unsigned int ion_base_addr;

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    stride  = ALIGN(HDMI_MAX_WIDTH, 16);
    vstride = ALIGN(HDMI_MAX_HEIGHT, 16);

#if defined(USE_MEMCPY_USERPTR_GSC)
    mIonBufSize_GSC = stride * vstride * MAX_BUFFERS_GSCALER_OUT * HDMI_VIDEO_BUFFER_BPP_SIZE;
#endif

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Already Destroyed fail", __func__);
        goto DESTROY_FAIL;
    }

#if defined(USE_MEMCPY_USERPTR_GSC)
    ion_unmap((void *)mIonAddr_GSC, ALIGN(mIonBufSize_GSC, PAGE_SIZE));
#endif

    for (int i = 0; i < MAX_BUFFERS_GSCALER_CAP; i++) {
        ion_unmap((void *)ui_src_memory[i], ALIGN(ui_memory_size, PAGE_SIZE));
    }

    for (int i = 0; i < MAX_BUFFERS_MIXER; i++) {
        ion_unmap((void *)ui_dst_memory[i], ALIGN(ui_memory_size, PAGE_SIZE));
    }

    if (0 < mFBaddr)
        ion_unmap((void *)mFBaddr, ALIGN(mFBsize, PAGE_SIZE));

    if (0 < mFBionfd)
        ion_free(mFBionfd);


    if (0 < mDefaultFBFd)
        close(mDefaultFBFd);
    mDefaultFBFd = -1;

    if (m_gsc_out_handle) {
        if (exynos_gsc_stop_exclusive(m_gsc_out_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
            return false;
        }
        exynos_gsc_destroy(m_gsc_out_handle);
    }

    if (m_gsc_cap_handle) {
        if (exynos_gsc_stop_exclusive(m_gsc_cap_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
            return false;
        }
        exynos_gsc_destroy(m_gsc_cap_handle);
    }

    if (m_mxr_handle_grp0) {
        if (exynos_mxr_stop_n_clear(m_mxr_handle_grp0) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_stop_n_clear", __func__);
            return false;
        }
        exynos_mxr_destroy(m_mxr_handle_grp0);
    }

    if (m_mxr_handle_grp1) {
        if (exynos_mxr_stop_n_clear(m_mxr_handle_grp1) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_stop_n_clear", __func__);
            return false;
        }
        exynos_mxr_destroy(m_mxr_handle_grp1);
    }

    if (m_destroy_ExternalHandle(mExynosRotator) == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::m_destroy_ExternalHandle() failed", __func__);
        return false;
    }

    m_gsc_out_handle  = NULL;
    m_gsc_cap_handle  = NULL;
    m_mxr_handle_grp0 = NULL;
    m_mxr_handle_grp1 = NULL;
    mExynosRotator    = NULL;

    mFlagCreate  = false;

    return true;

DESTROY_FAIL :

    return false;
}

bool ExynosHdmi::connect(void)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);
    {
        Mutex::Autolock lock_UI(mLock_UI);
        Mutex::Autolock lock_VIDEO(mLock_VIDEO);

        if (mFlagCreate == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
            return false;
        }

        if (mFlagConnected == true) {
            ALOGD("%s::Already Connected..", __func__);
            return true;
        }

        if (m_flagHWConnected() == false) {
            ALOGD("%s::m_flagHWConnected() fail", __func__);
#if defined(SCALABLE_FB)
            if (mPreviousResolution == -1)
                mPreviousResolution = mHdmiResolutionValue;
#endif //SCALABLE_FB
            return false;
        }

#if defined(BOARD_USES_EDID)
        if (!EDIDOpen())
            HDMI_Log(HDMI_LOG_ERROR, "EDIDInit() failed!");

        if (!EDIDRead()) {
            HDMI_Log(HDMI_LOG_ERROR, "EDIDRead() failed!");
            if (!EDIDClose())
                HDMI_Log(HDMI_LOG_ERROR, "EDIDClose() failed!");
        }
#endif

#if defined(BOARD_USES_CEC)
        if (!(mCECThread->mFlagRunning))
            mCECThread->start();
#endif
    }

    if (this->setHdmiOutputMode(mHdmiOutputMode, true) == false)
        HDMI_Log(HDMI_LOG_ERROR, "%s::setHdmiOutputMode(%d) fail", __func__, mHdmiOutputMode);

    if (this->setHdmiResolution(mHdmiResolutionValue, mHdmiS3DMode, true) == false)
        HDMI_Log(HDMI_LOG_ERROR, "%s::setHdmiResolution(%d) fail", __func__, mHdmiResolutionValue);

    if (this->setHdcpMode(mHdcpMode, false) == false)
        HDMI_Log(HDMI_LOG_ERROR, "%s::setHdcpMode(%d) fail", __func__, mHdcpMode);

    if (m_create_ExternalHandle(mExynosRotator) == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::m_create_ExternalHandle() failed", __func__);
        return false;
    }

    mHdmiInfoChange = true;
    mHdmiPathChange = true;
    mFlagConnected = true;

#if defined(BOARD_USES_EDID)
    display_menu();
#endif

#if defined(SCALABLE_FB)
    if (mPreviousResolution != -1 && mPreviousResolution != mHdmiResolutionValue)
        kill(getpid(), SIGKILL);
    else
        mPreviousResolution = mHdmiResolutionValue;
#endif //SCALABLE_FB

    return true;
}

bool ExynosHdmi::disconnect(void)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct v4l2_requestbuffers reqbuf;

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if (mFlagConnected == false) {
        ALOGD("%s::Already Disconnected..", __func__);
        return true;
    }

#if defined(BOARD_USES_CEC)
    if (mCECThread->mFlagRunning)
        mCECThread->stop();
#endif

#if defined(BOARD_USES_EDID)
    if (!EDIDClose()) {
        HDMI_Log(HDMI_LOG_ERROR, "EDIDClose() failed!");
        return false;
    }
#endif

    if (m_gsc_out_handle) {
        if (exynos_gsc_stop_exclusive(m_gsc_out_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
            return false;
        }
        exynos_gsc_destroy(m_gsc_out_handle);
    }

    if (m_gsc_cap_handle) {
        if (exynos_gsc_stop_exclusive(m_gsc_cap_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
            return false;
        }
        exynos_gsc_destroy(m_gsc_cap_handle);
    }

    if (m_destroy_ExternalHandle(mExynosRotator) == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::m_destroy_ExternalHandle() failed", __func__);
        return false;
    }

    for (int layer = HDMI_LAYER_GRAPHIC_0; layer < HDMI_LAYER_MAX; layer++) {
        if (m_stopHdmi(layer) == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::m_stopHdmi: layer[%d] fail", __func__, layer);
            return false;
        }
    }

    m_gsc_out_handle  = NULL;
    m_gsc_cap_handle  = NULL;
    mExynosRotator    = NULL;

    mFlagConnected = false;

    mHdmiOutputMode = DEFAULT_OUPUT_MODE;
    mHdmiResolutionValue = DEFAULT_HDMI_RESOLUTION_VALUE;
    mHdmiPresetId = DEFAULT_HDMI_DV_ID;
    mCurrentHdmiOutputMode = -1;
    mCurrentHdmiResolutionValue = 0;
    mCurrentHdmiS3DMode = HDMI_2D;
    mCurrentAudioMode = -1;
    mCurrentHdmiPath = -1;
    mHdmiDstAddrIndex_UI = 0;

    return true;
}

bool ExynosHdmi::flagConnected(void)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    return mFlagConnected;
}

bool ExynosHdmi::flush(int srcW, int srcH, int srcColorFormat,
        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
        int dstX, int dstY,
        int hdmiLayer,
        int hdmiMode,
        int flag_full_display)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiLayer=%d, hdmiMode=%d",
            __func__, hdmiLayer, hdmiMode);

    int ret;

    if (hdmiMode == HDMI_MODE_VIDEO) {
        Mutex::Autolock lock_VIDEO(mLock_VIDEO);
        if (mHdmiPathChange == true)
            Mutex::Autolock lock_UI(mLock_UI);

        ret = m_Doflush(srcW, srcH, srcColorFormat,
                srcYAddr, srcCbAddr, srcCrAddr,
                dstX, dstY,
                hdmiLayer,
                hdmiMode,
                flag_full_display);
    } else {
        Mutex::Autolock lock_UI(mLock_UI);
        if (mHdmiPathChange == true)
            Mutex::Autolock lock_VIDEO(mLock_VIDEO);

        ret = m_Doflush(srcW, srcH, srcColorFormat,
                srcYAddr, srcCbAddr, srcCrAddr,
                dstX, dstY,
                hdmiLayer,
                hdmiMode,
                flag_full_display);
    }
    return ret;
}

bool ExynosHdmi::m_Doflush(int srcW, int srcH, int srcColorFormat,
        unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr,
        int dstX, int dstY,
        int hdmiLayer,
        int hdmiMode,
        int flag_full_display)
{
    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::hdmiLayer=%d\r\n"
            "    srcW = %d, srcH = %d, srcColorFormat = 0x%x\r\n"
            "    srcYAddr= 0x%x, srcCbAddr = 0x%x, srcCrAddr = 0x%x\r\n"
            "    dstX = %d, dstY = %d, hdmiLayer = %d, flag_full_display=%d\r\n"
            "    saved param(mSrcWidth=%d, mSrcHeight=%d, mSrcColorFormat=%d)",
            __func__, hdmiLayer,
            srcW, srcH, srcColorFormat,
            srcYAddr, srcCbAddr, srcCrAddr,
            dstX, dstY, hdmiLayer, flag_full_display,
            mSrcWidth[hdmiLayer], mSrcHeight[hdmiLayer], mSrcColorFormat[hdmiLayer]);

    struct v4l2_rect scaled_dstRect;
    int tempSrcW, tempSrcH;
    unsigned int dstYAddr  = 0;
    unsigned int dstCbAddr = 0;
    unsigned int dstCrAddr = 0;

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if (mFlagConnected == false) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::Not Yet connected", __func__);
        return false;
    }

    if (mUIRotVal == 0 || mUIRotVal == 180) {
        tempSrcW = srcW;
        tempSrcH = srcH;
    } else {
        tempSrcW = srcH;
        tempSrcH = srcW;
    }

    hdmi_cal_rect(tempSrcW, tempSrcH, mHdmiDstWidth, mHdmiDstHeight, &scaled_dstRect);

    if (hdmiLayer == HDMI_LAYER_VIDEO) {
        mDstWidth[hdmiLayer] = mHdmiDstWidth;
        mDstHeight[hdmiLayer] = mHdmiDstHeight;
    } else {
        if (hdmiMode == HDMI_MODE_MIRROR) {
            if (mHdmiPath == HDMI_PATH_OVERLAY && flag_full_display) {
#if defined(BOARD_USES_HDMI_FIMGAPI)
                scaled_dstRect.left = 0;
                scaled_dstRect.top = 0;
                scaled_dstRect.width = mHdmiDstWidth;
                scaled_dstRect.height = mHdmiDstHeight;
#endif
            } else {
                mDstWidth[HDMI_LAYER_VIDEO] = 0;
                mDstHeight[HDMI_LAYER_VIDEO] = 0;
            }
            mDstWidth[hdmiLayer] = scaled_dstRect.width;
            mDstHeight[hdmiLayer] = scaled_dstRect.height;
        } else if ((hdmiMode == HDMI_MODE_UI_0) || (hdmiMode == HDMI_MODE_UI_1)) {
            if (flag_full_display) {
                mDstWidth[hdmiLayer] = scaled_dstRect.width;
                mDstHeight[hdmiLayer] = scaled_dstRect.height;
            } else {
                scaled_dstRect.left = dstX;
                scaled_dstRect.top  = dstY;
                mDstWidth[hdmiLayer] = srcW;
                mDstHeight[hdmiLayer] = srcH;
            }
        }
    }

    mDstRect.width  = mDstWidth [hdmiLayer];
    mDstRect.height = mDstHeight[hdmiLayer];
    mDstRect.left = scaled_dstRect.left;
    mDstRect.top  = scaled_dstRect.top;

    HDMI_Log(HDMI_LOG_DEBUG,
            "m_reset param(mDstWidth=%d, mDstHeight=%d, mPrevDstWidth=%d, mPrevDstHeight=%d)\r\n"
            "             (mHdmiDstWidth=%d, mHdmiDstHeight=%d, mHdmiResolutionWidth=%d, mHdmiResolutionHeight=%d)",
            mDstWidth[hdmiLayer], mDstHeight[hdmiLayer], mPrevDstWidth[hdmiLayer], mPrevDstHeight[hdmiLayer],
            mHdmiDstWidth, mHdmiDstHeight, mHdmiResolutionWidth[hdmiLayer], mHdmiResolutionHeight[hdmiLayer]);

    if (srcW != mSrcWidth[hdmiLayer] ||
        srcH != mSrcHeight[hdmiLayer] ||
        srcColorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        mDstWidth[hdmiLayer] != mPrevDstWidth[hdmiLayer] ||
        mDstHeight[hdmiLayer] != mPrevDstHeight[hdmiLayer] ||
        mHdmiInfoChange == true ||
        mHdmiPathChange == true) {
        HDMI_Log(HDMI_LOG_DEBUG,
                "m_reset param(srcW=%d, mSrcWidth=%d, srcH=%d, mSrcHeight=%d\r\n"
                "    srcColorFormat=%d, mSrcColorFormat=%d, hdmiLayer=%d)",
                srcW, mSrcWidth[hdmiLayer], srcH, mSrcHeight[hdmiLayer],
                srcColorFormat, mSrcColorFormat[hdmiLayer], hdmiLayer);

        if (m_reset(srcW, srcH, mDstRect.left, mDstRect.top, srcColorFormat, hdmiLayer, hdmiMode, flag_full_display) == false) {
            HDMI_Log(HDMI_LOG_ERROR,
                    "%s::m_reset(srcW=%d, srcH=%d, srcColorFormat=%d, hdmiLayer=%d) failed",
                    __func__, srcW, srcH, srcColorFormat, hdmiLayer);
            return false;
        }
    }

    if (srcYAddr == 0 && mHdmiPath == HDMI_PATH_OVERLAY) {
        unsigned int FB_size = ALIGN(srcW, 16) * ALIGN(srcH, 16) * HDMI_UI_BUFFER_BPP_SIZE * 2;
        struct s3c_fb_user_ion_client ion_handle;

        if (0 < mFBaddr)
            ion_unmap((void *)mFBaddr, ALIGN(mFBsize, PAGE_SIZE));

        if (0 < mFBionfd)
            ion_free(mFBionfd);

        if (ioctl(mDefaultFBFd, S3CFB_GET_ION_USER_HANDLE, &ion_handle) == -1) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:ioctl(S3CFB_GET_ION_USER_HANDLE) failed", __func__);
            return false;
        }
        mFBaddr  = (unsigned int)ion_map(ion_handle.fd, ALIGN(FB_size, PAGE_SIZE), 0);
        mFBsize  = FB_size;
        mFBionfd = ion_handle.fd;

        srcYAddr = (unsigned int)mFBaddr + ion_handle.offset;
        srcCbAddr = srcYAddr;

        HDMI_Log(HDMI_LOG_DEBUG,
                "%s::mFBaddr=0x%08x, srcYAddr=0x%08x, ion_handle.offset=0x%08x",
                __func__, mFBaddr, srcYAddr, ion_handle.offset);
    }

    dstYAddr  = srcYAddr;
    dstCbAddr = srcCbAddr;
    dstCrAddr = srcCrAddr;

    if (hdmiLayer == HDMI_LAYER_VIDEO) {
    } else {
#if CHECK_GRAPHIC_LAYER_TIME
        nsecs_t start, end;
        start = systemTime();
#endif

        if (mHdmiPath == HDMI_PATH_OVERLAY) {
#if defined(BOARD_USES_HDMI_FIMGAPI)
            mHdmiDstAddrIndex_UI++;
            if (MAX_BUFFERS_MIXER <= mHdmiDstAddrIndex_UI)
                mHdmiDstAddrIndex_UI = 0;

            dstYAddr = (unsigned int)ui_dst_memory[mHdmiDstAddrIndex_UI];
            if (hdmi_Blit_byG2D(srcColorFormat, srcW, srcH, srcYAddr,
                    mDstRect.width, mDstRect.height, dstYAddr, mUIRotVal) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_Blit_byG2D() failed", __func__);
                return false;
            }
#else
            dstYAddr = srcYAddr;
#endif
        } else if (mHdmiPath == HDMI_PATH_WRITEBACK) {
            /* 1. capture from FIMD->GSC */
            if (hdmi_captureRun_byGSC((unsigned int)ui_src_memory[0], m_gsc_cap_handle) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_captureRun_byGSC() failed", __func__);
                return false;
            }

            /* 2. if need rotation, rotate */
            if (mExynosRotator != NULL && (mUIRotVal == 90 || mUIRotVal == 180 || mUIRotVal == 270)) {
                mHdmiDstAddrIndex_UI++;
                if (MAX_BUFFERS_MIXER <= mHdmiDstAddrIndex_UI)
                    mHdmiDstAddrIndex_UI = 0;

                if (hdmi_rotateRun(mExynosRotator,
                                    (unsigned int)ui_src_memory[0],
                                    (unsigned int)ui_dst_memory[mHdmiDstAddrIndex_UI],
                                    mUIRotVal) < 0) {
                    HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_rotateRun() failed", __func__);
                    return false;
                }
                dstYAddr = (unsigned int)ui_dst_memory[mHdmiDstAddrIndex_UI];
            } else {
                dstYAddr = (unsigned int)ui_src_memory[0];
            }
            dstCbAddr = dstYAddr;
            dstCrAddr = dstYAddr;
        }

        HDMI_Log(HDMI_LOG_DEBUG,
                "%s::hdmiLayer=%d, hdmiMode=%d\r\n"
                "    mHdmiDstAddrIndex_UI=%d\r\n"
                "    srcYAddr=0x%08x, srcCbAddr=0x%08x, srcCrAddr=0x%08x\r\n"
                "    dstYAddr=0x%08x, dstCbAddr=0x%08x, dstCrAddr=0x%08x\r\n",
                __func__, hdmiLayer, hdmiMode,
                mHdmiDstAddrIndex_UI,
                srcYAddr, srcCbAddr, srcCrAddr,
                dstYAddr, dstCbAddr, dstCrAddr);

#if CHECK_GRAPHIC_LAYER_TIME
        end = systemTime();
        ALOGD("[UI] hdmi_set_graphiclayer[end-start] = %ld ms", long(ns2ms(end)) - long(ns2ms(start)));
#endif
    }

    if (mFlagConnected) {
        if (m_runHdmi(hdmiLayer, (unsigned int)dstYAddr, (unsigned int)dstCbAddr, (unsigned int)dstCrAddr) == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::m_runHdmi(%d) failed", __func__, hdmiLayer);
            return false;
        }
    }
    return true;
}

bool ExynosHdmi::clearHdmiWriteBack()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::layer=%d", __func__);

    if (m_gsc_cap_handle) {
        if (exynos_gsc_just_stop(m_gsc_cap_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::stream off failed (Writeback)", __func__);
            return false;
        }
    }

    return true;
}

bool ExynosHdmi::clear(int hdmiLayer)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiLayer = %d", __func__, hdmiLayer);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if (m_stopHdmi(hdmiLayer) == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::m_stopHdmi: layer[%d] fail", __func__, hdmiLayer);
        return false;
    }

    return true;
}

bool ExynosHdmi::setHdmiLayerEnable(int hdmiLayer)
{
    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiLayer(%d)",__func__, hdmiLayer);

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        if (mFlagConnected) {
/*
            if (m_runHdmi(hdmiLayer) == false) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::start hdmiLayer(%d) failed", __func__, hdmiLayer);
                return false;
            }
*/
        }
        break;
    default:
        return false;
    }
    return true;
}

bool ExynosHdmi::setHdmiLayerDisable(int hdmiLayer)
{
    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiLayer(%d)",__func__, hdmiLayer);

    switch (hdmiLayer) {
    case HDMI_LAYER_VIDEO:
    case HDMI_LAYER_GRAPHIC_0:
    case HDMI_LAYER_GRAPHIC_1:
        if (mFlagConnected) {
            if (m_stopHdmi(hdmiLayer) == false) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::m_stopHdmi: layer[%d] fail", __func__, hdmiLayer);
                return false;
            }
        }
        break;
    default:
        return false;
    }
    return true;
}

bool ExynosHdmi::setHdmiPath(int hdmiPath)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiPath = %d", __func__, hdmiPath);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    switch (hdmiPath) {
    case HDMI_PATH_OVERLAY:
    case HDMI_PATH_WRITEBACK:
        if (mHdmiPath != hdmiPath) {
            mHdmiPath = hdmiPath;
            mHdmiPathChange = true;
        }
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unsupported path (hdmiPath=%d)", __func__, hdmiPath);
        return false;
        break;
    }

    return true;
}

bool ExynosHdmi::setHdmiDrmMode(int drmMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::drmMode = %d, mHdmiPath=%d", __func__, drmMode, mHdmiPath);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mHdmiDrmMode != drmMode) {
        mHdmiDrmMode = drmMode;
    }

    return true;
}

bool ExynosHdmi::setHdmiOutputMode(int hdmiOutputMode, bool forceRun)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiOutputMode=%d, forceRun=%d", __func__, hdmiOutputMode, forceRun);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if (forceRun == false && mHdmiOutputMode == hdmiOutputMode) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdmiOutputMode(%d)", __func__, hdmiOutputMode);
        return true;
    }

    int newHdmiOutputMode = hdmiOutputMode;

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_outputmode_2_v4l2_output_type(%d) fail", __func__, hdmiOutputMode);
        return false;
    }

#if defined(BOARD_USES_EDID)
    int newV4l2OutputType = hdmi_check_output_mode(v4l2OutputType);
    if (newV4l2OutputType != v4l2OutputType) {
        newHdmiOutputMode = hdmi_v4l2_output_type_2_outputmode(newV4l2OutputType);
        if (newHdmiOutputMode < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_v4l2_output_type_2_outputmode(%d) fail", __func__, newV4l2OutputType);
            return false;
        }

        HDMI_Log(HDMI_LOG_DEBUG, "%s::calibration mode(%d -> %d)...", __func__, hdmiOutputMode, newHdmiOutputMode);
    }

    if ((newHdmiOutputMode != hdmiOutputMode) && (newHdmiOutputMode == HDMI_OUTPUT_MODE_DVI))
        newHdmiOutputMode = hdmiOutputMode;
#endif

    if (mHdmiOutputMode != newHdmiOutputMode) {
        mHdmiOutputMode = newHdmiOutputMode;
        mHdmiInfoChange = true;
    }

    return true;
}

bool ExynosHdmi::setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode, bool forceRun)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::hdmiResolutionValue=%d, s3dMode=%d, forceRun=%d",
            __func__, hdmiResolutionValue, s3dMode, forceRun);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if ((forceRun == false) && (mHdmiS3DMode == s3dMode) && (mHdmiResolutionValue == hdmiResolutionValue)) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdmiResolutionValue(%d) s3dMode(%d)",
                __func__, hdmiResolutionValue, s3dMode);
        return true;
    }

    unsigned int newHdmiResolutionValue = hdmiResolutionValue;
    unsigned int newHdmiS3DMode = s3dMode;
    int w = 0;
    int h = 0;

    __u32 preset_id;

#if defined(BOARD_USES_EDID)
    // find perfect resolutions..
    if (hdmi_resolution_2_preset_id(newHdmiResolutionValue, newHdmiS3DMode, &w, &h, &preset_id) < 0 ||
        hdmi_check_resolution(preset_id) < 0) {
        bool flagFoundIndex = false;
        int resolutionValueIndex = 0;

        if(s3dMode == HDMI_S3D_TB) {
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_RESOLUTION_VALUE_S3D_TB, newHdmiS3DMode);
            if (resolutionValueIndex >= 0) {
                for (int i = resolutionValueIndex; i < NUM_SUPPORTED_RESOLUTION_S3D_TB; i++) {
                    if (hdmi_resolution_2_preset_id(mHdmiS3dTbResolutionValueList[i], newHdmiS3DMode, &w, &h, &preset_id) == 0 &&
                        hdmi_check_resolution(preset_id) == 0) {
                        newHdmiResolutionValue = mHdmiS3dTbResolutionValueList[i];
                        flagFoundIndex = true;
                        break;
                    }
                }
            }
        } else if (s3dMode == HDMI_S3D_SBS) {
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_RESOLUTION_VALUE_S3D_SBS, newHdmiS3DMode);
            if (resolutionValueIndex >= 0) {
                for (int i = resolutionValueIndex; i < NUM_SUPPORTED_RESOLUTION_S3D_SBS; i++) {
                    if (hdmi_resolution_2_preset_id(mHdmiS3dSbsResolutionValueList[i], newHdmiS3DMode, &w, &h, &preset_id) == 0 &&
                        hdmi_check_resolution(preset_id) == 0) {
                        newHdmiResolutionValue = mHdmiS3dSbsResolutionValueList[i];
                        flagFoundIndex = true;
                        break;
                    }
                }
            }
        }

        if (flagFoundIndex == false) {
            newHdmiS3DMode = HDMI_2D;
            resolutionValueIndex = m_resolutionValueIndex(DEFAULT_HDMI_RESOLUTION_VALUE, newHdmiS3DMode);
            if (resolutionValueIndex >= 0) {
                for (int i = resolutionValueIndex; i < NUM_SUPPORTED_RESOLUTION_2D; i++) {
                    if (hdmi_resolution_2_preset_id(mHdmiResolutionValueList[i], newHdmiS3DMode, &w, &h, &preset_id) == 0 &&
                        hdmi_check_resolution(preset_id) == 0) {
                        newHdmiResolutionValue = mHdmiResolutionValueList[i];
                        flagFoundIndex = true;
                        break;
                    }
                }
            }
        }

        if (flagFoundIndex == false) {
            newHdmiS3DMode = HDMI_2D;
            HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi cannot control this resolution(%d) fail", __func__, hdmiResolutionValue);
            // Set resolution to 480P
            newHdmiResolutionValue = mHdmiResolutionValueList[NUM_SUPPORTED_RESOLUTION_2D-1];
        } else {
            HDMI_Log(HDMI_LOG_DEBUG, "%s::HDMI resolutions size is calibrated(%d -> %d)..", __func__, hdmiResolutionValue, newHdmiResolutionValue);
        }
    } else {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::find resolutions(%d) at once", __func__, hdmiResolutionValue);
    }
#endif

    if (m_setHdmiResolution(newHdmiResolutionValue, newHdmiS3DMode) == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::m_setHdmiResolution() failed", __func__);
        return false;
    }

    if (mHdmiResolutionValue != newHdmiResolutionValue) {
        mHdmiResolutionValue = newHdmiResolutionValue;
        mHdmiInfoChange = true;
    }

    if (mHdmiS3DMode != newHdmiS3DMode) {
        mHdmiS3DMode = newHdmiS3DMode;
        mHdmiInfoChange = true;
    }

    return true;
}

void ExynosHdmi::getHdmiResolution(uint32_t *width, uint32_t *height)
{
    *width = mHdmiDstWidth;
    *height = mHdmiDstHeight;
}

bool ExynosHdmi::setHdcpMode(bool hdcpMode, bool forceRun)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        ALOGE("%s::Not Yet Created \n", __func__);
        return false;
    }

    if (forceRun == false && mHdcpMode == hdcpMode) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdcpMode(%d)", __func__, hdcpMode);
        return true;
    }

    mHdcpMode = hdcpMode;
    mHdmiInfoChange = true;

    return true;
}

bool ExynosHdmi::setUIRotation(unsigned int rotVal, unsigned int hwcLayer)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::rotVal=%d, mUIRotVal=%d, hwcLayer=%d", __func__, rotVal, mUIRotVal, hwcLayer);

    Mutex::Autolock lock_UI(mLock_UI);
    Mutex::Autolock lock_VIDEO(mLock_VIDEO);

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not Yet Created", __func__);
        return false;
    }

    if (rotVal % 90 != 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Invalid rotation value(%d)", __func__, rotVal);
        return false;
    }

    /* G2D rotation */
    if (rotVal != mUIRotVal) {
        mUIRotVal = rotVal;
        mHdmiInfoChange = true;
    }

    return true;
}

bool ExynosHdmi::setDisplaySize(int width, int height)
{
    mDisplayWidth = width;
    mDisplayHeight = height;

    return true;
}

bool ExynosHdmi::m_changeHdmiPath(void)
{
    if (mCurrentHdmiPath == mHdmiPath)
        return true;

    if (mCurrentHdmiPath == HDMI_PATH_OVERLAY) {
        if (m_gsc_out_handle) {
            if (exynos_gsc_stop_exclusive(m_gsc_out_handle) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
              return false;
            }
            exynos_gsc_destroy(m_gsc_out_handle);
        }
        m_gsc_out_handle = NULL;
    } else {
        if (m_gsc_cap_handle) {
            if (exynos_gsc_stop_exclusive(m_gsc_cap_handle) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
             return false;
            }
            exynos_gsc_destroy(m_gsc_cap_handle);
        }
        m_gsc_cap_handle = NULL;
    }

    if (mHdmiPath == HDMI_PATH_OVERLAY) {
        int gsc_out_tv_mode = mHdmiOutputMode + 2;
        m_gsc_out_handle = exynos_gsc_create_exclusive(
                                            DEFAULT_GSC_OUT_INDEX,
                                            GSC_OUTPUT_MODE,
                                            gsc_out_tv_mode);
        if (m_gsc_out_handle == NULL) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::create gsc_out failed", __func__);
            return false;
        }
    } else {
        m_gsc_cap_handle = exynos_gsc_create_exclusive(
                                            DEFAULT_GSC_CAP_INDEX,
                                            GSC_CAPTURE_MODE,
                                            0);
        if (m_gsc_cap_handle == NULL) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::create gsc_cap failed", __func__);
            return false;
        }
    }

    mCurrentHdmiPath = mHdmiPath;

    if (m_enableLayerBlending() == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Layer Blending failed", __func__);
        return false;
    }
    return true;
}

bool ExynosHdmi::m_clearbuffer(int layer)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::layer=%d", __func__, layer);

    struct v4l2_requestbuffers reqbuf;

    switch (layer) {
    case HDMI_LAYER_VIDEO:
        if (m_gsc_out_handle) {
            if (exynos_gsc_stop_exclusive(m_gsc_out_handle) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
                return false;
            }
        }
        break;
    case HDMI_LAYER_GRAPHIC_0:
        if (m_mxr_handle_grp0) {
            if (exynos_mxr_stop_n_clear(m_mxr_handle_grp0) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_stop_n_clear", __func__);
                return false;
            }
        }
        break;
    case HDMI_LAYER_GRAPHIC_1:
        if (m_mxr_handle_grp1) {
            if (exynos_mxr_stop_n_clear(m_mxr_handle_grp1) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_stop_n_clear", __func__);
                return false;
            }
        }
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmatched layer[%d]", __func__, layer);
        return false;
        break;
    }

    mSrcWidth      [layer] = 0;
    mSrcHeight     [layer] = 0;
    mSrcColorFormat[layer] = 0;
    mHdmiResolutionWidth  [layer] = 0;
    mHdmiResolutionHeight [layer] = 0;

    return true;
}

bool ExynosHdmi::m_reset(int w, int h, int dstX, int dstY, int colorFormat, int hdmiLayer, int hdmiMode, int flag_full_display)
{
    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::w=%d, h=%d\r\n"
            "    dstX=%d, dstY=%d, colorFormat=%d, hdmiLayer=%d, hdmiMode=%d, flag_full_display=%d, PathMode=%d",
            __func__, w, h,
            dstX, dstY, colorFormat, hdmiLayer, hdmiMode, flag_full_display, mHdmiPath);

    int srcW = w;
    int srcH = h;
    struct MXR_HANDLE *mxr_handle = (struct MXR_HANDLE *)m_mxr_handle_grp1;

    if (mHdmiPathChange == true) {
        if (m_changeHdmiPath() == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:Path change failed", __func__);
            return false;
        }
        mHdmiInfoChange = true;
    }

    if (mHdmiInfoChange == true || mHdmiPathChange == true) {
        HDMI_Log(HDMI_LOG_DEBUG, "mHdmiInfoChange: %d", mHdmiInfoChange);

#if defined(BOARD_USES_CEC)
        if (mCECThread->mFlagRunning)
            mCECThread->stop();
#endif

        if (m_setHdmiOutputMode(mHdmiOutputMode) == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::m_setHdmiOutputMode() failed", __func__);
            return false;
        }

        if (m_setHdmiResolution(mHdmiResolutionValue, mHdmiS3DMode) == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::m_setHdmiResolution() failed", __func__);
            return false;
        }

        if (m_setHdcpMode(mHdcpMode) == false) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::m_setHdcpMode() failed", __func__);
            return false;
        }

        if (mCurrentHdmiPresetId != mHdmiPresetId) {
            for (int layer = HDMI_LAYER_BASE + 1; layer < HDMI_LAYER_MAX; layer++) {
                if (m_stopHdmi(layer) == false) {
                    HDMI_Log(HDMI_LOG_ERROR, "%s::m_stopHdmi(%d) failed", __func__, layer);
                    return false;
                }
            }

            if (tvout_init(mxr_handle->mxr_vd_entity->fd, mHdmiPresetId) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::tvout_init(mHdmiPresetId=%d) failed", __func__, mHdmiPresetId);
                return false;
            }
            mCurrentHdmiPresetId = mHdmiPresetId;
        }

        if (hdmi_check_interlaced_resolution(mHdmiResolutionValue) == true)
            mHdmiOutFieldOrder = GSC_TV_OUT_INTERLACED;
        else
            mHdmiOutFieldOrder = GSC_TV_OUT_PROGRESSIVE;

#if defined(BOARD_USES_CEC)
        if (!(mCECThread->mFlagRunning))
            mCECThread->start();
#endif

    }

    if (w != mSrcWidth[hdmiLayer] ||
        h != mSrcHeight[hdmiLayer] ||
        mHdmiDstWidth != mHdmiResolutionWidth[hdmiLayer] ||
        mHdmiDstHeight != mHdmiResolutionHeight[hdmiLayer] ||
        mDstWidth[hdmiLayer] != mPrevDstWidth[hdmiLayer] ||
        mDstHeight[hdmiLayer] != mPrevDstHeight[hdmiLayer] ||
        colorFormat != mSrcColorFormat[hdmiLayer] ||
        mHdmiPathChange == true ||
        mHdmiInfoChange == true) {

        if (hdmiLayer == HDMI_LAYER_VIDEO) {
            if (mHdmiPathChange == false) {
                if (m_clearbuffer(hdmiLayer) == false) {
                    HDMI_Log(HDMI_LOG_ERROR, "%s::m_clearbuffer: layer[%d] failed", __func__, hdmiLayer);
                    return false;
                }
            }

            /* calculate destination buffer width and height */
            struct v4l2_rect scaled_dstRect;
            int width_align_value = 16;

            hdmi_cal_rect(srcW, srcH, mHdmiDstWidth, mHdmiDstHeight, &scaled_dstRect);

            if (colorFormat == HAL_PIXEL_FORMAT_YV12)
                width_align_value = 32;

            exynos_gsc_img src_info;
            exynos_gsc_img dst_info;

            memset(&src_info, 0, sizeof(src_info));
            memset(&dst_info, 0, sizeof(dst_info));

            src_info.x = 0;
            src_info.y = 0;

            if (mHdmiOutFieldOrder == GSC_TV_OUT_INTERLACED) {
                hdmi_align_for_interlaced(&src_info.fw, &src_info.fh, srcW, srcH, width_align_value);
                src_info.w  = ROUND_DOWN(srcW, 2);
                src_info.h  = ROUND_DOWN(srcH, 2) / 2;
                scaled_dstRect.top /= 2;
            } else {
                src_info.fw = ROUND_UP(srcW, width_align_value);
                src_info.fh = ROUND_UP(srcH, 16);
                src_info.w  = ROUND_DOWN(srcW, 2);
                src_info.h  = ROUND_DOWN(srcH, 2);
            }

            src_info.format = colorFormat;
            src_info.yaddr = 0;
            src_info.uaddr = 0;
            src_info.vaddr = 0;
            src_info.rot = 0;
            src_info.cacheable = 0;
            src_info.drmMode = mHdmiDrmMode;

            dst_info.x  = scaled_dstRect.left;
            dst_info.y  = scaled_dstRect.top;
            dst_info.fw = mHdmiDstWidth;
            dst_info.fh = mHdmiDstHeight;
            dst_info.w  = scaled_dstRect.width;

            if (mHdmiOutFieldOrder == GSC_TV_OUT_INTERLACED)
                dst_info.h  = scaled_dstRect.height / 2;
            else
                dst_info.h  = scaled_dstRect.height;

            dst_info.format = colorFormat;
            dst_info.yaddr = 0;
            dst_info.uaddr = 0;
            dst_info.vaddr = 0;
            dst_info.rot = 0;
            dst_info.cacheable = 0;
            dst_info.drmMode = 0;
            dst_info.fieldOrder = mHdmiOutFieldOrder;

            HDMI_Log(HDMI_LOG_DEBUG,
                    "gsc_out_config : src_info fw=%d, fh=%d, w=%d, h=%d\r\n"
                    "gsc_out_config : dst_info fw=%d, fh=%d, w=%d, h=%d, x=%d, y=%d",
                    src_info.fw, src_info.fh, src_info.w, src_info.h,
                    dst_info.fw, dst_info.fh, dst_info.w, dst_info.h, dst_info.x, dst_info.y);

            if (exynos_gsc_config_exclusive(m_gsc_out_handle, &src_info, &dst_info) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_config_exclusive", __func__);
                return false;
            }

            mPrevDstWidth[hdmiLayer] = mHdmiDstWidth;
            mPrevDstHeight[hdmiLayer] = mHdmiDstHeight;
        } else {
            exynos_gsc_img src_info;
            exynos_gsc_img dst_info;
            exynos_mxr_img src_img;
            exynos_mxr_img dst_img;

            void *m_mxr_handle_grp = NULL;
            int mxr_dstFW = 0;
            int mxr_dstFH = 0;
            int mxr_dstW  = 0;
            int mxr_dstH  = 0;

            memset(&src_info, 0, sizeof(src_info));
            memset(&dst_info, 0, sizeof(dst_info));
            memset(&src_img,  0, sizeof(src_img));
            memset(&dst_img,  0, sizeof(dst_img));

            if (hdmiMode == HDMI_MODE_MIRROR) {
                if (mHdmiPath == HDMI_PATH_WRITEBACK) {
                    int dstW = 0;
                    int dstH = 0;
                    if (mUIRotVal == 0 || mUIRotVal == 180) {
                        dstW = mDstRect.width;
                        dstH = mDstRect.height;
                    } else {
                        dstW = mDstRect.height;
                        dstH = mDstRect.width;
                    }

                    if (m_gsc_cap_handle) {
                        if (exynos_gsc_stop_exclusive(m_gsc_cap_handle) < 0) {
                            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_stop_exclusive", __func__);
                            return false;
                        }
                    }

                    src_info.x  = 0;
                    src_info.y  = 0;
                    src_info.fw = srcW;
                    src_info.fh = srcH;
                    src_info.w  = srcW;
                    src_info.h  = srcH;
                    src_info.format = HAL_PIXEL_FORMAT_RGBA_8888;
                    src_info.rot = 0;
                    src_info.cacheable = 0;
                    src_info.drmMode = 0;

                    dst_info.x = 0;
                    dst_info.y = 0;
                    dst_info.fw = ROUND_UP(dstW, 16);
                    dst_info.fh = ROUND_UP(dstH, 8);
                    dst_info.w  = ROUND_DOWN(dstW, 4);
                    dst_info.h  = ROUND_DOWN(dstH, 4);
                    dst_info.format = HAL_PIXEL_FORMAT_RGBA_8888;
                    dst_info.rot = 0;
                    dst_info.cacheable = 0;
                    dst_info.drmMode = 0;
                    dst_info.reqBufCnt = 1;

                    mxr_dstFW = dst_info.fw;
                    mxr_dstFH = dst_info.fh;
                    mxr_dstW  = dst_info.w;
                    mxr_dstH  = dst_info.h;

                    HDMI_Log(HDMI_LOG_DEBUG,
                            "gsc_cap_config : src_info fw=%d, fh=%d, w=%d, h=%d\r\n"
                            "gsc_cap_config : dst_info fw=%d, fh=%d, w=%d, h=%d",
                            src_info.fw, src_info.fh, src_info.w, src_info.h,
                            dst_info.fw, dst_info.fh, dst_info.w, dst_info.h);

                    if (exynos_gsc_config_exclusive(m_gsc_cap_handle, &src_info, &dst_info) < 0) {
                        HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_config_exclusive", __func__);
                        return false;
                    }

                    /* if need rotation, reset rotation h/w(src, dest buffer info) */
                    if (mExynosRotator != NULL && (mUIRotVal == 90 || mUIRotVal == 180 || mUIRotVal == 270)) {
                        int rot_dstW = dst_info.w;
                        int rot_dstH = dst_info.h;

                        if (mUIRotVal == 90 || mUIRotVal == 270) {
                            rot_dstW = dst_info.h;
                            rot_dstH = dst_info.w;
                        }

                        hdmi_rotateConf(mExynosRotator,
                                                  dst_info.fw,
                                                  dst_info.fh,
                                                  dst_info.w,
                                                  dst_info.h,
                                                  dst_info.format,
                                                  rot_dstW,
                                                  rot_dstH,
                                                  dst_info.format);

                        mxr_dstFW = rot_dstW;
                        mxr_dstFH = rot_dstH;
                        mxr_dstW  = rot_dstW;
                        mxr_dstH  = rot_dstH;
                    }
                } else {
                    mxr_dstFW = mDstRect.width;
                    mxr_dstFH = mDstRect.height;
                    mxr_dstW  = mxr_dstFW;
                    mxr_dstH  = mxr_dstFH;
                }

                mPrevDstWidth[HDMI_LAYER_VIDEO] = 0;
                mPrevDstHeight[HDMI_LAYER_VIDEO] = 0;
            } else if ((hdmiMode == HDMI_MODE_UI_0) || (hdmiMode == HDMI_MODE_UI_1)) {
                src_info.fw = srcW;
                src_info.fh = srcH;
                src_info.w  = srcW;
                src_info.h  = srcH;

                dst_info.fw = mDstRect.width;
                dst_info.fh = mDstRect.height;
                dst_info.w  = dst_info.fw;
                dst_info.h  = dst_info.fh;

                mxr_dstFW = dst_info.fw;
                mxr_dstFH = dst_info.fh;
                mxr_dstW  = dst_info.w;
                mxr_dstH  = dst_info.h;
            }

            if (m_clearbuffer(hdmiLayer) == false) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::m_clearbuffer: layer[%d] failed", __func__, hdmiLayer);
                return false;
            }

            if (((hdmiMode == HDMI_MODE_UI_0) || (hdmiMode == HDMI_MODE_UI_1)) &&
                ((mUIRotVal == 90 || mUIRotVal == 270))) {
                mxr_dstW = dst_info.h;
                mxr_dstH = dst_info.w;
            }

            src_img.x  = 0;
            src_img.y  = 0;
            src_img.fw = mxr_dstFW;
            src_img.fh = mxr_dstFH;
            src_img.w  = mxr_dstW;
            src_img.h  = mxr_dstH;
            src_img.format = colorFormat;
            src_img.addr = 0;

            if (mHdmiPath == HDMI_PATH_WRITEBACK)
                src_img.blending = HDMI_BLENDING_DISABLE;
            else
                src_img.blending = HDMI_BLENDING_ENABLE;

            dst_img.x  = dstX;
            dst_img.y  = dstY;
            dst_img.fw = mHdmiDstWidth;
            dst_img.fh = mHdmiDstHeight;
            dst_img.w  = mxr_dstW;
            dst_img.h  = mxr_dstH;
            dst_img.format = colorFormat;
            dst_img.addr = 0;
            dst_img.blending = src_img.blending;

            HDMI_Log(HDMI_LOG_DEBUG,
                    "mxr_config : src_img fw=%d, fh=%d, w=%d, h=%d\r\n"
                    "mxr_config : dst_img fw=%d, fh=%d, w=%d, h=%d, x=%d, y=%d",
                    src_img.fw, src_img.fh, src_img.w, src_img.h,
                    dst_img.fw, dst_img.fh, dst_img.w, dst_img.h, dst_img.x, dst_img.y);

            if (hdmiLayer == HDMI_LAYER_GRAPHIC_0)
                m_mxr_handle_grp = m_mxr_handle_grp0;
            else
                m_mxr_handle_grp = m_mxr_handle_grp1;

            if (exynos_mxr_config(m_mxr_handle_grp, &src_img, &dst_img, V4L2_OUTPUT_TYPE_HDMI_RGB) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_config", __func__);
                return false;
            }

            mPrevDstWidth[hdmiLayer] = mDstRect.width;
            mPrevDstHeight[hdmiLayer] = mDstRect.height;
       }

        mSrcWidth[hdmiLayer] = srcW;
        mSrcHeight[hdmiLayer] = srcH;
        mSrcColorFormat[hdmiLayer] = colorFormat;

        mHdmiResolutionWidth[hdmiLayer] = mHdmiDstWidth;
        mHdmiResolutionHeight[hdmiLayer] = mHdmiDstHeight;

        mHdmiPathChange = false;
        mHdmiInfoChange = false;

        HDMI_Log(HDMI_LOG_DEBUG,
                "m_reset saved param(srcW=%d, mSrcWidth=%d, "
                "    srcH=%d, mSrcHeight=%d, "
                "    colorFormat=%d, mSrcColorFormat=%d, hdmiLayer=%d)",
                srcW, mSrcWidth[hdmiLayer],
                srcH, mSrcHeight[hdmiLayer],
                colorFormat, mSrcColorFormat[hdmiLayer], hdmiLayer);
    }

    return true;
}

bool ExynosHdmi::m_runHdmi(int layer, unsigned int srcYAddr, unsigned int srcCbAddr, unsigned int srcCrAddr)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::layer = %d", __func__, layer);

    struct v4l2_crop   crop;

    if (mFlagCreate == false) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Not yet created", __func__);
        return false;
    }

    switch (layer) {
    case HDMI_LAYER_VIDEO:
        if (mHdmiPath != HDMI_PATH_OVERLAY)
            return true;

        exynos_gsc_img src_info;
        exynos_gsc_img dst_info;

        src_info.yaddr = (uint32_t)srcYAddr;
        src_info.uaddr = (uint32_t)srcCbAddr;
        src_info.vaddr = (uint32_t)srcCrAddr;
        src_info.cacheable = 0;
        src_info.drmMode = 0;

        if (mHdmiOutFieldOrder == GSC_TV_OUT_INTERLACED) {
            struct GSC_HANDLE *gsc_handle;
            gsc_handle = (struct GSC_HANDLE *)m_gsc_out_handle;

            crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

            if (exynos_v4l2_g_crop(gsc_handle->gsc_vd_entity->fd, &crop) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::videodev set crop failed (layer=%d)", __func__, layer);
                return false;
            }

            crop.c.top    = 0;

            if (gsc_handle->src.buf_idx % 2 == 0)
                crop.c.left   = 0;
            else
                crop.c.left   = ROUND_UP(crop.c.width, 16);

            if (exynos_v4l2_s_crop(gsc_handle->gsc_vd_entity->fd, &crop) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::videodev set crop failed (layer=%d)", __func__, layer);
                return false;
            }

            HDMI_Log(HDMI_LOG_DEBUG, "%s::crop.c.left=%d, c.top=%d, c.width=%d, c.height=%d",
                    __func__, crop.c.left, crop.c.top, crop.c.width, crop.c.height);
        }

        if (exynos_gsc_run_exclusive(m_gsc_out_handle, &src_info, &dst_info) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_run_exclusive", __func__);
            return false;
        }

        if (exynos_gsc_wait_done(m_gsc_out_handle) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_gsc_wait_done", __func__);
            return false;
        }
        break;
    case HDMI_LAYER_GRAPHIC_0:
        if (exynos_mxr_run(m_mxr_handle_grp0, srcYAddr) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_run", __func__);
            return false;
        }
        if (exynos_mxr_wait_done(m_mxr_handle_grp0) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_wait_done", __func__);
            return false;
        }
        break;
    case HDMI_LAYER_GRAPHIC_1:
        if (exynos_mxr_run(m_mxr_handle_grp1, (unsigned int)srcYAddr) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_run", __func__);
            return false;
        }

        if (exynos_mxr_wait_done(m_mxr_handle_grp1) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:: error : exynos_mxr_wait_done", __func__);
            return false;
        }
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced layer(%d) failed", __func__, layer);
        return false;
        break;
    }

    return true;
}

bool ExynosHdmi::m_stopHdmi(int layer)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s::layer=%d", __func__, layer);

    switch (layer) {
    case HDMI_LAYER_VIDEO:
        if (m_gsc_out_handle) {
            if (exynos_gsc_just_stop(m_gsc_out_handle) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::stream off failed (layer=%d)", __func__, layer);
                return false;
            }
        }
        break;
    case HDMI_LAYER_GRAPHIC_0:
        if (m_mxr_handle_grp0) {
            if (exynos_mxr_just_stop(m_mxr_handle_grp0) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::stream off failed (layer=%d)", __func__, layer);
                return false;
            }
        }
        break;
    case HDMI_LAYER_GRAPHIC_1:
        if (m_mxr_handle_grp1) {
            if (exynos_mxr_just_stop(m_mxr_handle_grp1) < 0) {
                HDMI_Log(HDMI_LOG_ERROR, "%s::stream off failed (layer=%d)", __func__, layer);
                return false;
            }
        }
        break;
    default:
        HDMI_Log(HDMI_LOG_ERROR, "%s::unmathced layer(%d) failed", __func__, layer);
        return false;
        break;
    }

    return true;
}

bool ExynosHdmi::m_setHdmiOutputMode(int hdmiOutputMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);
    int s_value = 0;

    if (hdmiOutputMode == mCurrentHdmiOutputMode) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdmiOutputMode(%d)", __func__, hdmiOutputMode);
        return true;
    }

    int v4l2OutputType = hdmi_outputmode_2_v4l2_output_type(hdmiOutputMode);
    if (v4l2OutputType < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_outputmode_2_v4l2_output_type(%d) fail", __func__, hdmiOutputMode);
        return false;
    }

    output_type = v4l2OutputType;
    mCurrentHdmiOutputMode = hdmiOutputMode;

    if (mCurrentHdmiOutputMode == HDMI_OUTPUT_MODE_DVI)
        s_value = 1;
    else
        s_value = 0;

    if (exynos_mxr_set_ctrl(m_mxr_handle_grp1, V4L2_CID_TV_SET_DVI_MODE, s_value)) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::V4L2_CID_TV_SET_DVI_MODE(value=%d) fail", __func__, s_value);
    }

    return true;
}

bool ExynosHdmi::m_setHdmiResolution(unsigned int hdmiResolutionValue, unsigned int s3dMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if ((hdmiResolutionValue == mCurrentHdmiResolutionValue) && (s3dMode == mCurrentHdmiS3DMode)) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdmiResolutionValue(%d), mCurrentHdmiS3DMode(%d)",
                __func__, hdmiResolutionValue, mCurrentHdmiS3DMode);
        return true;
    }

    int w = 0;
    int h = 0;

    if (hdmi_resolution_2_preset_id(hdmiResolutionValue, s3dMode, &w, &h, &mHdmiPresetId) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_resolution_2_preset_id(%d) fail", __func__, hdmiResolutionValue);
        return false;
    }

    mHdmiDstWidth  = w;
    mHdmiDstHeight = h;
    mCurrentHdmiResolutionValue = hdmiResolutionValue;
    mCurrentHdmiS3DMode = s3dMode;

    HDMI_Log(HDMI_LOG_DEBUG,
            "%s::mHdmiDstWidth=%d, mHdmiDstHeight=%d, "
            "mHdmiPresetId=0x%x, hdmiResolutionValue=0x%x, s3dMode=0x%x",
            __func__, mHdmiDstWidth, mHdmiDstHeight,
            mHdmiPresetId, hdmiResolutionValue, s3dMode);

    return true;
}

bool ExynosHdmi::m_setHdcpMode(bool hdcpMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (hdcpMode == mCurrentHdcpMode) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same hdcpMode(%d)", __func__, hdcpMode);
        return true;
    }

    if (hdcpMode == true)
        g_hdcp_en = 1;
    else
        g_hdcp_en = 0;

    mCurrentHdcpMode = hdcpMode;

    return true;
}

#if 0 // Before activate this code, check the driver support, first.
bool ExynosHdmi::m_setAudioMode(int audioMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    struct MXR_HANDLE *mxr_handle = (struct MXR_HANDLE *)m_mxr_handle_grp1;

    if (audioMode == mCurrentAudioMode) {
        HDMI_Log(HDMI_LOG_DEBUG, "%s::same audioMode(%d)", __func__, audioMode);
        return true;
    }

    if (hdmi_check_audio(mxr_handle->mxr_vd_entity->fd) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::hdmi_check_audio() fail", __func__);
        return false;
    }

    mCurrentAudioMode = audioMode;

    return true;
}
#endif

int ExynosHdmi::m_resolutionValueIndex(unsigned int ResolutionValue, unsigned int s3dMode)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    int index = -1;

    if (s3dMode == HDMI_2D) {
        for (int i = 0; i < NUM_SUPPORTED_RESOLUTION_2D; i++) {
            if (mHdmiResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else if (s3dMode == HDMI_S3D_TB) {
        for (int i = 0; i < NUM_SUPPORTED_RESOLUTION_S3D_TB; i++) {
            if (mHdmiS3dTbResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else if (s3dMode == HDMI_S3D_SBS) {
        for (int i = 0; i < NUM_SUPPORTED_RESOLUTION_S3D_SBS; i++) {
            if (mHdmiS3dSbsResolutionValueList[i] == ResolutionValue) {
                index = i;
                break;
            }
        }
    } else {
        HDMI_Log(HDMI_LOG_ERROR, "%s::Unsupported S3D mode(%d)\n", __func__, s3dMode);
    }

    return index;
}

bool ExynosHdmi::m_flagHWConnected(void)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    bool cable_status = true;
    int hdmiStatus    = 0;

    if (exynos_mxr_get_ctrl(m_mxr_handle_grp1, V4L2_CID_TV_HPD_STATUS, &hdmiStatus) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "Get HPD_STATUS fail");
        cable_status = false;
    } else {
        if (0 < hdmiStatus)
            cable_status = true;
        else
            cable_status = false;
    }

    return cable_status;
}

bool ExynosHdmi::m_enableLayerBlending()
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    if (mHdmiPath == HDMI_PATH_OVERLAY) {
        if (exynos_mxr_set_ctrl(m_mxr_handle_grp0, V4L2_CID_TV_PIXEL_BLEND_ENABLE, HDMI_BLENDING_ENABLE) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:failed to enable blending of graphic layer 0", __func__);
            return false;
        }

        if (exynos_mxr_set_ctrl(m_mxr_handle_grp1, V4L2_CID_TV_PIXEL_BLEND_ENABLE, HDMI_BLENDING_ENABLE) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:failed to enable blending of graphic layer 1", __func__);
            return false;
        }
    } else {
        if (exynos_mxr_set_ctrl(m_mxr_handle_grp0, V4L2_CID_TV_PIXEL_BLEND_ENABLE, HDMI_BLENDING_DISABLE) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:failed to disable blending of graphic layer 0", __func__);
            return false;
        }

        if (exynos_mxr_set_ctrl(m_mxr_handle_grp1, V4L2_CID_TV_PIXEL_BLEND_ENABLE, HDMI_BLENDING_DISABLE) < 0) {
            HDMI_Log(HDMI_LOG_ERROR, "%s:failed to disable blending of graphic layer 1", __func__);
            return false;
        }
        HDMI_Log(HDMI_LOG_DEBUG, "%s::In Writeback Mode, Layer blending is not supported", __func__);
    }
    return true;
}

int ExynosHdmi::m_setMemory(int ionClient, int * fd, unsigned int map_size, unsigned int * ion_map_ptr, unsigned int flag)
{
    HDMI_Log(HDMI_LOG_DEBUG, "%s", __func__);

    void * ptr = NULL;
    unsigned int align_size = 0;

    if (flag == ION_HEAP_EXYNOS_CONTIG_MASK)
        align_size = 0x00100000; // SIZE_1M
    else
        align_size = PAGE_SIZE;

    (*fd) = ion_alloc(ionClient, ALIGN(map_size, align_size), 0, flag);

    if ((*fd) < 0) {
        HDMI_Log(HDMI_LOG_ERROR, "%s::ION memory allocation failed", __func__);
        return -1;
    } else {
        ptr = ion_map(*fd, ALIGN(map_size, align_size), 0);
        if (ptr == MAP_FAILED) {
            HDMI_Log(HDMI_LOG_ERROR, "%s::ION mmap failed", __func__);
            return -1;
        }
    }

    *ion_map_ptr = (unsigned int)ptr;
    return 0;
}

#define CHK_FRAME_CNT 30

void ExynosHdmi::m_CheckFps(void)
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
            ALOGE(">>>>>>>> HDMI [FPS]:%d", FPS);
            total = 0;
            cnt = 10;
        }
    } else {
        memcpy(&tick_old, &tick, sizeof(timeval));
        total = 0;
    }
}

}; // namespace android
