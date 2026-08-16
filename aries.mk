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
    vendor.lineage.touch-service.aries
