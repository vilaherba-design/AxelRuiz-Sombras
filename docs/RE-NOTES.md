# FO4VR renderer notes

Starting facts come from CSAH VR's `findings.md` (brunocatani, GPL-3.0). Addresses are absolute with the
image base 0x140000000 unless written `base+`. Status: **unverified** until checked in our own decrypted
Fallout4VR.exe 1.2.72.

| # | Fact | Source | Status |
|---|---|---|---|
| 1 | Deferred, tiled lighting: BSLightingShader writes a GBuffer; a 3-dispatch compute pass composes light (FUN_142848e70) | CSAH F13, F17 | unverified |
| 2 | Two BSLightingShader objects: "Lighting" 0x14689b410 (vtable 0x1430bbdb8) is not the renderer; "VR Extended" 0x14689b8a0 (vtable 0x1430bf3c8, BeginTechnique 0x14291DA20) is | CSAH F18 | unverified |
| 3 | Batched draws skip BeginTechnique; every path ends in Renderer::SetShaders base+0x1D92C00 | CSAH F15 | unverified |
| 4 | BSGraphics renderer wrapper at 0x146235ab0, PSSetShader at wrapper vtable +0x1E0, delegates to the real context | CSAH F15 | unverified |
| 5 | All shaders are created before PostPostLoad; the device must be caught earlier (D3D11CreateDevice import or DllMain) | CSAH F9 | unverified |
| 6 | Shaders012_VR.fxp: ~2283 BSLightingShader pixel shaders, 105 input layouts, all with EYEINDEX (instanced stereo) | CSAH F10 | unverified |
| 7 | GBuffer: MRT0 albedo R8G8B8A8_UNORM_SRGB, MRT1 normals R16G16_UNORM, MRT2 material, MRT3 emissive, MRT4 extra SRGB, MRT5 motion R16G16_FLOAT | CSAH F20 | unverified |
| 8 | GBuffer pixel shaders bind 5 RTs, forward ones 1, depth prepass 0-1: RT count separates them | CSAH F11 | unverified |
| 9 | VR composite render target indices 99, 100, 102, 103 (flat: 28, 29, 32, 33) | CSAH F20 | unverified |

## Phase 0 method

1. Decrypt the SteamStub image (same technique as the HBCM port, Variant 3.1 x64) in the cloud; never commit it.
2. Check each address above by its code bytes and callers; record the real values here.
3. Extract the VR .fxp, list every shader with its DXBC hash, input signature and outputs into a catalog
   (`tools/`), and commit only the catalog, never the shader bytes.
