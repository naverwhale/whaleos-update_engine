//
// Copyright (C) 2017 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_CROS_METRICS_REPORTER_OMAHA_H_
#define UPDATE_ENGINE_CROS_METRICS_REPORTER_OMAHA_H_

#include <memory>
#include <string>

#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <metrics/metrics_library.h>

#include "update_engine/certificate_checker.h"
#include "update_engine/common/constants.h"
#include "update_engine/common/error_code.h"
#include "update_engine/common/metrics_constants.h"
#include "update_engine/common/metrics_reporter_interface.h"

namespace chromeos_update_engine {

class SystemState;

namespace metrics {

// UpdateEngine.Daily.* metrics.
extern const char kMetricDailyOSAgeDays[];

// UpdateEngine.Check.* metrics.
extern const char kMetricCheckDownloadErrorCode[];
extern const char kMetricCheckReaction[];
extern const char kMetricCheckResult[];
extern const char kMetricCheckTargetVersion[];
extern const char kMetricCheckRollbackTargetVersion[];
extern const char kMetricCheckTimeSinceLastCheckMinutes[];
extern const char kMetricCheckTimeSinceLastCheckUptimeMinutes[];

// UpdateEngine.Attempt.* metrics.
extern const char kMetricAttemptNumber[];
extern const char kMetricAttemptPayloadType[];
extern const char kMetricAttemptPayloadSizeMiB[];
extern const char kMetricAttemptConnectionType[];
extern const char kMetricAttemptDurationMinutes[];
extern const char kMetricAttemptDurationUptimeMinutes[];
extern const char kMetricAttemptTimeSinceLastAttemptMinutes[];
extern const char kMetricAttemptTimeSinceLastAttemptUptimeMinutes[];
extern const char kMetricAttemptPayloadBytesDownloadedMiB[];
extern const char kMetricAttemptPayloadDownloadSpeedKBps[];
extern const char kMetricAttemptDownloadSource[];
extern const char kMetricAttemptResult[];
extern const char kMetricAttemptInternalErrorCode[];
extern const char kMetricAttemptDownloadErrorCode[];

// UpdateEngine.SuccessfulUpdate.* metrics.
extern const char kMetricSuccessfulUpdateAttemptCount[];
extern const char kMetricSuccessfulUpdateBytesDownloadedMiB[];
extern const char kMetricSuccessfulUpdateDownloadOverheadPercentage[];
extern const char kMetricSuccessfulUpdateDownloadSourcesUsed[];
extern const char kMetricSuccessfulUpdateDurationFromSeenDays[];
extern const char kMetricSuccessfulUpdateDurationFromSeenTimeRestrictedDays[];
extern const char kMetricSuccessfulUpdatePayloadType[];
extern const char kMetricSuccessfulUpdatePayloadSizeMiB[];
extern const char kMetricSuccessfulUpdateRebootCount[];
extern const char kMetricSuccessfulUpdateTotalDurationMinutes[];
extern const char kMetricSuccessfulUpdateTotalDurationUptimeMinutes[];
extern const char kMetricSuccessfulUpdateUpdatesAbandonedCount[];
extern const char kMetricSuccessfulUpdateUrlSwitchCount[];

// UpdateEngine.Rollback.* metric.
extern const char kMetricRollbackResult[];

// UpdateEngine.EnterpriseRollback.* metrics.
extern const char kMetricEnterpriseRollbackFailure[];
extern const char kMetricEnterpriseRollbackSuccess[];
extern const char kMetricEnterpriseRollbackBlockedByFSI[];

// UpdateEngine.CertificateCheck.* metrics.
extern const char kMetricCertificateCheckUpdateCheck[];
extern const char kMetricCertificateCheckDownload[];

// UpdateEngine.KernelKey.* metrics.
extern const char kMetricKernelMinVersion[];
extern const char kMetricKernelMaxRollforwardVersion[];
extern const char kMetricKernelMaxRollforwardSetSuccess[];

// UpdateEngine.* metrics.
extern const char kMetricEnterpriseUpdateInvalidatedResult[];
extern const char kMetricFailedUpdateCount[];
extern const char kMetricInstallDateProvisioningSource[];
extern const char kMetricInvalidatedUpdate[];
extern const char kMetricTimeToRebootMinutes[];

// UpdateEngine.ConsecutiveUpdate.* metrics.
extern const char kMetricConsecutiveUpdateCount[];
extern const char kMetricConsecutiveUpdateFailed[];

}  // namespace metrics

class MetricsReporterOmaha : public MetricsReporterInterface {
 public:
  MetricsReporterOmaha();
  MetricsReporterOmaha(const MetricsReporterOmaha&) = delete;
  MetricsReporterOmaha& operator=(const MetricsReporterOmaha&) = delete;

