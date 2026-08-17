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

# Temporary bring-up debugging. Remove before release builds.
WITH_ADB_INSECURE := true
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.sys.usb.config=mtp,adb \
    service.adb.root=1

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit from cancro device
$(call inherit-product, device/xiaomi/aries/aries.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Linux 3.4 has no EROFS support, so keep preinstalled APEX payloads on ext4.
PRODUCT_DEFAULT_APEX_PAYLOAD_TYPE := ext4

# Keep all locales while making Simplified Chinese the default.
PRODUCT_LOCALES := zh_CN $(filter-out zh_CN,$(PRODUCT_LOCALES))

PRODUCT_NAME := lineage_aries
PRODUCT_CHARACTERISTICS := nosdcard
PRODUCT_DEVICE := aries
PRODUCT_BRAND := Xiaomi
PRODUCT_MODEL := MI 2
PRODUCT_MANUFACTURER := Xiaomi

PRODUCT_GMS_CLIENTID_BASE := android-xiaomi

# Device prop
PRODUCT_BUILD_PROP_OVERRIDES += \
    BuildDesc="aries-userdebug 6.0.1 MMB29M 6.3.22 test-keys" \
    BuildFingerprint=Xiaomi/aries/aries:6.0.1/MMB29M/6.3.22:userdebug/test-keys \
    DeviceName=aries \
    DeviceProduct=aries \
    SystemDevice=aries \
    SystemName=aries
