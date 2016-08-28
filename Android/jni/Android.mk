LOCAL_PATH := $(call my-dir)/..

include $(CLEAR_VARS)

LOCAL_MODULE := hashtest

PROJECT_FILES := $(LOCAL_PATH)/../main.cpp
PROJECT_FILES += $(wildcard $(LOCAL_PATH)/../HashFunctions/*.c)
PROJECT_FILES += $(wildcard $(LOCAL_PATH)/../HashFunctions/*.cc)
PROJECT_FILES += $(wildcard $(LOCAL_PATH)/../HashFunctions/*.cpp)

LOCAL_CPPFLAGS := -std=c++11
LOCAL_CPPFLAGS += -D__STDC_LIMIT_MACROS

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../HashFunctions/

LOCAL_SRC_FILES := $(PROJECT_FILES)

LOCAL_LDLIBS := -landroid -llog -lz

LOCAL_DISABLE_FORMAT_STRING_CHECKS := true
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true

LOCAL_STATIC_LIBRARIES += android_native_app_glue
LOCAL_STATIC_LIBRARIES += cpufeatures

LOCAL_ARM_NEON := true

include $(BUILD_SHARED_LIBRARY)

$(call import-module, android/native_app_glue)
$(call import-module, android/cpufeatures)
