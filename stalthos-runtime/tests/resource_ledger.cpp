#include "../domain/resource_ledger.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(\
    std::string("check failed: ") + #condition + " at line " + std::to_string(__LINE__)); } while (false)

using namespace stalthos;

void happyPathAndReplay() {
    ResourceLedger ledger;
    CHECK(ledger.load({{101, ResourceKind::Shrub, 50.0, 0.0},
                       {102, ResourceKind::Creek, 100.0, 20.0}}));
    CHECK(ledger.consume(1, 101, 10.0));
    CHECK(ledger.consume(1, 101, 10.0));
    CHECK(!ledger.consume(1, 101, 11.0));
    CHECK(ledger.consume(2, 101, 5.0));
    const auto state = ledger.snapshot();
    CHECK(state.size() == 2 && state[0].id == 101 && state[0].consumedQuantity == 15.0);
    CHECK(state[1].consumedQuantity == 20.0);
}

void failuresDoNotMutate() {
    ResourceLedger ledger;
    CHECK(ledger.load({{7, ResourceKind::Shrub, 10.0, 2.0}}));
    CHECK(!ledger.consume(1, 7, 9.0));
    CHECK(!ledger.consume(1, 99, 1.0));
    CHECK(ledger.snapshot()[0].consumedQuantity == 2.0);
    for (double amount : {-1.0, 0.0, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        bool threw = false;
        try { (void)ledger.consume(2, 7, amount); }
        catch (const std::invalid_argument&) { threw = true; }
        CHECK(threw);
    }
    bool zeroSessionThrew = false;
    try { (void)ledger.consume(0, 7, 1.0); }
    catch (const std::invalid_argument&) { zeroSessionThrew = true; }
    CHECK(zeroSessionThrew);
}

void transactionalLoadAndRestore() {
    ResourceLedger ledger;
    CHECK(ledger.load({{1, ResourceKind::Creek, 8.0, 0.0}}));
    CHECK(ledger.consume(44, 1, 3.0));
    CHECK(!ledger.load({{2, ResourceKind::Shrub, 1.0, 2.0}}));
    CHECK(ledger.snapshot()[0].id == 1 && ledger.snapshot()[0].consumedQuantity == 3.0);

    const auto saved = ledger.continuationSnapshot();
    ResourceLedger restored;
    CHECK(restored.restore(saved));
    CHECK(restored.consume(44, 1, 3.0));
    CHECK(!restored.consume(44, 1, 4.0));
    CHECK(restored.snapshot()[0].consumedQuantity == 3.0);
    CHECK(restored.consume(45, 1, 2.0));
    CHECK(restored.snapshot()[0].consumedQuantity == 5.0);

    ResourceLedgerDto invalid = saved;
    invalid.sessions.push_back({44, 999, 3.0});
    CHECK(!restored.restore(invalid));
    CHECK(restored.snapshot()[0].consumedQuantity == 5.0);
    CHECK(restored.consume(46, 1, 1.0)); // Failed restore preserved history too.
    invalid = saved;
    invalid.sessions[0].amount = 4.0; // Session ledger cannot exceed recorded depletion.
    CHECK(!restored.restore(invalid));
}

void strictCapsAndNoEviction() {
    bool capThrew = false;
    try { ResourceLedger oversized(4097, 256); }
    catch (const std::invalid_argument&) { capThrew = true; }
    CHECK(capThrew);
    ResourceLedger ledger(1, 2);
    CHECK(!ledger.load({{1, ResourceKind::Shrub, 1.0, 0.0},
                        {2, ResourceKind::Shrub, 1.0, 0.0}}));
    CHECK(ledger.load({{1, ResourceKind::Shrub, 10.0, 0.0}}));
    CHECK(ledger.consume(1, 1, 1.0));
    CHECK(ledger.consume(2, 1, 1.0));
    bool threw = false;
    try { (void)ledger.consume(3, 1, 1.0); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    CHECK(ledger.consume(1, 1, 1.0)); // Full history retains old replay keys.
    CHECK(ledger.snapshot()[0].consumedQuantity == 2.0);

    ResourceLedgerDto tooMany = ledger.continuationSnapshot();
    tooMany.sessions.push_back({4, 1, 1.0});
    ResourceLedger unchanged(1, 2);
    CHECK(unchanged.load({{9, ResourceKind::Creek, 5.0, 0.0}}));
    CHECK(!unchanged.restore(tooMany));
    CHECK(unchanged.snapshot()[0].id == 9);
}

void rejectsMalformedDtos() {
    ResourceLedger ledger;
    CHECK(!ledger.load({{1, ResourceKind::Shrub, 1.0, 0.0},
                        {1, ResourceKind::Creek, 1.0, 0.0}}));
    CHECK(!ledger.load({{1, static_cast<ResourceKind>(77), 1.0, 0.0}}));
    CHECK(!ledger.load({{1, ResourceKind::Shrub, std::numeric_limits<double>::infinity(), 0.0}}));
}

int main() {
    try {
        ResourceLedger ongoing(1,1);
        CHECK(ongoing.load({{99,ResourceKind::Shrub,20,0}}));
        for(int bite=1;bite<=1000;++bite) CHECK(ongoing.consumeCumulative(7,99,double(bite)/100));
        CHECK(ongoing.continuationSnapshot().sessions.size()==1);
        CHECK(std::abs(ongoing.snapshot()[0].consumedQuantity-10)<1e-10);
        CHECK(ongoing.consumeCumulative(7,99,10));
        CHECK(!ongoing.consumeCumulative(7,99,9));
        CHECK(!ongoing.consumeCumulative(7,99,25));
        ResourceLedger resumed(1,1);
        CHECK(resumed.restore(ongoing.continuationSnapshot()));
        CHECK(resumed.consumeCumulative(7,99,11));
        CHECK(std::abs(resumed.snapshot()[0].consumedQuantity-11)<1e-10);
        happyPathAndReplay();
        failuresDoNotMutate();
        transactionalLoadAndRestore();
        strictCapsAndNoEviction();
        rejectsMalformedDtos();
        std::cout << "All ResourceLedger tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