  ~MetricsReporterOmaha() override = default;

  void ReportRollbackMetrics(metrics::RollbackResult result) override;

  void ReportEnterpriseRollbackMetrics(
      const std::string& metric, const std::string& rollback_version) override;

  void ReportDailyMetrics(base::TimeDelta os_age) override;

  void ReportUpdateCheckMetrics(
      metrics::CheckResult result,
      metrics::CheckReaction reaction,
      metrics::DownloadErrorCode download_error_code) override;

  void ReportUpdateAttemptMetrics(int attempt_number,
                                  PayloadType payload_type,
                                  base::TimeDelta duration,
                                  base::TimeDelta duration_uptime,
                                  int64_t payload_size,
                                  metrics::AttemptResult attempt_result,
                                  ErrorCode internal_error_code) override;

  void ReportUpdateAttemptDownloadMetrics(
      int64_t payload_bytes_downloaded,
      int64_t payload_download_speed_bps,
      DownloadSource download_source,
      metrics::DownloadErrorCode payload_download_error_code,
      metrics::ConnectionType connection_type) override;

  void ReportAbnormallyTerminatedUpdateAttemptMetrics() override;

  void ReportSuccessfulUpdateMetrics(
      int attempt_count,
      int updates_abandoned_count,
      PayloadType payload_type,
      int64_t payload_size,
      int64_t num_bytes_downloaded[kNumDownloadSources],
      int download_overhead_percentage,
      base::TimeDelta total_duration,
      base::TimeDelta total_duration_uptime,
      int reboot_count,
      int url_switch_count) override;

  void ReportCertificateCheckMetrics(ServerToCheck server_to_check,
                                     CertificateCheckResult result) override;

  void ReportFailedUpdateCount(int target_attempt) override;

  void ReportTimeToReboot(int time_to_reboot_minutes) override;

  void ReportInvalidatedUpdate(bool success) override;

  void ReportEnterpriseUpdateInvalidatedResult(bool success) override;

  void ReportInstallDateProvisioningSource(int source, int max) override;

  void ReportInternalErrorCode(ErrorCode error_code) override;

  void ReportEnterpriseUpdateSeenToDownloadDays(
      bool has_time_restriction_policy, int time_to_update_days) override;

  void ReportConsecutiveUpdateCount(int count) override;

  void ReportFailedConsecutiveUpdate() override;

 private:
  friend class MetricsReporterOmahaTest;
  FRIEND_TEST(MetricsReporterOmahaTest, WallclockDurationHelper);
  FRIEND_TEST(MetricsReporterOmahaTest, MonotonicDurationHelper);

  // This function returns the duration on the wallclock since the last
  // time it was called for the same |state_variable_key| value.
  //
  // If the function returns |true|, the duration (always non-negative)
  // is returned in |out_duration|. If the function returns |false|
  // something went wrong or there was no previous measurement.
  bool WallclockDurationHelper(const std::string& state_variable_key,
                               base::TimeDelta* out_duration);

  // This function returns the duration on the monotonic clock since the
  // last time it was called for the same |storage| pointer.
  //
  // You should pass a pointer to a 64-bit integer in |storage| which
  // should be initialized to 0.
  //
  // If the function returns |true|, the duration (always non-negative)
  // is returned in |out_duration|. If the function returns |false|
  // something went wrong or there was no previous measurement.
  bool MonotonicDurationHelper(int64_t* storage, base::TimeDelta* out_duration);

  std::unique_ptr<MetricsLibraryInterface> metrics_lib_;
};  // class metrics

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CROS_METRICS_REPORTER_OMAHA_H_
