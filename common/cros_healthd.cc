//
// Copyright (C) 2021 The Android Open Source Project
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

#include "update_engine/common/cros_healthd.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <base/check.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/dbus/file_descriptor.h>
#include <dbus/bus.h>
#include <dbus/cros_healthd/dbus-constants.h>
#include <dbus/message.h>
#include <mojo/public/cpp/platform/platform_channel.h>
#include <mojo/public/cpp/system/invitation.h>

#include "update_engine/cros/dbus_connection.h"

using chromeos::cros_healthd::mojom::ProbeCategoryEnum;

namespace chromeos_update_engine {

namespace {

#define SET_MOJO_VALUE(x) \
  { TelemetryCategoryEnum::x, ProbeCategoryEnum::x }
static const std::unordered_map<TelemetryCategoryEnum, ProbeCategoryEnum>
    kTelemetryMojoMapping{
        SET_MOJO_VALUE(kBattery),
        SET_MOJO_VALUE(kNonRemovableBlockDevices),
        SET_MOJO_VALUE(kCpu),
        SET_MOJO_VALUE(kTimezone),
        SET_MOJO_VALUE(kMemory),
        SET_MOJO_VALUE(kBacklight),
        SET_MOJO_VALUE(kFan),
        SET_MOJO_VALUE(kStatefulPartition),
        SET_MOJO_VALUE(kBluetooth),
        SET_MOJO_VALUE(kSystem),
        SET_MOJO_VALUE(kSystem2),
        SET_MOJO_VALUE(kNetwork),
        SET_MOJO_VALUE(kAudio),
        SET_MOJO_VALUE(kBootPerformance),
        SET_MOJO_VALUE(kBus),
    };

void PrintError(const chromeos::cros_healthd::mojom::ProbeErrorPtr& error,
                std::string info) {
  LOG(ERROR) << "Failed to get " << info << ","
             << " error_type=" << error->type << " error_msg=" << error->msg;
}

}  // namespace

std::unique_ptr<CrosHealthdInterface> CreateCrosHealthd() {
  return std::make_unique<CrosHealthd>();
}

bool CrosHealthd::Init() {
  mojo::core::Init();
  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      base::ThreadTaskRunnerHandle::Get() /* io_thread_task_runner */,
      mojo::core::ScopedIPCSupport::ShutdownPolicy::
          CLEAN /* blocking shutdown */);
  return BootstrapMojo();
}

TelemetryInfo* const CrosHealthd::GetTelemetryInfo() {
  return telemetry_info_.get();
}

void CrosHealthd::ProbeTelemetryInfo(
    const std::unordered_set<TelemetryCategoryEnum>& categories,
    ProbeTelemetryInfoCallback once_callback) {
  std::vector<ProbeCategoryEnum> categories_mojo;
  for (const auto& category : categories) {
    auto it = kTelemetryMojoMapping.find(category);
    if (it != kTelemetryMojoMapping.end())
      categories_mojo.push_back(it->second);
  }
  cros_healthd_service_factory_->GetProbeService(
      cros_healthd_probe_service_.BindNewPipeAndPassReceiver());
  cros_healthd_probe_service_->ProbeTelemetryInfo(
      categories_mojo,
      base::BindOnce(&CrosHealthd::OnProbeTelemetryInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(once_callback)));
}

dbus::ObjectProxy* CrosHealthd::GetCrosHealthdObjectProxy() {
  return DBusConnection::Get()->GetDBus()->GetObjectProxy(
      diagnostics::kCrosHealthdServiceName,
      dbus::ObjectPath(diagnostics::kCrosHealthdServicePath));
}

bool CrosHealthd::BootstrapMojo() {
  mojo::PlatformChannel channel;
  brillo::dbus_utils::FileDescriptor fd(
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD().release());
  brillo::ErrorPtr error;
  auto response = brillo::dbus_utils::CallMethodAndBlock(
      GetCrosHealthdObjectProxy(),
      diagnostics::kCrosHealthdServiceInterface,
      diagnostics::kCrosHealthdBootstrapMojoConnectionMethod,
      &error,
      fd,
      /*is_chrome=*/false);
  if (!response) {
    LOG(ERROR) << "Failed to bootstrap mojo connection with cros_healthd.";
    return false;
  }

  std::string token;
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(&token)) {
    LOG(ERROR) << "Failed to get token from cros_healthd DBus response.";
    return false;
  }

  mojo::IncomingInvitation invitation =
      mojo::IncomingInvitation::Accept(channel.TakeLocalEndpoint());
  auto opt_pending_service_factory = mojo::PendingRemote<
      chromeos::cros_healthd::mojom::CrosHealthdServiceFactory>(
      invitation.ExtractMessagePipe(token), 0u /* version */);
  if (!opt_pending_service_factory) {
    LOG(ERROR) << "Failed to create pending service factory for cros_healthd.";
    return false;
  }
  cros_healthd_service_factory_.Bind(std::move(opt_pending_service_factory));

  return true;
}

void CrosHealthd::OnProbeTelemetryInfo(
    ProbeTelemetryInfoCallback once_callback,
    chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  if (!result) {
    LOG(WARNING) << "Failed to parse telemetry information.";
    std::move(once_callback).Run({});
    return;
  }
  if (!ParseSystemResultV2(&result, telemetry_info_.get()))
    LOG(WARNING) << "Failed to parse system_v2 information.";
  if (!ParseMemoryResult(&result, telemetry_info_.get()))
    LOG(WARNING) << "Failed to parse memory information.";
  if (!ParseNonRemovableBlockDeviceResult(&result, telemetry_info_.get()))
    LOG(WARNING) << "Failed to parse non-removable block device information.";
  if (!ParseCpuResult(&result, telemetry_info_.get()))
    LOG(WARNING) << "Failed to parse physical CPU information.";
  std::move(once_callback).Run(*telemetry_info_);
}

