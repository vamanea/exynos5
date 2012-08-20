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

ifeq ($(BOARD_USES_HDMI),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := \
	libutils \
	liblog \
	libedid \
	libcec \
	libexynosv4l2 \
	libexynosutils \
	libexynosgscaler \
	libhdmiutils \
	libhdmimodule \
	libion_exynos

LOCAL_SRC_FILES := \
	ExynosHdmi.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../include \
	$(LOCAL_PATH)/../../libhdmi/libhdmiutils \
	$(LOCAL_PATH)/../../libhdmi/libsForhdmi/libedid \
	$(LOCAL_PATH)/../../libhdmi/libsForhdmi/libcec \
	$(LOCAL_PATH)/../../libhdmi/libsForhdmi/libddc \
	$(LOCAL_PATH)/../../libgscaler \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/libhdmimodule \
	$(TOP)/hardware/samsung_slsi/$(TARGET_SOC)/include \
	$(TOP)/hardware/samsung_slsi/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi/exynos/include

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
	LOCAL_CFLAGS  += -DBOARD_USES_HDMI_SUBTITLES
endif

ifeq ($(BOARD_USES_FIMGAPI),true)
ifeq ($(BOARD_USES_HDMI_FIMGAPI),true)
	LOCAL_CFLAGS += -DBOARD_USES_HDMI_FIMGAPI
	LOCAL_C_INCLUDES += hardware/samsung_slsi/exynos/libfimg4x
	LOCAL_C_INCLUDES += external/skia/include/core
	LOCAL_SHARED_LIBRARIES += libfimg
endif
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

LOCAL_MODULE := libhdmi
include $(BUILD_SHARED_LIBRARY)

endif
