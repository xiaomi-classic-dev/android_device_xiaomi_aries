// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#include <aidl/android/hardware/power/BnPower.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unistd.h>

using namespace aidl::android::hardware::power;
using ndk::ScopedAStatus;
constexpr char kPulse[] = "/sys/devices/system/cpu/cpufreq/interactive/boostpulse";
constexpr char kDuration[] = "/sys/devices/system/cpu/cpufreq/interactive/boostpulse_duration";
constexpr char kBoost[] = "/sys/devices/system/cpu/cpufreq/interactive/boost";
class AriesPower final : public BnPower {
    std::mutex lock;
    bool interactive = true;
    uint64_t pulses = 0;
    int lastDuration = 0;
    ScopedAStatus pulseLocked(int ms) {
        if (!interactive) return ScopedAStatus::ok();
        ms = std::clamp(ms, 1, 1000);
        if (!android::base::WriteStringToFile(std::to_string(ms * 1000), kDuration)
                || !android::base::WriteStringToFile("1", kPulse)) {
            PLOG(ERROR) << "interactive boostpulse failed";
            return ScopedAStatus::fromServiceSpecificError(1);
        }
        lastDuration = ms;
        ++pulses;
        return ScopedAStatus::ok();
    }
public:
    ScopedAStatus setMode(Mode type, bool enabled) override {
        std::lock_guard<std::mutex> guard(lock);
        if (type == Mode::INTERACTIVE) {
            interactive = enabled;
            if (!enabled && !android::base::WriteStringToFile("0", kBoost)) {
                return ScopedAStatus::fromServiceSpecificError(1);
            }
        } else if (type == Mode::LAUNCH && enabled) return pulseLocked(500);
        return ScopedAStatus::ok();
    }
    ScopedAStatus isModeSupported(Mode type, bool* supported) override {
        *supported = type == Mode::INTERACTIVE || type == Mode::LAUNCH;
        return ScopedAStatus::ok();
    }
    ScopedAStatus setBoost(Boost type, int32_t durationMs) override {
        std::lock_guard<std::mutex> guard(lock);
        if (type != Boost::INTERACTION) return ScopedAStatus::ok();
        if (durationMs < 0) {
            if (!android::base::WriteStringToFile("0", kBoost))
                return ScopedAStatus::fromServiceSpecificError(1);
            return ScopedAStatus::ok();
        }
        return pulseLocked(durationMs == 0 ? 80 : durationMs);
    }
    ScopedAStatus isBoostSupported(Boost type, bool* supported) override {
        *supported = type == Boost::INTERACTION && access(kPulse, W_OK) == 0
                && access(kDuration, W_OK) == 0;
        return ScopedAStatus::ok();
    }
    ScopedAStatus createHintSession(int32_t, int32_t, const std::vector<int32_t>&, int64_t,
            std::shared_ptr<IPowerHintSession>* session) override {
        *session = nullptr;
        return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    ScopedAStatus getHintSessionPreferredRate(int64_t* rate) override {
        *rate = 0;
        return ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    binder_status_t dump(int fd, const char**, uint32_t) override {
        std::lock_guard<std::mutex> guard(lock);
        dprintf(fd, "Aries interactive governor Power HAL\ninteractive=%d\npulse_count=%llu\nlast_duration_ms=%d\n",
                interactive, static_cast<unsigned long long>(pulses), lastDuration);
        return STATUS_OK;
    }
};
int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger());
    ABinderProcess_setThreadPoolMaxThreadCount(1);
    auto service = ndk::SharedRefBase::make<AriesPower>();
    std::string name = std::string(IPower::descriptor) + "/default";
    if (AServiceManager_addService(service->asBinder().get(), name.c_str()) != STATUS_OK) return 1;
    ABinderProcess_joinThreadPool();
    return 1;
}