bool CrosHealthd::ParseSystemResultV2(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
    TelemetryInfo* telemetry_info) {
  const auto& system_result_v2 = (*result)->system_result_v2;
  if (system_result_v2) {
    if (system_result_v2->is_error()) {
      PrintError(system_result_v2->get_error(), "system_v2 information");
      return false;
    }
    const auto& system_info_v2 = system_result_v2->get_system_info_v2();

    const auto& dmi_info = system_info_v2->dmi_info;
    if (dmi_info) {
      if (dmi_info->board_vendor.has_value())
        telemetry_info->system_v2_info.dmi_info.board_vendor =
            dmi_info->board_vendor.value();
      if (dmi_info->board_name.has_value())
        telemetry_info->system_v2_info.dmi_info.board_name =
            dmi_info->board_name.value();
      if (dmi_info->board_version.has_value())
        telemetry_info->system_v2_info.dmi_info.board_version =
            dmi_info->board_version.value();
      if (dmi_info->bios_version.has_value())
        telemetry_info->system_v2_info.dmi_info.bios_version =
            dmi_info->bios_version.value();
    }

    const auto& os_info = system_info_v2->os_info;
    if (os_info) {
      telemetry_info->system_v2_info.os_info.boot_mode =
          static_cast<TelemetryInfo::SystemV2Info::OsInfo::BootMode>(
              os_info->boot_mode);
    }
  }
  return true;
}

bool CrosHealthd::ParseMemoryResult(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
    TelemetryInfo* telemetry_info) {
  const auto& memory_result = (*result)->memory_result;
  if (memory_result) {
    if (memory_result->is_error()) {
      PrintError(memory_result->get_error(), "memory information");
      return false;
    }
    const auto& memory_info = memory_result->get_memory_info();
    if (memory_info) {
      telemetry_info->memory_info.total_memory_kib =
          memory_info->total_memory_kib;
    }
  }
  return true;
}

bool CrosHealthd::ParseNonRemovableBlockDeviceResult(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
    TelemetryInfo* telemetry_info) {
  const auto& non_removable_block_device_result =
      (*result)->block_device_result;
  if (non_removable_block_device_result) {
    if (non_removable_block_device_result->is_error()) {
      PrintError(non_removable_block_device_result->get_error(),
                 "non-removable block device information");
      return false;
    }
    const auto& non_removable_block_device_infos =
        non_removable_block_device_result->get_block_device_info();
    for (const auto& non_removable_block_device_info :
         non_removable_block_device_infos) {
      telemetry_info->block_device_info.push_back({
          .size = non_removable_block_device_info->size,
      });
    }
  }
  return true;
}

bool CrosHealthd::ParseCpuResult(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
    TelemetryInfo* telemetry_info) {
  const auto& cpu_result = (*result)->cpu_result;
  if (cpu_result) {
    if (cpu_result->is_error()) {
      PrintError(cpu_result->get_error(), "CPU information");
      return false;
    }
    const auto& cpu_info = cpu_result->get_cpu_info();
    for (const auto& physical_cpu : cpu_info->physical_cpus) {
      if (physical_cpu->model_name.has_value()) {
        telemetry_info->cpu_info.physical_cpus.push_back({
            .model_name = physical_cpu->model_name.value(),
        });
      }
    }
  }
  return true;
}

bool CrosHealthd::ParseBusResult(
    chromeos::cros_healthd::mojom::TelemetryInfoPtr* result,
    TelemetryInfo* telemetry_info) {
  const auto& bus_result = (*result)->bus_result;
  if (bus_result) {
    if (bus_result->is_error()) {
      PrintError(bus_result->get_error(), "bus information");
      return false;
    }
    const auto& bus_devices = bus_result->get_bus_devices();
    for (const auto& bus_device : bus_devices) {
      switch (bus_device->bus_info->which()) {
        case chromeos::cros_healthd::mojom::BusInfo::Tag::PCI_BUS_INFO: {
          const auto& pci_bus_info = bus_device->bus_info->get_pci_bus_info();
          telemetry_info->bus_devices.push_back({
              .device_class =
                  static_cast<TelemetryInfo::BusDevice::BusDeviceClass>(
                      bus_device->device_class),
              .bus_type_info =
                  TelemetryInfo::BusDevice::PciBusInfo{
                      .vendor_id = pci_bus_info->vendor_id,
                      .device_id = pci_bus_info->device_id,
                      .driver = pci_bus_info->driver.has_value()
                                    ? pci_bus_info->driver.value()
                                    : "",
                  },
          });
          break;
        }
        case chromeos::cros_healthd::mojom::BusInfo::Tag::USB_BUS_INFO: {
          const auto& usb_bus_info = bus_device->bus_info->get_usb_bus_info();
          telemetry_info->bus_devices.push_back({
              .device_class =
                  static_cast<TelemetryInfo::BusDevice::BusDeviceClass>(
                      bus_device->device_class),
              .bus_type_info =
                  TelemetryInfo::BusDevice::UsbBusInfo{
                      .vendor_id = usb_bus_info->vendor_id,
                      .product_id = usb_bus_info->product_id,
                  },
          });
          break;
        }
      }
    }
  }
  return true;
}

}  // namespace chromeos_update_engine
