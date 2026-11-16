# Plan

The full plan (approved by Axel on 2026-11-16) is in the project's Claude Doc
"Plan: framework de shaders propio para Fallout 4 VR". Summary:

| Phase | Goal | Done when |
|---|---|---|
| 0 | Verify the renderer facts of RE-NOTES in a decrypted exe; dump and catalog the VR shaders | Every fact marked verified or wrong |
| 1 | Invisible core: early D3D11 hooks, shader catalog in the log, identical-copy shader replacement | No visual change, no fps loss, log shows replaced shaders |
| 2 | First visible effect: screen-space (contact) shadows, both eyes consistent | Axel's test in Pruebas7.1 |
| 3 | Materials: per-permutation GBuffer pixel shaders (PBR, parallax) | Axel's test |
| 4 | Advanced lighting: SSGI/AO, skylighting, IBL within a 90 Hz budget | Axel's test |
| 5 | Release: Nexus, Wabbajack, credits, zip without nested archives | Published |
