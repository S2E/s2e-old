LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog

LOCAL_MODULE    := s2eandroid
LOCAL_SRC_FILES := inlinearm.c
#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_EXECUTABLE)