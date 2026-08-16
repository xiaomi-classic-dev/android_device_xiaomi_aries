/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#include <cerrno>

#include "KeyDisabler.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

constexpr const char kControlPath[] =
    "/proc/touchscreen/disable_keys";

KeyDisabler::KeyDisabler() {
    mHasKeyDisabler = !access(kControlPath, F_OK);
}

ndk::ScopedAStatus KeyDisabler::getEnabled(bool* _aidl_return) {
    std::string buf;

    if (!mHasKeyDisabler) {
        *_aidl_return = false;
        return ndk::ScopedAStatus::ok();
    }

    if (!android::base::ReadFileToString(kControlPath, &buf)) {
        LOG(ERROR) << "Failed to read " << kControlPath;
        return ndk::ScopedAStatus::fromServiceSpecificError(errno);
    }

    *_aidl_return = std::stoi(android::base::Trim(buf)) == 0;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus KeyDisabler::setEnabled(bool enabled) {
    if (!mHasKeyDisabler) return ndk::ScopedAStatus::ok();

    if (!android::base::WriteStringToFile((enabled ? "1" : "0"), kControlPath)) {
        LOG(ERROR) << "Failed to write " << kControlPath;
        return ndk::ScopedAStatus::fromServiceSpecificError(errno);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
