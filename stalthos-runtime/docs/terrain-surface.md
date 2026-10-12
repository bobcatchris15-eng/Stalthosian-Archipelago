# Runtime terrain surface detail

`engine/terrain_surface.hpp` adds a bounded PBR material to the existing terrain mesh. It assigns UV coordinates from world X/Z at a 3 m repeat interval, so texture density stays stable across the full ground plane. One 512×512 sRGB base-color texture is generated from the runtime seed and shared by all terrain subsets. It uses five smooth periodic value-noise scales with restrained warm neutral variation. The material's near-white base color and texture preserve the existing habitat vertex colors, including their darker creek and riparian bank values.

The helper uses Wicked Engine's `wi::texturehelper::CreateTexture`, `MaterialComponent::textures[BASECOLORMAP].resource.SetTexture`, mesh `vertex_uvset_0`, and `CreateRenderData`. Texture generation has fixed dimensions and iteration counts; it has no external asset or runtime simulation dependency. Roughness is set to 0.96 and metalness to zero. The returned `TerrainSurfaceResult` identifies applied, missing-mesh, unavailable-device, and texture-create-failure outcomes and reports UV vertex count and texture dimensions/bytes on success. This provides actual sampled surface color detail, not a normal map or geometric displacement.

The current verified texture-helper API creates a single mip level; the repeat scale and texture resolution are bounded, but distant shimmer should be checked in the integrated capture.

The helper should be called after the terrain's vertex colors have been populated and its initial render data created. A runtime image capture is still required to assess whether the detail reads at player and overview distances; this implementation alone makes no visual acceptance or frame-rate claim.
