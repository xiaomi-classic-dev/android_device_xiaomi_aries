/*
 * Copyright (C) 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.gnss@1.0-service.aries"

#include <android/hardware/gnss/1.0/IGnss.h>
#include <binder/ProcessState.h>
#include <hidl/LegacySupport.h>

int main() {
    // libnetmgr binds NETLINK_GENERIC to getpid(). Loading this HAL inside
    // system_server collides with its other netlink users and aborts boot.
    // The legacy GPS wakelock client uses SystemSuspend on the system binder.
    android::ProcessState::initWithDriver("/dev/binder");
    android::ProcessState::self()->startThreadPool();
    return android::hardware::defaultPassthroughServiceImplementation<
            android::hardware::gnss::V1_0::IGnss>();
}
