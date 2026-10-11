# External tool inventory

Worktree: `E:\Stalthosian Archipelago\var\worktrees\luna-tool-audit`
Base revision: `ca7409f45b034adc8067a82fda1d727141c57400`
Probe date: 2026-10-11

## Installed and directly verified

| Tool | Executable/path | Probe and result | Toolkit discovery |
|---|---|---|---|
| Blender | `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe` | `blender.exe --version` -> `Blender 5.2.0 LTS` (build date 2026-07-14) | `stalthos doctor` finds it via the hard-coded standard path; `Get-Command blender` reports missing because it is not on PATH. |
| FFmpeg | `C:\ProgramData\chocolatey\bin\ffmpeg.exe` | `ffmpeg -version` -> `9.0.2-essentials_build.www.gyan.dev` | `stalthos doctor` finds it and parses `9.0.2-essentials`. |
| glTF-Transform CLI | `C:\Users\Chris\AppData\Roaming\npm\gltf-transform.ps1`; package at `%APPDATA%\npm\node_modules\@gltf-transform\cli` | `node <package>\bin\cli.js --version` -> `4.5.1`; package.json version -> `4.5.1`. `Get-Command gltf-transform` finds the PS1 shim. | `optimize/tools.py` candidate `gltf-transform` supports this command, but `doctor` does not probe/report it. The shim probe emitted no visible output; direct Node invocation did. |
| Git | `C:\Program Files\Git\cmd\git.EXE` | Found via `Get-Command`; doctor finds it. Version not probed. | Reported by doctor without version. |

## Missing from PATH / standard candidates

PowerShell `Get-Command` found no `gltfpack`, `meshoptimizer`, `compressonatorcli`, `compressonator`, `ktx`, `magick`, `gltf_validator`, or `gltf-validator`. Toolkit `doctor` independently reports `gltfpack`, `meshoptimizer`, `compressonator`, `ktx`, and `magick` unavailable. No known likely installation path was found for gltfpack or Compressonator at the two standard Program Files locations checked.

`gltf_validator` is registry-supported by `optimize/tools.py` but not checked by doctor. `gltf-transform` is also registry-supported but not checked by doctor. `meshoptimizer` is probed as a standalone command; the actual user-facing simplifier is `gltfpack` from the meshoptimizer project, so a missing standalone `meshoptimizer` executable does not alone establish that its library/code is unavailable.

## Discovery issues and actionable installation candidates

- Blender is installed and correctly found by toolkit fallback paths despite not being on PATH. Setting `STEALTHOS_BLENDER` to the verified executable is an explicit portable option.
- `doctor` omits supported tools `gltf_transform` and `gltf_validator`, and its Blender row provides only a path (no version). Add them to the doctor report for complete inventory; version-probe Blender separately if desired.
- `doctor` parses the FFmpeg version as `9.0.2-essentials`, while direct output includes the full build string. It is sufficient as a version indicator.
- `gltfpack`: official meshoptimizer project/release source: https://github.com/zeux/meshoptimizer (release Windows binaries or build the CLI and put `gltfpack.exe` on PATH). The toolkit hint already points there.
- AMD Compressonator: official project/downloads: https://gpuopen.com/compressonator/ (put `compressonatorcli.exe` on PATH). The toolkit hint already points there.
- KTX-Software: official project/releases: https://github.com/KhronosGroup/KTX-Software (install Windows CLI with `ktx.exe` on PATH).
- ImageMagick: official Windows install instructions: https://imagemagick.org/script/download.php#windows (install `magick.exe` and expose it on PATH). FFmpeg is already installed for montage/review use.
- glTF-Transform: official project: https://github.com/donmccurdy/glTF-Transform; installed npm CLI 4.5.1 is usable by Node, but omitted from doctor.
- glTF Validator: official project: https://github.com/KhronosGroup/glTF-Validator; can be installed through the documented npm package (`npm i -g gltf-validator`) and exposed as `gltf_validator`/`gltf-validator`.

No installs or source edits were made. Probes were limited to version/help queries; no GPU or asset workload was launched.
