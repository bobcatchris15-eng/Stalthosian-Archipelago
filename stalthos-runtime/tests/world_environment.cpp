#include "../domain/world_environment.hpp"
#include <iostream>
#include <limits>
#include <cmath>

using namespace stalthos::environment;

void test_exact_repeat() {
    auto c1 = sample(12345, 1, 100000.0, {0.5, 0.2, 0.8, 0.0});
    auto c2 = sample(12345, 1, 100000.0, {0.5, 0.2, 0.8, 0.0});
    
    if (c1.dayFraction != c2.dayFraction || c1.seasonFraction != c2.seasonFraction ||
        c1.cloud != c2.cloud || c1.rainfall != c2.rainfall || 
        c1.windMetersPerSecond != c2.windMetersPerSecond || c1.temperatureC != c2.temperatureC ||
        c1.wetness != c2.wetness || c1.floodRisk != c2.floodRisk ||
        c1.drinkable != c2.drinkable || c1.weather != c2.weather) {
        throw std::runtime_error("Outputs are not exactly deterministic and repeatable.");
    }
}

void test_time_zero() {
    auto c = sample(12345, 1, 0.0);
    if (!std::isfinite(c.dayFraction) || !std::isfinite(c.seasonFraction) ||
        !std::isfinite(c.cloud) || !std::isfinite(c.rainfall) || 
        !std::isfinite(c.windMetersPerSecond) || !std::isfinite(c.temperatureC) ||
        !std::isfinite(c.wetness) || !std::isfinite(c.floodRisk)) {
        throw std::runtime_error("Time 0 should not produce non-finite outputs.");
    }
}

void test_huge_time() {
    auto c = sample(12345, 1, std::numeric_limits<double>::max());
    if (!std::isfinite(c.dayFraction) || !std::isfinite(c.seasonFraction) ||
        !std::isfinite(c.cloud) || !std::isfinite(c.rainfall) || 
        !std::isfinite(c.windMetersPerSecond) || !std::isfinite(c.temperatureC) ||
        !std::isfinite(c.wetness) || !std::isfinite(c.floodRisk)) {
        throw std::runtime_error("Huge finite time should be bounded safely and produce finite outputs.");
    }
}

