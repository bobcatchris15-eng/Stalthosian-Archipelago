#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stalthos {

enum class ResourceKind { Shrub, Creek };

// Admission is observational: NewKey and Update mean a request can be applied.
// The same statuses returned by tryConsume* describe the applied operation.
enum class ResourceAdmission {
    Replay, Update, NewKey, HistoryFull, MissingResource, InvalidRequest,
    Conflict, InsufficientQuantity, StorageFailure
};

struct ResourceHistoryCapacity {
    std::size_t used;
    std::size_t limit;
    std::size_t available;
};

struct ResourceDto {
    std::uint64_t id;
    ResourceKind kind;
    double initialQuantity;
    double consumedQuantity;
};

// A continuation snapshot includes interaction keys: resource quantities alone
// cannot preserve idempotence after restoring the same interaction session.
struct ResourceSessionDto {
    std::uint64_t sessionId;
    std::uint64_t resourceId;
    double amount;
};

struct ResourceLedgerDto {
    std::vector<ResourceDto> resources;
    std::vector<ResourceSessionDto> sessions;
};

class ResourceLedger {
    struct ResourceRecord {
        ResourceKind kind;
        double initialQuantity;
        double consumedQuantity;
    };
    std::size_t maxResources_;
    std::size_t maxHistory_;
    std::map<std::uint64_t, ResourceRecord> resources_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, double> history_;

    static bool validKind(ResourceKind kind) {
        return kind == ResourceKind::Shrub || kind == ResourceKind::Creek;
    }

public:
    explicit ResourceLedger(std::size_t maxResources = 4096, std::size_t maxHistory = 256)
        : maxResources_(maxResources), maxHistory_(maxHistory) {
        if (maxResources > 4096 || maxHistory > 256) {
            throw std::invalid_argument("ledger capacity exceeds hard limit");
        }
    }

    // Quantity-only load explicitly begins a fresh interaction-history epoch.
    bool load(const std::vector<ResourceDto>& dtos) {
        if (dtos.size() > maxResources_) return false;
        std::map<std::uint64_t, ResourceRecord> next;
        for (const auto& dto : dtos) {
            if (!validKind(dto.kind) || !std::isfinite(dto.initialQuantity) ||
                dto.initialQuantity < 0.0 || !std::isfinite(dto.consumedQuantity) ||
                dto.consumedQuantity < 0.0 || dto.consumedQuantity > dto.initialQuantity ||
                !next.emplace(dto.id, ResourceRecord{dto.kind, dto.initialQuantity,
                                                      dto.consumedQuantity}).second) {
                return false;
            }
        }
        resources_.swap(next);
        history_.clear();
        return true;
    }

    // Full transactional restore preserves idempotence across save/restore.
    bool restore(const ResourceLedgerDto& dto) {
        if (dto.resources.size() > maxResources_ || dto.sessions.size() > maxHistory_) return false;
        std::map<std::uint64_t, ResourceRecord> nextResources;
        for (const auto& item : dto.resources) {
            if (!validKind(item.kind) || !std::isfinite(item.initialQuantity) ||
                item.initialQuantity < 0.0 || !std::isfinite(item.consumedQuantity) ||
                item.consumedQuantity < 0.0 || item.consumedQuantity > item.initialQuantity ||
                !nextResources.emplace(item.id, ResourceRecord{item.kind, item.initialQuantity,
                                                                item.consumedQuantity}).second) {
                return false;
            }
        }
        std::map<std::pair<std::uint64_t, std::uint64_t>, double> nextHistory;
        std::map<std::uint64_t, double> consumedByHistory;
        for (const auto& session : dto.sessions) {
            const auto resource = nextResources.find(session.resourceId);
            if (session.sessionId == 0 || resource == nextResources.end() || !std::isfinite(session.amount) ||
                session.amount <= 0.0 ||
                !nextHistory.emplace(std::make_pair(session.sessionId, session.resourceId),
                                     session.amount).second) {
                return false;
            }
            double& total = consumedByHistory[session.resourceId];
            total += session.amount;
            const double tolerance = std::numeric_limits<double>::epsilon() *
                (resource->second.consumedQuantity > 1.0 ? resource->second.consumedQuantity : 1.0) *
                static_cast<double>(maxHistory_ + 1);
            if (!std::isfinite(total) || total - resource->second.consumedQuantity > tolerance) return false;
        }
        resources_.swap(nextResources);
        history_.swap(nextHistory);
        return true;
    }

