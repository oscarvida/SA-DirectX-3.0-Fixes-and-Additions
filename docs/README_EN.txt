SA DIRECTX 3.0 — FIXES & ADDITIONS
Fixes, additional effects and integration: Oscar Vidal
Package prepared on November 5, 2026

REVISION r1 (November 5, 2026)
===============================
The only code change is LucesNocturnasENB.SA.asi (2.0k-r1): the screenshot folder no
longer defaults to an author-specific path, and a relative folder is created
inside the game folder. Text line endings were also fixed.
Details and hashes in CHANGES_r1.txt.

ABOUT
=====
An add-on for the classic PC version of GTA San Andreas, built for SA DirectX
3.0 with Inari Lú's unofficial update as its base. It keeps that preset and
extends it through custom plugins, modified shaders and integration settings.

This download does NOT contain the original preset or Inari Lú's full update.
Download and install both separately before applying this add-on. The game,
replacement maps, vehicles and a complete modpack are not included either.

FEATURES
========
- ShadowCasterFix: addresses complex shadows disappearing when their casting
  objects leave the camera view. The game-side visibility fix keeps relevant
  casters available within ENB's first shadow cascade.
- LucesNocturnasENB: light coronas drawn after ENB composition; local lighting
  on the ground and surfaces; headlights, brake and reverse lights; light
  beams, wet-surface response and lights from gunfire, explosions and fire.
- Fog and sky: fog density shaped by weather/time, visible overcast skies at
  night and changes to the daytime clear-weather cloud layer.
- Rain: lit, refractive drops, depth-tested splashes and checks for interiors
  and overhead cover.
- Smoke and grass: smoke lighting and a minimum grass-light level, especially
  useful on maps with dark collision lighting.
- Interiors and dawn: interior detection and shader fixes, including the sun
  visibility check used for dawn god rays.
- ENBShaderCache: reuses compiled effects to speed up subsequent launches.
  The initial shader compilation is still necessary.
- enbseries_guardia: handles a known failure when ENB cannot create internal
  effects. It is not a general-purpose fix for every possible game crash.
- Optional RendimientoHiloPrincipal: reduces selected repeated system calls
  and failed file/directory lookups.

REQUIREMENTS AND DOWNLOADS
=========================
1. Classic GTA San Andreas for Windows, 32-bit 1.0 US executable. This package
   targets single player, not Definitive Edition, Android/iOS, SA:MP or MTA.
2. A working ASI Loader and Mod Loader. Install CLEO as required by the
   original preset's instructions. These loaders are not bundled here.
3. The legacy DirectX 9 components needed by ENB, including d3dx9_43.dll.
   Having DirectX 11/12 does not necessarily provide those legacy files.
4. Makarus's SA DirectX 3.0 beta, Single Player / Ultra version,
   using ENBSeries 0.430, the base used to prepare this package:
   https://www.patreon.com/c/sadx3/posts
5. SA DirectX 3.0 — Unofficial Update By Inari Lú, Ultra variant:
   https://www.gtainside.com/sanandreas/mods/213892-sa-directx-3-0-unofficial-update-by-inari-l/

Those first two graphical layers supply the runtime, unchanged resources
and Inari's corrected models. This add-on contains only new or modified files
to apply on top. Deferred but Forward SkyGFX and Proper Shaders are not
runtime dependencies of this add-on.

INSTALLATION
============
1. Close the game. Back up your existing graphical files before replacing
   anything. Keep a copy of your personal INI settings as well.
2. Install SA DirectX 3.0 following Makarus's instructions. Use the contents
   of Single Player / Ultra, not the entire downloaded package folder.
3. Install Inari Lú's unofficial update next, using its Ultra variant,
   including its resources and prelighting fixes. Accept its replacements.
