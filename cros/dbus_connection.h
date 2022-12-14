//
// Copyright (C) 2016 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_CROS_DBUS_CONNECTION_H_
#define UPDATE_ENGINE_CROS_DBUS_CONNECTION_H_

#include <base/memory/ref_counted.h>
#include <brillo/dbus/dbus_connection.h>
#include <dbus/bus.h>

namespace chromeos_update_engine {

class DBusConnection {
 public:
  DBusConnection();
  DBusConnection(const DBusConnection&) = delete;
  DBusConnection& operator=(const DBusConnection&) = delete;

  const scoped_refptr<dbus::Bus>& GetDBus();

  static DBusConnection* Get();

 private:
  scoped_refptr<dbus::Bus> bus_;

  brillo::DBusConnection dbus_connection_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CROS_DBUS_CONNECTION_H_
