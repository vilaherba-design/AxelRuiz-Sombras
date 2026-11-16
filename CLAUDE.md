# CLAUDE.md

AxelRuiz-Sombras: C++23 F4SE plugin, a shader framework for Fallout 4 VR 1.2.72 (Community Shaders-like).
CommonLibF4VR (ArthurHub) is a submodule at `lib/CommonLibF4VR`, pinned to 9057052, VR only. GPL-3.0.
User-facing docs in Spanish, code and comments in English.

- Phased plan: `docs/PLAN.md`. Renderer facts and their verification status: `docs/RE-NOTES.md`.
  Never treat an address as known until RE-NOTES marks it verified in our decrypted exe.
- Fallout4VR.exe is SteamStub-encrypted on disk: read code only from a decrypted image. The exe, decrypted
  images and shader dumps are game files: never commit them (`re/` is git-ignored).
- F4SE VR reports runtime 1.10.138 in Query: accept it and 1.2.72.
- Grep every CommonLibF4VR REL ID the plugin reaches against the VR address library csv; IDs can exist and still
  point at the wrong object (verify NiRTTI via its constructor).
- Coexist with ReShade (d3d11.dll in the game root) and BlurAway VR (DLAA): chain hooks, never overwrite them.
- Mark functions by thread in comments: `[game]`, `[render]`, `[loader]`.
- Phase 1 core: `src/Render.cpp` (device via the D3D11CreateDeviceAndSwapChain IAT, vtable-slot hooks by index with
  untyped pointers, no d3d11.h; `SetShaders` call sites via write_call). Off-game tests: `tests/run.sh`.
- Build on Axel's PC through Remote Control with `build.ps1` (temp dir, zip to C:\Users\cereBRO\Modding, nothing left behind).
