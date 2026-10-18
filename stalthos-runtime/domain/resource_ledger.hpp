#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stalthos {

enum class ResourceKind { Shrub, Creek };

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

    // Returns true for a first successful application or an exact replay.
    // Reusing a key with a different amount is rejected to avoid masking conflicts.
    bool consume(std::uint64_t sessionId, std::uint64_t resourceId, double amount) {
        if (sessionId == 0 || !std::isfinite(amount) || amount <= 0.0) {
            throw std::invalid_argument("invalid consume key or amount");
        }
        const auto resource = resources_.find(resourceId);
        if (resource == resources_.end()) return false;
        const auto key = std::make_pair(sessionId, resourceId);
        const auto prior = history_.find(key);
        if (prior != history_.end()) return prior->second == amount;
        if (history_.size() >= maxHistory_) {
            throw std::runtime_error("ledger history capacity exceeded");
        }
        ResourceRecord& record = resource->second;
        const double remaining = record.initialQuantity - record.consumedQuantity;
        if (amount > remaining) return false;
        // Insert first: allocation failure must not deduct without recording replay key.
        const auto inserted = history_.emplace(key, amount);
        if (!inserted.second) return inserted.first->second == amount;
        record.consumedQuantity += amount;
        return true;
    }

    // A continuous actor contact keeps one bounded replay key. The caller
    // supplies its total consumption for that contact, so replay is idempotent
    // and long-running browsing does not allocate one record per frame/bite.
    bool consumeCumulative(std::uint64_t sessionId, std::uint64_t resourceId, double total) {
        if(sessionId==0 || !std::isfinite(total) || total<=0) throw std::invalid_argument("invalid cumulative consume key or amount");
        const auto resource=resources_.find(resourceId);
        if(resource==resources_.end()) return false;
        const auto key=std::make_pair(sessionId,resourceId);
        const auto prior=history_.find(key);
        if(prior==history_.end()) return consume(sessionId,resourceId,total);
        if(total<prior->second) return false;
        const double delta=total-prior->second;
        auto& record=resource->second;
        if(delta>record.initialQuantity-record.consumedQuantity) return false;
        record.consumedQuantity+=delta;
        prior->second=total;
        return true;
    }

    void reset(std::uint64_t seed = 0) {
        (void)seed;
        resources_.clear();
        history_.clear();
    }
};

} // namespace stalthos
