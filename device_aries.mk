$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)
# The gps config appropriate for this device
$(call inherit-product-if-exists, device/common/gps/gps_us_supl.mk)

$(call inherit-product-if-exists, vendor/xiaomi/aries/aries-vendor.mk)

DEVICE_PACKAGE_OVERLAYS += device/xiaomi/aries/overlay
include $(call all-subdir-makefiles)
PRODUCT_COPY_FILES += \
    device/xiaomi/aries/kernel:kernel

$(call inherit-product, build/target/product/embedded.mk)

PRODUCT_BUILD_PROP_OVERRIDES += BUILD_UTC_DATE=0
PRODUCT_NAME := full_aries
PRODUCT_DEVICE := aries
