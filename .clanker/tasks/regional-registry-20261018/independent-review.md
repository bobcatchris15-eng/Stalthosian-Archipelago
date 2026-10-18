# Independent registry review

Reviewed isolated regional_population.hpp, CMake registration, tests and domain documentation. No blocking defect for explicit foundation-only scope. Reviewer independently built MSVC 19.51 Release target in var/build/regional-registry-independent-review and ran REGIONAL_POPULATION_FOUNDATION_OK. Root inspected candidate implementation and tests separately.

Admission checks duplicate region, seed, nonempty population, global individual/region limits, per-region/global reservation limits, ledger restore validation and cross-region actor/resource identity conflicts before map insertion/counter commit. Detached snapshots prevent mutation of retained state. No erase or reservation expansion API. Camera admission and activation rejected. Mandatory legacy import counts used keys within reservation. Full history replay/update continues, new keys fail without eviction.

Integration limits are explicit: no physical bounds/host wiring, actor-session contact binding/reach, edible-site depletion sync, accepted-delta relief or registry save/load. Separate host ledgers would escape these budgets. Native exact eight-actor continuation is existing-host regression evidence only, not registry integration evidence.

No exhaustive allocation-failure injection. Initial exact departure/return test used zero elapsed time; elapsed dormancy separately checks retained routes/needs. Worker asked to strengthen complete expected elapsed-dormancy comparison before collection.

Final candidate strengthened elapsed dormancy and return to full expected Population snapshot plus ledger equality; root inspected added comparisons before collection. Candidate f5fc982 collected as e22bc62.
