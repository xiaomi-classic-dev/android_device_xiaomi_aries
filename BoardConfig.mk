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

DEVICE_PATH := device/xiaomi/aries

# Architecture
TARGET_ARCH_VARIANT_CPU    := cortex-a9
TARGET_CPU_ABI             := armeabi-v7a
TARGET_CPU_ABI2            := armeabi
TARGET_CPU_SMP             := true
TARGET_CPU_VARIANT         := krait
TARGET_ARCH                := arm
TARGET_ARCH_VARIANT        := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true
BOARD_USES_QCOM_HARDWARE   := true

# Assert
TARGET_OTA_ASSERT_DEVICE := aries

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := MSM8960
TARGET_BOOTLOADER_NAME       := aries
TARGET_NO_BOOTLOADER         := true
TARGET_NO_RADIOIMAGE         := true

# Flags
BOARD_GLOBAL_CFLAGS += -mfpu=neon -mfloat-abi=softfp -DQCOM_HARDWARE
BOARD_GLOBAL_CPPFLAGS += -mfpu=neon -mfloat-abi=softfp -DQCOM_HARDWARE
BOARD_GLOBAL_CFLAGS += -D__ARM_USE_PLD -D__ARM_CACHE_LINE_SIZE=64

# Krait optimizations
TARGET_USE_QCOM_BIONIC_OPTIMIZATION  := true
TARGET_USE_KRAIT_PLD_SET             := true
TARGET_KRAIT_BIONIC_PLDOFFS          := 10
TARGET_KRAIT_BIONIC_PLDTHRESH        := 10
TARGET_KRAIT_BIONIC_BBTHRESH         := 64
TARGET_KRAIT_BIONIC_PLDSIZE          := 64

# Platform
QCOM_BOARD_PLATFORMS                    := msm8960
TARGET_BOARD_PLATFORM                   := msm8960

# Audio
USE_CUSTOM_AUDIO_POLICY := 1
AUDIO_FEATURE_ENABLED_PROXY_DEVICE      := true
BOARD_USES_ALSA_AUDIO                   := true
BOARD_HAVE_CSD_FAST_CALL_SWITCH         := true
BOARD_USES_LEGACY_ALSA_AUDIO            := false
BOARD_HAVE_NEW_QCOM_CSDCLIENT           := true
QCOM_ANC_HEADSET_ENABLED                := true
QCOM_FLUENCE_ENABLED                    := true
QCOM_MULTI_VOICE_SESSION_ENABLED        := true
QCOM_PROXY_DEVICE_ENABLED               := true
TARGET_USES_QCOM_MM_AUDIO               := true
TARGET_USES_QCOM_COMPRESSED_AUDIO       := true

# Binder API version
TARGET_USES_64_BIT_BINDER := true

# Bionic
TARGET_LD_SHIM_LIBS := \
    /vendor/bin/mpdecision|libshims_atomic.so

# Bluetooth
BLUETOOTH_HCI_USE_MCT                       := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(DEVICE_PATH)/bluetooth
BOARD_HAVE_BLUETOOTH                        := true
BOARD_HAVE_BLUETOOTH_QCOM                   := true

# Camera
TARGET_HAS_LEGACY_CAMERA_HAL1 := true
BOARD_GLOBAL_CFLAGS                         += -DQCOM_BSP
USE_DEVICE_SPECIFIC_CAMERA                  := true
USE_DEVICE_SPECIFIC_QCOM_PROPRIETARY        := true
TARGET_NEEDS_PLATFORM_TEXT_RELOCATIONS      := true
TARGET_PROCESS_SDK_VERSION_OVERRIDE := \
    /system/bin/cameraserver=22 \
    /system/bin/mediaserver=22 \
    /system/vendor/bin/mm-qcamera-daemon=22

# Dexpreopt
ifeq ($(HOST_OS),linux)
  ifneq ($(TARGET_BUILD_VARIANT),eng)
    ifeq ($(WITH_DEXPREOPT),)
      WITH_DEXPREOPT := true
      WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY := true
    endif
  endif
endif

