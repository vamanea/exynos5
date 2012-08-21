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

#
# libTVOut
#

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
	ExynosTVOutService.cpp \
	IExynosTVOut.cpp \
	MessageQueue.cpp

LOCAL_C_INCLUDES := \

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libutils \
	libcutils \
	libexynosutils \
	libhdmi

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../include/ \
	$(LOCAL_PATH)/../../libhdmi \
	$(LOCAL_PATH)/../../libgscaler \
	$(TOP)/hardware/samsung_slsi/exynos/libexynosutils \
	$(TOP)/hardware/samsung_slsi/exynos/include

ifeq ($(BOARD_USES_HDMI_SUBTITLES),true)
	LOCAL_CFLAGS  += -DBOARD_USES_HDMI_SUBTITLES
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

LOCAL_MODULE := libTVOut

include $(BUILD_SHARED_LIBRARY)

#
# libhdmiclient
#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES:= \
	ExynosHdmiClient.cpp

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES := \
	libbinder \
	libutils \
	libTVOut

ifeq ($(TARGET_SIMULATOR),true)
ifeq ($(TARGET_OS),linux)
ifeq ($(TARGET_ARCH),x86)
LOCAL_LDLIBS += -lpthread -ldl -lrt
endif
endif
endif

ifeq ($(WITH_MALLOC_LEAK_CHECK),true)
	LOCAL_CFLAGS += -DMALLOC_LEAK_CHECK
endif

LOCAL_MODULE:= libhdmiclient

include $(BUILD_SHARED_LIBRARY)

endif
