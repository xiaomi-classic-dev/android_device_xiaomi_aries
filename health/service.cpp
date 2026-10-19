// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#define LOG_TAG "android.hardware.health@2.1-service.aries"

#include <memory>
#include <string>
#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <health/utils.h>
#include <health2impl/BinderHealth.h>
#include <health2impl/Health.h>

using android::hardware::health::V2_1::HealthInfo;
using android::hardware::health::V2_1::implementation::BinderHealth;
using android::hardware::health::V2_1::implementation::Health;

class AriesHealth : public Health {
  public:
    explicit AriesHealth(std::unique_ptr<healthd_config>&& config)
        : Health(std::move(config)) {}

  protected:
    void UpdateHealthInfo(HealthInfo* info) override {
        // pm8921_charger returns usb_ma_table values in mA for CURRENT_MAX.
        // The framework's maxChargingCurrent field requires microamps.
        std::string value;
        int milliamps;
        auto& battery = info->legacy.legacy;
        if ((battery.chargerAcOnline || battery.chargerUsbOnline)
                && android::base::ReadFileToString(
                        "/sys/class/power_supply/usb/current_max", &value)
                && android::base::ParseInt(android::base::Trim(value), &milliamps)
                && milliamps > 0 && milliamps <= 3000
                && battery.maxChargingCurrent == milliamps) {
            battery.maxChargingCurrent = milliamps * 1000;
        }
    }
};

int main() {
    auto config = std::make_unique<healthd_config>();
    android::hardware::health::InitHealthdConfig(config.get());
    android::sp<AriesHealth> impl = new AriesHealth(std::move(config));
    android::sp<BinderHealth> service = new BinderHealth("default", impl);
    return service->StartLoop();
}
