//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/cros/daemon_chromeos.h"

#include <sysexits.h>

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>

#include "update_engine/cros/real_system_state.h"

using brillo::Daemon;
using std::unique_ptr;

namespace chromeos_update_engine {

unique_ptr<DaemonBase> DaemonBase::CreateInstance() {
  return std::make_unique<DaemonChromeOS>();
}

int DaemonChromeOS::OnInit() {
  // Register the |subprocess_| singleton with this Daemon as the signal
  // handler.
  subprocess_.Init(this);

  int exit_code = Daemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  // Initialize update engine global state.
  // TODO(deymo): Move the initialization to a factory method avoiding the
  // explicit re-usage of the |bus| instance, shared between D-Bus service and
  // D-Bus client calls.
  RealSystemState::SetInstance(&system_state_);

  // Create the DBus service.
  dbus_adaptor_.reset(new UpdateEngineAdaptor());
  SystemState::Get()->update_attempter()->AddObserver(dbus_adaptor_.get());

  dbus_adaptor_->RegisterAsync(
      base::Bind(&DaemonChromeOS::OnDBusRegistered, base::Unretained(this)));
  LOG(INFO) << "Waiting for DBus object to be registered.";
  return EX_OK;
}

void DaemonChromeOS::OnDBusRegistered(bool succeeded) {
  if (!succeeded) {
    LOG(ERROR) << "Registering the UpdateEngineAdaptor";
    QuitWithExitCode(1);
    return;
  }

  // Take ownership of the service now that everything is initialized. We need
  // to this now and not before to avoid exposing a well known DBus service
  // path that doesn't have the service it is supposed to implement.
  if (!dbus_adaptor_->RequestOwnership()) {
    LOG(ERROR) << "Unable to take ownership of the DBus service, is there "
               << "other update_engine daemon running?";
    QuitWithExitCode(1);
    return;
  }

  // Need to wait for DBus ownership as bootstraping requires requesting
  // `cros_healthd` DBus API calls.
  auto* cros_healthd = SystemState::Get()->cros_healthd();
  if (!cros_healthd->Init()) {
    LOG(ERROR) << "Failed to initialize cros_healthd connection.";
    SystemState::Get()->update_attempter()->StartUpdater();
  } else {
    LOG(INFO) << "Requesting telemetry info from cros_healthd.";
    // Update the telemetry information before starting the updater, to request
    // once and continue caching on boot.
    cros_healthd->ProbeTelemetryInfo(
        {
            TelemetryCategoryEnum::kNonRemovableBlockDevices,
            TelemetryCategoryEnum::kCpu,
            TelemetryCategoryEnum::kMemory,
            TelemetryCategoryEnum::kSystem2,
            TelemetryCategoryEnum::kBus,
        },
        base::BindOnce([](const TelemetryInfo&) {
          SystemState::Get()->update_attempter()->StartUpdater();
        }));
  }
}

}  // namespace chromeos_update_engine
