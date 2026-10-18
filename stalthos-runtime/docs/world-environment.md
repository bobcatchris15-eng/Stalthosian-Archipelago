# World Environment

## Overview
The `stalthos::environment` module provides a bounded, pure native C++17 implementation for computing local environmental conditions within The Stalthosian Archipelago. This includes the deterministic evaluation of weather, temperature, wetness, flood risk, and other site-specific phenomena.

The module operates without engine dependencies and evaluates deterministically based on an absolute shared calendar and a regional hash, preserving absolute stability under the same world seed, region ID, calendar time, and local site properties.

## Implementation
- Calculates `Conditions` via the `sample()` function based purely on `calendarSeconds`, `worldSeed`, `RegionId`, and `Site` properties.
- **Weather** is strictly classified into `Clear`, `Overcast`, `Rain`, or `Storm` which derives safely from bounded periodic rainfall thresholds.
- Environmental outputs like wind, temperature, and wetness are computed coherently. For instance, temperature features diurnal, seasonal, and localized shade adjustments, but it remains a purely provisional, uncalibrated natural-history illusion rather than true historical climate data.
- Input `calendarSeconds` is safely bounded via a macro period wrap before trigonometric calls to protect against double precision drift at extremely large elapsed times.
- Does not contain atmospheric/hydrology solvers or historical event logs.
- Outputs are finite and normalized `[0,1]`, except for explicitly dimensional fields (like temperature and wind speed). Salinity determines drinkability (0 = drinkable).

## Actual Tests
- Validates that outputs map strictly and repeatedly exactly.
- Asserts that extreme time constraints (e.g., `0`, huge finite values) remain deterministic and bounded.
- Asserts exception throws on infinite, negative, or `NaN` time inputs.
- Validates wet/dry relative difference constraints against shelter and moisture bounds.
- Validates salinity drinkability.
- Standalone offline tests confirmed working via standard `g++ -std=c++17 -O2` release build constraints without runtime engine bindings.

## Pending Validation
- Binding the outputs within NPC routine consumers (e.g., thermal regulation or shelter-seeking).
- Wiring this data into the engine visual renderer (clouds, rain particles, wind).
- Polishing performance of massive multi-site sampling should it eventually exceed runtime budgets on target hardware.