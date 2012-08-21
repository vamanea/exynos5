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

#ifndef __EXYNOS_HDMI_LOG_H__
#define __EXYNOS_HDMI_LOG_H__

//#define DEBUG_LIB_HDMI
#define HDMI_LOG_TAG     "ExynosHDMI_LOG"

typedef enum _HDMI_LOG_LEVEL {
    HDMI_LOG_DEBUG,
    HDMI_LOG_WARNING,
    HDMI_LOG_ERROR,
} HDMI_LOG_LEVEL;

#ifdef DEBUG_LIB_HDMI
#define HDMI_Log(a, ...)    ((void)_HDMI_Log(a, HDMI_LOG_TAG, __VA_ARGS__))
#else
#define HDMI_Log(a, ...)                                            \
    do {                                                                   \
        if (a == HDMI_LOG_ERROR)                                         \
            ((void)_HDMI_Log(a, HDMI_LOG_TAG, __VA_ARGS__)); \
    } while (0)
#endif

void _HDMI_Log(HDMI_LOG_LEVEL logLevel, const char *tag, const char *msg, ...);

#endif
