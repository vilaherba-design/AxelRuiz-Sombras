# FO4VR renderer notes

Checked on 2026-11-16 in a decrypted image of Axel's Fallout4VR.exe 1.2.72 (sha256 680de0e1...85f9, SteamStub
removed in the cloud, never committed). Addresses are RVAs (add 0x140000000 for the absolute address).
Starting facts came from CSAH VR's `findings.md` (brunocatani, GPL-3.0); several turned out wrong.

## Verified

| Fact | Evidence |
|---|---|
| The device is created in one place: `D3D11CreateDeviceAndSwapChain` (only d3d11 import, IAT 0x2c4d3f0, thunk 0x29929b2) is called once, at 0x1d9c0b8 | import table + call scan |
| Right after that call the game stores the **immediate context** at 0x6235ab0 (and a copy at 0x6235ab8) | `mov [rip→0x6235ab0], rax` at 0x1d9c152 |
| `SetShaders` at 0x1d92c00 (6 callers) sets VS/HS/DS/PS on the **raw** D3D11 context (vtable +0x58 VSSetShader, +0x1E0 HSSetShader, +0x200 DSSetShader, +0x48 PSSetShader), caching the current ones in the renderer state (+0x1ee0+0x688/0x690/0x698). Game shader objects keep the D3D11 shader pointer at +0x08 | disassembly |
| Shader families (RTTI vtables): BSShader 0x30b6988, BSLightingShader 0x30bbdb8, **BSDFPrePassShader 0x30b8c68**, BSDFLightShader 0x30bf3c8, BSDFCompositeShader 0x30bf228, BSEffectShader 0x30bc738, BSWaterShader 0x30bae98, BSSkyShader 0x30bf5f8, BSUtilityShader 0x30bd988, BSDistantTreeShader 0x30baa28, BSImagespaceShader 0x30be958, BSImagespaceComputeShader 0x30bebf8, BSComputeShader 0x30c0c08, BSParticleShader 0x30bf548, BSBloodSplatterShader 0x30bf158, BSFaceCustomizationShader 0x30ba618 | `tools`-side RTTI scan |
| The VR shaders live in `Data\Fallout4_VR - Shaders.ba2` → `ShadersFX\Shaders012_VR.fxp` (exe pattern `SHADERSFX/Shaders%03u_VR.fxp`). `Fallout4 - Shaders.ba2` holds the flat `Shaders011.fxp`, unused by VR | exe strings + archive listing |
| Shaders012_VR.fxp: 3819 DXBC blobs (1982 unique): 2283 PS, 1293 VS, 105 CS, 69 HS, 69 DS. 91 distinct PS input layouts | `tools/dxbc_catalog.py`, `docs/catalog/Shaders012_VR.csv` |
| **Stereo = instanced, both eyes in one draw**: 1168 VS read `SV_InstanceID`, 1167 write `SV_ClipDistance`/`SV_CullDistance` (eye split by clip planes) and 794 pass `EYEINDEX` to the PS; 1876 of 2283 PS read `EYEINDEX` | catalog |
| PS by number of render targets: 0→108, 1→1345, 2→338, 4→32, 5→141, 6→318. The 5-6 target ones are the GBuffer writers | catalog |

## Corrected (CSAH notes were wrong)

- 0x689b410 / 0x689b8a0 are 32-bit values read with `mov ecx/edx, dword`, not shader object pointers.
- Vtable 0x30bf3c8 is **BSDFLightShader** (the deferred light pass), not a second "VR Extended" BSLightingShader.
- The renderer is not a wrapper with PSSetShader at +0x1E0: +0x1E0 is the standard `HSSetShader` slot of
  ID3D11DeviceContext.
- The VR file is Shaders012_VR.fxp; Shaders011.fxp is the flat game's.

## Still to check at runtime (phase 1, by logging)

- GBuffer formats and order (CSAH: albedo sRGB, normals R16G16, material, emissive, extra sRGB, motion R16G16F).
- Which family draws the GBuffer (expected BSDFPrePassShader) and how batched draws reach `SetShaders`.
- Deferred composite compute passes and their render target indices.

## Consequences for phase 1

1. Catch the device at `F4SEPlugin_Load` by patching the `D3D11CreateDeviceAndSwapChain` IAT entry
   (one caller), chaining to whatever ReShade/OpenComposite installed. No DllMain tricks needed.
2. Identify shaders at `Create*Shader` by DXBC hash against our catalog, and swap them either there
   (simplest, one-time) or in a `SetShaders` detour (per draw, for conditional effects).
3. Every replacement shader must keep the instanced-stereo contract: same `SV_InstanceID` → clip-distance eye
   split in the VS and `EYEINDEX` use in the PS.
