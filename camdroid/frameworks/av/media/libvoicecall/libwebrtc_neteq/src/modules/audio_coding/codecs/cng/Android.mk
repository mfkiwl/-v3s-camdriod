# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_WEBRTC_ROOT_PATH := $(LOCAL_PATH)/../../../../..

include $(MY_WEBRTC_ROOT_PATH)/android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_cng
LOCAL_MODULE_TAGS := optional
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := \
    webrtc_cng.c \
    cng_helpfuns.c

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(MY_WEBRTC_ROOT_PATH)/include \
    $(MY_WEBRTC_ROOT_PATH)/src \
    $(MY_WEBRTC_ROOT_PATH)/src/common_audio/signal_processing/include

LOCAL_SHARED_LIBRARIES := \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
