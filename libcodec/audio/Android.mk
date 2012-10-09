LOCAL_PATH := $(call my-dir)
EXYNOS_AUDIO_CODEC := $(LOCAL_PATH)

include $(CLEAR_VARS)

ifeq ($(BOARD_USE_ALP_AUDIO), true)
include $(EXYNOS_AUDIO_CODEC)/alp/Android.mk
endif

ifeq ($(BOARD_USE_WMA_CODEC), true)
include $(EXYNOS_AUDIO_CODEC)/ffmpeg/Android.mk
endif
