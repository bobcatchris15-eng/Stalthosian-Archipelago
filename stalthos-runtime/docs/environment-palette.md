# Environment Palette & Scenery Materials

`domain/environment_palette.hpp` defines deterministic, engine-independent scenery material parameters and linear color/roughness palettes for The Stalthosian Archipelago runtime and reference scenes.

## Overview & Roles

The environment palette provides bounded physical and authoring material properties for seven key scenery roles:
- **`DryBank`**: Loamy earthy bank above the high-water line. Matte, muted brownish-olive silt and dry soil. Roughness `[0.82, 0.94]`.
- **`WetBank`**: Moist riparian soil along the water boundary. Visibly darkened albedo due to moisture absorption and lower specular roughness `[0.45, 0.65]`.
- **`CreekBed`**: Submerged gravel, sand, and bed substrate under the water channel. Opaque solid PBR substrate, submerged pebble roughness `[0.50, 0.75]`.
- **`Bluff`**: Resistant sedimentary / mudstone cliff walls. Weathered olive-khaki mineral tones, fractured rough stone `[0.75, 0.92]`.
- **`Rock`**: Detached boulders, caprock, and limestone outcrops. Muted mineral grey with subtle weathering undertones, roughness `[0.70, 0.88]`.
- **`WoodyDebris`**: Weathered driftwood, branch litter, and fallen bark. Desaturated weathered grey-brown, fibrous matte bark roughness `[0.80, 0.95]`.
- **`WaterSurface`**: Transparent and refractive fluid boundary mesh. Subdued natural freshwater tone (absorptive greenish-teal), low specular roughness `[0.08, 0.22]`, refraction delta ~0.04 - 0.06, and transmission amount ~0.85 with `isWaterShader = true`.

## Key Principles & Design Boundaries

1. **Restrained Naturalistic Colors (No Neon)**: All color values are linear RGBA with strictly clamped albedos to prevent bright flat neon or oversaturated primaries.
2. **Deterministic Seed Math**: Uses the standard 64-bit splitmix hasher (`hashMix`), consistent with `ReferenceLandscape` and `StalthosState`. The same `(role, seed)` produces identical parameter bytes.
3. **Restrained Seed Variation**: Hue and albedo jitter stay within tight, plausible physical bounds (approx. ±8% luminance and subtle warm/cool shifts).
4. **Water Surface vs Creek Bed Separation**: The fluid surface is physically distinct from the bed substrate:
   - Water surface uses `isWaterShader = true` (`wi::scene::MaterialComponent::SHADERTYPE_WATER`), high transmission, and low roughness.
   - Creek bed uses standard opaque PBR (`isWaterShader = false`), zero refraction/transmission, and submerged coarse sediment roughness.
5. **No Photographic Realism Claims**: These parameters represent a bounded, plausible heuristic baseline for real-time rasterization in Wicked Engine; they do not simulate optical radiative transfer, complex microfacet scattering, or full sediment petrology.

## Wicked Host Wiring Instructions

In `stalthos-runtime/engine/main.cpp` (or future runtime scene assemblers):

```cpp
#include "domain/environment_palette.hpp"

// Example: Applying environment palette parameters to a Wicked material component
void ApplySceneryMaterial(wi::scene::MaterialComponent& material,
                          stalthos::domain::SceneryMaterialRole role,
                          std::uint64_t worldSeed) {
    using namespace stalthos::domain;
    const auto params = EnvironmentPalette::evaluate(role, worldSeed);

    material.SetBaseColor(DirectX::XMFLOAT4(
        params.baseColor.r,
        params.baseColor.g,
        params.baseColor.b,
        params.baseColor.a
    ));
    material.SetRoughness(params.roughness);
    material.SetMetalness(params.metalness);
    material.SetRefractionAmount(params.refractionAmount);
    material.SetTransmissionAmount(params.transmissionAmount);

    if (params.isWaterShader) {
        material.shaderType = wi::scene::MaterialComponent::SHADERTYPE_WATER;
    } else {
        material.shaderType = wi::scene::MaterialComponent::SHADERTYPE_PBR;
    }
}
```

For vertex-colored terrain meshes like `ReferenceLandscape` in `main.cpp`:
- Evaluate `DryBank`, `WetBank`, `CreekBed`, `Bluff`, and `Rock` base colors.
- Pack RGB linear base colors into `wi::Color(r * 255, g * 255, b * 255)` for vertex color interpolation across habitat boundaries.

## Standalone Offline Verification

The tests run completely offline without Wicked Engine, DirectX, or Blender dependencies. They compile under standard C++17 with MSVC `/W4 /WX /permissive-`:

```powershell
cmd.exe /c "call `"%ProgramFiles(x86)%\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat`" && cl /nologo /std:c++17 /O2 /W4 /WX /EHsc /permissive- /Fe:var\tests\test_environment_palette.exe stalthos-runtime\tests\environment_palette.cpp"
.\var\tests\test_environment_palette.exe
```
