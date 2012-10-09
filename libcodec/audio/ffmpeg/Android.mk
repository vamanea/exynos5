LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PREBUILT_LIBS := lib/libavcodec.so \
                       lib/libavformat.so \
                       lib/libavutil.so \
                       lib/libswscale.so
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
include $(BUILD_MULTI_PREBUILT)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	dec/ffmpeg_api.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include/

LOCAL_MODULE := libffmpegapi

LOCAL_MODULE_TAGS := optional

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := libavcodec libavformat libavutil libswscale \
                          libutils libcutils

include $(BUILD_SHARED_LIBRARY)
