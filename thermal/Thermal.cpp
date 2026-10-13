// Copyright (C) 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
#include <android/hardware/thermal/2.0/IThermal.h>
#include <android-base/file.h>
#include <android-base/strings.h>
#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace t = android::hardware::thermal::V2_0;
namespace t1 = android::hardware::thermal::V1_0;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::hidl_vec;
using android::sp;
static t1::ThermalStatus status(bool ok, const char* why = "") {
    return {ok ? t1::ThermalStatusCode::SUCCESS : t1::ThermalStatusCode::FAILURE, why};
}
constexpr size_t kCpuSensors = 4;
constexpr size_t kSensors = kCpuSensors + 2;
struct Sensor {t::TemperatureType type; std::string name, path; float scale;};
class AriesThermal final : public t::IThermal {
    struct Client {
        sp<t::IThermalChangedCallback> callback;
        bool filter;
        t::TemperatureType type;
        std::array<int, kSensors> last{{-1, -1, -1, -1, -1, -1}};
    };
    std::array<Sensor, kSensors> sensors{{
        {t::TemperatureType::CPU, "cpu-thermal", "", 1.0f},
        {t::TemperatureType::CPU, "cpu-thermal-8", "", 1.0f},
        {t::TemperatureType::CPU, "cpu-thermal-9", "", 1.0f},
        {t::TemperatureType::CPU, "cpu-thermal-10", "", 1.0f},
        {t::TemperatureType::UNKNOWN, "pmic", "", 1000.0f},
        {t::TemperatureType::BATTERY, "battery", "/sys/class/power_supply/battery/temp", 10.0f},
    }};
    std::mutex mutex;
    mutable std::mutex sensorMutex;
    std::vector<Client> clients;
    std::atomic<bool> stopping{false};
    mutable std::array<int, kCpuSensors> cpuSeverity{{0, 0, 0, 0}};
    std::thread worker;
    std::vector<t::Temperature> temperatures() const {
        std::lock_guard<std::mutex> guard(sensorMutex);
        std::vector<t::Temperature> result;
        for (size_t sensor = 0; sensor < sensors.size(); ++sensor) {
            const auto& s = sensors[sensor];
            std::string text;
            float value = NAN;
            bool available = !s.path.empty() && android::base::ReadFileToString(s.path, &text);
            if (!available && s.type == t::TemperatureType::CPU && !s.path.empty()) {
                // Legacy thermald reconfigures the main TSENS sensor during
                // startup. The 3.4 driver then disables the subordinate sensors.
                // Re-enable sensing for each CPU channel we read; leave all trip
                // thresholds and thermald's mitigation settings unchanged.
                std::string modePath = s.path.substr(0, s.path.rfind('/')) + "/mode";
                std::string mode;
                if (android::base::ReadFileToString(modePath, &mode)
                        && android::base::Trim(mode) == "disabled"
                        && android::base::WriteStringToFile("enabled", modePath)) {
                    available = android::base::ReadFileToString(s.path, &text);
                }
            }
            if (available) {
                char* end = nullptr;
                float raw = strtof(text.c_str(), &end);
                if (end != text.c_str() && std::isfinite(raw)) value = raw / s.scale;
            }
            auto severity = t::ThrottlingSeverity::NONE;
            if (s.type == t::TemperatureType::CPU && std::isfinite(value)) {
                // Mirror the existing APQ8064 thermald CPU mitigation thresholds.
                const float hot[] = {NAN, 70, 90, 95, 100, 110, 120};
                const float cold[] = {NAN, 67, 85, 90, 95, 105, 115};
                int level = cpuSeverity[sensor];
                while (level > 0 && value <= cold[level]) --level;
                while (level < 6 && value >= hot[level+1]) ++level;
                cpuSeverity[sensor] = level;
                severity = static_cast<t::ThrottlingSeverity>(level);
            }
            result.push_back({s.type, s.name, value, severity});
        }
        return result;
    }
    void poll() {
        while (!stopping) {
            auto current = temperatures();
            std::vector<std::pair<sp<t::IThermalChangedCallback>,t::Temperature>> notifications;
            {
                std::lock_guard<std::mutex> guard(mutex);
                for (auto& c : clients) for (size_t i=0; i<current.size(); ++i) {
                    const auto& temp = current[i];
                    if (!std::isfinite(temp.value) || (c.filter && c.type != temp.type)) continue;
                    int severity = static_cast<int>(temp.throttlingStatus);
                    if (c.last[i] != severity) {
                        c.last[i] = severity;
                        notifications.emplace_back(c.callback,temp);
                    }
                }
            }
            for (auto& event : notifications) {
                auto result = event.first->notifyThrottling(event.second);
                if (!result.isOk()) {
                    std::lock_guard<std::mutex> guard(mutex);
                    clients.erase(std::remove_if(clients.begin(),clients.end(),[&](const Client& c) {
                        return android::hardware::interfacesEqual(c.callback,event.first);
                    }),clients.end());
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
public:
    AriesThermal() {
        for (int i=0; i<64; ++i) {
            std::string base = "/sys/class/thermal/thermal_zone"+std::to_string(i);
            std::string type;
            if (!android::base::ReadFileToString(base+"/type", &type)) continue;
            type = android::base::Trim(type);
            for (size_t cpu = 0; cpu < kCpuSensors; ++cpu)
                if (type == "tsens_tz_sensor" + std::to_string(cpu + 7))
                    sensors[cpu].path = base + "/temp";
            if (type == "pm8921_tz") sensors[kCpuSensors].path = base+"/temp";
        }
    }
    ~AriesThermal() {stopping=true; if(worker.joinable())worker.join();}
    void start() {worker=std::thread([this]{poll();});}
    Return<void> getCurrentTemperatures(bool filter,t::TemperatureType type,getCurrentTemperatures_cb cb) override {
        auto all=temperatures();std::vector<t::Temperature> selected;
        for(const auto& temp:all) if(!filter||type==temp.type)selected.push_back(temp);
        cb(status(true),selected);return Void();
    }
    Return<void> getTemperatureThresholds(bool filter,t::TemperatureType type,getTemperatureThresholds_cb cb) override {
        std::vector<t::TemperatureThreshold> selected;
        for(const auto& s:sensors) {
            if(filter&&type!=s.type)continue;
            t::TemperatureThreshold threshold{};
            threshold.type=s.type;threshold.name=s.name;
            for(size_t i=0;i<7;++i) {
                threshold.hotThrottlingThresholds[i]=NAN;
                threshold.coldThrottlingThresholds[i]=NAN;
            }
            threshold.vrThrottlingThreshold=NAN;
            if(s.type==t::TemperatureType::CPU) {
                const float hot[]={NAN,70,90,95,100,110,120};
                const float cold[]={NAN,67,85,90,95,105,115};
                for(size_t i=0;i<7;++i) {
                    threshold.hotThrottlingThresholds[i]=hot[i];
                    threshold.coldThrottlingThresholds[i]=cold[i];
                }
            }
            selected.push_back(threshold);
        }
        cb(status(true),selected);return Void();
    }
    Return<void> registerThermalChangedCallback(const sp<t::IThermalChangedCallback>& callback,
            bool filter,t::TemperatureType type,registerThermalChangedCallback_cb cb) override {
        if(callback==nullptr){cb(status(false,"null callback"));return Void();}
        bool duplicate=false;
        {std::lock_guard<std::mutex> guard(mutex);
         for(const auto& c:clients)if(android::hardware::interfacesEqual(c.callback,callback))duplicate=true;
         if(!duplicate)clients.push_back({callback,filter,type,{{-1,-1,-1,-1,-1,-1}}});}
        cb(status(!duplicate,duplicate?"already registered":""));return Void();
    }
    Return<void> unregisterThermalChangedCallback(const sp<t::IThermalChangedCallback>& callback,
            unregisterThermalChangedCallback_cb cb) override {
        bool removed=false;
        {std::lock_guard<std::mutex> guard(mutex);auto before=clients.size();
         clients.erase(std::remove_if(clients.begin(),clients.end(),[&](const Client& c){
            return android::hardware::interfacesEqual(c.callback,callback);
         }),clients.end());removed=before!=clients.size();}
        cb(status(removed,removed?"":"not registered"));return Void();
    }
    Return<void> getCurrentCoolingDevices(bool,t::CoolingType,getCurrentCoolingDevices_cb cb) override {
        // This kernel exposes no cooling_device state; do not invent readings.
        cb(status(true),hidl_vec<t::CoolingDevice>{});return Void();
    }
    Return<void> getTemperatures(getTemperatures_cb cb) override {
        std::vector<t1::Temperature> result;
        for(const auto& temp:temperatures())result.push_back({static_cast<t1::TemperatureType>(temp.type),
            temp.name,temp.value,temp.type==t::TemperatureType::CPU?95.0f:NAN,
            temp.type==t::TemperatureType::CPU?120.0f:NAN,NAN});
        cb(status(true),result);return Void();
    }
    Return<void> getCpuUsages(getCpuUsages_cb cb) override {
        cb(status(false,"CPU usage is available from proc/stat"),hidl_vec<t1::CpuUsage>{});return Void();
    }
    Return<void> getCoolingDevices(getCoolingDevices_cb cb) override {
        cb(status(true),hidl_vec<t1::CoolingDevice>{});return Void();
    }
};
int main() {
    android::base::InitLogging(nullptr,android::base::LogdLogger());
    android::hardware::configureRpcThreadpool(4,true);
    sp<AriesThermal> service = new AriesThermal();
    if(service->registerAsService()!=android::OK)return 1;
    service->start();
    android::hardware::joinRpcThreadpool();return 1;
}
