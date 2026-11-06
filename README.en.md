[Español](README.md) | **English**

# SA DirectX 3.0 — Fixes & Additions

ASI plugins, modified shaders and settings for **GTA San Andreas (PC 1.0 US)** with the
**SA DirectX 3.0** preset (Makarus, ENBSeries 0.430) and Inari Lú's *Unofficial Update*.
Plugins, fixes and integration by **Oscar Vidal**.

> **Download:** the ready-to-install package is in [Releases](../../releases).
> Full instructions: [`docs/README_EN.txt`](docs/README_EN.txt) · [`docs/README_ES.txt`](docs/README_ES.txt).

## Included plugins

| Plugin | What it does |
|---|---|
| `ShadowCasterFix` | Stops complex shadows from disappearing when the object casting them leaves the camera view (ENB's first cascade). |
| `LucesNocturnasENB` | Light coronas drawn after ENB composition, local lighting on the ground and surfaces, headlights, brake lights, light beams, lit rain, and lights from gunfire, explosions and fire. |
| `HumoLucesENB` | Smoke lighting and a minimum light level for grass. |
| `enbseries_interiores` | Interior detection and shader fixes (including the sun visibility check for dawn god rays). |
| `ENBShaderCache` | Reuses compiled effects: later launches go from ~40 s to ~2-3 s. |
| `enbseries_guardia` | Handles a known failure when ENB cannot create its internal effects. |
| `zzENBParametros` | Additional game parameters for the ENB effects. |
| `RendimientoHiloPrincipal` *(optional)* | Reduces repeated system calls and failed file/folder lookups. |

## About antivirus warnings

Some automatic scanners (Google Drive, MediaFire, Windows Defender with `Wacatac.B!ml`) may flag the `.asi`
files as suspicious. `!ml` means a **machine-learning heuristic detection**, not a malware signature: an ASI
plugin is an unsigned DLL that patches the memory of `gta_sa.exe` (`VirtualProtect`), reads keys
(`GetAsyncKeyState`) and, in the case of `RendimientoHiloPrincipal`, intercepts system functions for its cache.
That is normal for a mod of this kind.

That is why the source code is here: **the package binaries can be rebuilt from this code**. Building with
`compilar.cmd` (MSVC 19.51) produces `.text`, `.data` and `.reloc` sections that are byte-identical; only the
timestamps differ. The exception is `LucesNocturnasENB`: its rain uses textures and a shader based on
Proper Shaders, which are not published here ([details](src/LucesNocturnasENB/FALTAN-ARCHIVOS-LLUVIA.md)).
The r2 `.asi` files also carry version information and a PE checksum (`tools/agregar_version.py`) without any
code change. The SHA-256 hash of every file is listed in `FILES.sha256` inside the ZIP.

No plugin uses the network, external processes, downloads or registry writes.

## Building

Requirements: Visual Studio 2026 (or 2022) with the x86 C++ tools, and the D3DX9 headers from the DirectX SDK
(June 2010) for `zzENBParametros`.

```bat
set DXSDK_INCLUDE=C:\path\to\DirectX SDK\Include
compilar.cmd
rem optional, as in r2:
python tools\agregar_version.py salida %TEMP%\rc
```

The `.asi` files are written to `salida\`.

## Credits

- **Oscar Vidal**: plugins, fixes, additional effects and integration.
- **Makarus**: SA DirectX 3.0, the required preset and the base of the modified shaders.
- **Inari Lú**: the required Unofficial Update and its original fixes.
- **Boris Vorontsov**: ENBSeries.
- **Junior_Djjr**: Complex Shadows Culling Fix (earlier technical reference); Proper Shaders, together with
  **Jéssica Natália** (reference for lighting and rain).
- **stephi.tl**: Deferred but Forward SkyGFX (reference for the effects studied).

## License

Oscar Vidal's code is released under the [MIT](LICENSE) license. It does not cover the third-party work listed above.

This repository does not contain the original preset, ENBSeries, Inari Lú's update or any game files.