# MK Hardware
JAVA_SOURCE_OVERLAYS := org.mokee.hardware|$(DEVICE_PATH)/mkhw|**/*.java

# Display
BOARD_USES_LEGACY_MMAP          := true
HAVE_ADRENO_SOURCE              := false
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3
OVERRIDE_RS_DRIVER              := libRSDriver_adreno.so
USE_OPENGL_RENDERER             := true
TARGET_ADDITIONAL_GRALLOC_10_USAGE_BITS := 0x02000000U
TARGET_USES_C2D_COMPOSITION     := true
TARGET_USES_ION                 := true

# Extended filesystem support
TARGET_EXFAT_DRIVER := sdfat

# Filesystem
TARGET_FS_CONFIG_GEN := $(DEVICE_PATH)/config.fs

# FM
BOARD_HAVE_QCOM_FM              := true
QCOM_FM_ENABLED                 := true
TARGET_FM_LEGACY_PATCHLOADER    := true

# Fonts
EXCLUDE_SERIF_FONTS          := true
USE_REDUCED_CJK_FONT_WEIGHTS := true

# GPS
BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE := $(TARGET_BOARD_PLATFORM)
BOARD_VENDOR_QCOM_LOC_PDK_FEATURE_SET  := true
TARGET_NO_RPC                          := true
TARGET_PROVIDES_GPS_LOC_API            := true

# HIDL
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/manifest.xml
DEVICE_MATRIX_FILE   := $(DEVICE_PATH)/compatibility_matrix.xml

# Init
TARGET_INIT_VENDOR_LIB := libinit_msm8960
TARGET_LIBINIT_DEFINES_FILE := $(DEVICE_PATH)/init/init_msm8960.cpp

# Lights
TARGET_PROVIDES_LIBLIGHT := true

# Kernel
BOARD_KERNEL_BASE                    := 0x80200000
BOARD_KERNEL_IMAGE_NAME              := zImage
BOARD_KERNEL_PAGESIZE                := 2048
BOARD_RAMDISK_OFFSET                 := 0x02000000
BOARD_MKBOOTIMG_ARGS                 := --ramdisk_offset 0x02000000
BOARD_KERNEL_CMDLINE                 := console=null androidboot.hardware=qcom ehci-hcd.park=3 maxcpus=2 androidboot.bootdevice=msm_sdcc.1 androidboot.selinux=permissive
TARGET_KERNEL_ARCH                   := arm
TARGET_KERNEL_SOURCE                 := kernel/xiaomi/aries
TARGET_KERNEL_CONFIG                 := aries-perf-user_defconfig
TARGET_KERNEL_CROSS_COMPILE_PREFIX   := arm-linux-androideabi-

# Malloc
MALLOC_SVELTE := true

# Partitions
TARGET_USERIMAGES_USE_EXT4         := true
TARGET_USERIMAGES_USE_F2FS         := true
BOARD_BOOTIMAGE_PARTITION_SIZE     := 0x01E00000 # 44M
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 0x00F00000 # 22M
BOARD_SYSTEMIMAGE_PARTITION_SIZE   := 1073741824
BOARD_USERDATAIMAGE_PARTITION_SIZE := 536870912
BOARD_PERSISTIMAGE_PARTITION_SIZE  := 8388608
BOARD_CACHEIMAGE_PARTITION_SIZE    := 402653184
BOARD_FLASH_BLOCK_SIZE             := 131072 # (BOARD_KERNEL_PAGESIZE * 64)

# Power
TARGET_HAS_LEGACY_POWER_STATS := true
TARGET_HAS_NO_WLAN_STATS      := true
TARGET_USES_INTERACTION_BOOST := true

# QCOM enhanced A/V
TARGET_ENABLE_QC_AV_ENHANCEMENTS := true

# QCOM Hardware
BOARD_USES_QCOM_HARDWARE   := true
TARGET_USES_QCOM_BSP       := true

# Radio
TARGET_RIL_VARIANT := caf
BOARD_RIL_NO_CELLINFOLIST := true
TARGET_USES_OLD_MNC_FORMAT := true

# Recovery
TARGET_RECOVERY_FSTAB            := $(DEVICE_PATH)/rootdir/root/fstab.qcom
RECOVERY_FSTAB_VERSION           := 2
TARGET_RECOVERY_PIXEL_FORMAT     := "RGBX_8888"
BOARD_HAS_NO_SELECT_BUTTON       := true

# Remove secdiscard command
TARGET_REMOVE_SECDISCARD_COMMAND := true

# Sepolicy
include device/qcom/sepolicy-legacy/sepolicy.mk

# Security patch level
VENDOR_SECURITY_PATCH := 2016-01-01

# Use mke2fs to create ext4 images
TARGET_USES_MKE2FS := true

# Wifi
BOARD_HAS_QCOM_WLAN              := true
BOARD_WLAN_DEVICE                := qcwcn
BOARD_WPA_SUPPLICANT_DRIVER      := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_HOSTAPD_DRIVER             := NL80211
BOARD_HOSTAPD_PRIVATE_LIB        := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
PRODUCT_VENDOR_MOVE_ENABLED      := true
TARGET_PROVIDES_WCNSS_QMI        := true
TARGET_USES_WCNSS_CTRL           := true
TARGET_USES_QCOM_WCNSS_QMI       := true
TARGET_WCNSS_MAC_PREFIX          := e8bba8
WIFI_DRIVER_FW_PATH_AP           := "ap"
WIFI_DRIVER_FW_PATH_STA          := "sta"
WPA_SUPPLICANT_VERSION           := VER_0_8_X

-include vendor/xiaomi/aries/BoardConfigVendor.mk
