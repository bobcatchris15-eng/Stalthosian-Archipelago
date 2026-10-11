#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace stalthos {
using Identity = std::uint64_t;
using RegionId = std::uint64_t;
struct Vec3 { double x = 0, y = 0, z = 0; };
inline bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
enum class HeroSpecies { Smalltrotter, Brackenback, Reedcrest, Thistletail, Longstride };
inline constexpr std::array<const char*, 5> heroSpeciesNames {
    "Smalltrotter", "Brackenback", "Reedcrest", "Thistletail", "Longstride" };

// Calendar age/environment advances together. Input/contact consume physicalSeconds.
struct TimeStep { double physicalSeconds; double calendarSeconds; };
class SimCore {
    double calendar_ = 0;
public:
    double calendarSeconds() const { return calendar_; }
    TimeStep advance(double physicalSeconds, double calendarScale = 1) {
        if (!std::isfinite(physicalSeconds) || physicalSeconds < 0 ||
            !std::isfinite(calendarScale) || calendarScale < 0)
            throw std::invalid_argument("invalid time step");
        double elapsed = physicalSeconds * calendarScale;
        if (!std::isfinite(elapsed) || !std::isfinite(calendar_ + elapsed))
            throw std::overflow_error("calendar overflow");
        calendar_ += elapsed;
        return {physicalSeconds, elapsed};
    }
};

struct BodyStimulus { Identity source; Vec3 point; double urgency; };
struct CameraStimulus { Identity source; Vec3 point; double urgency; };
class BodyPerception {
    std::optional<BodyStimulus> attention_;
public:
    void observe(BodyStimulus s) {
        if (!finite(s.point) || !std::isfinite(s.urgency) || s.urgency < 0)
            throw std::invalid_argument("invalid body stimulus");
        attention_ = s;
    }
    const std::optional<BodyStimulus>& attention() const { return attention_; }
    void clear() { attention_.reset(); }
};
struct HeroView { double cameraHeadingRadians = 0; std::optional<CameraStimulus> cameraAttention; };
struct BeastCore {
    // Y up, heading zero is +Z; camera heading is deliberately absent.
    static Vec3 bodyForward(double bodyHeadingRadians) {
        if (!std::isfinite(bodyHeadingRadians)) throw std::invalid_argument("invalid heading");
        return {std::sin(bodyHeadingRadians), 0, std::cos(bodyHeadingRadians)};
    }
};
enum class Authority { Autonomy, DestinationOrContext, Direct };
enum class GoalStatus { Active, Paused, Replanning, Completed, Abandoned, PlayerCancelled };
struct Goal { Vec3 target; double tolerance; bool contextual; GoalStatus status = GoalStatus::Active; };
class HeroSim {
    bool direct_ = false;
    std::optional<Goal> goal_;
    static bool outstanding(GoalStatus s) {
        return s == GoalStatus::Active || s == GoalStatus::Paused || s == GoalStatus::Replanning;
    }
public:
    BodyPerception perception;
    void suggest(Vec3 target, double tolerance, bool contextual = false) {
        if (!finite(target) || !std::isfinite(tolerance) || tolerance < 0)
            throw std::invalid_argument("invalid goal");
        goal_ = Goal{target, tolerance, contextual};
        // A destination cannot displace actively held direct steering.
        if (direct_) goal_->status = GoalStatus::PlayerCancelled;
    }
    void setDirect(bool held, bool preserveGoal = false) {
        direct_ = held;
        if (held && !preserveGoal && goal_ && outstanding(goal_->status))
            goal_->status = GoalStatus::PlayerCancelled;
    }
    void setGoalStatus(GoalStatus status) {
        if (!goal_ || !outstanding(goal_->status)) throw std::logic_error("no outstanding goal");
        goal_->status = status;
    }
    Authority authority() const {
        if (direct_) return Authority::Direct;
        return goal_ && outstanding(goal_->status) ? Authority::DestinationOrContext : Authority::Autonomy;
    }
    const std::optional<Goal>& goal() const { return goal_; }
    // Routine needs inform compatible actions; they never seize voluntary movement.
    void routineHunger() const noexcept {}
};