    std::vector<ResourceDto> snapshot() const {
        std::vector<ResourceDto> result;
        result.reserve(resources_.size());
        for (const auto& item : resources_) {
            result.push_back({item.first, item.second.kind, item.second.initialQuantity,
                              item.second.consumedQuantity});
        }
        return result;
    }

    ResourceLedgerDto continuationSnapshot() const {
        ResourceLedgerDto result;
        result.resources = snapshot();
        result.sessions.reserve(history_.size());
        for (const auto& entry : history_) {
            result.sessions.push_back({entry.first.first, entry.first.second, entry.second});
        }
        return result;
    }

    ResourceHistoryCapacity historyCapacity() const noexcept {
        return {history_.size(), maxHistory_, maxHistory_ - history_.size()};
    }

    // Existing keys are inspected before capacity, so a full history still
    // permits exact replays and increasing totals for retained contacts.
    ResourceAdmission inspectConsume(std::uint64_t sessionId, std::uint64_t resourceId,
                                     double amount, bool cumulative = false) const noexcept {
        if (sessionId == 0 || !std::isfinite(amount) || amount <= 0.0)
            return ResourceAdmission::InvalidRequest;
        const auto resource = resources_.find(resourceId);
        if (resource == resources_.end()) return ResourceAdmission::MissingResource;
        const auto prior = history_.find(std::make_pair(sessionId, resourceId));
        double delta = amount;
        if (prior != history_.end()) {
            if (amount == prior->second) return ResourceAdmission::Replay;
            if (!cumulative || amount < prior->second) return ResourceAdmission::Conflict;
            delta -= prior->second;
        } else if (history_.size() >= maxHistory_) {
            return ResourceAdmission::HistoryFull;
        }
        const auto& record = resource->second;
        if (delta > record.initialQuantity - record.consumedQuantity)
            return ResourceAdmission::InsufficientQuantity;
        return prior == history_.end() ? ResourceAdmission::NewKey : ResourceAdmission::Update;
    }

    ResourceAdmission tryConsume(std::uint64_t sessionId, std::uint64_t resourceId,
                                 double amount, bool cumulative = false) noexcept {
        const auto admission = inspectConsume(sessionId, resourceId, amount, cumulative);
        if (admission != ResourceAdmission::NewKey && admission != ResourceAdmission::Update)
            return admission;
        const auto key = std::make_pair(sessionId, resourceId);
        auto& record = resources_.find(resourceId)->second;
        if (admission == ResourceAdmission::NewKey) {
            // Allocate before deduction; failure cannot partially consume.
            try { history_.emplace(key, amount); }
            catch (...) { return ResourceAdmission::StorageFailure; }
            record.consumedQuantity += amount;
        } else {
            auto prior = history_.find(key);
            record.consumedQuantity += amount - prior->second;
            prior->second = amount;
        }
        return admission;
    }

    ResourceAdmission tryConsumeCumulative(std::uint64_t sessionId, std::uint64_t resourceId,
                                           double total) noexcept {
        return tryConsume(sessionId, resourceId, total, true);
    }

    // Legacy bool/exception contract retained for existing callers.
    bool consume(std::uint64_t sessionId, std::uint64_t resourceId, double amount) {
        return legacyResult(tryConsume(sessionId, resourceId, amount));
    }

    // A continuous actor contact retains one cumulative replay key.
    bool consumeCumulative(std::uint64_t sessionId, std::uint64_t resourceId, double total) {
        return legacyResult(tryConsumeCumulative(sessionId, resourceId, total));
    }

private:
    static bool legacyResult(ResourceAdmission status) {
        if (status == ResourceAdmission::InvalidRequest)
            throw std::invalid_argument("invalid consume key or amount");
        if (status == ResourceAdmission::HistoryFull)
            throw std::runtime_error("ledger history capacity exceeded");
        if (status == ResourceAdmission::StorageFailure) throw std::bad_alloc();
        return status == ResourceAdmission::Replay || status == ResourceAdmission::Update ||
               status == ResourceAdmission::NewKey;
    }

public:
    void reset(std::uint64_t seed = 0) {
        (void)seed;
        resources_.clear();
        history_.clear();
    }
};

} // namespace stalthos
