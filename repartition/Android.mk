LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := aries-repartition
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := RECOVERY_EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/sbin
LOCAL_SRC_FILES := aries_repartition.cpp
LOCAL_CPPFLAGS := -std=c++11 -fexceptions -Wall -Wextra -Werror
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_PACK_MODULE_RELOCATIONS := false
LOCAL_CXX_STL := libc++_static
LOCAL_STATIC_LIBRARIES := libcutils liblog libc
include $(BUILD_EXECUTABLE)
