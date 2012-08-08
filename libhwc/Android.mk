# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := liblog libcutils libEGL libGLESv1_CM  libhardware  libhardware_legacy

LOCAL_SHARED_LIBRARIES += libion_exynos libexynosutils libexynosgscaler

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := ExynosHWCLog.cpp ExynosHWCUtils.cpp ExynosHWC.cpp

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../libfimg \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/libhwcmodule \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/include \
	$(TOP)/hardware/samsung_slsi/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi/exynos/include

ifeq ($(BOARD_USES_FIMGAPI),true)
ifeq ($(BOARD_USES_HWC_FIMGAPI),true)
LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi/exynos/libfimg4x
LOCAL_CFLAGS     += -DBOARD_USES_HWC_FIMGAPI
LOCAL_CFLAGS     += -DFIMG2D4X
LOCAL_SHARED_LIBRARIES += libfimg
endif
endif

ifeq ($(BOARD_USES_HDMI),true)
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libhdmi/libhdmiservice

LOCAL_SHARED_LIBRARIES += libhdmiclient libTVOut

LOCAL_CFLAGS += -DBOARD_USES_HDMI
LOCAL_CFLAGS += -DBOARD_HDMI_STD=$(BOARD_HDMI_STD)
LOCAL_CFLAGS += -DVIDEO_DUAL_DISPLAY

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
LOCAL_CFLAGS += -DBOARD_USES_HDMI_SUBTITLES
endif

ifeq ($(BOARD_HDMI_STD), STD_NTSC_M)
LOCAL_CFLAGS  += -DSTD_NTSC_M
endif

ifeq ($(BOARD_HDMI_STD),STD_480P)
LOCAL_CFLAGS  += -DSTD_480P
endif

ifeq ($(BOARD_HDMI_STD),STD_720P)
LOCAL_CFLAGS  += -DSTD_720P
endif

ifeq ($(BOARD_HDMI_STD),STD_1080P)
LOCAL_CFLAGS  += -DSTD_1080P
endif
endif

LOCAL_SHARED_LIBRARIES  += libhwcmodule.$(TARGET_SOC)

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
