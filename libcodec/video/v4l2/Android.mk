LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	dec/ExynosVideoDecoder.c \
	enc/ExynosVideoEncoder.c

ifeq ($(BOARD_USE_DMA_BUF), true)
LOCAL_CFLAGS += -DUSE_DMA_BUF
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(TOP)/hardware/samsung_slsi/exynos/include

ifeq ($(BOARD_USE_KHRONOS_OMX_HEADER), true)
LOCAL_C_INCLUDES += $(TOP)/hardware/samsung_slsi/openmax/include/khronos
else
LOCAL_C_INCLUDES += $(TOP)/frameworks/native/include/media/openmax
endif

LOCAL_MODULE := libExynosVideoApi
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm

include $(BUILD_STATIC_LIBRARY)
