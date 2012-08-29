#
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
#

# codecs
PRODUCT_PACKAGES += \
	libOMX.Exynos.AVC.Decoder \
	libOMX.Exynos.MPEG4.Decoder \
	libOMX.Exynos.VP8.Decoder \
	libOMX.Exynos.MPEG4.Encoder \
	libOMX.Exynos.AVC.Encoder \
	libOMX.Exynos.WMV.Decoder

# ALP Audio
ifeq ($(BOARD_USE_ALP_AUDIO),true)
PRODUCT_PACKAGES += \
	libOMX.Exynos.MP3.Decoder
endif

# stagefright and device specific modules
PRODUCT_PACKAGES += \
	libstagefrighthw \
	libExynosOMX_Core \
	libExynosOMX_Resourcemanager

# hw composer HAL
PRODUCT_PACKAGES += \
	hwcomposer.exynos5
