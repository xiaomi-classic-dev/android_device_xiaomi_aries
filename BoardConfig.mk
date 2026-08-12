#
# Copyright (C) 2011 The Android Open-Source Project
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
#

-include device/xiaomi/msm8960-common/BoardConfigCommon.mk

DEVICE_PATH := device/xiaomi/aries

# Assert
TARGET_OTA_ASSERT_DEVICE := aries

# Bootloader
TARGET_BOOTLOADER_NAME       := aries

# HIDL
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml

# SELinux
BOARD_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy

# Init
$(call soong_config_set,libinit,vendor_init_lib,libinit_msm8960)

# Kernel
TARGET_KERNEL_CONFIG := aries-perf-user_defconfig

# Partitions
BOARD_SYSTEMIMAGE_PARTITION_SIZE   := 2147483648

# OTA
TARGET_RELEASETOOLS_EXTENSIONS := $(DEVICE_PATH)

-include vendor/xiaomi/cancro/BoardConfigVendor.mk
