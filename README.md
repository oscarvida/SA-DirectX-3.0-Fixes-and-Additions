**Español** | [English](README.en.md)

# SA DirectX 3.0 — Fixes & Additions

Plugins ASI, shaders modificados y ajustes para **GTA San Andreas (PC 1.0 US)** con el preset
**SA DirectX 3.0** (Makarus, ENBSeries 0.430) y la *Unofficial Update* de Inari Lú.
Autor de los plugins, correcciones e integración: **Oscar Vidal**.

> **Descarga:** el paquete listo para instalar está en [Releases](../../releases).
> Instrucciones completas: [`docs/README_ES.txt`](docs/README_ES.txt) · [`docs/README_EN.txt`](docs/README_EN.txt).

## Plugins incluidos

| Plugin | Qué hace |
|---|---|
| `ShadowCasterFix` | Evita que las sombras complejas desaparezcan cuando el objeto que las proyecta sale de la cámara (primera cascada de ENB). |
| `LucesNocturnasENB` | Coronas dibujadas tras la composición de ENB, luz local en suelo y superficies, faros, frenos, haces de luz, lluvia iluminada, luces de disparos/explosiones/fuego. |
| `HumoLucesENB` | Iluminación del humo y nivel mínimo de luz en la hierba. |
| `enbseries_interiores` | Detección de interiores y correcciones de shaders (incluida la visibilidad del sol para los rayos del amanecer). |
| `ENBShaderCache` | Reutiliza los efectos compilados: los arranques siguientes pasan de ~40 s a ~2-3 s. |
| `enbseries_guardia` | Maneja un fallo conocido cuando ENB no puede crear sus efectos internos. |
| `zzENBParametros` | Parámetros adicionales del juego para los efectos de ENB. |
| `RendimientoHiloPrincipal` *(opcional)* | Reduce llamadas al sistema repetidas y búsquedas fallidas de archivos/carpetas. |

## Sobre las alertas de antivirus

Algunos escáneres automáticos (Google Drive, MediaFire, Windows Defender con `Wacatac.B!ml`) pueden marcar
los `.asi` como sospechosos. `!ml` indica una **detección heurística por aprendizaje automático**, no una firma
de malware: un plugin ASI es una DLL sin firmar que modifica la memoria de `gta_sa.exe` (`VirtualProtect`),
lee teclas (`GetAsyncKeyState`) y, en el caso de `RendimientoHiloPrincipal`, intercepta funciones del sistema
para su caché. Es lo normal en un mod de este tipo.

Por eso el código está aquí: **los binarios del paquete se reproducen desde este código**. Al compilar con
`compilar.cmd` (MSVC 19.51), las secciones `.text`, `.data` y `.reloc` salen idénticas byte a byte; solo cambian
las marcas de tiempo. La excepción es `LucesNocturnasENB`: su lluvia usa texturas y un sombreador basados en
Proper Shaders, que no se publican aquí ([detalles](src/LucesNocturnasENB/FALTAN-ARCHIVOS-LLUVIA.md)). Los `.asi` de la r2 llevan además información de versión y checksum PE
(`tools/agregar_version.py`), sin cambiar el código. Los hashes SHA-256 de cada archivo están en `FILES.sha256`
dentro del ZIP.

Ningún plugin usa red, procesos externos, descargas ni escritura en el registro.

## Compilar

Requisitos: Visual Studio 2026 (o 2022) con herramientas C++ x86, y las cabeceras D3DX9 del DirectX SDK
(junio 2010) para `zzENBParametros`.

```bat
set DXSDK_INCLUDE=C:\ruta\al\DirectX SDK\Include
compilar.cmd
rem opcional, como en la r2:
python tools\agregar_version.py salida %TEMP%\rc
```

Los `.asi` quedan en `salida\`.

## Créditos

- **Oscar Vidal**: plugins, correcciones, efectos adicionales e integración.
- **Makarus**: SA DirectX 3.0, preset requerido y base de los shaders modificados.
- **Inari Lú**: Unofficial Update requerida y sus correcciones originales.
- **Boris Vorontsov**: ENBSeries.
- **Junior_Djjr**: Complex Shadows Culling Fix (referencia técnica previa); Proper Shaders, junto a
  **Jéssica Natália** (referencia de iluminación y lluvia).
- **stephi.tl**: Deferred but Forward SkyGFX (referencia de los efectos estudiados).

## Licencia

Código de Oscar Vidal bajo licencia [MIT](LICENSE). No cubre el trabajo de terceros mencionado arriba.

Este repositorio no contiene el preset original, ENBSeries, la actualización de Inari Lú ni archivos del juego.