4. Open Copy-to-game-folder in this download. Copy its CONTENTS into the
   folder containing gta_sa.exe, accepting replacements. Do not place the
   containing folder itself inside the game directory.

   Example of the resulting locations:
   GTA San Andreas\gta_sa.exe
   GTA San Andreas\ENBShaderCache.asi
   GTA San Andreas\enbseries\AportesENB.fx
   GTA San Andreas\modloader\_ARREGLO sombras ENB al girar la camara\ShadowCasterFix.asi

5. Start the game and check the installation. The first launch may take
   longer while effects compile. Later launches reuse the shader cache.
6. Optional-performance is separate. Test the graphical part first. To add
   the optional plugin, copy the contents of Optional-performance into the
   same game directory. This adds only its modloader folder.

Order: original DirectX 3.0 -> Inari Lú's update -> this add-on.
If you reinstall either dependency afterward, reapply the add-on last so its
modified shaders are not overwritten.

The README files, LibertyCity descriptions, Screenshots and FILES.sha256 are
documentation/presentation material; do not copy them into the game folder.
Do not simultaneously install another graphics engine replacing the same
rendering passes, such as Deferred but Forward SkyGFX or Proper Shaders.

INCLUDED CONTROLS
=================
Insert        Toggle ShadowCasterFix.
Home          Toggle LucesNocturnasENB local lights.
F7            Toggle the light-corona rendering order.
F8            Toggle the version 2.0 lighting features.
Shift+Enter   Open the ENB editor.
Shift+F12     Enable/disable ENB.

These are the values in the supplied INI files. If another mod uses those
keys, change TeclaAlternar in ShadowCasterFix.ini or [Teclas] in
LucesNocturnasENB.SA.ini.

SETTINGS, TROUBLESHOOTING AND REMOVAL
====================================
- Each plugin's INI is beside its ASI. Edit it with the game closed. Modules
  using Activo can be disabled with Activo=0; ENBShaderCache uses Enabled=0.
  Keep the original section and key names.
- ShadowCasterFix.ini: Ajuste=B is the configured mode. The fix is limited
  to the first cascade; distant shadows can still be clipped and there is a
  rendering cost. No general FPS increase is promised.
- LucesNocturnasENB.SA.ini: GananciaFarolas/GananciaFaros adjust intensity.
  Individual sections control lights, beams and rain separately.
- HumoLucesENB.SA.ini includes [Humo] and [Pasto]. The minimum grass-light
  values can be adjusted for other maps.
- ENBShaderCache.ini: Carpeta=enbseries\cache_efectos. If you change the
  DirectX runtime or need a fresh cache, delete only that folder with the
  game closed; it will be regenerated. No precompiled cache is bundled.
- If an effect is missing, inspect ENBShaderCache.log, enbseries_guardia.log,
  ShadowCasterFix.log and the plugin logs beside their respective files.
- To uninstall, restore your backed-up files and remove this add-on's new
  plugins. Deleting the ASI files alone does not restore replaced shaders.
  FILES.sha256 lists the delivered files.
- To remove only the optional plugin, delete its _ARREGLO rendimiento hilo
  principal folder from modloader or set Activo=0 in RendimientoHiloPrincipal.ini.

SCREENSHOTS AND SCOPE
====================
The 12 images in Screenshots come from real gameplay recordings. Some use
GTA Mixed and other visual assets from the author's installation; those maps
and assets are not included. OFF/ON pairs identify the state shown, without
promising identical results on every installation.
Performance, light-emitting models and the final appearance depend on your
hardware and other mods. This package has been checked on disk; a new clean
GTA installation/playtest was not performed during this packaging session.

CREDITS
=======
Oscar Vidal: custom plugins, fixes, additional effects and integration.
Makarus: SA DirectX 3.0, the required preset and base for the modified shaders.
Inari Lú: the required unofficial update and its original fixes.
Boris Vorontsov: ENBSeries.
Junior_Djjr: Complex Shadows Culling Fix, earlier technical reference.
stephi.tl: Deferred but Forward SkyGFX, reference for the effects studied.
Junior_Djjr and Jéssica Natália: Proper Shaders, reference for lighting and
rain effects studied during development.
The technical references above are not additional runtime requirements.
