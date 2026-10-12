#include "../domain/relationships.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace stalthos::relationships;
using stalthos::Identity;

template <typename F>
void assert_throws_invalid_argument(F func) {
    bool thrown = false;
    try {
        func();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    if (!thrown) {
        throw std::runtime_error("Expected std::invalid_argument was not thrown");
    }
}

template <typename F>
void assert_throws_logic_error(F func) {
    bool thrown = false;
    try {
        func();
    } catch (const std::logic_error&) {
        thrown = true;
    }
    if (!thrown) {
        throw std::runtime_error("Expected std::logic_error was not thrown");
    }
}

template <typename F>
void assert_throws_overflow_error(F func) {
    bool thrown = false;
    try {
        func();
    } catch (const std::overflow_error&) {
        thrown = true;
    }
    if (!thrown) {
        throw std::runtime_error("Expected std::overflow_error was not thrown");
    }
}

void test_peer_symmetry() {
    RelationshipTracker tracker;
    tracker.formPeerLink(1, 2, 10.0);

    const auto* links1 = tracker.getLinks(1);
    if (!links1 || links1->size() != 1 || links1->at(0).target != 2 || links1->at(0).role != Role::Peer || links1->at(0).status != ContactStatus::Present || links1->at(0).lastObservationTime != 10.0) {
        throw std::runtime_error("Peer link for 1 failed");
    }

    const auto* links2 = tracker.getLinks(2);
    if (!links2 || links2->size() != 1 || links2->at(0).target != 1 || links2->at(0).role != Role::Peer || links2->at(0).status != ContactStatus::Present || links2->at(0).lastObservationTime != 10.0) {
        throw std::runtime_error("Peer link for 2 failed");
    }
}

void test_caregiver_dependent_semantics() {
    RelationshipTracker tracker;
    tracker.formCaregiverDependentLink(10, 20, 5.0); // 10 is caregiver, 20 is dependent

    const auto* linksCaregiver = tracker.getLinks(10);
    if (!linksCaregiver || linksCaregiver->size() != 1 || linksCaregiver->at(0).target != 20 || linksCaregiver->at(0).role != Role::Dependent) {
        throw std::runtime_error("Caregiver tracking dependent failed");
    }

    const auto* linksDependent = tracker.getLinks(20);
    if (!linksDependent || linksDependent->size() != 1 || linksDependent->at(0).target != 10 || linksDependent->at(0).role != Role::Caregiver) {
        throw std::runtime_error("Dependent tracking caregiver failed");
    }
}

void test_observe_separation() {
    RelationshipTracker tracker;
    tracker.formPeerLink(1, 2, 10.0);

    // Observer 1 sees 2 separated
    tracker.observeSeparation(1, 2, 15.0);

    const auto* links1 = tracker.getLinks(1);
    if (links1->at(0).status != ContactStatus::Separated || links1->at(0).lastObservationTime != 15.0) {
        throw std::runtime_error("Separation not recorded correctly for observer 1");
    }

    // Observer 2's state should NOT change just because 1 observed separation
    // (though in reality they might both observe it, we test that state updates are directional)
    const auto* links2 = tracker.getLinks(2);
    if (links2->at(0).status != ContactStatus::Present || links2->at(0).lastObservationTime != 10.0) {
        throw std::runtime_error("Target 2 state should not change automatically");
    }
}

void test_resurrection_prevention() {
    RelationshipTracker tracker;
    tracker.formPeerLink(1, 2, 10.0);

    tracker.observeDeath(1, 2, 20.0);
    if (!tracker.isConfirmedDead(2) || tracker.isConfirmedDead(1)) {
        throw std::runtime_error("Death truth must attach to the target identity only");
    }

    assert_throws_logic_error([&]() {
        tracker.observePresent(1, 2, 25.0);
    });

    assert_throws_logic_error([&]() {
        tracker.observeSeparation(1, 2, 25.0);
    });

    // A different observer cannot create a new present edge to an identity
    // whose death has already been confirmed.
    assert_throws_logic_error([&]() {
        tracker.formPeerLink(2, 3, 30.0);
    });
    if (tracker.nodeCount() != 2 || tracker.getLinks(3) != nullptr) {
        throw std::runtime_error("Rejected post-death link must leave graph unchanged");
    }
}

void test_time_validation() {
    RelationshipTracker tracker;

    assert_throws_invalid_argument([&]() {
        tracker.formPeerLink(1, 2, -5.0);
    });

    assert_throws_invalid_argument([&]() {
        tracker.formPeerLink(1, 2, std::numeric_limits<double>::infinity());
    });
    assert_throws_invalid_argument([&]() {
        tracker.formPeerLink(1, 2, std::numeric_limits<double>::quiet_NaN());
    });

    tracker.formPeerLink(1, 2, 10.0);

    assert_throws_invalid_argument([&]() {
        tracker.observePresent(1, 2, 5.0); // Time going backwards
    });
    const auto* links = tracker.getLinks(1);
    if (!links || links->at(0).status != ContactStatus::Present || links->at(0).lastObservationTime != 10.0) {
        throw std::runtime_error("Rejected backwards observation must preserve prior state");
    }
    assert_throws_invalid_argument([&]() {
        tracker.observeDeath(1, 2, std::numeric_limits<double>::infinity());
    });
    if (tracker.isConfirmedDead(2) || links->at(0).status != ContactStatus::Present || links->at(0).lastObservationTime != 10.0) {
        throw std::runtime_error("Rejected non-finite death time must preserve prior state");
    }
}

void test_absent_identity_is_not_created_by_lost_contact() {
    RelationshipTracker tracker;
    assert_throws_invalid_argument([&]() {
        tracker.observeSeparation(7, 8, 1.0);
    });
    if (tracker.nodeCount() != 0 || tracker.getLinks(7) != nullptr || tracker.isConfirmedDead(8)) {
        throw std::runtime_error("Lost-contact observation must not create identities or links");
    }
}

void test_duplicates_and_self_links() {
    RelationshipTracker tracker;

    assert_throws_invalid_argument([&]() {
        tracker.formPeerLink(1, 1, 10.0);
    });

    tracker.formPeerLink(1, 2, 10.0);

    assert_throws_invalid_argument([&]() {
        tracker.formPeerLink(1, 2, 15.0); // duplicate
    });

    assert_throws_invalid_argument([&]() {
        tracker.formCaregiverDependentLink(1, 2, 15.0); // conflicting duplicate
    });
}

void test_bounds() {
    RelationshipTracker noLinks(2, 0);
    assert_throws_overflow_error([&]() {
        noLinks.formPeerLink(1, 2, 0.0);
    });
    if (noLinks.nodeCount() != 0) {
        throw std::runtime_error("Zero link capacity must reject a pair of new nodes without mutation");
    }

    RelationshipTracker tracker(2, 1);

    tracker.formPeerLink(1, 2, 10.0);

    // Adding another node should exceed maxNodes
    assert_throws_overflow_error([&]() {
        tracker.formPeerLink(1, 3, 10.0);
    });

    // Using a different tracker with enough nodes but max 1 link per node
    RelationshipTracker tracker2(10, 1);
    tracker2.formPeerLink(1, 2, 10.0);
    assert_throws_overflow_error([&]() {
        tracker2.formPeerLink(1, 3, 10.0); // 1 already has 1 link
    });
}

void test_zero_identity_is_valid() {
    RelationshipTracker tracker;
    tracker.formPeerLink(0, 1, 0.0);
    const auto* links = tracker.getLinks(0);
    if (!links || links->size() != 1 || links->at(0).target != 1) {
        throw std::runtime_error("Identity zero is a valid identity value");
    }
}

int main() {
    try {
        test_peer_symmetry();
        test_caregiver_dependent_semantics();
        test_observe_separation();
        test_resurrection_prevention();
        test_time_validation();
        test_absent_identity_is_not_created_by_lost_contact();
        test_duplicates_and_self_links();
        test_bounds();
        test_zero_identity_is_valid();
        std::cout << "All tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
