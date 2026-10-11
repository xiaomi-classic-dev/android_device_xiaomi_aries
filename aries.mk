DEVICE_PATH := device/xiaomi/aries

$(call inherit-product, device/xiaomi/msm8960-common/msm8960-common.mk)
$(call inherit-product, vendor/xiaomi/aries/aries-vendor.mk)

# Mi 2 launched on Android 4.1 (API 16).
PRODUCT_SHIPPING_API_LEVEL := 16

# System properties
-include $(DEVICE_PATH)/system_prop.mk

# Audio
PRODUCT_PACKAGES += \
    audio_amplifier.msm8960

# Touch
PRODUCT_PACKAGES += \
    vendor.lineage.touch@1.0-service.aries

# Authorize the development host without disabling ADB authentication.
ifneq ($(filter eng userdebug,$(TARGET_BUILD_VARIANT)),)
PRODUCT_ADB_KEYS := $(DEVICE_PATH)/configs/adb_keys
PRODUCT_PACKAGES += adb_keys
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/init/init.aries.debug.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.aries.debug.rc
endif
