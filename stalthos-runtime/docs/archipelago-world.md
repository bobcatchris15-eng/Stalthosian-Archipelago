# The Stalthosian Archipelago — Coarse Archipelago World Foundation

## Scope and Intent

This component (`stalthos::world::Archipelago` in `stalthos-runtime/domain/archipelago_world.hpp`) implements the coarse actual world foundation for the Stalthosian Archipelago. It aligns with the design thesis set forth in `Rauken-Archipelago/docs/geography.md` and provides continuous coarse macro terrain elevations, continuous moisture distributions, and regional demographics across the archipelago.

## Key Design Decisions & Geological Thesis

1. **Shared Geological Structure, Not Independent Islands:**
   - As mandated by `geography.md`, the archipelago is treated as a drowned continental upland rather than four disconnected procedural noise blobs.
   - A regional tectonic strike/dip corrugation (SW-NE structural trend, faulted valleys and resistant ridges) extends continuously across the entire basin.
   - The four highland masses (Main Island, Rainridge, Stone Island, Outer Reach) are continuous uplift swells superimposed upon this shared structural field, bounded by a continuous subsidence sea floor.
   - Drowned valleys form rias and inter-island channels sitting below sea level ($y < 0\text{ m}$).

2. **Provisional Canonical Extents & Centroids:**
   - **World Bounds:** Fixed at $\pm 14000\text{ m}$ ($\approx 28\times 28\text{ km}$ total marine bounding extent).
   - **Main Island (`1001`):** Center $(0, 0)$, span $\approx 7.5\text{ km}$ ($7\text{--}8\text{ km}$ canonical range).
     - *Note:* The local detailed reference watershed patch (e.g. Splitstone Valley / `ReferenceLandscape`) corresponds around here, though it is not yet dynamically terrain-stitched.
   - **Rainridge (`1002`):** Center $(-6200, 4500)$, span $\approx 5.5\text{ km}$ ($5\text{--}6\text{ km}$). Wetter climate ($0.88$ moisture), high canopy ($0.78$), high shelter ($0.82$).
   - **Stone Island (`1003`):** Center $(5800, -4200)$, span $\approx 4.5\text{ km}$ ($4\text{--}5\text{ km}$ flexible platform). Characterized by rocky limestone/karst platform conditions ($0.42$ moisture, $0.48$ shelter).
   - **Outer Reach (`1004`):** Center $(6800, 5200)$, span $\approx 5.5\text{ km}$ ($5\text{--}6\text{ km}$). More open/arid conditions ($0.28$ moisture, $0.22$ canopy), separated by deep inter-island marine passage.

3. **Determinism and Input Seed Variation:**
   - No `std::hash` or global random engine is used; all pseudo-random perturbations use a deterministic 64-bit integer mixing function (`mix`).
   - Changing the input seed introduces bounded low-amplitude surface undulation ($\le 15\text{ m}$) without disrupting the macro-geological island masses or drowning channel separation.

4. **Demographics and Population Counts:**
   - `coarsePopulation` represents coarse bounded demographic capacity (order of hundreds to a couple thousand), not individual instantiated agent simulations or conserved ledgers.

5. **Renderer Status and Footprint:**
   - **Honest status:** The runtime renderer footprint remains focused on the local detailed patch until streaming terrain integration is introduced.
   - Coarse macro elevation and moisture fields provide the world-scale coordinate frame, regional metadata, and global boundary references needed for macro simulation and navigation.

## API Summary

```cpp
namespace stalthos::world {

struct Region {
    RegionId id;
    const char* name;
    double centerX;
    double centerZ;
    double referenceSpanMeters;
    double moisture;
    double canopy;
    double shelter;
    uint32_t coarsePopulation;
};

class Archipelago {
public:
    static constexpr double worldBoundsMeters = 14000.0;

    explicit Archipelago(uint64_t seed);
    const std::array<Region, 4>& regions() const;
    double elevation(double x, double z) const;
    double moisture(double x, double z) const;
};

} // namespace stalthos::world
```

## Testing & Atlas Export

The standalone test harness `stalthos-runtime/tests/archipelago_world.cpp` actively verifies:
- Exactly 4 stable regions with canonical IDs (`1001`–`1004`) and names.
- Main Island centered at $(0, 0)$.
- Spaced centroids with inter-island distances $> 5\text{ km}$.
- Region centroids sitting safely above sea level ($> 0\text{ m}$).
- Inter-island channels sitting below sea level ($< 0\text{ m}$).
- Strict coordinate validation: `std::invalid_argument` on non-finite coordinates, `std::out_of_range` on queries beyond $\pm 14000\text{ m}$.
- Bit-identical determinism for identical seeds, and bounded surface variation across distinct seeds.
- Optional CLI export via `--atlas <path>` outputting a 64x64 resolution JSON grid with complete elevation, moisture, and regional metadata.
