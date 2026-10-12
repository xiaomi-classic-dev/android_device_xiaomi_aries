LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ALL_PREBUILT += $(INSTALLED_KERNEL_TARGET)

# include the non-open-source counterpart to this file
-include vendor/xiaomi/aries/AndroidBoardVendor.mk

$(PRODUCT_OUT)/recovery.img $(PRODUCT_OUT)/ramdisk-recovery.img $(PRODUCT_OUT)/ramdisk-recovery.cpio: $(wildcard device/xiaomi/aries/repartition/*)
