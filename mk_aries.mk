# Copyright (C) 2014 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Boot animation
TARGET_SCREEN_HEIGHT := 1280
TARGET_SCREEN_WIDTH := 720

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from cancro device
$(call inherit-product, device/xiaomi/aries/aries.mk)

PRODUCT_NAME := mk_aries
PRODUCT_CHARACTERISTICS := nosdcard
PRODUCT_DEVICE := aries
PRODUCT_BRAND := Xiaomi
PRODUCT_MODEL := MI 2
PRODUCT_MANUFACTURER := Xiaomi

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi

# Device prop
PRODUCT_BUILD_PROP_OVERRIDES += \
    TARGET_DEVICE="aries" \
    PRODUCT_NAME="aries" \
    PRIVATE_BUILD_DESC="aries-userdebug 6.0.1 MMB29M 6.3.22 test-keys"

BUILD_FINGERPRINT := Xiaomi/aries/aries:6.0.1/MMB29M/6.3.22:userdebug/test-keys