void test_invalid_time() {
    bool caught = false;
    try {
        sample(12345, 1, -10.0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    if (!caught) throw std::runtime_error("Failed to throw on negative time.");

    caught = false;
    try {
        sample(12345, 1, std::numeric_limits<double>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    if (!caught) throw std::runtime_error("Failed to throw on NaN time.");

    caught = false;
    try {
        sample(12345, 1, std::numeric_limits<double>::infinity());
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    if (!caught) throw std::runtime_error("Failed to throw on infinity time.");
}

void test_site_bounds() {
    const Site invalid[] = {
        {-0.1, 0.5, 0.5, 0.0}, {1.1, 0.5, 0.5, 0.0},
        {0.5, -0.1, 0.5, 0.0}, {0.5, 1.1, 0.5, 0.0},
        {0.5, 0.5, -0.1, 0.0}, {0.5, 0.5, 1.1, 0.0},
        {0.5, 0.5, 0.5, -0.1}, {0.5, 0.5, 0.5, 1.1},
        {std::numeric_limits<double>::quiet_NaN(), 0.5, 0.5, 0.0},
        {0.5, 0.5, 0.5, std::numeric_limits<double>::infinity()}
    };
    for (const auto& site : invalid) {
        bool caught = false;
        try { (void)sample(12345, 1, 100.0, site); }
        catch (const std::invalid_argument&) { caught = true; }
        if (!caught) throw std::runtime_error("Invalid site input was accepted.");
    }
}

void test_finite_outputs_and_ranges() {
    auto c = sample(12345, 1, 1000.0);
    if (c.dayFraction < 0 || c.dayFraction >= 1.0) throw std::runtime_error("Day fraction out of bounds.");
    if (c.seasonFraction < 0 || c.seasonFraction >= 1.0) throw std::runtime_error("Season fraction out of bounds.");
    if (c.cloud < 0 || c.cloud > 1.0) throw std::runtime_error("Cloud out of bounds.");
    if (c.rainfall < 0 || c.rainfall > 1.0) throw std::runtime_error("Rainfall out of bounds.");
    if (c.windMetersPerSecond < 0) throw std::runtime_error("Wind speed must be >= 0.");
    if (c.wetness < 0 || c.wetness > 1.0) throw std::runtime_error("Wetness out of bounds.");
    if (c.floodRisk < 0 || c.floodRisk > 1.0) throw std::runtime_error("Flood risk out of bounds.");

    if (c.weather == Weather::Rain && c.rainfall <= 0.0) {
        throw std::runtime_error("Weather Rain should have rainfall > 0.");
    }
    if (c.weather == Weather::Storm && c.rainfall <= 0.0) {
        throw std::runtime_error("Weather Storm should have rainfall > 0.");
    }
}

void test_wet_dry_differences() {
    // Let's sample a time when it is raining to test flood risk and wetness
    // We can just try different times until we get a storm or rain
    double time = 0;
    Conditions cWet;
    while (true) {
        cWet = sample(12345, 1, time, {0.0, 0.0, 1.0, 0.0}); // High moisture
        if (cWet.rainfall > 0.5) break;
        time += 1000.0;
    }

    auto cDryRidge = sample(12345, 1, time, {0.0, 0.8, 0.0, 0.0}); // Low moisture, high shelter (ridge)
    
    if (cWet.floodRisk <= cDryRidge.floodRisk) {
        throw std::runtime_error("High rain + high moisture + low shelter should have higher flood risk than dry ridge.");
    }
}

void test_salinity_rule() {
    auto c1 = sample(12345, 1, 100.0, {0.0, 0.0, 0.5, 0.0});
    if (!c1.drinkable) throw std::runtime_error("Salinity 0 should be drinkable.");

    auto c2 = sample(12345, 1, 100.0, {0.0, 0.0, 0.5, 0.01});
    if (c2.drinkable) throw std::runtime_error("Salinity > 0 should not be drinkable.");
}

void test_periodicity() {
    constexpr double DAY_SECONDS = 86400.0;
    auto c1 = sample(12345, 1, 100.0);
    auto c2 = sample(12345, 1, 100.0 + DAY_SECONDS * 90.0 * 7.0); // Exact macro period wrap

    if (std::abs(c1.dayFraction - c2.dayFraction) > 1e-5) throw std::runtime_error("Day fraction not periodic properly.");
    if (std::abs(c1.seasonFraction - c2.seasonFraction) > 1e-5) throw std::runtime_error("Season fraction not periodic properly.");
}

void test_seed_and_region_variation() {
    const auto baseline = sample(12345, 1, 345678.0);
    const auto otherSeed = sample(12346, 1, 345678.0);
    const auto otherRegion = sample(12345, 2, 345678.0);
    if (baseline.cloud == otherSeed.cloud && baseline.rainfall == otherSeed.rainfall &&
        baseline.windMetersPerSecond == otherSeed.windMetersPerSecond)
        throw std::runtime_error("World seed does not affect regional conditions.");
    if (baseline.cloud == otherRegion.cloud && baseline.rainfall == otherRegion.rainfall &&
        baseline.windMetersPerSecond == otherRegion.windMetersPerSecond)
        throw std::runtime_error("Region does not affect regional conditions.");
}

int main() {
    try {
        // Category changes must not cause physical wind/temperature jumps.
        // Check sunrise/sunset explicitly and a complete two-week weather cycle.
        for(double boundary:{21600.0,64800.0}) {
            const auto before=sample(12345,1,boundary-.1,{1,0,.5,0});
            const auto after=sample(12345,1,boundary+.1,{1,0,.5,0});
            if(std::abs(after.temperatureC-before.temperatureC)>.001)
                throw std::runtime_error("Canopy temperature jumps at day boundary.");
        }
        auto prior=sample(12345,1,0);
        for(int second=600;second<=86400*14;second+=600) {
            const auto current=sample(12345,1,second);
            if(std::abs(current.windMetersPerSecond-prior.windMetersPerSecond)>.2)
                throw std::runtime_error("Wind jumps at weather category boundary.");
            prior=current;
        }
        test_exact_repeat();
        test_time_zero();
        test_huge_time();
        test_invalid_time();
        test_site_bounds();
        test_finite_outputs_and_ranges();
        test_wet_dry_differences();
        test_salinity_rule();
        test_periodicity();
        test_seed_and_region_variation();
        std::cout << "All environment tests passed.\n";
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
