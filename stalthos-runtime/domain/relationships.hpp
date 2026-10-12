#pragma once

#include <cstdint>
#include <stdexcept>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>
#include "foundation.hpp" // for Identity

namespace stalthos::relationships {

enum class Role {
    Peer,
    Caregiver,
    Dependent
};

enum class ContactStatus {
    Present,
    Separated,
    ConfirmedDead
};

struct LinkRecord {
    Identity target;
    Role role;
    ContactStatus status;
    double lastObservationTime;

    bool operator==(const LinkRecord& other) const {
        return target == other.target &&
               role == other.role &&
               status == other.status &&
               lastObservationTime == other.lastObservationTime;
    }
};

class RelationshipTracker {
    std::size_t maxNodes_;
    std::size_t maxLinksPerNode_;
    std::map<Identity, std::vector<LinkRecord>> graph_;
    // Death is identity truth, while separation and present observations remain
    // directional. Every dead identity already belongs to a bounded graph node.
    std::set<Identity> confirmedDead_;

public:
    explicit RelationshipTracker(std::size_t maxNodes = 1000, std::size_t maxLinksPerNode = 10)
        : maxNodes_(maxNodes), maxLinksPerNode_(maxLinksPerNode) {}

    void formPeerLink(Identity a, Identity b, double time) {
        validateTime(time);
        if (a == b) throw std::invalid_argument("self-link not allowed");
        checkAliveForNewLink(a, b);

        checkCanAddSymmetric(a, b);

        addSymmetricLinks(a, b, Role::Peer, Role::Peer, time);
    }

    void formCaregiverDependentLink(Identity caregiver, Identity dependent, double time) {
        validateTime(time);
        if (caregiver == dependent) throw std::invalid_argument("self-link not allowed");
        checkAliveForNewLink(caregiver, dependent);

        checkCanAddSymmetric(caregiver, dependent);

        addSymmetricLinks(caregiver, dependent, Role::Dependent, Role::Caregiver, time);
    }

    void observeSeparation(Identity observer, Identity target, double time) {
        updateObservation(observer, target, ContactStatus::Separated, time);
    }

    void observePresent(Identity observer, Identity target, double time) {
        updateObservation(observer, target, ContactStatus::Present, time);
    }

    void observeDeath(Identity observer, Identity target, double time) {
        updateObservation(observer, target, ContactStatus::ConfirmedDead, time);
    }

    const std::vector<LinkRecord>* getLinks(Identity id) const {
        auto it = graph_.find(id);
        if (it != graph_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::size_t nodeCount() const {
        return graph_.size();
    }

    bool isConfirmedDead(Identity id) const {
        return confirmedDead_.find(id) != confirmedDead_.end();
    }

private:
    void validateTime(double time) const {
        if (!std::isfinite(time) || time < 0) {
            throw std::invalid_argument("invalid time");
        }
    }

    void checkCanAddSymmetric(Identity a, Identity b) const {
        // Every newly formed relationship gives each endpoint one edge, even
        // when both nodes are new and therefore have no existing vector to test.
        if (maxLinksPerNode_ == 0) {
            throw std::overflow_error("max links per node exceeded");
        }
        bool aNew = (graph_.find(a) == graph_.end());
        bool bNew = (graph_.find(b) == graph_.end());
        std::size_t newNodes = (aNew ? 1 : 0) + (bNew ? 1 : 0);

        if (graph_.size() + newNodes > maxNodes_) {
            throw std::overflow_error("max nodes exceeded");
        }

        if (!aNew) {
            const auto& links = graph_.at(a);
            for (const auto& l : links) {
                if (l.target == b) throw std::invalid_argument("duplicate link");
            }
            if (links.size() >= maxLinksPerNode_) {
                throw std::overflow_error("max links per node exceeded");
            }
        }

        if (!bNew) {
            const auto& links = graph_.at(b);
            for (const auto& l : links) {
                if (l.target == a) throw std::invalid_argument("duplicate link");
            }
            if (links.size() >= maxLinksPerNode_) {
                throw std::overflow_error("max links per node exceeded");
            }
        }
    }

    void checkAliveForNewLink(Identity a, Identity b) const {
        if (isConfirmedDead(a) || isConfirmedDead(b)) {
            throw std::logic_error("cannot form a relationship with a confirmed-dead identity");
        }
    }

    static void addLinkUnchecked(std::map<Identity, std::vector<LinkRecord>>& graph,
                                 Identity subject, Identity target, Role role, double time) {
        graph[subject].push_back({target, role, ContactStatus::Present, time});
    }

    void addSymmetricLinks(Identity a, Identity b, Role roleA, Role roleB, double time) {
        // Stage both records in a bounded copy. If any allocation fails, the
        // live graph stays unchanged; map::swap commits both edges together.
        auto updated = graph_;
        addLinkUnchecked(updated, a, b, roleA, time);
        addLinkUnchecked(updated, b, a, roleB, time);
        graph_.swap(updated);
    }

    void updateObservation(Identity observer, Identity target, ContactStatus status, double time) {
        validateTime(time);
        auto it = graph_.find(observer);
        if (it == graph_.end()) {
            throw std::invalid_argument("observer has no links");
        }

        auto& links = it->second;
        bool found = false;
        for (auto& link : links) {
            if (link.target == target) {
                if (time < link.lastObservationTime) {
                    throw std::invalid_argument("time cannot go backwards");
                }
                if (link.status == ContactStatus::ConfirmedDead && status != ContactStatus::ConfirmedDead) {
                    throw std::logic_error("cannot resurrect a confirmed-dead identity");
                }
                if (status == ContactStatus::Present && isConfirmedDead(target)) {
                    throw std::logic_error("cannot observe a confirmed-dead identity as present");
                }
                if (status == ContactStatus::Separated && isConfirmedDead(target)) {
                    throw std::logic_error("cannot observe a confirmed-dead identity as separated");
                }
                // Insert before changing the edge so allocation failure leaves
                // relationship state unchanged. The following assignments do
                // not throw.
                if (status == ContactStatus::ConfirmedDead) {
                    confirmedDead_.insert(target);
                }
                link.status = status;
                link.lastObservationTime = time;
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::invalid_argument("target link not found");
        }
    }
};

} // namespace stalthos::relationships