enum class TreeCondition { StandingDamaged, Fallen, Dead };
struct TreeException { Identity tree; TreeCondition condition; double changedAt; };
struct RegionSnapshot { std::uint64_t seed; std::uint64_t revision; std::vector<TreeException> trees; };
enum class RetentionReason { BondedPeer, DependentYoung, Caregiver, NotableEncounter };
struct RetainedAnimal {
    Identity id; RegionId region; HeroSpecies species; RetentionReason reason;
    bool alive = true; bool active = false;
};
class StalthosState {
    struct Region { std::uint64_t revision = 0; std::map<Identity, TreeException> trees; };
    std::uint64_t seed_;
    std::size_t retentionLimit_;
    std::map<RegionId, Region> regions_;
    std::map<Identity, RetainedAnimal> retained_;
    std::map<Identity, RegionId> active_;
    std::optional<Identity> hero_;
    static std::uint64_t mix(std::uint64_t x) {
        x += UINT64_C(0x9e3779b97f4a7c15);
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        return x ^ (x >> 31);
    }
public:
    explicit StalthosState(std::uint64_t seed, std::size_t retentionLimit = 64)
        : seed_(seed), retentionLimit_(retentionLimit) {}
    std::uint64_t worldSeed() const { return seed_; }
    std::uint64_t regionSeed(RegionId region) const { return mix(seed_ ^ mix(region)); }
    bool instantiate(Identity id, RegionId region) {
        if (active_.count(id) || retained_.count(id) || deadHeroes_.count(id)) return false;
        active_.emplace(id, region); return true;
    }
    bool retain(Identity id, HeroSpecies species, RetentionReason reason) {
        auto a = active_.find(id);
        if (a == active_.end() || retained_.count(id) || retained_.size() >= retentionLimit_) return false;
        retained_.emplace(id, RetainedAnimal{id, a->second, species, reason, true, true}); return true;
    }
    void retire(Identity id) {
        if (hero_ && *hero_ == id) throw std::logic_error("current hero must use heroDied transition");
        active_.erase(id);
        auto r = retained_.find(id); if (r != retained_.end()) r->second.active = false;
    }
    bool reactivate(Identity id) {
        auto r = retained_.find(id);
        if (r == retained_.end() || !r->second.alive || r->second.active || active_.count(id)) return false;
        active_[id] = r->second.region; r->second.active = true; return true;
    }
    bool markRetainedDead(Identity id) {
        if (hero_ && *hero_ == id) throw std::logic_error("current hero must use heroDied transition");
        auto r = retained_.find(id); if (r == retained_.end()) return false;
        r->second.alive = false; r->second.active = false; active_.erase(id); return true;
    }
    const RetainedAnimal* retained(Identity id) const {
        auto r = retained_.find(id); return r == retained_.end() ? nullptr : &r->second;
    }
    std::size_t retainedCount() const { return retained_.size(); }
    bool excluded(Identity id) const { return active_.count(id) || retained_.count(id) || deadHeroes_.count(id); }
    void setHero(Identity id, RegionId region) {
        if (hero_) throw std::logic_error("current hero must die before replacement");
        if (!instantiate(id, region)) throw std::logic_error("hero identity unavailable");
        hero_ = id;
    }
    void heroDied() {
        if (!hero_) throw std::logic_error("no current hero");
        // Keep recognizable hero death as region-level conspicuous truth.
        auto id = *hero_; auto region = active_.at(id);
        deadHeroes_[id] = region; hero_.reset(); retire(id); markRetainedDead(id);
    }
    bool heroDeathKnown(Identity id) const { return deadHeroes_.count(id) != 0; }
    void recordTree(RegionId region, TreeException tree) {
        if (!std::isfinite(tree.changedAt) || tree.changedAt < 0) throw std::invalid_argument("invalid tree timestamp");
        auto& r = regions_[region]; r.trees.insert_or_assign(tree.tree, tree); ++r.revision;
    }
    RegionSnapshot regionSnapshot(RegionId region) const {
        RegionSnapshot s{regionSeed(region), 0, {}};
        auto r = regions_.find(region);
        if (r != regions_.end()) { s.revision = r->second.revision; for (const auto& t : r->second.trees) s.trees.push_back(t.second); }
        return s;
    }
    bool snapshotCurrent(RegionId region, std::uint64_t revision) const {
        auto r = regions_.find(region); return revision == (r == regions_.end() ? 0 : r->second.revision);
    }
private:
    std::map<Identity, RegionId> deadHeroes_; // Conspicuous exceptions; not remote biographies.
};

// One bounded contact session, not a forever-growing ledger of all collisions.
class InteractionsCore {
    std::uint64_t session_ = 0;
    std::size_t capacity_;
    std::set<std::uint64_t> applied_;
public:
    explicit InteractionsCore(std::size_t capacity = 256) : capacity_(capacity) {}
    void beginSession(std::uint64_t session) {
        if (session <= session_) throw std::invalid_argument("contact session must increase");
        session_ = session; applied_.clear();
    }
    bool applyContact(std::uint64_t session, std::uint64_t eventId, bool physicallyValid, double amount, double& resource) {
        if (session != session_ || session_ == 0 || !physicallyValid || applied_.count(eventId)) return false;
        if (!std::isfinite(amount) || amount <= 0 || !std::isfinite(resource) || resource < amount) return false;
        if (applied_.size() >= capacity_) return false; // Fail closed; never evict and replay an old transfer.
        applied_.insert(eventId); resource -= amount; return true;
    }
};
} // namespace stalthos
