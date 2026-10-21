#ifndef KSV_PROFILE_IMPORT_H
#define KSV_PROFILE_IMPORT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "domain/user_profile.h"

namespace ksv::application {
    enum class ProfileImportMode { Refresh, Rebuild };
    enum class ProfileImportReason { Startup, Manual, Live, SourceSettingsChanged };
    enum class ProfileImportPhase { Idle, Scanning, Importing, Saving };
    enum class ProfileImportOutcome { None, Succeeded, Partial, Failed, Stale };

    struct ProfileImportIssue {
        std::string path;
        std::string message;
        bool retryable = false;
    };
    struct ProfileImportSummary {
        std::size_t added = 0;
        std::size_t enriched = 0;
        std::vector<ProfileImportIssue> issues;
        bool save_failed = false;
    };
    struct ProfileImportToken {
        std::uint64_t generation = 0;
        std::uint64_t request_id = 0;
        bool operator==(const ProfileImportToken &) const = default;
    };
    struct ProfileSourceAlias {
        std::string path;
        std::string root;
        std::string filename;
        domain::ScenarioRunId run_id;
    };
    struct ProfileImportRequest {
        ProfileImportToken token;
        ProfileImportMode mode = ProfileImportMode::Refresh;
        ProfileImportReason reason = ProfileImportReason::Manual;
        std::vector<std::string> roots;
        std::shared_ptr<const domain::UserProfile> baseline;
        std::vector<ProfileSourceAlias> known_aliases;
    };
    struct ProfileImportResult {
        ProfileImportToken token;
        std::shared_ptr<const domain::UserProfile> candidate;
        ProfileImportSummary summary;
        bool changed = false;
        bool failed = false;
        std::vector<ProfileSourceAlias> successful_aliases;
        std::vector<std::pair<domain::ScenarioRunId, domain::ScenarioRunId>> rekeyed_runs;
    };
    struct ProfileImportState {
        ProfileImportMode mode = ProfileImportMode::Refresh;
        ProfileImportPhase phase = ProfileImportPhase::Idle;
        ProfileImportOutcome outcome = ProfileImportOutcome::None;
        std::size_t done = 0;
        std::size_t total = 0;
        bool busy = false;
        ProfileImportSummary summary;
    };
    using ProfileImportProgress = std::function<void(ProfileImportPhase, std::size_t, std::size_t)>;
}
#endif
