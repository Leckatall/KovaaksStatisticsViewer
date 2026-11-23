#include "benchmarks_service.h"

#include <algorithm>
#include <utility>

#include "benchmarks/benchmark_validation.h"
#include "files/file_naming.h"

namespace ksv::data {
    namespace {
        std::string nameOf(const std::string &filename) { return files::stemOf(filename, kBenchmarkFileExtension); }

        BenchmarkSaveError saveErrorFor(const std::optional<BenchmarkWriteFailure> &failure) {
            return failure == BenchmarkWriteFailure::ExternalModificationConflict ? BenchmarkSaveError::Conflict
                                                                                  : BenchmarkSaveError::WriteFailed;
        }
    }

    BenchmarksService::BenchmarksService(std::shared_ptr<IBenchmarkStore> store)
        : m_store(std::move(store)) {
        refresh();
    }

    void BenchmarksService::notifyChanged() {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarksService::publish() {
        ++m_revision;
        notifyChanged();
    }

    void BenchmarksService::identify(BenchmarkFileEntry &entry, const domain::BenchmarkId &key) {
        entry.id = key;
        entry.name = nameOf(entry.filename);
        if (auto *loaded = std::get_if<LoadedBenchmark>(&entry.content)) {
            loaded->benchmark.id = key;
            loaded->benchmark.name = entry.name;
            loaded->completeness = domain::validateBenchmark(loaded->benchmark);
        }
    }

    void BenchmarksService::refresh() {
        auto result = m_store->scan();
        if (!result.snapshot) {
            // Directory-level failure: keep the last accepted snapshot; report the diagnostic.
            m_lastRefreshFailed = true;
            notifyChanged();
            return;
        }
        std::map<std::string, domain::BenchmarkId> keys;
        for (auto &entry: result.snapshot->entries) {
            const auto known = m_keys.find(entry.filename);
            const auto key = known != m_keys.end()
                                 ? known->second
                                 : domain::BenchmarkId{"benchmark-file-" + std::to_string(++m_nextKey)};
            identify(entry, key);
            keys.emplace(entry.filename, key);
        }
        m_keys = std::move(keys);
        m_snapshot = std::move(result.snapshot);
        m_lastRefreshFailed = false;
        publish();
    }

    bool BenchmarksService::knows(const std::string &filename) const {
        return m_snapshot && std::ranges::any_of(m_snapshot->entries, [&](const BenchmarkFileEntry &entry) {
            return entry.filename == filename;
        });
    }

    const BenchmarkFileEntry *BenchmarksService::acceptedEntry(const BenchmarkEditToken &token) const {
        if (!m_snapshot) return nullptr;
        for (const auto &entry: m_snapshot->entries) {
            if (entry.filename != token.filename) continue;
            if (entry.digest != token.digest) return nullptr;
            const auto *loaded = std::get_if<LoadedBenchmark>(&entry.content);
            if (!loaded || !(loaded->benchmark.id == token.id)) return nullptr;
            return &entry;
        }
        return nullptr;
    }

    std::optional<BenchmarkSaveError> BenchmarksService::refusal(const BenchmarkEditToken &token) const {
        if (acceptedEntry(token)) return std::nullopt;
        return knows(token.filename) ? BenchmarkSaveError::StaleToken : BenchmarkSaveError::UnknownTarget;
    }

    void BenchmarksService::admit(const std::string &filename, const std::string &digest,
                                  const domain::BenchmarkDefinition &benchmark, const domain::BenchmarkId &key) {
        if (!m_snapshot) m_snapshot = BenchmarkLibrarySnapshot{};
        BenchmarkFileEntry admitted{filename, digest, LoadedBenchmark{benchmark, {}}};
        identify(admitted, key);
        m_keys[filename] = key;
        auto &entries = m_snapshot->entries;
        const auto it = std::ranges::lower_bound(entries, filename, {}, &BenchmarkFileEntry::filename);
        if (it != entries.end() && it->filename == filename) *it = std::move(admitted);
        else entries.insert(it, std::move(admitted));
    }

    void BenchmarksService::drop(const std::string &filename) {
        std::erase_if(m_snapshot->entries, [&](const BenchmarkFileEntry &entry) { return entry.filename == filename; });
        m_keys.erase(filename);
    }

    BenchmarkSaveOutcome BenchmarksService::save(const BenchmarkSaveRequest &request) {
        const auto &benchmark = request.benchmark;
        const auto target = benchmark.name + std::string{kBenchmarkFileExtension};

        if (!request.token) {
            const auto written = m_store->write(benchmark, target, std::nullopt);
            if (!written.succeeded()) return {std::nullopt, saveErrorFor(written.failure), {}};
            admit(target, *written.digest, benchmark, benchmark.id);
            publish();
            return {BenchmarkEditToken{benchmark.id, target, *written.digest}, std::nullopt, benchmark.name};
        }

        const auto &token = *request.token;
        if (const auto refused = refusal(token)) return {std::nullopt, refused, {}};
        const auto written = m_store->write(benchmark, token.filename, token.digest);
        if (!written.succeeded()) return {std::nullopt, saveErrorFor(written.failure), {}};

        const auto oldName = nameOf(token.filename);
        if (benchmark.name != oldName) {
            if (m_store->rename(token.filename, target, *written.digest).ok) {
                drop(token.filename);
                admit(target, *written.digest, benchmark, token.id);
                publish();
                return {BenchmarkEditToken{token.id, target, *written.digest}, std::nullopt, benchmark.name};
            }
            admit(token.filename, *written.digest, benchmark, token.id);
            publish();
            return {BenchmarkEditToken{token.id, token.filename, *written.digest}, BenchmarkSaveError::RenameFailed,
                    oldName};
        }
        admit(token.filename, *written.digest, benchmark, token.id);
        publish();
        return {BenchmarkEditToken{token.id, token.filename, *written.digest}, std::nullopt, oldName};
    }

    BenchmarkReplacementBatchOutcome BenchmarksService::replaceBatch(
        const std::vector<BenchmarkReplacementRequest> &requests) {
        BenchmarkReplacementBatchOutcome result;
        bool changed = false;
        for (const auto &[benchmark, token]: requests) {
            if (const auto refused = refusal(token)) {
                result.outcomes.push_back({std::nullopt, refused, {}});
                continue;
            }
            const auto written = m_store->write(benchmark, token.filename, token.digest);
            if (!written.succeeded()) {
                result.outcomes.push_back({std::nullopt, saveErrorFor(written.failure), {}});
                continue;
            }
            admit(token.filename, *written.digest, benchmark, token.id);
            changed = true;
            result.outcomes.push_back(
                {BenchmarkEditToken{token.id, token.filename, *written.digest}, std::nullopt, nameOf(token.filename)});
        }
        if (changed) publish();
        return result;
    }

    BenchmarkRemoveOutcome BenchmarksService::remove(const BenchmarkEditToken &token) {
        if (!acceptedEntry(token))
            return {knows(token.filename) ? BenchmarkRemoveError::StaleToken : BenchmarkRemoveError::UnknownTarget};

        const auto result = m_store->remove(token.filename, token.digest);
        if (!result.ok) {
            return {result.failure == BenchmarkWriteFailure::ExternalModificationConflict
                        ? BenchmarkRemoveError::Conflict
                        : BenchmarkRemoveError::WriteFailed};
        }
        drop(token.filename);
        publish();
        return {};
    }
}
