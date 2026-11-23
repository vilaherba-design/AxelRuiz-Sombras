# AguaDiag (water diagnostic)

Read-only F4SE plugin for Fallout 4 VR 1.2.72 used to study the "bright blue sheet" water bug: at some view
angles parts of the loaded-cell water stop showing the real-time reflection and show a flat, bright surface.
It installs no hook. A background thread reads the game's water system every 100 ms through `ReadProcessMemory`
and logs every changed line to `Documents\My Games\Fallout4VR\F4SE\AguaDiag.log`. F9 writes a `MARK` line and a
full snapshot.

Build (any x64 Windows C++ compiler; from Linux with zig):

    python3 -m ziglang c++ -target x86_64-windows-gnu -O2 -std=c++20 -shared -s -o AguaDiag.dll AguaDiag.cpp -lole32 -lshell32 -luuid

## What is known (decrypted exe, sha256 680de0e1..., RVAs)

- `TESWaterSystem` constructor at 0x7cb220 stores `this` at **0x5932540** (76 code references).
  Arrays (data, +0x10 = count): water objects +0x18 (0x70 bytes each), reflections +0x30 (0x88), a per-water-form
  array +0x48 (0x48), displacements +0x60. Lock at +0x118. `uMaxExteriorWaterReflections` is copied to +0x98 and
  `uMaxInteriorWaterReflections` to +0x9c, and **nothing in the water code reads +0x98 again**.
- The [Water] settings `fExteriorWaterReflectionThreshold`, `fInteriorWaterReflectionThreshold`,
  `fWaterGroupHeightRange`, `bForceLowDetailReflections` and `bUseWaterLOD` have **no code reference at all**: dead.
  That is why changing them did nothing (tested in game on 2026-11-23).
- Add-water function 0x7ccb20 (system, shape, water form, height, ...): makes the 0x70-byte water object
  (plane at +0x10, shape +0x20, form +0x30, reflection +0x38, flags +0x68). If the water shader property
  (`[geometry+0x178]`, flags at +0x70, plane at +0x78) has flag 0x40, it looks for an existing reflection whose plane
  matches within **0.01** on all four components (0x7ce350). If none matches it creates a new reflection object
  (0x7c9d80, 0x88 bytes, flags word at +0x10, plane at +0x14) with flags 0x408 (0x48 when
  `bForceHighDetailReflections=1`; cube-map variants 0x2/0x4/0xA set property flag 0x2000 and need
  `bUseCubeMapReflections=1`).
- Remove-water function 0x7cd750. Per-object visibility test 0x7cde20.

## Open question the log should answer

Which field differs between a water object that shows the real reflection and one that shows the flat sheet:
the reflection it points to (+0x38), the reflection's flags (+0x10), or the property flags (+0x70).
