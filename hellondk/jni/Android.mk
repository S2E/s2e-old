LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog

#LOCAL_MODULE    := s2eandroid
#LOCAL_SRC_FILES := inlinearm.c
#include $(BUILD_EXECUTABLE)

LOCAL_MODULE    := s2etest
LOCAL_SRC_FILES := native.c \
                   s2ewrapper.c
LOCAL_CFLAGS := -marm
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := s2ecommands
LOCAL_SRC_FILES := inlinearm.c
LOCAL_CFLAGS := -marm
include $(BUILD_EXECUTABLE)