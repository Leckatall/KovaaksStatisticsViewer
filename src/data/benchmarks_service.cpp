#include "benchmarks_service.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "benchmarks/benchmark_validation.h"

namespace ksv::data {
    namespace {
        bool isBlank(const std::string &text) {
            return std::ranges::all_of(text, [](unsigned char ch) { return std::isspace(ch) != 0; });
        }
    }

    // Tasks D5-D6 supply conditional delete and coherent batch replacement behavior.

    BenchmarksService::BenchmarksService(std::shared_ptr<IBenchmarkStore> store)
        : m_store(std::move(store)) {
        refresh();
    }

    void BenchmarksService::notifyChanged() {
        for (const auto &callback: m_callbacks) callback();
    }

    void BenchmarksService::refresh() {
        auto result = m_store->scan();
        if (!result.snapshot) {
            // Directory-level failure: keep the last accepted snapshot; report the diagnostic.
            m_lastRefreshFailed = true;
            notifyChanged();
            return;
        }
        m_snapshot = std::move(result.snapshot);
        m_lastRefreshFailed = false;
        ++m_revision;
        notifyChanged();
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

    void BenchmarksService::upsertSnapshotEntry(const std::string &filename, const std::string &digest,
                                                LoadedBenchmark content) {
        if (!m_snapshot) m_snapshot = BenchmarkLibrarySnapshot{};
        auto &entries = m_snapshot->entries;
        const auto it = std::ranges::lower_bound(entries, filename, {}, &BenchmarkFileEntry::filename);
        if (it != entries.end() && it->filename == filename) {
            it->digest = digest;
            it->content = std::move(content);
            return;
        }
        entries.insert(it, BenchmarkFileEntry{filename, digest, std::move(content)});
    }

    BenchmarkSaveOutcome BenchmarksService::writeAccepted(const domain::Benchmark &benchmark,
                                                         const std::optional<BenchmarkEditToken> &token,
                                                         bool &changed) {
        if (isBlank(benchmark.name)) return {std::nullopt, BenchmarkSaveError::EmptyName};

        std::string filename;
        std::optional<std::string> expectedDigest;
        if (token) {
            const bool filenameKnown = m_snapshot &&
                std::ranges::any_of(m_snapshot->entries, [&](const BenchmarkFileEntry &entry) {
                    return entry.filename == token->filename;
                });
            if (!acceptedEntry(*token))
                return {std::nullopt, filenameKnown ? BenchmarkSaveError::StaleToken
                                                    : BenchmarkSaveError::UnknownTarget};
            filename = token->filename;
            expectedDigest = token->digest;
        } else {
            filename = benchmark.id.value + ".json";
        }

        const auto result = m_store->write(benchmark, filename, expectedDigest);
        if (!result.succeeded()) {
            return {std::nullopt,
                    result.failure == BenchmarkWriteFailure::ExternalModificationConflict
                        ? BenchmarkSaveError::Conflict
                        : BenchmarkSaveError::WriteFailed};
        }

        upsertSnapshotEntry(filename, *result.digest,
                            LoadedBenchmark{benchmark, domain::validateBenchmark(benchmark)});
        changed = true;
        return {BenchmarkEditToken{benchmark.id, filename, *result.digest}, std::nullopt};
    }

    BenchmarkSaveOutcome BenchmarksService::save(const BenchmarkSaveRequest &request) {
        bool changed = false;
        auto outcome = writeAccepted(request.benchmark, request.token, changed);
        if (changed) {
            ++m_revision;
            notifyChanged();
        }
        return outcome;
    }

    BenchmarkReplacementBatchOutcome BenchmarksService::replaceBatch(
        const std::vector<BenchmarkReplacementRequest> &requests) {
        BenchmarkReplacementBatchOutcome result;
        bool changed = false;
        for (const auto &request: requests)
            result.outcomes.push_back(writeAccepted(request.benchmark, request.token, changed));
        if (changed) {
            ++m_revision;
            notifyChanged();
        }
        return result;
    }

    BenchmarkRemoveOutcome BenchmarksService::remove(const BenchmarkEditToken &token) {
        const bool filenameKnown = m_snapshot &&
            std::ranges::any_of(m_snapshot->entries, [&](const BenchmarkFileEntry &entry) {
                return entry.filename == token.filename;
            });
        if (!acceptedEntry(token))
            return {filenameKnown ? BenchmarkRemoveError::StaleToken
                                  : BenchmarkRemoveError::UnknownTarget};

        const auto result = m_store->remove(token.filename, token.digest);
        if (!result.ok) {
            return {result.failure == BenchmarkWriteFailure::ExternalModificationConflict
                        ? BenchmarkRemoveError::Conflict
                        : BenchmarkRemoveError::WriteFailed};
        }
        std::erase_if(m_snapshot->entries,
                      [&](const BenchmarkFileEntry &entry) { return entry.filename == token.filename; });
        ++m_revision;
        notifyChanged();
        return {};
    }
}
