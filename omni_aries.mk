## Specify phone tech before including full_phone

# Release name
PRODUCT_RELEASE_NAME := aries

# Inherit device configuration
$(call inherit-product, device/xiaomi/aries/device_aries.mk)

## Device identifier. This must come after all inclusions
PRODUCT_DEVICE := aries
PRODUCT_NAME := omni_aries
PRODUCT_BRAND := Xiaomi
PRODUCT_MODEL := MI 2
PRODUCT_MANUFACTURER := Xiaomi
