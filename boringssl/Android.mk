LOCAL_PATH := $(call my-dir)

aries_boringssl_imports := \
    $(TARGET_OUT_ETC)/init/hw/init.boringssl.zygote32.rc \
    $(TARGET_OUT_ETC)/init/hw/init.boringssl.no_zygote.rc \
    $(TARGET_OUT_VENDOR_ETC)/boringssl_self_test.zygote32.rc \
    $(TARGET_OUT_VENDOR_ETC)/boringssl_self_test.no_zygote.rc

# Run after the platform has installed its canonical files and companion links.
# The installed marker makes image/file-list generation wait for this step.
include $(CLEAR_VARS)
LOCAL_MODULE := aries_boringssl_init_configs
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_MODULE_STEM := aries-boringssl-init-configs
LOCAL_SRC_FILES := regular-init-imports.txt
LOCAL_REQUIRED_MODULES := init.boringssl.zygote64_32.rc boringssl_self_test.zygote64_32.rc
LOCAL_ADDITIONAL_DEPENDENCIES := $(aries_boringssl_imports) $(LOCAL_PATH)/materialize-init-imports.py
LOCAL_POST_INSTALL_CMD := /usr/bin/python3 $(LOCAL_PATH)/materialize-init-imports.py $(PRODUCT_OUT) $(TARGET_OUT_ETC)/aries-boringssl-init-configs
include $(BUILD_PREBUILT)

aries_boringssl_imports :=
