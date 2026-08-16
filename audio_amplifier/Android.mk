LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_C_FLAGS += 

ifeq ($(strip $(BOARD_USES_QCOM_HARDWARE)),true)
    LOCAL_CFLAGS += -DAUDIO_CAF
endif

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	external/tinycompress/include \
	$(call include-path-for, audio-route) \
	$(call include-path-for, audio-effects) \
	$(call project-path-for,qcom-audio)/hal \
	$(call project-path-for,qcom-audio)/hal/$(TARGET_BOARD_PLATFORM)/ \
	$(call project-path-for,qcom-audio)/hal/audio_extn \
	hardware/libhardware/include

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libdl

LOCAL_HEADER_LIBRARIES := generated_kernel_headers libaudio_system_headers

LOCAL_SRC_FILES := audio_amplifier.c

LOCAL_MODULE := audio_amplifier.$(TARGET_BOARD_PLATFORM)

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_MODULE_TAGS := optional

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
